#pragma once

#include "packet.hpp"
#include "packet_validator.hpp"
#include "packet_classifier.hpp"
#include <chrono>
#include <atomic>

namespace packet_engine {

struct ProcessingResult {
    uint64_t packet_id{0};
    bool valid{false};
    PacketCategory category{PacketCategory::INVALID};
    ValidationErrorCode error_code{ValidationErrorCode::OK};
    uint32_t computed_hash{0};
    uint64_t queue_dwell_time_ns{0};
    uint64_t processing_time_ns{0};
    uint64_t total_latency_ns{0};
};

class PacketProcessor {
public:
    PacketProcessor(bool enable_checksum = true,
                    size_t compute_iterations = 50,
                    uint32_t simulated_delay_us = 0);

    ProcessingResult process(Packet& packet, const PacketValidator& validator, const PacketClassifier& classifier);

    void setComputeIterations(size_t iters) noexcept {
        compute_iterations_.store(iters, std::memory_order_relaxed);
    }

    void setSimulatedDelayUs(uint32_t delay_us) noexcept {
        simulated_delay_us_.store(delay_us, std::memory_order_relaxed);
    }

    bool isChecksumEnabled() const noexcept { return enable_checksum_; }
    void setChecksumEnabled(bool enable) noexcept { enable_checksum_ = enable; }

private:
    uint32_t executeComputeWorkload(const std::vector<uint8_t>& payload, size_t iterations) const;

    bool enable_checksum_;
    std::atomic<size_t> compute_iterations_;
    std::atomic<uint32_t> simulated_delay_us_;
};

} // namespace packet_engine
