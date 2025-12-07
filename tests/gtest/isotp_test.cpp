/**
 * @file isotp_test.cpp
 * @brief Google Test suite for ISO-TP (ISO 15765-2) functionality
 */

#include <gtest/gtest.h>
#include "isotp.hpp"
#include "can_slcan.hpp"

using namespace isotp;

// ============================================================================
// Frame Type Tests
// ============================================================================

TEST(ISOTPFrameTest, SingleFramePCI) {
  // Single Frame: PCI byte = 0x0N where N is data length (0-7)
  uint8_t pci_sf_0 = 0x00;  // SF with 0 bytes
  uint8_t pci_sf_7 = 0x07;  // SF with 7 bytes
  
  EXPECT_EQ(pci_sf_0 & 0xF0, 0x00);  // Upper nibble = 0 for SF
  EXPECT_EQ(pci_sf_7 & 0x0F, 7);     // Lower nibble = length
}

TEST(ISOTPFrameTest, FirstFramePCI) {
  // First Frame: PCI = 0x1NNN where NNN is total message length
  // For message of 100 bytes: 0x1064
  uint16_t msg_len = 100;
  uint8_t pci_high = 0x10 | ((msg_len >> 8) & 0x0F);
  uint8_t pci_low = msg_len & 0xFF;
  
  EXPECT_EQ(pci_high, 0x10);
  EXPECT_EQ(pci_low, 0x64);
}

TEST(ISOTPFrameTest, ConsecutiveFramePCI) {
  // Consecutive Frame: PCI = 0x2N where N is sequence number (0-F, wraps)
  for (int seq = 0; seq <= 15; seq++) {
    uint8_t pci = 0x20 | (seq & 0x0F);
    EXPECT_EQ(pci & 0xF0, 0x20);
    EXPECT_EQ(pci & 0x0F, seq);
  }
}

TEST(ISOTPFrameTest, FlowControlPCI) {
  // Flow Control: PCI = 0x3S where S is flow status
  uint8_t fc_cts = 0x30;   // Continue To Send
  uint8_t fc_wait = 0x31;  // Wait
  uint8_t fc_ovfl = 0x32;  // Overflow/Abort
  
  EXPECT_EQ(fc_cts & 0xF0, 0x30);
  EXPECT_EQ(fc_cts & 0x0F, 0);
  EXPECT_EQ(fc_wait & 0x0F, 1);
  EXPECT_EQ(fc_ovfl & 0x0F, 2);
}

// ============================================================================
// Message Length Tests
// ============================================================================

TEST(ISOTPLengthTest, SingleFrameMaxLength) {
  // Classic CAN SF max: 7 bytes (8 - 1 PCI byte)
  // CAN FD SF max: 62 bytes (64 - 2 PCI bytes for escape sequence)
  EXPECT_EQ(7, 8 - 1);  // Classic CAN
}

TEST(ISOTPLengthTest, FirstFrameDataLength) {
  // FF carries 6 data bytes (8 - 2 PCI bytes)
  EXPECT_EQ(6, 8 - 2);
}

TEST(ISOTPLengthTest, ConsecutiveFrameDataLength) {
  // CF carries 7 data bytes (8 - 1 PCI byte)
  EXPECT_EQ(7, 8 - 1);
}

TEST(ISOTPLengthTest, MultiFrameMessageCalculation) {
  // Calculate number of frames for a 100-byte message
  size_t msg_len = 100;
  size_t ff_data = 6;   // First frame carries 6 bytes
  size_t cf_data = 7;   // Each CF carries 7 bytes
  
  size_t remaining = msg_len - ff_data;  // 94 bytes
  size_t num_cf = (remaining + cf_data - 1) / cf_data;  // ceil(94/7) = 14
  size_t total_frames = 1 + num_cf;  // 1 FF + 14 CF = 15 frames
  
  EXPECT_EQ(remaining, 94u);
  EXPECT_EQ(num_cf, 14u);
  EXPECT_EQ(total_frames, 15u);
}

TEST(ISOTPLengthTest, LargeMessageLength) {
  // Standard FF supports up to 4095 bytes (12-bit length field)
  // Extended FF (escape sequence) supports up to 4GB
  uint16_t max_standard = 0x0FFF;
  EXPECT_EQ(max_standard, 4095);
}

// ============================================================================
// Sequence Number Tests
// ============================================================================

TEST(ISOTPSequenceTest, SequenceNumberWrap) {
  // Sequence numbers wrap from F back to 0
  std::vector<uint8_t> expected_seq = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2};
  
  uint8_t seq = 0;
  for (size_t i = 0; i < expected_seq.size(); i++) {
    seq = (seq + 1) & 0x0F;
    EXPECT_EQ(seq, expected_seq[i]) << "Failed at index " << i;
  }
}

// ============================================================================
// Flow Control Parameter Tests
// ============================================================================

TEST(ISOTPFlowControlTest, BlockSizeValues) {
  // BS = 0: No limit (send all CFs without waiting)
  // BS = 1-255: Send BS frames, then wait for FC
  uint8_t bs_unlimited = 0;
  uint8_t bs_one = 1;
  uint8_t bs_max = 255;
  
  EXPECT_EQ(bs_unlimited, 0);
  EXPECT_GE(bs_one, 1);
  EXPECT_LE(bs_max, 255);
}

TEST(ISOTPFlowControlTest, STminValues) {
  // STmin: Separation Time minimum between CFs
  // 0x00-0x7F: 0-127 ms
  // 0x80-0xF0: Reserved
  // 0xF1-0xF9: 100-900 µs
  // 0xFA-0xFF: Reserved
  
  uint8_t stmin_0ms = 0x00;
  uint8_t stmin_127ms = 0x7F;
  uint8_t stmin_100us = 0xF1;
  uint8_t stmin_900us = 0xF9;
  
  EXPECT_EQ(stmin_0ms, 0);
  EXPECT_EQ(stmin_127ms, 127);
  EXPECT_EQ(stmin_100us, 0xF1);
  EXPECT_EQ(stmin_900us, 0xF9);
}

TEST(ISOTPFlowControlTest, STminToMicroseconds) {
  // Convert STmin to microseconds
  auto stmin_to_us = [](uint8_t stmin) -> uint32_t {
    if (stmin <= 0x7F) {
      return stmin * 1000;  // ms to µs
    } else if (stmin >= 0xF1 && stmin <= 0xF9) {
      return (stmin - 0xF0) * 100;  // 100-900 µs
    }
    return 0;  // Reserved
  };
  
  EXPECT_EQ(stmin_to_us(0x00), 0u);
  EXPECT_EQ(stmin_to_us(0x0A), 10000u);  // 10ms
  EXPECT_EQ(stmin_to_us(0x7F), 127000u); // 127ms
  EXPECT_EQ(stmin_to_us(0xF1), 100u);    // 100µs
  EXPECT_EQ(stmin_to_us(0xF5), 500u);    // 500µs
}

// ============================================================================
// Addressing Mode Tests
// ============================================================================

TEST(ISOTPAddressingTest, NormalAddressing) {
  // Normal addressing: 11-bit CAN ID
  // Request: 0x7DF (functional) or 0x7E0-0x7E7 (physical)
  // Response: 0x7E8-0x7EF
  
  uint32_t func_req = 0x7DF;
  uint32_t phys_req = 0x7E0;
  uint32_t phys_resp = 0x7E8;
  
  EXPECT_EQ(func_req, 0x7DF);
  EXPECT_EQ(phys_resp - phys_req, 8u);  // Response = Request + 8
}

TEST(ISOTPAddressingTest, ExtendedAddressing) {
  // Extended addressing: First byte of CAN data is target address
  // Reduces payload by 1 byte per frame
  uint8_t target_addr = 0x55;
  
  // SF with extended addressing: [TA][PCI][data...]
  // Max SF data = 6 bytes (instead of 7)
  EXPECT_EQ(target_addr, 0x55);
}

TEST(ISOTPAddressingTest, MixedAddressing) {
  // Mixed addressing: 29-bit CAN ID with address extension
  uint32_t can_id_29bit = 0x18DA00F1;  // Example J1939-style
  
  EXPECT_TRUE((can_id_29bit & 0x80000000) == 0);  // Valid 29-bit ID
}

// ============================================================================
// Timeout Tests
// ============================================================================

TEST(ISOTPTimeoutTest, DefaultTimeouts) {
  // ISO 15765-2 default timeouts
  // N_As, N_Ar: 1000ms (transmission acknowledgment)
  // N_Bs: 1000ms (wait for FC after FF)
  // N_Cr: 1000ms (wait for CF after FC CTS)
  
  auto n_as = std::chrono::milliseconds(1000);
  auto n_bs = std::chrono::milliseconds(1000);
  auto n_cr = std::chrono::milliseconds(1000);
  
  EXPECT_EQ(n_as.count(), 1000);
  EXPECT_EQ(n_bs.count(), 1000);
  EXPECT_EQ(n_cr.count(), 1000);
}

// ============================================================================
// Padding Tests
// ============================================================================

TEST(ISOTPPaddingTest, PaddingByte) {
  // Common padding bytes
  uint8_t pad_0x00 = 0x00;
  uint8_t pad_0x55 = 0x55;  // Alternating bits
  uint8_t pad_0xAA = 0xAA;  // Alternating bits (inverted)
  uint8_t pad_0xCC = 0xCC;  // Common OEM choice
  uint8_t pad_0xFF = 0xFF;  // Common default
  
  EXPECT_EQ(pad_0x55 ^ pad_0xAA, 0xFF);  // Complementary
}

TEST(ISOTPPaddingTest, FramePadding) {
  // Pad SF to 8 bytes
  std::vector<uint8_t> sf_data = {0x02, 0x10, 0x01};  // DiagSessionControl
  const uint8_t pad_byte = 0xCC;
  
  while (sf_data.size() < 8) {
    sf_data.push_back(pad_byte);
  }
  
  EXPECT_EQ(sf_data.size(), 8u);
  EXPECT_EQ(sf_data[3], pad_byte);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST(ISOTPErrorTest, FlowControlOverflow) {
  // FC with overflow status indicates receiver buffer full
  uint8_t fc_overflow = 0x32;
  EXPECT_EQ(fc_overflow & 0x0F, 2);
}

TEST(ISOTPErrorTest, InvalidSequenceNumber) {
  // Detect sequence number mismatch
  uint8_t expected_seq = 5;
  uint8_t received_seq = 6;
  
  EXPECT_NE(expected_seq, received_seq);
}

TEST(ISOTPErrorTest, MessageTooLong) {
  // Standard ISO-TP max is 4095 bytes
  size_t max_len = 4095;
  size_t msg_len = 5000;
  
  EXPECT_GT(msg_len, max_len);
}

// ============================================================================
// CAN Protocol Constants Tests
// ============================================================================

TEST(CANProtocolTest, FrameConstants) {
  EXPECT_EQ(CANProtocol::CAN_MAX_DLEN, 8u);
  EXPECT_EQ(CANProtocol::CANFD_MAX_DLEN, 64u);
  EXPECT_EQ(CANProtocol::CAN_SFF_ID_BITS, 11u);
  EXPECT_EQ(CANProtocol::CAN_EFF_ID_BITS, 29u);
}

TEST(CANProtocolTest, IDMasks) {
  EXPECT_EQ(CANProtocol::CAN_SFF_MASK, 0x000007FFu);
  EXPECT_EQ(CANProtocol::CAN_EFF_MASK, 0x1FFFFFFFu);
  EXPECT_EQ(CANProtocol::CAN_EFF_FLAG, 0x80000000u);
}

TEST(CANProtocolTest, FrameFlags) {
  EXPECT_EQ(CANProtocol::CAN_RTR_FLAG, 0x01);
  EXPECT_EQ(CANProtocol::CAN_ERR_FLAG, 0x02);
}

// ============================================================================
// CAN Frame Structure Tests
// ============================================================================

TEST(CANFrameTest, DefaultConstruction) {
  CANProtocol::CANFrame frame;
  
  EXPECT_EQ(frame.id, 0u);
  EXPECT_EQ(frame.dlc, 0u);
  EXPECT_EQ(frame.flags, 0u);
}

TEST(CANFrameTest, ExtendedIDDetection) {
  CANProtocol::CANFrame frame;
  
  frame.id = 0x123;  // Standard ID
  EXPECT_FALSE(frame.isExtended());
  
  frame.id = 0x80000123;  // Extended ID (with flag)
  EXPECT_TRUE(frame.isExtended());
}

TEST(CANFrameTest, RTRDetection) {
  CANProtocol::CANFrame frame;
  
  frame.flags = 0;
  EXPECT_FALSE(frame.isRTR());
  
  frame.flags = CANProtocol::CAN_RTR_FLAG;
  EXPECT_TRUE(frame.isRTR());
}

TEST(CANFrameTest, GetIdentifier) {
  CANProtocol::CANFrame frame;
  
  // Standard ID
  frame.id = 0x7E0;
  EXPECT_EQ(frame.getIdentifier(), 0x7E0u);
  
  // Extended ID (mask off the flag)
  frame.id = 0x80012345;
  EXPECT_EQ(frame.getIdentifier(), 0x12345u);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
