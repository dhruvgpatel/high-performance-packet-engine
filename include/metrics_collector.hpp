#pragma once

#include "packet_classifier.hpp"
#include "packet_processor.hpp"

#include <cstdint>
#include <atomic>
#include <vector>
#include <string>
#include <mutex>
#include <chrono>

namespace packet_engine {

struct MetricsSnapshot {
    uint64_t total_received{0};
    uint64_t total_processed{0};
    uint64_t valid_packets{0};
    uint64_t invalid_packets{0};
    uint64_t dropped_packets{0};
    uint64_t total_bytes_received{0};
    uint64_t total_bytes_processed{0};

    uint64_t control_packets{0};
    uint64_t data_packets{0};
    uint64_t ack_packets{0};
    uint64_t high_priority_packets{0};

    double elapsed_seconds{0.0};
    double throughput_pps{0.0};
    double throughput_mbps{0.0};

    double avg_latency_us{0.0};
    double p50_latency_us{0.0};
    double p95_latency_us{0.0};
    double p99_latency_us{0.0};
    double min_latency_us{0.0};
    double max_latency_us{0.0};

    double avg_queue_dwell_us{0.0};
    double avg_processing_time_us{0.0};

    std::vector<uint64_t> per_worker_processed;
};

class MetricsCollector {
public:
    explicit MetricsCollector(size_t worker_count = 4, size_t sample_capacity = 100000);

    void recordReceived(size_t bytes = 0);
    void recordDropped();
    void recordProcessed(size_t worker_id, const ProcessingResult& result, size_t payload_bytes);

    void reset();

    MetricsSnapshot getSnapshot() const;
    std::string generateSummaryString() const;
    std::string generateJsonReport() const;

private:
    // Align atomic counters to cache lines (64 bytes) to eliminate False Sharing
    alignas(64) std::atomic<uint64_t> total_received_{0};
    alignas(64) std::atomic<uint64_t> total_processed_{0};
    alignas(64) std::atomic<uint64_t> valid_packets_{0};
    alignas(64) std::atomic<uint64_t> invalid_packets_{0};
    alignas(64) std::atomic<uint64_t> dropped_packets_{0};
    alignas(64) std::atomic<uint64_t> total_bytes_received_{0};
    alignas(64) std::atomic<uint64_t> total_bytes_processed_{0};

    alignas(64) std::atomic<uint64_t> control_packets_{0};
    alignas(64) std::atomic<uint64_t> data_packets_{0};
    alignas(64) std::atomic<uint64_t> ack_packets_{0};
    alignas(64) std::atomic<uint64_t> high_priority_packets_{0};

    // Per-worker counters
    std::vector<std::atomic<uint64_t>> per_worker_processed_;

    // Latency accumulation
    alignas(64) std::atomic<uint64_t> total_latency_ns_sum_{0};
    alignas(64) std::atomic<uint64_t> total_queue_dwell_ns_sum_{0};
    alignas(64) std::atomic<uint64_t> total_processing_ns_sum_{0};

    // Latency reservoir for percentiles
    mutable std::mutex latency_mutex_;
    std::vector<uint64_t> latency_samples_ns_;
    size_t sample_capacity_;
    std::atomic<uint64_t> sample_count_{0};

    std::chrono::high_resolution_clock::time_point start_time_;
};

} // namespace packet_engine
