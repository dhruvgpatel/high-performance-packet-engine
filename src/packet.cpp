#include "packet.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <sstream>

namespace packet_engine {

namespace {
// Standard CRC32 table implementation
constexpr uint32_t CRC32_POLYNOMIAL = 0xEDB88320;
uint32_t calculate_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ ((crc & 1) ? CRC32_POLYNOMIAL : 0);
        }
    }
    return ~crc;
}
} // namespace

Packet::Packet() {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
    timestamp_ns_ = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

Packet::Packet(uint64_t id,
               std::string src_ip,
               std::string dst_ip,
               uint16_t src_port,
               uint16_t dst_port,
               IpProtocol protocol,
               uint8_t priority,
               std::vector<uint8_t> payload)
    : id_(id),
      src_ip_(std::move(src_ip)),
      dst_ip_(std::move(dst_ip)),
      src_port_(src_port),
      dst_port_(dst_port),
      protocol_(protocol),
      priority_(priority),
      payload_(std::move(payload)) {
    
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
    timestamp_ns_ = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    
    src_ip_num_ = ipToNumeric(src_ip_);
    dst_ip_num_ = ipToNumeric(dst_ip_);
    checksum_ = computeChecksum();
}

void Packet::setSrcIp(const std::string& ip) {
    src_ip_ = ip;
    src_ip_num_ = ipToNumeric(ip);
}

void Packet::setDstIp(const std::string& ip) {
    dst_ip_ = ip;
    dst_ip_num_ = ipToNumeric(ip);
}

void Packet::setPayload(std::vector<uint8_t> payload) {
    payload_ = std::move(payload);
}

uint32_t Packet::ipToNumeric(const std::string& ip_str) {
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str.c_str(), &addr) == 1) {
        return ntohl(addr.s_addr);
    }
    return 0;
}

std::string Packet::numericToIp(uint32_t ip_num) {
    struct in_addr addr;
    addr.s_addr = htonl(ip_num);
    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)) != nullptr) {
        return std::string(buf);
    }
    return "0.0.0.0";
}

uint32_t Packet::computeChecksum() const {
    if (payload_.empty()) {
        return calculate_crc32(reinterpret_cast<const uint8_t*>(&id_), sizeof(id_));
    }
    return calculate_crc32(payload_.data(), payload_.size());
}

std::vector<uint8_t> Packet::serialize() const {
    WireHeader header;
    header.magic = htonl(0x504B5445);
    header.id = id_;
    header.timestamp_ns = timestamp_ns_;
    header.src_ip = htonl(src_ip_num_);
    header.dst_ip = htonl(dst_ip_num_);
    header.src_port = htons(src_port_);
    header.dst_port = htons(dst_port_);
    header.protocol = static_cast<uint8_t>(protocol_);
    header.priority = priority_;
    header.payload_size = htons(static_cast<uint16_t>(payload_.size()));
    header.checksum = htonl(checksum_);

    std::vector<uint8_t> buffer(sizeof(WireHeader) + payload_.size());
    std::memcpy(buffer.data(), &header, sizeof(WireHeader));
    if (!payload_.empty()) {
        std::memcpy(buffer.data() + sizeof(WireHeader), payload_.data(), payload_.size());
    }
    return buffer;
}

bool Packet::deserialize(const uint8_t* buffer, size_t size, Packet& out_packet) {
    if (size < sizeof(WireHeader)) {
        return false;
    }

    WireHeader header;
    std::memcpy(&header, buffer, sizeof(WireHeader));

    if (ntohl(header.magic) != 0x504B5445) {
        return false;
    }

    uint16_t payload_len = ntohs(header.payload_size);
    if (size < sizeof(WireHeader) + payload_len) {
        return false;
    }

    out_packet.id_ = header.id;
    out_packet.timestamp_ns_ = header.timestamp_ns;
    out_packet.src_ip_num_ = ntohl(header.src_ip);
    out_packet.dst_ip_num_ = ntohl(header.dst_ip);
    out_packet.src_ip_ = numericToIp(out_packet.src_ip_num_);
    out_packet.dst_ip_ = numericToIp(out_packet.dst_ip_num_);
    out_packet.src_port_ = ntohs(header.src_port);
    out_packet.dst_port_ = ntohs(header.dst_port);
    out_packet.protocol_ = static_cast<IpProtocol>(header.protocol);
    out_packet.priority_ = header.priority;
    out_packet.checksum_ = ntohl(header.checksum);

    out_packet.payload_.resize(payload_len);
    if (payload_len > 0) {
        std::memcpy(out_packet.payload_.data(), buffer + sizeof(WireHeader), payload_len);
    }

    return true;
}

} // namespace packet_engine
