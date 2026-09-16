#include "packet_processor.hpp"
#include <thread>

namespace packet_engine {

PacketProcessor::PacketProcessor(bool enable_checksum,
                                 size_t compute_iterations,
                                 uint32_t simulated_delay_us)
    : enable_checksum_(enable_checksum),
      compute_iterations_(compute_iterations),
      simulated_delay_us_(simulated_delay_us) {}

uint32_t PacketProcessor::executeComputeWorkload(const std::vector<uint8_t>& payload, size_t iterations) const {
    if (payload.empty()) {
        return 0;
    }

    // High-performance CPU compute loop (FNV-1a / Murmur-like round mixing)
    uint32_t hash = 2166136261u;
    for (size_t iter = 0; iter < iterations; ++iter) {
        for (uint8_t byte : payload) {
            hash ^= byte;
            hash *= 16777619u;
            hash = (hash << 13) | (hash >> 19); // Bitwise rotate
        }
    }
    return hash;
}

ProcessingResult PacketProcessor::process(Packet& packet,
                                          const PacketValidator& validator,
                                          const PacketClassifier& classifier) {
    auto start_proc = std::chrono::high_resolution_clock::now();
    uint64_t start_proc_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(start_proc.time_since_epoch()).count());

    ProcessingResult result;
    result.packet_id = packet.getId();

    // 1. Validation
    ValidationResult val_res = validator.validate(packet);
    result.valid = val_res.is_valid;
    result.error_code = val_res.error_code;

    if (!val_res.is_valid) {
        result.category = PacketCategory::INVALID;
    } else {
        // 2. Classification
        result.category = classifier.classify(packet);

        // 3. Workload execution
        size_t iters = compute_iterations_.load(std::memory_order_relaxed);
        result.computed_hash = executeComputeWorkload(packet.getPayload(), iters);

        // Optional micro-delay for realistic simulation benchmarking
        uint32_t delay = simulated_delay_us_.load(std::memory_order_relaxed);
        if (delay > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(delay));
        }
    }

    auto end_proc = std::chrono::high_resolution_clock::now();
    uint64_t end_proc_ns = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end_proc.time_since_epoch()).count());

    result.processing_time_ns = end_proc_ns >= start_proc_ns ? (end_proc_ns - start_proc_ns) : 0;

    if (packet.getDequeueTimestampNs() >= packet.getEnqueueTimestampNs() && packet.getEnqueueTimestampNs() > 0) {
        result.queue_dwell_time_ns = packet.getDequeueTimestampNs() - packet.getEnqueueTimestampNs();
    }

    if (end_proc_ns >= packet.getTimestampNs() && packet.getTimestampNs() > 0) {
        result.total_latency_ns = end_proc_ns - packet.getTimestampNs();
    } else {
        result.total_latency_ns = result.processing_time_ns + result.queue_dwell_time_ns;
    }

    return result;
}

} // namespace packet_engine
