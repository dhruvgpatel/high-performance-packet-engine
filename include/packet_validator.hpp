#pragma once

#include "packet.hpp"
#include <string>

namespace packet_engine {

enum class ValidationErrorCode : uint8_t {
    OK = 0,
    SIZE_UNDERFLOW,
    SIZE_OVERFLOW,
    INVALID_PORT,
    INVALID_IP,
    INVALID_PROTOCOL,
    CHECKSUM_MISMATCH,
    MALFORMED_PAYLOAD
};

inline const char* validationErrorToString(ValidationErrorCode code) {
    switch (code) {
        case ValidationErrorCode::OK: return "OK";
        case ValidationErrorCode::SIZE_UNDERFLOW: return "SIZE_UNDERFLOW";
        case ValidationErrorCode::SIZE_OVERFLOW: return "SIZE_OVERFLOW";
        case ValidationErrorCode::INVALID_PORT: return "INVALID_PORT";
        case ValidationErrorCode::INVALID_IP: return "INVALID_IP";
        case ValidationErrorCode::INVALID_PROTOCOL: return "INVALID_PROTOCOL";
        case ValidationErrorCode::CHECKSUM_MISMATCH: return "CHECKSUM_MISMATCH";
        case ValidationErrorCode::MALFORMED_PAYLOAD: return "MALFORMED_PAYLOAD";
        default: return "UNKNOWN_ERROR";
    }
}

struct ValidationResult {
    bool is_valid{true};
    ValidationErrorCode error_code{ValidationErrorCode::OK};
    std::string error_message{"Valid"};

    static ValidationResult ok() {
        return {true, ValidationErrorCode::OK, "Valid"};
    }

    static ValidationResult error(ValidationErrorCode code, std::string msg) {
        return {false, code, std::move(msg)};
    }
};

class PacketValidator {
public:
    PacketValidator(size_t min_payload_size = 0,
                    size_t max_payload_size = 65507,
                    bool verify_checksum = true);

    ValidationResult validate(const Packet& packet) const;

    void setVerifyChecksum(bool enable) noexcept { verify_checksum_ = enable; }
    void setMinPayloadSize(size_t size) noexcept { min_payload_size_ = size; }
    void setMaxPayloadSize(size_t size) noexcept { max_payload_size_ = size; }

private:
    size_t min_payload_size_;
    size_t max_payload_size_;
    bool verify_checksum_;
};

} // namespace packet_engine
