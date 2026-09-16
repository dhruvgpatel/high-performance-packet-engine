#pragma once

#include "packet.hpp"
#include <string>

namespace packet_engine {

enum class PacketCategory : uint8_t {
    INVALID = 0,
    CONTROL = 1,
    DATA = 2,
    ACK = 3,
    HIGH_PRIORITY = 4
};

inline const char* packetCategoryToString(PacketCategory cat) {
    switch (cat) {
        case PacketCategory::INVALID:       return "INVALID";
        case PacketCategory::CONTROL:       return "CONTROL";
        case PacketCategory::DATA:          return "DATA";
        case PacketCategory::ACK:           return "ACK";
        case PacketCategory::HIGH_PRIORITY: return "HIGH_PRIORITY";
        default:                            return "UNKNOWN";
    }
}

class PacketClassifier {
public:
    PacketClassifier() = default;

    PacketCategory classify(const Packet& packet) const;
};

} // namespace packet_engine
