#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include <iostream>

namespace packet_engine {

enum class IpProtocol : uint8_t {
    UNKNOWN = 0,
    ICMP = 1,
    TCP = 6,
    UDP = 17
};

inline const char* protocolToString(IpProtocol proto) {
    switch (proto) {
        case IpProtocol::TCP: return "TCP";
        case IpProtocol::UDP: return "UDP";
        case IpProtocol::ICMP: return "ICMP";
        default: return "UNKNOWN";
    }
}

inline IpProtocol stringToProtocol(const std::string& str) {
    if (str == "udp" || str == "UDP") return IpProtocol::UDP;
    if (str == "tcp" || str == "TCP") return IpProtocol::TCP;
    if (str == "icmp" || str == "ICMP") return IpProtocol::ICMP;
    return IpProtocol::UNKNOWN;
}

#pragma pack(push, 1)
struct WireHeader {
    uint32_t magic{0x504B5445}; // 'PKTE' (Packet Engine)
    uint64_t id{0};
    uint64_t timestamp_ns{0};
    uint32_t src_ip{0};
    uint32_t dst_ip{0};
    uint16_t src_port{0};
    uint16_t dst_port{0};
    uint8_t  protocol{0};
    uint8_t  priority{0};
    uint16_t payload_size{0};
    uint32_t checksum{0};
};
#pragma pack(pop)

class Packet {
public:
    Packet();
    Packet(uint64_t id,
           std::string src_ip,
           std::string dst_ip,
           uint16_t src_port,
           uint16_t dst_port,
           IpProtocol protocol,
           uint8_t priority,
           std::vector<uint8_t> payload);

    // Rule of 5: Move-enabled for zero-copy concurrency
    ~Packet() = default;
    Packet(const Packet&) = default;
    Packet& operator=(const Packet&) = default;
    Packet(Packet&&) noexcept = default;
    Packet& operator=(Packet&&) noexcept = default;

    // Getters
    uint64_t getId() const noexcept { return id_; }
    uint64_t getTimestampNs() const noexcept { return timestamp_ns_; }
    const std::string& getSrcIp() const noexcept { return src_ip_; }
    const std::string& getDstIp() const noexcept { return dst_ip_; }
    uint32_t getSrcIpNumeric() const noexcept { return src_ip_num_; }
    uint32_t getDstIpNumeric() const noexcept { return dst_ip_num_; }
    uint16_t getSrcPort() const noexcept { return src_port_; }
    uint16_t getDstPort() const noexcept { return dst_port_; }
    IpProtocol getProtocol() const noexcept { return protocol_; }
    uint8_t getPriority() const noexcept { return priority_; }
    size_t getPayloadSize() const noexcept { return payload_.size(); }
    const std::vector<uint8_t>& getPayload() const noexcept { return payload_; }
    std::vector<uint8_t>& getPayload() noexcept { return payload_; }
    uint32_t getChecksum() const noexcept { return checksum_; }
    uint64_t getEnqueueTimestampNs() const noexcept { return enqueue_timestamp_ns_; }
    uint64_t getDequeueTimestampNs() const noexcept { return dequeue_timestamp_ns_; }

    // Setters
    void setId(uint64_t id) noexcept { id_ = id; }
    void setTimestampNs(uint64_t ts) noexcept { timestamp_ns_ = ts; }
    void setSrcIp(const std::string& ip);
    void setDstIp(const std::string& ip);
    void setSrcPort(uint16_t port) noexcept { src_port_ = port; }
    void setDstPort(uint16_t port) noexcept { dst_port_ = port; }
    void setProtocol(IpProtocol proto) noexcept { protocol_ = proto; }
    void setPriority(uint8_t prio) noexcept { priority_ = prio; }
    void setPayload(std::vector<uint8_t> payload);
    void setChecksum(uint32_t checksum) noexcept { checksum_ = checksum; }
    void setEnqueueTimestampNs(uint64_t ts) noexcept { enqueue_timestamp_ns_ = ts; }
    void setDequeueTimestampNs(uint64_t ts) noexcept { dequeue_timestamp_ns_ = ts; }

    // Wire serialization / deserialization
    std::vector<uint8_t> serialize() const;
    static bool deserialize(const uint8_t* buffer, size_t size, Packet& out_packet);

    // Compute simple CRC32/Checksum
    uint32_t computeChecksum() const;

    // Helper static IP conversion
    static uint32_t ipToNumeric(const std::string& ip_str);
    static std::string numericToIp(uint32_t ip_num);

private:
    uint64_t id_{0};
    uint64_t timestamp_ns_{0};
    std::string src_ip_{"0.0.0.0"};
    std::string dst_ip_{"0.0.0.0"};
    uint32_t src_ip_num_{0};
    uint32_t dst_ip_num_{0};
    uint16_t src_port_{0};
    uint16_t dst_port_{0};
    IpProtocol protocol_{IpProtocol::UNKNOWN};
    uint8_t priority_{0};
    std::vector<uint8_t> payload_{};
    uint32_t checksum_{0};

    // Performance tracing timestamps
    uint64_t enqueue_timestamp_ns_{0};
    uint64_t dequeue_timestamp_ns_{0};
};

} // namespace packet_engine
