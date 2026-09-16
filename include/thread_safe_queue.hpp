#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>
#include <chrono>
#include <utility>
#include <cstddef>

namespace packet_engine {

/**
 * @brief ThreadSafeQueue
 *
 * A bounded, thread-safe, multi-producer multi-consumer (MPMC) FIFO queue.
 *
 * Design & Concurrency Invariants:
 * 1. Mutual Exclusion: All state access to `queue_` is protected by `mutex_`.
 * 2. Producer-Consumer Synchronization:
 *    - Consumers block on `not_empty_cv_` when `queue_.empty()`.
 *    - Producers block on `not_full_cv_` when `queue_.size() >= max_capacity_`.
 *    - Spurious wakeups are guarded against via predicate lambdas.
 * 3. Why condition variables instead of busy-polling?
 *    Busy-polling (`while (queue.empty()) {}`) consumes 100% of a CPU core,
 *    causing cache-line contention, power consumption spikes, and starving other threads.
 *    `std::condition_variable` puts waiting threads into a dormant kernel wait-state (futex on Linux),
 *    yielding CPU time slices to productive worker and receiver threads.
 * 4. Zero-copy / Move Semantics:
 *    Accepts and delivers `T` via `std::move` to eliminate deep memory allocations
 *    for vector payloads in high-throughput network workloads.
 * 5. Graceful Shutdown:
 *    Calling `shutdown()` sets `is_shutdown_ = true` and invokes `notify_all()` on both
 *    condition variables, ensuring no threads remain blocked indefinitely in deadlocks.
 */
template <typename T>
class ThreadSafeQueue {
public:
    explicit ThreadSafeQueue(size_t max_capacity = 10000)
        : max_capacity_(max_capacity > 0 ? max_capacity : 1),
          is_shutdown_(false) {}

    ~ThreadSafeQueue() {
        shutdown();
    }

    // Disable copy semantics to prevent slicing or race conditions
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    /**
     * @brief Blocking push. Blocks if queue is full until space becomes available or shutdown.
     * @return true if pushed, false if shutdown occurred before pushing.
     */
    bool push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_cv_.wait(lock, [this]() {
            return is_shutdown_.load(std::memory_order_relaxed) || (queue_.size() < max_capacity_);
        });

        if (is_shutdown_.load(std::memory_order_relaxed)) {
            return false;
        }

        queue_.push(std::move(item));
        lock.unlock();
        not_empty_cv_.notify_one();
        return true;
    }

    /**
     * @brief Non-blocking push.
     * @return true if pushed, false if queue is full or shut down.
     */
    bool try_push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (is_shutdown_.load(std::memory_order_relaxed) || queue_.size() >= max_capacity_) {
            return false;
        }

        queue_.push(std::move(item));
        lock.unlock();
        not_empty_cv_.notify_one();
        return true;
    }

    /**
     * @brief Blocking pop. Blocks until an item is available or queue is shut down.
     * @param value Output reference where popped item is moved.
     * @return true if an item was successfully retrieved, false if queue was shut down and empty.
     */
    bool wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_cv_.wait(lock, [this]() {
            return is_shutdown_.load(std::memory_order_relaxed) || !queue_.empty();
        });

        if (queue_.empty()) {
            return false; // Queue shut down and drained
        }

        value = std::move(queue_.front());
        queue_.pop();
        lock.unlock();
        not_full_cv_.notify_one();
        return true;
    }

    /**
     * @brief Timed blocking pop.
     * @return true if item retrieved within timeout, false otherwise.
     */
    template <typename Rep, typename Period>
    bool wait_for_pop(T& value, const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        bool ready = not_empty_cv_.wait_for(lock, timeout, [this]() {
            return is_shutdown_.load(std::memory_order_relaxed) || !queue_.empty();
        });

        if (!ready || queue_.empty()) {
            return false;
        }

        value = std::move(queue_.front());
        queue_.pop();
        lock.unlock();
        not_full_cv_.notify_one();
        return true;
    }

    /**
     * @brief Non-blocking pop.
     * @return true if item retrieved, false if queue is empty.
     */
    bool try_pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }

        value = std::move(queue_.front());
        queue_.pop();
        lock.unlock();
        not_full_cv_.notify_one();
        return true;
    }

    /**
     * @brief Check if queue is empty.
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    /**
     * @brief Get current number of items.
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /**
     * @brief Get queue capacity limit.
     */
    size_t capacity() const noexcept {
        return max_capacity_;
    }

    /**
     * @brief Signals shutdown to all waiting threads.
     */
    void shutdown() noexcept {
        bool expected = false;
        if (is_shutdown_.compare_exchange_strong(expected, true)) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
            }
            not_empty_cv_.notify_all();
            not_full_cv_.notify_all();
        }
    }

    /**
     * @brief Checks if queue has received shutdown signal.
     */
    bool is_shutdown() const noexcept {
        return is_shutdown_.load(std::memory_order_acquire);
    }

    /**
     * @brief Clears all remaining items in the queue.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty_queue;
        std::swap(queue_, empty_queue);
        not_full_cv_.notify_all();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_cv_;
    std::condition_variable not_full_cv_;
    std::queue<T> queue_;
    const size_t max_capacity_;
    std::atomic<bool> is_shutdown_;
};

} // namespace packet_engine
