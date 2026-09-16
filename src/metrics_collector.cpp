#include "metrics_collector.hpp"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace packet_engine {

MetricsCollector::MetricsCollector(size_t worker_count, size_t sample_capacity)
    : per_worker_processed_(worker_count),
      sample_capacity_(sample_capacity),
      start_time_(std::chrono::high_resolution_clock::now()) {
    latency_samples_ns_.reserve(sample_capacity_);
    for (size_t i = 0; i < worker_count; ++i) {
        per_worker_processed_[i].store(0, std::memory_order_relaxed);
    }
}

void MetricsCollector::reset() {
    total_received_.store(0, std::memory_order_relaxed);
    total_processed_.store(0, std::memory_order_relaxed);
    valid_packets_.store(0, std::memory_order_relaxed);
    invalid_packets_.store(0, std::memory_order_relaxed);
    dropped_packets_.store(0, std::memory_order_relaxed);
    total_bytes_received_.store(0, std::memory_order_relaxed);
    total_bytes_processed_.store(0, std::memory_order_relaxed);

    control_packets_.store(0, std::memory_order_relaxed);
    data_packets_.store(0, std::memory_order_relaxed);
    ack_packets_.store(0, std::memory_order_relaxed);
    high_priority_packets_.store(0, std::memory_order_relaxed);

    for (auto& counter : per_worker_processed_) {
        counter.store(0, std::memory_order_relaxed);
    }

    total_latency_ns_sum_.store(0, std::memory_order_relaxed);
    total_queue_dwell_ns_sum_.store(0, std::memory_order_relaxed);
    total_processing_ns_sum_.store(0, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        latency_samples_ns_.clear();
    }
    sample_count_.store(0, std::memory_order_relaxed);
    start_time_ = std::chrono::high_resolution_clock::now();
}

void MetricsCollector::recordReceived(size_t bytes) {
    total_received_.fetch_add(1, std::memory_order_relaxed);
    if (bytes > 0) {
        total_bytes_received_.fetch_add(bytes, std::memory_order_relaxed);
    }
}

void MetricsCollector::recordDropped() {
    dropped_packets_.fetch_add(1, std::memory_order_relaxed);
}

void MetricsCollector::recordProcessed(size_t worker_id, const ProcessingResult& result, size_t payload_bytes) {
    total_processed_.fetch_add(1, std::memory_order_relaxed);
    total_bytes_processed_.fetch_add(payload_bytes, std::memory_order_relaxed);

    if (worker_id < per_worker_processed_.size()) {
        per_worker_processed_[worker_id].fetch_add(1, std::memory_order_relaxed);
    }

    if (result.valid) {
        valid_packets_.fetch_add(1, std::memory_order_relaxed);
        switch (result.category) {
            case PacketCategory::CONTROL:
                control_packets_.fetch_add(1, std::memory_order_relaxed);
                break;
            case PacketCategory::DATA:
                data_packets_.fetch_add(1, std::memory_order_relaxed);
                break;
            case PacketCategory::ACK:
                ack_packets_.fetch_add(1, std::memory_order_relaxed);
                break;
            case PacketCategory::HIGH_PRIORITY:
                high_priority_packets_.fetch_add(1, std::memory_order_relaxed);
                break;
            default:
                break;
        }
    } else {
        invalid_packets_.fetch_add(1, std::memory_order_relaxed);
    }

    total_latency_ns_sum_.fetch_add(result.total_latency_ns, std::memory_order_relaxed);
    total_queue_dwell_ns_sum_.fetch_add(result.queue_dwell_time_ns, std::memory_order_relaxed);
    total_processing_ns_sum_.fetch_add(result.processing_time_ns, std::memory_order_relaxed);

    // Sample latency for percentiles (subsampling if high volume to avoid mutex contention)
    uint64_t count = sample_count_.fetch_add(1, std::memory_order_relaxed);
    if (count % 4 == 0) { // Subsample 25% for minimal lock overhead
        std::lock_guard<std::mutex> lock(latency_mutex_);
        if (latency_samples_ns_.size() < sample_capacity_) {
            latency_samples_ns_.push_back(result.total_latency_ns);
        }
    }
}

MetricsSnapshot MetricsCollector::getSnapshot() const {
    MetricsSnapshot snapshot;
    auto now = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(now - start_time_).count();
    if (elapsed <= 0.000001) elapsed = 0.000001;

    snapshot.elapsed_seconds = elapsed;
    snapshot.total_received = total_received_.load(std::memory_order_relaxed);
    snapshot.total_processed = total_processed_.load(std::memory_order_relaxed);
    snapshot.valid_packets = valid_packets_.load(std::memory_order_relaxed);
    snapshot.invalid_packets = invalid_packets_.load(std::memory_order_relaxed);
    snapshot.dropped_packets = dropped_packets_.load(std::memory_order_relaxed);
    snapshot.total_bytes_received = total_bytes_received_.load(std::memory_order_relaxed);
    snapshot.total_bytes_processed = total_bytes_processed_.load(std::memory_order_relaxed);

    snapshot.control_packets = control_packets_.load(std::memory_order_relaxed);
    snapshot.data_packets = data_packets_.load(std::memory_order_relaxed);
    snapshot.ack_packets = ack_packets_.load(std::memory_order_relaxed);
    snapshot.high_priority_packets = high_priority_packets_.load(std::memory_order_relaxed);

    snapshot.throughput_pps = static_cast<double>(snapshot.total_processed) / elapsed;
    snapshot.throughput_mbps = (static_cast<double>(snapshot.total_bytes_processed) * 8.0) / (elapsed * 1000000.0);

    if (snapshot.total_processed > 0) {
        snapshot.avg_latency_us = static_cast<double>(total_latency_ns_sum_.load(std::memory_order_relaxed)) /
                                  (static_cast<double>(snapshot.total_processed) * 1000.0);
        snapshot.avg_queue_dwell_us = static_cast<double>(total_queue_dwell_ns_sum_.load(std::memory_order_relaxed)) /
                                      (static_cast<double>(snapshot.total_processed) * 1000.0);
        snapshot.avg_processing_time_us = static_cast<double>(total_processing_ns_sum_.load(std::memory_order_relaxed)) /
                                          (static_cast<double>(snapshot.total_processed) * 1000.0);
    }

    snapshot.per_worker_processed.resize(per_worker_processed_.size());
    for (size_t i = 0; i < per_worker_processed_.size(); ++i) {
        snapshot.per_worker_processed[i] = per_worker_processed_[i].load(std::memory_order_relaxed);
    }

    // Percentiles
    std::vector<uint64_t> samples_copy;
    {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        samples_copy = latency_samples_ns_;
    }

    if (!samples_copy.empty()) {
        std::sort(samples_copy.begin(), samples_copy.end());
        size_t n = samples_copy.size();
        snapshot.min_latency_us = static_cast<double>(samples_copy.front()) / 1000.0;
        snapshot.max_latency_us = static_cast<double>(samples_copy.back()) / 1000.0;
        snapshot.p50_latency_us = static_cast<double>(samples_copy[static_cast<size_t>(0.50 * static_cast<double>(n - 1))]) / 1000.0;
        snapshot.p95_latency_us = static_cast<double>(samples_copy[static_cast<size_t>(0.95 * static_cast<double>(n - 1))]) / 1000.0;
        snapshot.p99_latency_us = static_cast<double>(samples_copy[static_cast<size_t>(0.99 * static_cast<double>(n - 1))]) / 1000.0;
    }

    return snapshot;
}

std::string MetricsCollector::generateSummaryString() const {
    MetricsSnapshot s = getSnapshot();
    std::ostringstream oss;
    oss << "\n=======================================================\n"
        << "           PACKET ENGINE PERFORMANCE REPORT             \n"
        << "=======================================================\n"
        << " Elapsed Time       : " << std::fixed << std::setprecision(2) << s.elapsed_seconds << " s\n"
        << " Total Ingested     : " << s.total_received << " packets (" << (static_cast<double>(s.total_bytes_received) / 1024.0 / 1024.0) << " MB)\n"
        << " Total Processed    : " << s.total_processed << " packets\n"
        << " Valid Packets      : " << s.valid_packets << "\n"
        << " Invalid Packets    : " << s.invalid_packets << "\n"
        << " Dropped Packets    : " << s.dropped_packets << "\n"
        << "-------------------------------------------------------\n"
        << " Throughput         : " << std::fixed << std::setprecision(0) << s.throughput_pps << " pkts/sec ("
        << std::setprecision(2) << s.throughput_mbps << " Mbps)\n"
        << " Avg Latency        : " << std::fixed << std::setprecision(2) << s.avg_latency_us << " us\n"
        << " Avg Queue Dwell    : " << s.avg_queue_dwell_us << " us\n"
        << " Avg Compute Time   : " << s.avg_processing_time_us << " us\n"
        << " p50 (Median) Latency: " << s.p50_latency_us << " us\n"
        << " p95 Latency        : " << s.p95_latency_us << " us\n"
        << " p99 Latency        : " << s.p99_latency_us << " us\n"
        << " Min / Max Latency  : " << s.min_latency_us << " / " << s.max_latency_us << " us\n"
        << "-------------------------------------------------------\n"
        << " Classification Breakdown:\n"
        << "   DATA             : " << s.data_packets << "\n"
        << "   HIGH_PRIORITY    : " << s.high_priority_packets << "\n"
        << "   CONTROL          : " << s.control_packets << "\n"
        << "   ACK              : " << s.ack_packets << "\n"
        << "-------------------------------------------------------\n"
        << " Worker Thread Load Distribution:\n";
    for (size_t i = 0; i < s.per_worker_processed.size(); ++i) {
        double pct = s.total_processed > 0 ? (100.0 * static_cast<double>(s.per_worker_processed[i]) / static_cast<double>(s.total_processed)) : 0.0;
        oss << "   Worker #" << i << " : " << s.per_worker_processed[i] << " pkts (" << std::fixed << std::setprecision(1) << pct << "%)\n";
    }
    oss << "=======================================================\n";
    return oss.str();
}

std::string MetricsCollector::generateJsonReport() const {
    MetricsSnapshot s = getSnapshot();
    std::ostringstream oss;
    oss << "{\n"
        << "  \"elapsed_seconds\": " << s.elapsed_seconds << ",\n"
        << "  \"total_received\": " << s.total_received << ",\n"
        << "  \"total_processed\": " << s.total_processed << ",\n"
        << "  \"valid_packets\": " << s.valid_packets << ",\n"
        << "  \"invalid_packets\": " << s.invalid_packets << ",\n"
        << "  \"dropped_packets\": " << s.dropped_packets << ",\n"
        << "  \"total_bytes_received\": " << s.total_bytes_received << ",\n"
        << "  \"total_bytes_processed\": " << s.total_bytes_processed << ",\n"
        << "  \"throughput_pps\": " << s.throughput_pps << ",\n"
        << "  \"throughput_mbps\": " << s.throughput_mbps << ",\n"
        << "  \"avg_latency_us\": " << s.avg_latency_us << ",\n"
        << "  \"p50_latency_us\": " << s.p50_latency_us << ",\n"
        << "  \"p95_latency_us\": " << s.p95_latency_us << ",\n"
        << "  \"p99_latency_us\": " << s.p99_latency_us << ",\n"
        << "  \"min_latency_us\": " << s.min_latency_us << ",\n"
        << "  \"max_latency_us\": " << s.max_latency_us << ",\n"
        << "  \"avg_queue_dwell_us\": " << s.avg_queue_dwell_us << ",\n"
        << "  \"avg_processing_time_us\": " << s.avg_processing_time_us << ",\n"
        << "  \"categories\": {\n"
        << "    \"data\": " << s.data_packets << ",\n"
        << "    \"high_priority\": " << s.high_priority_packets << ",\n"
        << "    \"control\": " << s.control_packets << ",\n"
        << "    \"ack\": " << s.ack_packets << "\n"
        << "  },\n"
        << "  \"worker_distribution\": [";
    for (size_t i = 0; i < s.per_worker_processed.size(); ++i) {
        oss << s.per_worker_processed[i] << (i + 1 < s.per_worker_processed.size() ? ", " : "");
    }
    oss << "]\n}";
    return oss.str();
}

} // namespace packet_engine
