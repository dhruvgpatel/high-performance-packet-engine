#include <gtest/gtest.h>
#include "worker_thread_pool.hpp"

using namespace packet_engine;

TEST(WorkerThreadPoolTest, ProcessAllPacketsWithoutLoss) {
    const size_t num_workers = 4;
    const size_t num_packets = 5000;

    ThreadSafeQueue<Packet> queue(10000);
    MetricsCollector metrics(num_workers);

    WorkerThreadPool pool(num_workers, queue, metrics);
    pool.start();
    EXPECT_TRUE(pool.isRunning());

    for (size_t i = 1; i <= num_packets; ++i) {
        Packet pkt(i, "192.168.1.1", "10.0.0.1", 1000, 2000, IpProtocol::UDP, 0, {0x01, 0x02});
        queue.push(std::move(pkt));
    }

    // Stop pool (which shuts down queue, drains packets, and joins)
    pool.stop();
    EXPECT_FALSE(pool.isRunning());

    MetricsSnapshot s = metrics.getSnapshot();
    EXPECT_EQ(s.total_processed, num_packets);
    EXPECT_EQ(s.valid_packets, num_packets);
    EXPECT_EQ(s.dropped_packets, 0);

    uint64_t sum_worker_processed = 0;
    for (uint64_t count : s.per_worker_processed) {
        sum_worker_processed += count;
    }
    EXPECT_EQ(sum_worker_processed, num_packets);
}
