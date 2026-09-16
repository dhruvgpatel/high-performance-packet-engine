#include "packet_validator.hpp"

namespace packet_engine {

PacketValidator::PacketValidator(size_t min_payload_size,
                                 size_t max_payload_size,
                                 bool verify_checksum)
    : min_payload_size_(min_payload_size),
      max_payload_size_(max_payload_size),
      verify_checksum_(verify_checksum) {}

ValidationResult PacketValidator::validate(const Packet& packet) const {
    // 1. Check Payload Size Bounds
    size_t payload_len = packet.getPayloadSize();
    if (payload_len < min_payload_size_) {
        return ValidationResult::error(
            ValidationErrorCode::SIZE_UNDERFLOW,
            "Payload size (" + std::to_string(payload_len) +
            ") is below minimum (" + std::to_string(min_payload_size_) + ")");
    }
    if (payload_len > max_payload_size_) {
        return ValidationResult::error(
            ValidationErrorCode::SIZE_OVERFLOW,
            "Payload size (" + std::to_string(payload_len) +
            ") exceeds maximum (" + std::to_string(max_payload_size_) + ")");
    }

    // 2. Check Port Validity (Port 0 is reserved/invalid in standard transport frames)
    if (packet.getSrcPort() == 0) {
        return ValidationResult::error(ValidationErrorCode::INVALID_PORT, "Source port cannot be 0");
    }
    if (packet.getDstPort() == 0) {
        return ValidationResult::error(ValidationErrorCode::INVALID_PORT, "Destination port cannot be 0");
    }

    // 3. Check Protocol
    if (packet.getProtocol() == IpProtocol::UNKNOWN) {
        return ValidationResult::error(ValidationErrorCode::INVALID_PROTOCOL, "Unknown IP protocol");
    }

    // 4. Check IP Numerics
    if (packet.getSrcIpNumeric() == 0) {
        return ValidationResult::error(ValidationErrorCode::INVALID_IP, "Invalid source IP (0.0.0.0)");
    }
    if (packet.getDstIpNumeric() == 0) {
        return ValidationResult::error(ValidationErrorCode::INVALID_IP, "Invalid destination IP (0.0.0.0)");
    }

    // 5. Verify Checksum if requested
    if (verify_checksum_) {
        uint32_t expected = packet.computeChecksum();
        if (packet.getChecksum() != 0 && packet.getChecksum() != expected) {
            return ValidationResult::error(
                ValidationErrorCode::CHECKSUM_MISMATCH,
                "Checksum mismatch. Packet=" + std::to_string(packet.getChecksum()) +
                " Expected=" + std::to_string(expected));
        }
    }

    return ValidationResult::ok();
}

} // namespace packet_engine
