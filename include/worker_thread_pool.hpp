#pragma once

#include "packet.hpp"
#include "thread_safe_queue.hpp"
#include "packet_validator.hpp"
#include "packet_classifier.hpp"
#include "packet_processor.hpp"
#include "metrics_collector.hpp"

#include <vector>
#include <thread>
#include <atomic>
#include <memory>

namespace packet_engine {

/**
 * @brief WorkerThreadPool
 *
 * Manages a fixed pool of consumer threads processing packets concurrently from
 * a shared bounded ThreadSafeQueue.
 *
 * Why a Thread Pool instead of one thread per packet?
 * 1. Thread Creation Overhead: Creating an OS thread involves allocating a kernel task_struct,
 *    an 8MB virtual stack, and context switching overhead (~1-10 microseconds). At 100,000 packets/sec,
 *    spawning one thread per packet would cause extreme kernel thrashing and out-of-memory (OOM) errors.
 * 2. CPU Core Saturation: A thread pool sized to hardware concurrency (# of physical/logical cores)
 *    maximizes L1/L2 cache locality and eliminates excessive preemptive context switches.
 */
class WorkerThreadPool {
public:
    WorkerThreadPool(size_t worker_count,
                     ThreadSafeQueue<Packet>& queue,
                     MetricsCollector& metrics,
                     std::shared_ptr<PacketProcessor> processor = nullptr,
                     std::shared_ptr<PacketValidator> validator = nullptr,
                     std::shared_ptr<PacketClassifier> classifier = nullptr);

    ~WorkerThreadPool();

    WorkerThreadPool(const WorkerThreadPool&) = delete;
    WorkerThreadPool& operator=(const WorkerThreadPool&) = delete;

    void start();
    void stop();
    void join();

    bool isRunning() const noexcept { return is_running_.load(std::memory_order_relaxed); }
    size_t getWorkerCount() const noexcept { return worker_count_; }

private:
    void workerRoutine(size_t worker_id);

    const size_t worker_count_;
    ThreadSafeQueue<Packet>& queue_;
    MetricsCollector& metrics_;

    std::shared_ptr<PacketProcessor> processor_;
    std::shared_ptr<PacketValidator> validator_;
    std::shared_ptr<PacketClassifier> classifier_;

    std::vector<std::thread> workers_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> stop_requested_{false};
};

} // namespace packet_engine
