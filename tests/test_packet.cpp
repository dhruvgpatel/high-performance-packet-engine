#include <gtest/gtest.h>
#include "packet.hpp"

using namespace packet_engine;

TEST(PacketTest, BasicCreationAndGetters) {
    std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04, 0x05};
    Packet pkt(101, "192.168.1.10", "10.0.0.1", 8080, 9000, IpProtocol::UDP, 10, payload);

    EXPECT_EQ(pkt.getId(), 101);
    EXPECT_EQ(pkt.getSrcIp(), "192.168.1.10");
    EXPECT_EQ(pkt.getDstIp(), "10.0.0.1");
    EXPECT_EQ(pkt.getSrcPort(), 8080);
    EXPECT_EQ(pkt.getDstPort(), 9000);
    EXPECT_EQ(pkt.getProtocol(), IpProtocol::UDP);
    EXPECT_EQ(pkt.getPriority(), 10);
    EXPECT_EQ(pkt.getPayloadSize(), 5);
    EXPECT_EQ(pkt.getPayload(), payload);
    EXPECT_GT(pkt.getTimestampNs(), 0);
    EXPECT_NE(pkt.getChecksum(), 0);
}

TEST(PacketTest, IpNumericConversions) {
    uint32_t num = Packet::ipToNumeric("192.168.1.1");
    std::string str = Packet::numericToIp(num);
    EXPECT_EQ(str, "192.168.1.1");

    uint32_t loopback = Packet::ipToNumeric("127.0.0.1");
    EXPECT_EQ(Packet::numericToIp(loopback), "127.0.0.1");

    EXPECT_EQ(Packet::ipToNumeric("invalid_ip"), 0);
}

TEST(PacketTest, SerializationAndDeserializationRoundtrip) {
    std::vector<uint8_t> payload(256, 0xAA);
    Packet original(42, "172.16.0.5", "172.16.0.100", 5000, 5001, IpProtocol::TCP, 200, payload);

    std::vector<uint8_t> serialized = original.serialize();
    EXPECT_EQ(serialized.size(), sizeof(WireHeader) + payload.size());

    Packet restored;
    bool success = Packet::deserialize(serialized.data(), serialized.size(), restored);
    EXPECT_TRUE(success);

    EXPECT_EQ(restored.getId(), original.getId());
    EXPECT_EQ(restored.getSrcIp(), original.getSrcIp());
    EXPECT_EQ(restored.getDstIp(), original.getDstIp());
    EXPECT_EQ(restored.getSrcPort(), original.getSrcPort());
    EXPECT_EQ(restored.getDstPort(), original.getDstPort());
    EXPECT_EQ(restored.getProtocol(), original.getProtocol());
    EXPECT_EQ(restored.getPriority(), original.getPriority());
    EXPECT_EQ(restored.getChecksum(), original.getChecksum());
    EXPECT_EQ(restored.getPayload(), original.getPayload());
}

TEST(PacketTest, CorruptedDeserializationRejection) {
    std::vector<uint8_t> payload = {1, 2, 3};
    Packet pkt(1, "1.1.1.1", "2.2.2.2", 100, 200, IpProtocol::UDP, 0, payload);
    std::vector<uint8_t> buffer = pkt.serialize();

    // 1. Truncated buffer
    Packet out;
    EXPECT_FALSE(Packet::deserialize(buffer.data(), sizeof(WireHeader) - 1, out));

    // 2. Corrupted Magic
    buffer[0] = 0x00;
    EXPECT_FALSE(Packet::deserialize(buffer.data(), buffer.size(), out));
}
