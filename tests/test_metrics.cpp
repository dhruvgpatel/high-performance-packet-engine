#include <gtest/gtest.h>
#include "metrics_collector.hpp"

#include <thread>
#include <vector>

using namespace packet_engine;

TEST(MetricsCollectorTest, BasicCounters) {
    MetricsCollector metrics(2);

    metrics.recordReceived(512);
    metrics.recordReceived(1024);
    metrics.recordDropped();

    ProcessingResult res;
    res.packet_id = 1;
    res.valid = true;
    res.category = PacketCategory::DATA;
    res.processing_time_ns = 5000;
    res.total_latency_ns = 15000;

    metrics.recordProcessed(0, res, 512);

    MetricsSnapshot s = metrics.getSnapshot();
    EXPECT_EQ(s.total_received, 2);
    EXPECT_EQ(s.total_bytes_received, 1536);
    EXPECT_EQ(s.dropped_packets, 1);
    EXPECT_EQ(s.total_processed, 1);
    EXPECT_EQ(s.valid_packets, 1);
    EXPECT_EQ(s.data_packets, 1);
    EXPECT_EQ(s.per_worker_processed[0], 1);
    EXPECT_EQ(s.per_worker_processed[1], 0);
}

TEST(MetricsCollectorTest, ConcurrentUpdates) {
    const size_t num_threads = 4;
    const size_t iters_per_thread = 10000;

    MetricsCollector metrics(num_threads);

    std::vector<std::thread> threads;
    for (size_t t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (size_t i = 0; i < iters_per_thread; ++i) {
                metrics.recordReceived(100);

                ProcessingResult res;
                res.packet_id = i;
                res.valid = true;
                res.category = PacketCategory::HIGH_PRIORITY;
                res.processing_time_ns = 1000;
                res.total_latency_ns = 5000;

                metrics.recordProcessed(t, res, 100);
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    MetricsSnapshot s = metrics.getSnapshot();
    EXPECT_EQ(s.total_received, num_threads * iters_per_thread);
    EXPECT_EQ(s.total_processed, num_threads * iters_per_thread);
    EXPECT_EQ(s.high_priority_packets, num_threads * iters_per_thread);
    for (size_t t = 0; t < num_threads; ++t) {
        EXPECT_EQ(s.per_worker_processed[t], iters_per_thread);
    }
}

TEST(MetricsCollectorTest, JsonReportContainsRequiredFields) {
    MetricsCollector metrics(1);
    ProcessingResult res;
    res.valid = true;
    res.category = PacketCategory::DATA;
    metrics.recordProcessed(0, res, 128);

    std::string json = metrics.generateJsonReport();
    EXPECT_NE(json.find("\"total_processed\": 1"), std::string::npos);
    EXPECT_NE(json.find("\"throughput_pps\""), std::string::npos);
    EXPECT_NE(json.find("\"p95_latency_us\""), std::string::npos);
}
