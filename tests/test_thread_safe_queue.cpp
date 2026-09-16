#include <gtest/gtest.h>
#include "thread_safe_queue.hpp"

#include <thread>
#include <vector>
#include <numeric>
#include <atomic>

using namespace packet_engine;

TEST(ThreadSafeQueueTest, BasicOperations) {
    ThreadSafeQueue<int> q(5);

    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0);
    EXPECT_EQ(q.capacity(), 5);

    EXPECT_TRUE(q.push(10));
    EXPECT_TRUE(q.push(20));
    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 2);

    int val = 0;
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 10);
    EXPECT_EQ(q.size(), 1);

    EXPECT_TRUE(q.wait_and_pop(val));
    EXPECT_EQ(val, 20);
    EXPECT_TRUE(q.empty());

    // try_pop on empty queue
    EXPECT_FALSE(q.try_pop(val));
}

TEST(ThreadSafeQueueTest, BoundedCapacityTryPush) {
    ThreadSafeQueue<int> q(2);

    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_FALSE(q.try_push(3)); // Exceeds capacity

    int val = 0;
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 1);

    EXPECT_TRUE(q.try_push(3)); // Now has space
}

TEST(ThreadSafeQueueTest, MultiProducerMultiConsumerIntegrity) {
    const size_t num_producers = 4;
    const size_t num_consumers = 4;
    const size_t items_per_producer = 25000;
    const size_t total_items = num_producers * items_per_producer;

    ThreadSafeQueue<uint64_t> q(1000);
    std::atomic<uint64_t> consumed_count{0};
    std::atomic<uint64_t> consumed_sum{0};

    // Consumers
    std::vector<std::thread> consumers;
    for (size_t c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            uint64_t item = 0;
            while (q.wait_and_pop(item)) {
                consumed_count.fetch_add(1, std::memory_order_relaxed);
                consumed_sum.fetch_add(item, std::memory_order_relaxed);
            }
        });
    }

    // Producers
    std::vector<std::thread> producers;
    for (size_t p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            for (size_t i = 1; i <= items_per_producer; ++i) {
                uint64_t val = (p * items_per_producer) + i;
                q.push(val);
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }

    // Signal shutdown and join consumers
    q.shutdown();
    for (auto& t : consumers) {
        t.join();
    }

    EXPECT_EQ(consumed_count.load(), total_items);

    // Verify mathematical sum (1 + 2 + ... + N)
    uint64_t expected_sum = 0;
    for (uint64_t i = 1; i <= total_items; ++i) {
        expected_sum += i;
    }
    EXPECT_EQ(consumed_sum.load(), expected_sum);
}

TEST(ThreadSafeQueueTest, ShutdownWakesBlockedConsumers) {
    ThreadSafeQueue<int> q(10);
    std::atomic<bool> thread_exited{false};

    std::thread consumer([&]() {
        int val = 0;
        bool result = q.wait_and_pop(val);
        EXPECT_FALSE(result);
        thread_exited.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(thread_exited.load());

    q.shutdown();
    consumer.join();
    EXPECT_TRUE(thread_exited.load());
    EXPECT_TRUE(q.is_shutdown());
}
