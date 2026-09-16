#include <gtest/gtest.h>
#include "packet_classifier.hpp"

using namespace packet_engine;

TEST(PacketClassifierTest, ClassificationRules) {
    PacketClassifier classifier;

    // 1. High Priority
    Packet p_high(1, "1.1.1.1", "2.2.2.2", 1000, 2000, IpProtocol::UDP, 192, {1, 2, 3, 4, 5});
    EXPECT_EQ(classifier.classify(p_high), PacketCategory::HIGH_PRIORITY);

    // 2. Control (DNS Port 53)
    Packet p_dns(2, "1.1.1.1", "2.2.2.2", 1000, 53, IpProtocol::UDP, 0, {1, 2, 3, 4, 5});
    EXPECT_EQ(classifier.classify(p_dns), PacketCategory::CONTROL);

    // 3. Control (ICMP)
    Packet p_icmp(3, "1.1.1.1", "2.2.2.2", 1000, 2000, IpProtocol::ICMP, 0, {1, 2, 3, 4, 5});
    EXPECT_EQ(classifier.classify(p_icmp), PacketCategory::CONTROL);

    // 4. ACK (empty payload)
    Packet p_ack(4, "1.1.1.1", "2.2.2.2", 1000, 2000, IpProtocol::TCP, 0, {});
    EXPECT_EQ(classifier.classify(p_ack), PacketCategory::ACK);

    // 5. Data (regular UDP with data)
    Packet p_data(5, "1.1.1.1", "2.2.2.2", 1000, 2000, IpProtocol::UDP, 0, {1, 2, 3, 4, 5, 6, 7, 8});
    EXPECT_EQ(classifier.classify(p_data), PacketCategory::DATA);
}
