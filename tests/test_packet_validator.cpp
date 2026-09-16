#include <gtest/gtest.h>
#include "packet_validator.hpp"

using namespace packet_engine;

TEST(PacketValidatorTest, ValidPacket) {
    PacketValidator validator(0, 1500, true);
    Packet pkt(1, "192.168.1.50", "192.168.1.1", 1234, 80, IpProtocol::TCP, 0, {0x10, 0x20});

    ValidationResult res = validator.validate(pkt);
    EXPECT_TRUE(res.is_valid);
    EXPECT_EQ(res.error_code, ValidationErrorCode::OK);
}

TEST(PacketValidatorTest, InvalidPorts) {
    PacketValidator validator;

    Packet src_zero(1, "192.168.1.50", "192.168.1.1", 0, 80, IpProtocol::TCP, 0, {});
    EXPECT_FALSE(validator.validate(src_zero).is_valid);
    EXPECT_EQ(validator.validate(src_zero).error_code, ValidationErrorCode::INVALID_PORT);

    Packet dst_zero(1, "192.168.1.50", "192.168.1.1", 1234, 0, IpProtocol::TCP, 0, {});
    EXPECT_FALSE(validator.validate(dst_zero).is_valid);
    EXPECT_EQ(validator.validate(dst_zero).error_code, ValidationErrorCode::INVALID_PORT);
}

TEST(PacketValidatorTest, InvalidProtocol) {
    PacketValidator validator;
    Packet pkt(1, "10.0.0.1", "10.0.0.2", 1000, 2000, IpProtocol::UNKNOWN, 0, {});
    EXPECT_FALSE(validator.validate(pkt).is_valid);
    EXPECT_EQ(validator.validate(pkt).error_code, ValidationErrorCode::INVALID_PROTOCOL);
}

TEST(PacketValidatorTest, PayloadSizeBounds) {
    PacketValidator validator(10, 50, false);

    Packet small_pkt(1, "10.0.0.1", "10.0.0.2", 1000, 2000, IpProtocol::UDP, 0, {1, 2});
    EXPECT_FALSE(validator.validate(small_pkt).is_valid);
    EXPECT_EQ(validator.validate(small_pkt).error_code, ValidationErrorCode::SIZE_UNDERFLOW);

    std::vector<uint8_t> large_payload(100, 0xFF);
    Packet large_pkt(1, "10.0.0.1", "10.0.0.2", 1000, 2000, IpProtocol::UDP, 0, large_payload);
    EXPECT_FALSE(validator.validate(large_pkt).is_valid);
    EXPECT_EQ(validator.validate(large_pkt).error_code, ValidationErrorCode::SIZE_OVERFLOW);
}

TEST(PacketValidatorTest, ChecksumVerification) {
    PacketValidator validator(0, 1500, true);
    Packet pkt(1, "10.0.0.1", "10.0.0.2", 1000, 2000, IpProtocol::UDP, 0, {1, 2, 3, 4});

    EXPECT_TRUE(validator.validate(pkt).is_valid);

    // Corrupt checksum
    pkt.setChecksum(pkt.getChecksum() ^ 0xDEADBEEF);
    ValidationResult res = validator.validate(pkt);
    EXPECT_FALSE(res.is_valid);
    EXPECT_EQ(res.error_code, ValidationErrorCode::CHECKSUM_MISMATCH);
}
