#include "packet_classifier.hpp"

namespace packet_engine {

PacketCategory PacketClassifier::classify(const Packet& packet) const {
    // 1. High Priority check (explicit priority bit/byte >= 128 or urgent traffic)
    if (packet.getPriority() >= 128) {
        return PacketCategory::HIGH_PRIORITY;
    }

    // 2. Control traffic check (ICMP or control ports)
    if (packet.getProtocol() == IpProtocol::ICMP ||
        packet.getDstPort() == 53 ||   // DNS
        packet.getDstPort() == 123 ||  // NTP
        packet.getDstPort() == 161) {  // SNMP
        return PacketCategory::CONTROL;
    }

    // 3. ACK / Heartbeat check (empty payload or 1-4 byte keepalives)
    if (packet.getPayloadSize() == 0 || packet.getPayloadSize() <= 4) {
        return PacketCategory::ACK;
    }

    // 4. Default Data payload
    return PacketCategory::DATA;
}

} // namespace packet_engine
