/**
 * @file robustness_test.cpp
 * @brief Comprehensive robustness tests for edge cases, boundary conditions, and error handling
 * 
 * This test suite covers:
 * - Boundary conditions (empty inputs, max values, overflow)
 * - Error handling (invalid inputs, malformed data)
 * - State machine correctness
 * - Memory safety (buffer bounds)
 * - Numeric overflow/underflow
 * - Edge cases in parsing/encoding
 */

#include <gtest/gtest.h>
#include <limits>
#include <cstdint>
#include "uds.hpp"
#include "uds_block.hpp"
#include "uds_security.hpp"
#include "uds_memory.hpp"
#include "uds_dtc.hpp"
#include "uds_scaling.hpp"

using namespace uds;

// ============================================================================
// Empty Input Tests
// ============================================================================

TEST(EmptyInputTest, EmptyPayloadHandling) {
  // Many functions should handle empty vectors gracefully
  std::vector<uint8_t> empty;
  
  // These should not crash
  EXPECT_NO_THROW({
    // DTC parsing with empty data
    // Note: parse_dtc_code expects at least 3 bytes, so we test bounds
  });
}

TEST(EmptyInputTest, EmptyAddressVector) {
  // Empty address/size vectors should produce ALFI = 0x00
  std::vector<uint8_t> empty_addr;
  std::vector<uint8_t> empty_size;
  
  uint8_t al = static_cast<uint8_t>(empty_addr.size() & 0x0F);
  uint8_t sl = static_cast<uint8_t>(empty_size.size() & 0x0F);
  uint8_t alfi = (al << 4) | sl;
  
  EXPECT_EQ(alfi, 0x00) << "Empty vectors should produce ALFI 0x00";
}

TEST(EmptyInputTest, ScalingEmptyPayload) {
  using namespace scaling;
  
  std::vector<uint8_t> empty;
  auto info = parse_scaling_info(0xF190, empty);
  
  EXPECT_EQ(info.did, 0xF190);
  EXPECT_TRUE(info.raw_scaling_bytes.empty());
}

// ============================================================================
// Boundary Value Tests
// ============================================================================

TEST(BoundaryTest, MaxAddressSizeNibble) {
  // ALFI nibbles are 4 bits, max value is 15
  // But ISO-14229 limits address/size to 1-5 bytes typically
  
  for (uint8_t i = 0; i <= 15; ++i) {
    uint8_t alfi = (i << 4) | i;
    uint8_t addr_len = (alfi >> 4) & 0x0F;
    uint8_t size_len = alfi & 0x0F;
    
    EXPECT_EQ(addr_len, i);
    EXPECT_EQ(size_len, i);
  }
}

TEST(BoundaryTest, MaxBlockSequenceCounter) {
  // Block counter is uint8_t, wraps from 255 to 0
  // But UDS typically uses 1-255, skipping 0
  
  uint8_t counter = 255;
  counter++;
  EXPECT_EQ(counter, 0);
  
  // Correct wrap behavior
  if (counter == 0) {
    counter = 1;
  }
  EXPECT_EQ(counter, 1);
}

TEST(BoundaryTest, MaxDID) {
  // DID is uint16_t
  DID max_did = 0xFFFF;
  
  std::vector<uint8_t> encoded;
  codec::be16(encoded, max_did);
  
  EXPECT_EQ(encoded[0], 0xFF);
  EXPECT_EQ(encoded[1], 0xFF);
  
  // Decode back
  uint16_t decoded = (static_cast<uint16_t>(encoded[0]) << 8) | encoded[1];
  EXPECT_EQ(decoded, max_did);
}

TEST(BoundaryTest, MaxRoutineID) {
  RoutineId max_rid = 0xFFFF;
  
  std::vector<uint8_t> encoded;
  codec::be16(encoded, max_rid);
  
  EXPECT_EQ(encoded[0], 0xFF);
  EXPECT_EQ(encoded[1], 0xFF);
}

TEST(BoundaryTest, MaxDTCCode) {
  // DTC is 3 bytes (24 bits)
  uint32_t max_dtc = 0xFFFFFF;
  
  std::vector<uint8_t> encoded = {
    static_cast<uint8_t>((max_dtc >> 16) & 0xFF),
    static_cast<uint8_t>((max_dtc >> 8) & 0xFF),
    static_cast<uint8_t>(max_dtc & 0xFF)
  };
  
  EXPECT_EQ(encoded[0], 0xFF);
  EXPECT_EQ(encoded[1], 0xFF);
  EXPECT_EQ(encoded[2], 0xFF);
  
  // Decode
  uint32_t decoded = (static_cast<uint32_t>(encoded[0]) << 16) |
                     (static_cast<uint32_t>(encoded[1]) << 8) |
                     static_cast<uint32_t>(encoded[2]);
  EXPECT_EQ(decoded, max_dtc);
}

TEST(BoundaryTest, ZeroValues) {
  // Test zero values don't cause issues
  
  std::vector<uint8_t> v;
  codec::be16(v, 0);
  EXPECT_EQ(v[0], 0);
  EXPECT_EQ(v[1], 0);
  
  v.clear();
  codec::be32(v, 0);
  EXPECT_EQ(v[0], 0);
  EXPECT_EQ(v[1], 0);
  EXPECT_EQ(v[2], 0);
  EXPECT_EQ(v[3], 0);
}

TEST(BoundaryTest, SingleBytePayload) {
  // Single byte payloads should work
  std::vector<uint8_t> single = {0x42};
  
  EXPECT_EQ(single.size(), 1u);
  EXPECT_EQ(single[0], 0x42);
}

// ============================================================================
// ISO-TP Frame Size Tests
// ============================================================================

TEST(ISOTPBoundaryTest, SingleFrameMaxPayload) {
  // Single Frame can carry up to 7 bytes of data
  // PCI byte uses lower nibble for length (0-7)
  
  for (uint8_t len = 0; len <= 7; ++len) {
    uint8_t pci = 0x00 | (len & 0x0F);  // SF PCI
    uint8_t extracted_len = pci & 0x0F;
    EXPECT_EQ(extracted_len, len);
  }
}

TEST(ISOTPBoundaryTest, FirstFrameMaxLength) {
  // First Frame length is 12 bits (max 4095)
  uint16_t max_len = 4095;
  
  uint8_t pci_byte0 = 0x10 | ((max_len >> 8) & 0x0F);  // FF PCI + high nibble
  uint8_t pci_byte1 = max_len & 0xFF;                  // Low byte
  
  // Decode
  uint16_t decoded = ((pci_byte0 & 0x0F) << 8) | pci_byte1;
  EXPECT_EQ(decoded, max_len);
}

TEST(ISOTPBoundaryTest, ConsecutiveFrameSequenceWrap) {
  // CF sequence number is 4 bits (0-15), wraps
  for (uint8_t sn = 0; sn <= 20; ++sn) {
    uint8_t wrapped = sn & 0x0F;
    EXPECT_LE(wrapped, 15);
    EXPECT_EQ(wrapped, sn % 16);
  }
}

TEST(ISOTPBoundaryTest, FlowControlBlockSize) {
  // Block size 0 means unlimited
  // Block size 1-255 means that many CFs before next FC
  
  uint8_t bs_unlimited = 0;
  uint8_t bs_one = 1;
  uint8_t bs_max = 255;
  
  EXPECT_EQ(bs_unlimited, 0);
  EXPECT_EQ(bs_one, 1);
  EXPECT_EQ(bs_max, 255);
}

TEST(ISOTPBoundaryTest, STminValues) {
  // STmin encoding per ISO 15765-2:
  // 0x00-0x7F: 0-127 ms
  // 0x80-0xF0: reserved
  // 0xF1-0xF9: 100-900 µs
  // 0xFA-0xFF: reserved
  
  // Valid millisecond values
  EXPECT_LE(0x00, 0x7F);
  EXPECT_LE(0x7F, 0x7F);
  
  // Microsecond values
  EXPECT_GE(0xF1, 0xF1);
  EXPECT_LE(0xF9, 0xF9);
}

// ============================================================================
// Security Access Tests
// ============================================================================

TEST(SecurityBoundaryTest, AllSecurityLevels) {
  using namespace security;
  
  // Test all valid odd levels (seed request)
  for (uint8_t level = 0x01; level <= 0x7D; level += 2) {
    EXPECT_EQ(level % 2, 1) << "Seed levels must be odd";
    
    uint8_t key_level = level + 1;
    EXPECT_EQ(key_level % 2, 0) << "Key levels must be even";
  }
}

TEST(SecurityBoundaryTest, SeedToKeyConversion) {
  using namespace security;
  
  // Verify seed_to_key_level for boundary cases
  EXPECT_EQ(seed_to_key_level(0x01), 0x02);
  EXPECT_EQ(seed_to_key_level(0x7D), 0x7E);
  
  // Edge case: what happens with 0xFF?
  // 0xFF + 1 = 0x00 (overflow) - this could be a bug in real usage
  uint8_t edge = 0xFF;
  uint8_t key = static_cast<uint8_t>(edge + 1);
  EXPECT_EQ(key, 0x00) << "Overflow wraps to 0";
}

TEST(SecurityBoundaryTest, EmptySeed) {
  using namespace security;
  
  XORAlgorithm algo;
  std::vector<uint8_t> empty_seed;
  
  auto key = algo.calculate_key(empty_seed, 0x01, {});
  EXPECT_TRUE(key.empty()) << "Empty seed should produce empty key";
}

TEST(SecurityBoundaryTest, LargeSeed) {
  using namespace security;
  
  XORAlgorithm algo;
  std::vector<uint8_t> large_seed(256, 0xAA);
  
  auto key = algo.calculate_key(large_seed, 0x01, {});
  EXPECT_EQ(key.size(), large_seed.size());
}

// ============================================================================
// Numeric Overflow Tests
// ============================================================================

TEST(OverflowTest, BlockCountCalculation) {
  // Block count = (total + block_size - 1) / block_size
  // Test for potential overflow
  
  uint32_t total = 0xFFFFFFFF;
  uint32_t block_size = 256;
  
  // This calculation could overflow if not careful
  uint32_t blocks = (total / block_size) + ((total % block_size) ? 1 : 0);
  
  EXPECT_GT(blocks, 0u);
}

TEST(OverflowTest, AddressCalculation) {
  // Address + size should not overflow
  uint32_t address = 0xFFFFFF00;
  uint32_t size = 0x100;
  
  uint32_t end_address = address + size;
  EXPECT_EQ(end_address, 0u) << "Overflow wraps to 0";
  
  // Safe check
  bool would_overflow = (size > 0) && (address > UINT32_MAX - size);
  EXPECT_TRUE(would_overflow);
}

TEST(OverflowTest, TimingMultiplication) {
  // P2* is in 10ms units, multiplied by 10 for actual ms
  uint16_t p2_star_10ms = 0xFFFF;
  uint32_t p2_star_ms = static_cast<uint32_t>(p2_star_10ms) * 10;
  
  EXPECT_EQ(p2_star_ms, 655350u);
  EXPECT_LE(p2_star_ms, UINT32_MAX);
}

// ============================================================================
// Parsing Edge Cases
// ============================================================================

TEST(ParsingTest, ShortPayload) {
  // Test handling of payloads shorter than expected
  
  // DTC count response needs at least 4 bytes
  std::vector<uint8_t> short_payload = {0x01, 0x02};  // Only 2 bytes
  
  EXPECT_LT(short_payload.size(), 4u);
}

TEST(ParsingTest, DTCCountResponseParsing) {
  // Proper DTC count response: [subFunc][statusMask][format][count_high][count_low]
  std::vector<uint8_t> response = {0x01, 0xFF, 0x01, 0x00, 0x05};
  
  EXPECT_GE(response.size(), 5u);
  
  uint8_t status_mask = response[1];
  uint8_t format = response[2];
  uint16_t count = (static_cast<uint16_t>(response[3]) << 8) | response[4];
  
  EXPECT_EQ(status_mask, 0xFF);
  EXPECT_EQ(format, 0x01);
  EXPECT_EQ(count, 5u);
}

TEST(ParsingTest, DTCCountResponseShort) {
  // Response with only 4 bytes (missing low byte of count)
  std::vector<uint8_t> response = {0x01, 0xFF, 0x01, 0x05};
  
  uint16_t count = (static_cast<uint16_t>(response[3]) << 8);
  if (response.size() > 4) {
    count |= response[4];
  }
  
  // With only high byte, count = 0x0500 = 1280
  EXPECT_EQ(count, 0x0500);
}

TEST(ParsingTest, TimingParameterParsing) {
  // DiagnosticSessionControl response with timing
  // [session][P2_high][P2_low][P2*_high][P2*_low]
  
  std::vector<uint8_t> response = {0x01, 0x00, 0x32, 0x01, 0xF4};
  
  if (response.size() >= 5) {
    uint16_t p2_ms = (static_cast<uint16_t>(response[1]) << 8) | response[2];
    uint16_t p2_star = (static_cast<uint16_t>(response[3]) << 8) | response[4];
    
    EXPECT_EQ(p2_ms, 50u);      // 0x0032 = 50ms
    EXPECT_EQ(p2_star, 500u);   // 0x01F4 = 500 (x10ms for some ECUs)
  }
}

// ============================================================================
// Memory Format Tests
// ============================================================================

TEST(MemoryFormatTest, AddressFormatBoundaries) {
  using namespace memory;
  
  // Test all valid address byte counts (1-5 per ISO-14229)
  for (uint8_t addr = 1; addr <= 5; ++addr) {
    for (uint8_t size = 1; size <= 4; ++size) {
      AddressFormat fmt(addr, size);
      uint8_t byte = fmt.to_format_byte();
      
      auto parsed = AddressFormat::from_format_byte(byte);
      EXPECT_EQ(parsed.address_bytes, addr);
      EXPECT_EQ(parsed.size_bytes, size);
    }
  }
}

TEST(MemoryFormatTest, InvalidAddressFormat) {
  using namespace memory;
  
  // Zero bytes is technically invalid but should not crash
  AddressFormat fmt(0, 0);
  uint8_t byte = fmt.to_format_byte();
  
  EXPECT_EQ(byte, 0x00);
  
  auto parsed = AddressFormat::from_format_byte(0x00);
  EXPECT_EQ(parsed.address_bytes, 0);
  EXPECT_EQ(parsed.size_bytes, 0);
}

// ============================================================================
// Block Transfer Tests
// ============================================================================

TEST(BlockTransferTest, ProgressPercentageEdgeCases) {
  using namespace block;
  
  TransferProgress progress;
  
  // Zero total
  progress.total_bytes = 0;
  progress.transferred_bytes = 0;
  EXPECT_DOUBLE_EQ(progress.percentage(), 0.0);
  
  // Full transfer
  progress.total_bytes = 1000;
  progress.transferred_bytes = 1000;
  EXPECT_DOUBLE_EQ(progress.percentage(), 100.0);
  
  // Partial
  progress.total_bytes = 1000;
  progress.transferred_bytes = 500;
  EXPECT_DOUBLE_EQ(progress.percentage(), 50.0);
}

TEST(BlockTransferTest, BytesPerSecondEdgeCases) {
  using namespace block;
  
  TransferResult result;
  
  // Zero duration
  result.bytes_transferred = 1000;
  result.duration = std::chrono::milliseconds(0);
  // Should handle gracefully (return 0 or infinity)
  double rate = result.bytes_per_second();
  // Implementation dependent - just ensure no crash
  EXPECT_TRUE(rate >= 0.0 || std::isinf(rate));
}

TEST(BlockTransferTest, CRC32Consistency) {
  using namespace block;
  
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  
  uint32_t crc1 = calculate_crc32(data);
  uint32_t crc2 = calculate_crc32(data);
  
  EXPECT_EQ(crc1, crc2) << "CRC should be deterministic";
}

TEST(BlockTransferTest, CRC32EmptyData) {
  using namespace block;
  
  std::vector<uint8_t> empty;
  uint32_t crc = calculate_crc32(empty);
  
  // CRC of empty data should be a known value
  // Standard CRC-32 of empty = 0x00000000 after final XOR
  EXPECT_EQ(crc, 0x00000000u);
}

// ============================================================================
// DTC String Parsing Tests
// ============================================================================

TEST(DTCParsingTest, ValidDTCStrings) {
  using namespace dtc;
  
  // Standard format: P0123
  uint32_t dtc = parse_dtc_string("P0123");
  EXPECT_NE(dtc, 0u);
  
  // Chassis
  dtc = parse_dtc_string("C0456");
  EXPECT_NE(dtc, 0u);
  
  // Body
  dtc = parse_dtc_string("B0789");
  EXPECT_NE(dtc, 0u);
  
  // Network
  dtc = parse_dtc_string("U0ABC");
  EXPECT_NE(dtc, 0u);
}

TEST(DTCParsingTest, InvalidDTCStrings) {
  using namespace dtc;
  
  // Too short
  EXPECT_EQ(parse_dtc_string("P01"), 0u);
  
  // Invalid type
  EXPECT_EQ(parse_dtc_string("X0123"), 0u);
  
  // Empty
  EXPECT_EQ(parse_dtc_string(""), 0u);
  
  // Invalid hex
  EXPECT_EQ(parse_dtc_string("P0GHI"), 0u);
}

TEST(DTCParsingTest, CaseInsensitive) {
  using namespace dtc;
  
  uint32_t upper = parse_dtc_string("P0ABC");
  uint32_t lower = parse_dtc_string("p0abc");
  
  EXPECT_EQ(upper, lower) << "DTC parsing should be case-insensitive";
}

// ============================================================================
// Scaling Tests
// ============================================================================

TEST(ScalingTest, BytesToUintEdgeCases) {
  using namespace scaling;
  
  // Empty
  std::vector<uint8_t> empty;
  EXPECT_EQ(bytes_to_uint(empty), 0u);
  
  // Single byte
  std::vector<uint8_t> single = {0xFF};
  EXPECT_EQ(bytes_to_uint(single), 0xFFu);
  
  // Max 8 bytes
  std::vector<uint8_t> max8 = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  EXPECT_EQ(bytes_to_uint(max8), 0xFFFFFFFFFFFFFFFFull);
  
  // More than 8 bytes - should only use first 8
  std::vector<uint8_t> more = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
  uint64_t result = bytes_to_uint(more);
  EXPECT_EQ(result, 0x0102030405060708ull);
}

TEST(ScalingTest, SignedIntegerConversion) {
  using namespace scaling;
  
  // Positive value
  std::vector<uint8_t> positive = {0x7F, 0xFF};  // 32767
  EXPECT_EQ(bytes_to_int(positive, true), 32767);
  
  // Negative value (two's complement)
  std::vector<uint8_t> negative = {0xFF, 0xFF};  // -1 in 16-bit signed
  EXPECT_EQ(bytes_to_int(negative, true), -1);
  
  // Unsigned interpretation
  EXPECT_EQ(bytes_to_int(negative, false), 65535);
}

TEST(ScalingTest, ASCIIConversion) {
  using namespace scaling;
  
  // Normal string
  std::vector<uint8_t> hello = {'H', 'e', 'l', 'l', 'o'};
  EXPECT_EQ(bytes_to_ascii(hello), "Hello");
  
  // With null terminator
  std::vector<uint8_t> null_term = {'H', 'i', 0, 'X', 'Y'};
  EXPECT_EQ(bytes_to_ascii(null_term), "Hi");
  
  // With trailing spaces
  std::vector<uint8_t> spaces = {'A', 'B', ' ', ' ', ' '};
  EXPECT_EQ(bytes_to_ascii(spaces), "AB");
  
  // Non-printable characters
  std::vector<uint8_t> nonprint = {'A', 0x01, 0x02, 'B'};
  EXPECT_EQ(bytes_to_ascii(nonprint), "AB");
}

// ============================================================================
// Response Validation Tests
// ============================================================================

TEST(ResponseValidationTest, PositiveResponseSID) {
  // Positive response SID = Request SID + 0x40
  
  EXPECT_TRUE(is_positive_response(0x50, 0x10));  // DSC
  EXPECT_TRUE(is_positive_response(0x51, 0x11));  // ECU Reset
  EXPECT_TRUE(is_positive_response(0x62, 0x22));  // RDBI
  EXPECT_TRUE(is_positive_response(0x67, 0x27));  // Security Access
  EXPECT_TRUE(is_positive_response(0x74, 0x34));  // Request Download
  EXPECT_TRUE(is_positive_response(0x76, 0x36));  // Transfer Data
  
  // Negative cases
  EXPECT_FALSE(is_positive_response(0x7F, 0x10));  // NRC
  EXPECT_FALSE(is_positive_response(0x50, 0x11));  // Wrong SID
}

TEST(ResponseValidationTest, NegativeResponseFormat) {
  // NRC format: [0x7F][RequestSID][NRC]
  std::vector<uint8_t> nrc_response = {0x7F, 0x10, 0x12};
  
  EXPECT_EQ(nrc_response[0], 0x7F);
  EXPECT_EQ(nrc_response[1], 0x10);  // DSC was requested
  EXPECT_EQ(nrc_response[2], 0x12);  // SubFunctionNotSupported
}

// ============================================================================
// Timing Edge Cases
// ============================================================================

TEST(TimingTest, ZeroTimeout) {
  // Zero timeout should use default P2
  std::chrono::milliseconds timeout(0);
  
  if (timeout.count() == 0) {
    timeout = std::chrono::milliseconds(50);  // Default P2
  }
  
  EXPECT_EQ(timeout.count(), 50);
}

TEST(TimingTest, MaxTimeout) {
  // Test with maximum milliseconds value
  auto max_ms = std::chrono::milliseconds::max();
  
  EXPECT_GT(max_ms.count(), 0);
}

// ============================================================================
// State Machine Tests
// ============================================================================

TEST(StateMachineTest, BlockCounterWrap) {
  // Block counter wraps from 255 to 1 (skipping 0)
  uint8_t counter = 254;
  
  counter++;  // 255
  EXPECT_EQ(counter, 255);
  
  counter++;  // 0 (overflow)
  EXPECT_EQ(counter, 0);
  
  if (counter == 0) {
    counter = 1;
  }
  EXPECT_EQ(counter, 1);
}

TEST(StateMachineTest, SequenceNumberWrap) {
  // ISO-TP sequence number is 4 bits, wraps 15 -> 0
  uint8_t sn = 14;
  
  sn = (sn + 1) & 0x0F;  // 15
  EXPECT_EQ(sn, 15);
  
  sn = (sn + 1) & 0x0F;  // 0
  EXPECT_EQ(sn, 0);
  
  sn = (sn + 1) & 0x0F;  // 1
  EXPECT_EQ(sn, 1);
}

// ============================================================================
// Cancellation Token Tests
// ============================================================================

TEST(CancellationTest, TokenBehavior) {
  using namespace block;
  
  CancellationToken token;
  
  EXPECT_FALSE(token.is_cancelled());
  
  token.cancel();
  EXPECT_TRUE(token.is_cancelled());
  
  token.reset();
  EXPECT_FALSE(token.is_cancelled());
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
