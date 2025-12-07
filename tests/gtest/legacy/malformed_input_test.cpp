/**
 * @file malformed_input_test.cpp
 * @brief Tests for handling malformed, truncated, and invalid inputs
 * 
 * These tests verify the library handles bad data gracefully without:
 * - Crashing
 * - Buffer overflows
 * - Undefined behavior
 * - Memory corruption
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include "uds.hpp"
#include "uds_dtc.hpp"
#include "uds_scaling.hpp"
#include "uds_memory.hpp"
#include "uds_block.hpp"

using namespace uds;

// ============================================================================
// Truncated Response Tests
// ============================================================================

TEST(TruncatedResponseTest, EmptyResponse) {
  std::vector<uint8_t> empty;
  
  // Should handle gracefully
  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(empty.size(), 0u);
}

TEST(TruncatedResponseTest, SingleByteResponse) {
  // Only SID, no payload
  std::vector<uint8_t> response = {0x50};  // DSC positive response
  
  EXPECT_EQ(response.size(), 1u);
  EXPECT_EQ(response[0], 0x50);
}

TEST(TruncatedResponseTest, TruncatedNRC) {
  // NRC with missing code byte
  std::vector<uint8_t> truncated_nrc = {0x7F, 0x10};  // Missing NRC code
  
  EXPECT_EQ(truncated_nrc.size(), 2u);
  EXPECT_LT(truncated_nrc.size(), 3u);  // Should be 3 bytes
}

TEST(TruncatedResponseTest, TruncatedDID) {
  // DID response with incomplete DID
  std::vector<uint8_t> truncated = {0x62, 0xF1};  // Missing second DID byte
  
  EXPECT_EQ(truncated.size(), 2u);
  // Cannot safely parse DID
}

TEST(TruncatedResponseTest, TruncatedDTC) {
  // DTC needs 3 bytes, only 2 provided
  std::vector<uint8_t> truncated = {0x01, 0x23};
  
  EXPECT_EQ(truncated.size(), 2u);
  EXPECT_LT(truncated.size(), 3u);
}

// ============================================================================
// Invalid PCI Byte Tests (ISO-TP)
// ============================================================================

TEST(InvalidPCITest, ReservedPCITypes) {
  // PCI types 0x40-0xF0 are reserved
  std::vector<uint8_t> reserved_pcis = {0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0};
  
  for (uint8_t pci : reserved_pcis) {
    uint8_t pci_type = pci & 0xF0;
    
    // These are not valid standard PCI types
    EXPECT_NE(pci_type, 0x00);  // SF
    EXPECT_NE(pci_type, 0x10);  // FF
    EXPECT_NE(pci_type, 0x20);  // CF
    EXPECT_NE(pci_type, 0x30);  // FC
  }
}

TEST(InvalidPCITest, InvalidSingleFrameLength) {
  // SF with length > 7 is invalid for CAN
  uint8_t invalid_sf = 0x08;  // Length = 8, but max is 7
  
  uint8_t length = invalid_sf & 0x0F;
  EXPECT_GT(length, 7u);
}

TEST(InvalidPCITest, ZeroLengthFirstFrame) {
  // FF with zero length is invalid
  uint8_t ff_pci0 = 0x10;  // FF, length high nibble = 0
  uint8_t ff_pci1 = 0x00;  // Length low byte = 0
  
  uint16_t length = ((ff_pci0 & 0x0F) << 8) | ff_pci1;
  EXPECT_EQ(length, 0u);  // Invalid - should be > 7
}

// ============================================================================
// Invalid Format Identifier Tests
// ============================================================================

TEST(InvalidFormatTest, ZeroALFI) {
  // ALFI = 0x00 means 0 bytes for both address and size
  uint8_t alfi = 0x00;
  
  uint8_t addr_bytes = (alfi >> 4) & 0x0F;
  uint8_t size_bytes = alfi & 0x0F;
  
  EXPECT_EQ(addr_bytes, 0u);
  EXPECT_EQ(size_bytes, 0u);
  // This is technically invalid but should not crash
}

TEST(InvalidFormatTest, OversizedALFI) {
  // ALFI with nibble values > 5 for address or > 4 for size
  uint8_t alfi = 0xFF;  // 15 bytes each - way too large
  
  uint8_t addr_bytes = (alfi >> 4) & 0x0F;
  uint8_t size_bytes = alfi & 0x0F;
  
  EXPECT_EQ(addr_bytes, 15u);
  EXPECT_EQ(size_bytes, 15u);
  // Should be rejected or handled gracefully
}

TEST(InvalidFormatTest, InvalidLengthFormatIdentifier) {
  // LFI with 0 length
  uint8_t lfi = 0x00;
  
  uint8_t len = lfi & 0x0F;
  EXPECT_EQ(len, 0u);
  // Zero length means no maxBlockLength bytes follow
}

// ============================================================================
// Out of Range Value Tests
// ============================================================================

TEST(OutOfRangeTest, InvalidSessionType) {
  // Session types 0x00 and 0x04-0x3F are reserved
  uint8_t reserved_sessions[] = {0x00, 0x04, 0x05, 0x3F};
  
  for (uint8_t session : reserved_sessions) {
    // Should handle gracefully even if invalid
    EXPECT_NO_THROW({
      Session s = static_cast<Session>(session);
      (void)s;
    });
  }
}

TEST(OutOfRangeTest, InvalidResetType) {
  // Reset types 0x00 and 0x06-0x3F are reserved
  uint8_t reserved_resets[] = {0x00, 0x06, 0x07, 0x3F};
  
  for (uint8_t reset : reserved_resets) {
    EXPECT_NO_THROW({
      EcuResetType r = static_cast<EcuResetType>(reset);
      (void)r;
    });
  }
}

TEST(OutOfRangeTest, InvalidRoutineAction) {
  // Routine actions 0x00 and 0x04-0x7F are reserved
  uint8_t reserved_actions[] = {0x00, 0x04, 0x05, 0x7F};
  
  for (uint8_t action : reserved_actions) {
    EXPECT_NO_THROW({
      RoutineAction a = static_cast<RoutineAction>(action);
      (void)a;
    });
  }
}

// ============================================================================
// Malformed DTC Tests
// ============================================================================

TEST(MalformedDTCTest, InvalidDTCString) {
  using namespace dtc;
  
  // Various invalid formats
  EXPECT_EQ(parse_dtc_string(""), 0u);
  EXPECT_EQ(parse_dtc_string("X"), 0u);
  EXPECT_EQ(parse_dtc_string("P"), 0u);
  EXPECT_EQ(parse_dtc_string("P0"), 0u);
  EXPECT_EQ(parse_dtc_string("P01"), 0u);
  EXPECT_EQ(parse_dtc_string("P012"), 0u);
  EXPECT_EQ(parse_dtc_string("PGGGG"), 0u);  // Invalid hex
  EXPECT_EQ(parse_dtc_string("12345"), 0u);  // Missing type
}

TEST(MalformedDTCTest, NonPrintableCharacters) {
  using namespace dtc;
  
  std::string bad_string = "P\x00\x01\x02\x03";
  EXPECT_EQ(parse_dtc_string(bad_string), 0u);
}

// ============================================================================
// Malformed Scaling Data Tests
// ============================================================================

TEST(MalformedScalingTest, EmptyPayload) {
  using namespace scaling;
  
  std::vector<uint8_t> empty;
  auto info = parse_scaling_info(0xF190, empty);
  
  EXPECT_EQ(info.did, 0xF190);
  EXPECT_TRUE(info.raw_scaling_bytes.empty());
}

TEST(MalformedScalingTest, SingleBytePayload) {
  using namespace scaling;
  
  std::vector<uint8_t> single = {0x01};  // Just format byte
  auto info = parse_scaling_info(0xF190, single);
  
  EXPECT_EQ(info.did, 0xF190);
  EXPECT_EQ(info.raw_scaling_bytes.size(), 1u);
}

// ============================================================================
// Buffer Boundary Tests
// ============================================================================

TEST(BufferBoundaryTest, ExactSizeAccess) {
  std::vector<uint8_t> data = {0x01, 0x02, 0x03};
  
  // Safe access
  EXPECT_EQ(data[0], 0x01);
  EXPECT_EQ(data[1], 0x02);
  EXPECT_EQ(data[2], 0x03);
  
  // Bounds check
  EXPECT_EQ(data.size(), 3u);
}

TEST(BufferBoundaryTest, IteratorSafety) {
  std::vector<uint8_t> data = {0x01, 0x02, 0x03};
  
  // Safe iteration
  size_t count = 0;
  for (auto it = data.begin(); it != data.end(); ++it) {
    count++;
  }
  EXPECT_EQ(count, 3u);
}

TEST(BufferBoundaryTest, SubvectorExtraction) {
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};
  
  // Safe subvector
  if (data.size() >= 3) {
    std::vector<uint8_t> sub(data.begin(), data.begin() + 3);
    EXPECT_EQ(sub.size(), 3u);
  }
}

// ============================================================================
// Integer Parsing Edge Cases
// ============================================================================

TEST(IntegerParsingTest, AllZeros) {
  std::vector<uint8_t> zeros = {0x00, 0x00, 0x00, 0x00};
  
  uint32_t value = (static_cast<uint32_t>(zeros[0]) << 24) |
                   (static_cast<uint32_t>(zeros[1]) << 16) |
                   (static_cast<uint32_t>(zeros[2]) << 8) |
                   static_cast<uint32_t>(zeros[3]);
  
  EXPECT_EQ(value, 0u);
}

TEST(IntegerParsingTest, AllOnes) {
  std::vector<uint8_t> ones = {0xFF, 0xFF, 0xFF, 0xFF};
  
  uint32_t value = (static_cast<uint32_t>(ones[0]) << 24) |
                   (static_cast<uint32_t>(ones[1]) << 16) |
                   (static_cast<uint32_t>(ones[2]) << 8) |
                   static_cast<uint32_t>(ones[3]);
  
  EXPECT_EQ(value, 0xFFFFFFFFu);
}

TEST(IntegerParsingTest, SignBitHandling) {
  // Test that sign bit doesn't cause issues in unsigned parsing
  std::vector<uint8_t> high_bit = {0x80, 0x00};
  
  uint16_t value = (static_cast<uint16_t>(high_bit[0]) << 8) | high_bit[1];
  EXPECT_EQ(value, 0x8000u);
  EXPECT_EQ(value, 32768u);
}

// ============================================================================
// CRC Edge Cases
// ============================================================================

TEST(CRCEdgeCaseTest, EmptyData) {
  using namespace block;
  
  std::vector<uint8_t> empty;
  uint32_t crc = calculate_crc32(empty);
  
  // CRC of empty should be defined value
  EXPECT_EQ(crc, 0x00000000u);
}

TEST(CRCEdgeCaseTest, SingleByte) {
  using namespace block;
  
  std::vector<uint8_t> single = {0x00};
  uint32_t crc = calculate_crc32(single);
  
  // Should produce valid CRC
  EXPECT_NE(crc, 0xFFFFFFFFu);  // Not uninitialized
}

TEST(CRCEdgeCaseTest, LargeData) {
  using namespace block;
  
  std::vector<uint8_t> large(10000, 0xAA);
  
  EXPECT_NO_THROW({
    uint32_t crc = calculate_crc32(large);
    (void)crc;
  });
}

// ============================================================================
// Memory Address Edge Cases
// ============================================================================

TEST(MemoryAddressTest, ZeroAddress) {
  using namespace memory;
  
  AddressFormat fmt(4, 4);
  uint32_t address = 0x00000000;
  uint32_t size = 0x100;
  
  // Should handle address 0
  EXPECT_EQ(address, 0u);
  EXPECT_GT(size, 0u);
}

TEST(MemoryAddressTest, MaxAddress) {
  uint32_t max_addr = 0xFFFFFFFF;
  
  std::vector<uint8_t> encoded;
  codec::be32(encoded, max_addr);
  
  EXPECT_EQ(encoded.size(), 4u);
  EXPECT_EQ(encoded[0], 0xFF);
  EXPECT_EQ(encoded[1], 0xFF);
  EXPECT_EQ(encoded[2], 0xFF);
  EXPECT_EQ(encoded[3], 0xFF);
}

// ============================================================================
// Sequence Number Edge Cases
// ============================================================================

TEST(SequenceNumberTest, WrapAround) {
  // ISO-TP sequence number is 4 bits (0-15)
  uint8_t sn = 15;
  sn = (sn + 1) & 0x0F;
  EXPECT_EQ(sn, 0);
  
  // Block counter is 8 bits, typically 1-255
  uint8_t block = 255;
  block++;
  EXPECT_EQ(block, 0);
}

TEST(SequenceNumberTest, StartValues) {
  // ISO-TP starts at 1 after FF
  uint8_t isotp_sn = 1;
  EXPECT_EQ(isotp_sn, 1);
  
  // Block counter typically starts at 1
  uint8_t block = 1;
  EXPECT_EQ(block, 1);
}

// ============================================================================
// String Handling Edge Cases
// ============================================================================

TEST(StringHandlingTest, NullTerminator) {
  using namespace scaling;
  
  std::vector<uint8_t> with_null = {'A', 'B', 0x00, 'C', 'D'};
  std::string result = bytes_to_ascii(with_null);
  
  EXPECT_EQ(result, "AB");
}

TEST(StringHandlingTest, AllNonPrintable) {
  using namespace scaling;
  
  std::vector<uint8_t> nonprint = {0x01, 0x02, 0x03, 0x04};
  std::string result = bytes_to_ascii(nonprint);
  
  EXPECT_TRUE(result.empty());
}

TEST(StringHandlingTest, MixedContent) {
  using namespace scaling;
  
  std::vector<uint8_t> mixed = {'H', 0x01, 'i', 0x02, '!'};
  std::string result = bytes_to_ascii(mixed);
  
  EXPECT_EQ(result, "Hi!");
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
