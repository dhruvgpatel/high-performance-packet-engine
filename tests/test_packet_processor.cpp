#include <gtest/gtest.h>
#include "packet_processor.hpp"

using namespace packet_engine;

TEST(PacketProcessorTest, ProcessValidPacket) {
    PacketValidator validator;
    PacketClassifier classifier;
    PacketProcessor processor(true, 50, 0);

    std::vector<uint8_t> payload = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    Packet pkt(42, "10.0.0.1", "10.0.0.2", 8080, 9000, IpProtocol::UDP, 0, payload);

    ProcessingResult res = processor.process(pkt, validator, classifier);
    EXPECT_TRUE(res.valid);
    EXPECT_EQ(res.packet_id, 42);
    EXPECT_EQ(res.category, PacketCategory::DATA);
    EXPECT_NE(res.computed_hash, 0);
    EXPECT_GT(res.processing_time_ns, 0);
}

TEST(PacketProcessorTest, ProcessInvalidPacket) {
    PacketValidator validator;
    PacketClassifier classifier;
    PacketProcessor processor(true, 50, 0);

    // Invalid source port 0
    Packet pkt(99, "10.0.0.1", "10.0.0.2", 0, 9000, IpProtocol::UDP, 0, {});

    ProcessingResult res = processor.process(pkt, validator, classifier);
    EXPECT_FALSE(res.valid);
    EXPECT_EQ(res.category, PacketCategory::INVALID);
    EXPECT_EQ(res.error_code, ValidationErrorCode::INVALID_PORT);
}
