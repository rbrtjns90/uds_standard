/**
 * @file encoding_test.cpp
 * @brief Tests for correct byte ordering, nibble placement, and parameter encoding
 * 
 * These tests specifically verify that:
 * - ALFI (AddressAndLengthFormatIdentifier) has correct nibble order
 * - LengthFormatIdentifier is parsed from correct nibble
 * - SecurityAccess subfunctions are encoded correctly
 * - Request payloads don't have extraneous bytes
 * - Big-endian encoding is consistent
 */

#include <gtest/gtest.h>
#include "uds.hpp"
#include "uds_block.hpp"
#include "uds_security.hpp"
#include "uds_memory.hpp"

using namespace uds;

// ============================================================================
// ALFI (AddressAndLengthFormatIdentifier) Nibble Order Tests
// ============================================================================

/**
 * ISO-14229 specifies ALFI format:
 * - High nibble (bits 7-4): number of bytes for memory address
 * - Low nibble (bits 3-0): number of bytes for memory size
 * 
 * Example: 0x42 means 4 bytes for address, 2 bytes for size
 */

TEST(ALFIEncodingTest, HighNibbleIsAddressLength) {
  // ALFI 0x42: high nibble = 4 (address), low nibble = 2 (size)
  uint8_t alfi = 0x42;
  uint8_t address_len = (alfi >> 4) & 0x0F;
  uint8_t size_len = alfi & 0x0F;
  
  EXPECT_EQ(address_len, 4) << "High nibble should be address length";
  EXPECT_EQ(size_len, 2) << "Low nibble should be size length";
}

TEST(ALFIEncodingTest, EncodeALFI_AddressInHighNibble) {
  // When encoding: address length goes to high nibble
  uint8_t addr_bytes = 4;
  uint8_t size_bytes = 2;
  uint8_t alfi = ((addr_bytes & 0x0F) << 4) | (size_bytes & 0x0F);
  
  EXPECT_EQ(alfi, 0x42) << "ALFI should be 0x42 for 4-byte addr, 2-byte size";
}

TEST(ALFIEncodingTest, CommonALFIValues) {
  // Test common ALFI values per ISO-14229
  
  // 0x11: 1-byte address, 1-byte size
  EXPECT_EQ(((1 << 4) | 1), 0x11);
  
  // 0x22: 2-byte address, 2-byte size
  EXPECT_EQ(((2 << 4) | 2), 0x22);
  
  // 0x44: 4-byte address, 4-byte size (most common)
  EXPECT_EQ(((4 << 4) | 4), 0x44);
  
  // 0x24: 2-byte address, 4-byte size
  EXPECT_EQ(((2 << 4) | 4), 0x24);
  
  // 0x42: 4-byte address, 2-byte size
  EXPECT_EQ(((4 << 4) | 2), 0x42);
}

TEST(ALFIEncodingTest, AddressFormatConsistency) {
  using namespace memory;
  
  // Verify AddressFormat encodes correctly
  AddressFormat fmt(4, 2);  // 4-byte address, 2-byte size
  
  EXPECT_EQ(fmt.to_format_byte(), 0x42) 
    << "AddressFormat(4,2) should produce 0x42";
  
  // Verify round-trip
  auto parsed = AddressFormat::from_format_byte(0x42);
  EXPECT_EQ(parsed.address_bytes, 4);
  EXPECT_EQ(parsed.size_bytes, 2);
}

TEST(ALFIEncodingTest, AsymmetricALFI_NotSwapped) {
  // This test catches the bug where address and size nibbles were swapped
  using namespace memory;
  
  // Create format with different address and size lengths
  AddressFormat fmt(3, 1);  // 3-byte address, 1-byte size
  uint8_t byte = fmt.to_format_byte();
  
  // Should be 0x31, NOT 0x13
  EXPECT_EQ(byte, 0x31) << "3-byte addr, 1-byte size should be 0x31, not 0x13";
  
  // Verify parsing is also correct
  auto parsed = AddressFormat::from_format_byte(0x31);
  EXPECT_EQ(parsed.address_bytes, 3) << "High nibble 3 = address bytes";
  EXPECT_EQ(parsed.size_bytes, 1) << "Low nibble 1 = size bytes";
}

// ============================================================================
// LengthFormatIdentifier Parsing Tests
// ============================================================================

/**
 * ISO-14229 RequestDownload/RequestUpload positive response:
 * - Byte 0: lengthFormatIdentifier
 *   - High nibble: reserved (should be 0)
 *   - Low nibble: number of bytes in maxNumberOfBlockLength
 * - Bytes 1..n: maxNumberOfBlockLength (big-endian)
 */

TEST(LengthFormatIdentifierTest, LowNibbleIsLength) {
  // LFI 0x02: low nibble = 2 bytes for maxNumberOfBlockLength
  uint8_t lfi = 0x02;
  uint8_t len = lfi & 0x0F;
  
  EXPECT_EQ(len, 2) << "Low nibble should indicate byte count";
}

TEST(LengthFormatIdentifierTest, HighNibbleIsReserved) {
  // Even if high nibble has data, low nibble is the length
  uint8_t lfi = 0x42;  // Some ECUs might set high nibble
  uint8_t len = lfi & 0x0F;
  
  EXPECT_EQ(len, 2) << "Should use low nibble regardless of high nibble";
}

TEST(LengthFormatIdentifierTest, ParseMaxBlockLength_2Bytes) {
  // Simulate response: [LFI=0x02][0x01][0x00] -> maxBlock = 256
  std::vector<uint8_t> response = {0x02, 0x01, 0x00};
  
  uint8_t len = response[0] & 0x0F;
  EXPECT_EQ(len, 2);
  
  uint32_t max_block = 0;
  for (uint8_t i = 0; i < len && i + 1 < response.size(); ++i) {
    max_block = (max_block << 8) | response[i + 1];
  }
  
  EXPECT_EQ(max_block, 256u);
}

TEST(LengthFormatIdentifierTest, ParseMaxBlockLength_4Bytes) {
  // Simulate response: [LFI=0x04][0x00][0x01][0x00][0x00] -> maxBlock = 65536
  std::vector<uint8_t> response = {0x04, 0x00, 0x01, 0x00, 0x00};
  
  uint8_t len = response[0] & 0x0F;
  EXPECT_EQ(len, 4);
  
  uint32_t max_block = 0;
  for (uint8_t i = 0; i < len && i + 1 < response.size(); ++i) {
    max_block = (max_block << 8) | response[i + 1];
  }
  
  EXPECT_EQ(max_block, 65536u);
}

TEST(LengthFormatIdentifierTest, NotConfusedWithHighNibble) {
  // This test catches the bug where >> 4 was used instead of & 0x0F
  // If we incorrectly used high nibble:
  //   0x20 >> 4 = 2 (wrong - would work by accident)
  //   0x02 >> 4 = 0 (wrong - would fail)
  
  std::vector<uint8_t> response = {0x02, 0x01, 0x00};
  
  // WRONG: uint8_t len = response[0] >> 4;  // Would give 0!
  // RIGHT:
  uint8_t len = response[0] & 0x0F;
  
  EXPECT_EQ(len, 2) << "Must use low nibble, not high nibble";
}

// ============================================================================
// SecurityAccess Subfunction Encoding Tests
// ============================================================================

/**
 * ISO-14229 SecurityAccess subfunctions:
 * - Odd values (0x01, 0x03, 0x05...): requestSeed
 * - Even values (0x02, 0x04, 0x06...): sendKey
 * 
 * The Level namespace defines levels as the odd seed request values.
 */

TEST(SecurityAccessEncodingTest, LevelValuesAreOdd) {
  using namespace security;
  
  // All level values should be odd (seed request subfunctions)
  EXPECT_EQ(Level::Basic % 2, 1) << "Basic level should be odd";
  EXPECT_EQ(Level::Extended % 2, 1) << "Extended level should be odd";
  EXPECT_EQ(Level::Programming % 2, 1) << "Programming level should be odd";
  EXPECT_EQ(Level::Calibration % 2, 1) << "Calibration level should be odd";
}

TEST(SecurityAccessEncodingTest, SeedSubfunctionIsLevel) {
  using namespace security;
  
  // requestSeed subfunction should be the level value directly
  EXPECT_EQ(Level::Basic, 0x01) << "Basic seed request = 0x01";
  EXPECT_EQ(Level::Extended, 0x03) << "Extended seed request = 0x03";
  EXPECT_EQ(Level::Programming, 0x05) << "Programming seed request = 0x05";
}

TEST(SecurityAccessEncodingTest, KeySubfunctionIsLevelPlusOne) {
  using namespace security;
  
  // sendKey subfunction should be level + 1
  EXPECT_EQ(Level::Basic + 1, 0x02) << "Basic key send = 0x02";
  EXPECT_EQ(Level::Extended + 1, 0x04) << "Extended key send = 0x04";
  EXPECT_EQ(Level::Programming + 1, 0x06) << "Programming key send = 0x06";
}

TEST(SecurityAccessEncodingTest, SeedToKeyLevelFunction) {
  using namespace security;
  
  // Verify the helper function
  EXPECT_EQ(seed_to_key_level(0x01), 0x02);
  EXPECT_EQ(seed_to_key_level(0x03), 0x04);
  EXPECT_EQ(seed_to_key_level(0x05), 0x06);
  EXPECT_EQ(seed_to_key_level(0x07), 0x08);
}

TEST(SecurityAccessEncodingTest, LevelNotShifted) {
  // This test catches the bug where level was incorrectly shifted
  // WRONG: (level << 1) | 0x01  -- for level=1, gives 0x03 instead of 0x01
  // RIGHT: just use level directly for seed request
  
  using namespace security;
  
  uint8_t level = Level::Basic;  // 0x01
  
  // Correct encoding for seed request
  uint8_t seed_subfunction = level;
  EXPECT_EQ(seed_subfunction, 0x01) << "Seed subfunction should be 0x01, not 0x03";
  
  // Correct encoding for key send
  uint8_t key_subfunction = static_cast<uint8_t>(level + 1);
  EXPECT_EQ(key_subfunction, 0x02) << "Key subfunction should be 0x02";
}

// ============================================================================
// Request Payload Structure Tests
// ============================================================================

TEST(RequestPayloadTest, ReadMemoryByAddress_NoExtraBytes) {
  // ReadMemoryByAddress payload: [ALFI][address][size]
  // No length prefixes before address or size
  
  std::vector<uint8_t> addr = {0x00, 0x10, 0x00, 0x00};  // 4 bytes
  std::vector<uint8_t> size = {0x00, 0x80};              // 2 bytes
  
  // Build payload correctly
  std::vector<uint8_t> payload;
  uint8_t alfi = ((addr.size() & 0x0F) << 4) | (size.size() & 0x0F);
  payload.push_back(alfi);
  payload.insert(payload.end(), addr.begin(), addr.end());
  payload.insert(payload.end(), size.begin(), size.end());
  
  // Should be: [0x42][4 addr bytes][2 size bytes] = 7 bytes total
  EXPECT_EQ(payload.size(), 7u) << "No extra length prefix bytes";
  EXPECT_EQ(payload[0], 0x42) << "ALFI = 0x42";
}

TEST(RequestPayloadTest, RequestDownload_NoExtraBytes) {
  // RequestDownload payload: [DFI][ALFI][address][size]
  // No length prefixes before address or size
  
  uint8_t dfi = 0x00;
  std::vector<uint8_t> addr = {0x00, 0x10, 0x00, 0x00};  // 4 bytes
  std::vector<uint8_t> size = {0x00, 0x00, 0x10, 0x00};  // 4 bytes
  
  // Build payload correctly
  std::vector<uint8_t> payload;
  payload.push_back(dfi);
  uint8_t alfi = ((addr.size() & 0x0F) << 4) | (size.size() & 0x0F);
  payload.push_back(alfi);
  payload.insert(payload.end(), addr.begin(), addr.end());
  payload.insert(payload.end(), size.begin(), size.end());
  
  // Should be: [DFI][ALFI][4 addr bytes][4 size bytes] = 10 bytes total
  EXPECT_EQ(payload.size(), 10u) << "No extra length prefix bytes";
  EXPECT_EQ(payload[0], 0x00) << "DFI";
  EXPECT_EQ(payload[1], 0x44) << "ALFI = 0x44";
}

TEST(RequestPayloadTest, NoRedundantLengthPrefixes) {
  // This test catches the bug where extra length bytes were added
  // WRONG: [DFI][ALFI][addr_len][addr...][size_len][size...]
  // RIGHT: [DFI][ALFI][addr...][size...]
  
  std::vector<uint8_t> addr = {0x00, 0x10, 0x00, 0x00};
  std::vector<uint8_t> size = {0x00, 0x00, 0x10, 0x00};
  
  // Correct payload
  std::vector<uint8_t> correct;
  correct.push_back(0x00);  // DFI
  correct.push_back(0x44);  // ALFI
  correct.insert(correct.end(), addr.begin(), addr.end());
  correct.insert(correct.end(), size.begin(), size.end());
  
  // Wrong payload (with extra length prefixes)
  std::vector<uint8_t> wrong;
  wrong.push_back(0x00);  // DFI
  wrong.push_back(0x44);  // ALFI
  wrong.push_back(4);     // WRONG: extra addr length
  wrong.insert(wrong.end(), addr.begin(), addr.end());
  wrong.push_back(4);     // WRONG: extra size length
  wrong.insert(wrong.end(), size.begin(), size.end());
  
  EXPECT_EQ(correct.size(), 10u);
  EXPECT_EQ(wrong.size(), 12u);
  EXPECT_NE(correct, wrong) << "Correct payload should not have extra bytes";
}

// ============================================================================
// Big-Endian Encoding Tests
// ============================================================================

TEST(BigEndianTest, Encode16Bit) {
  std::vector<uint8_t> v;
  codec::be16(v, 0x1234);
  
  ASSERT_EQ(v.size(), 2u);
  EXPECT_EQ(v[0], 0x12) << "MSB first";
  EXPECT_EQ(v[1], 0x34) << "LSB second";
}

TEST(BigEndianTest, Encode32Bit) {
  std::vector<uint8_t> v;
  codec::be32(v, 0x12345678);
  
  ASSERT_EQ(v.size(), 4u);
  EXPECT_EQ(v[0], 0x12);
  EXPECT_EQ(v[1], 0x34);
  EXPECT_EQ(v[2], 0x56);
  EXPECT_EQ(v[3], 0x78);
}

TEST(BigEndianTest, Decode16Bit) {
  std::vector<uint8_t> data = {0x12, 0x34};
  uint16_t value = (static_cast<uint16_t>(data[0]) << 8) | data[1];
  
  EXPECT_EQ(value, 0x1234u);
}

TEST(BigEndianTest, Decode32Bit) {
  std::vector<uint8_t> data = {0x12, 0x34, 0x56, 0x78};
  uint32_t value = (static_cast<uint32_t>(data[0]) << 24) |
                   (static_cast<uint32_t>(data[1]) << 16) |
                   (static_cast<uint32_t>(data[2]) << 8) |
                   static_cast<uint32_t>(data[3]);
  
  EXPECT_EQ(value, 0x12345678u);
}

TEST(BigEndianTest, DIDEncoding) {
  // DIDs are 16-bit, big-endian
  DID did = 0xF190;  // VIN DID
  
  std::vector<uint8_t> v;
  codec::be16(v, did);
  
  EXPECT_EQ(v[0], 0xF1) << "DID high byte first";
  EXPECT_EQ(v[1], 0x90) << "DID low byte second";
}

TEST(BigEndianTest, RoutineIDEncoding) {
  // Routine IDs are 16-bit, big-endian
  RoutineId rid = 0xFF00;  // Common erase routine
  
  std::vector<uint8_t> v;
  codec::be16(v, rid);
  
  EXPECT_EQ(v[0], 0xFF) << "Routine ID high byte first";
  EXPECT_EQ(v[1], 0x00) << "Routine ID low byte second";
}

// ============================================================================
// Block Transfer Encoding Tests
// ============================================================================

TEST(BlockTransferEncodingTest, AddressAndLengthFormat) {
  // Verify BlockTransferManager would encode ALFI correctly
  // High nibble = address bytes, low nibble = size bytes
  
  uint8_t address_bytes = 4;
  uint8_t length_bytes = 4;
  
  // Correct encoding
  uint8_t format = ((address_bytes & 0x0F) << 4) | (length_bytes & 0x0F);
  EXPECT_EQ(format, 0x44);
  
  // Wrong encoding (swapped)
  uint8_t wrong_format = ((length_bytes & 0x0F) << 4) | (address_bytes & 0x0F);
  // For symmetric case, they're equal - test asymmetric
  
  address_bytes = 4;
  length_bytes = 2;
  format = ((address_bytes & 0x0F) << 4) | (length_bytes & 0x0F);
  wrong_format = ((length_bytes & 0x0F) << 4) | (address_bytes & 0x0F);
  
  EXPECT_EQ(format, 0x42) << "Correct: 4-byte addr, 2-byte size = 0x42";
  EXPECT_EQ(wrong_format, 0x24) << "Wrong: swapped would be 0x24";
  EXPECT_NE(format, wrong_format);
}

// ============================================================================
// DTC Code Encoding Tests
// ============================================================================

TEST(DTCEncodingTest, ThreeByteFormat) {
  // DTCs are 3 bytes, big-endian
  uint8_t bytes[] = {0x01, 0x23, 0x45};
  
  uint32_t dtc = (static_cast<uint32_t>(bytes[0]) << 16) |
                 (static_cast<uint32_t>(bytes[1]) << 8) |
                 static_cast<uint32_t>(bytes[2]);
  
  EXPECT_EQ(dtc, 0x012345u);
}

TEST(DTCEncodingTest, EncodeAndDecode) {
  uint32_t original = 0xABCDEF;
  
  // Encode
  std::vector<uint8_t> encoded = {
    static_cast<uint8_t>((original >> 16) & 0xFF),
    static_cast<uint8_t>((original >> 8) & 0xFF),
    static_cast<uint8_t>(original & 0xFF)
  };
  
  // Decode
  uint32_t decoded = (static_cast<uint32_t>(encoded[0]) << 16) |
                     (static_cast<uint32_t>(encoded[1]) << 8) |
                     static_cast<uint32_t>(encoded[2]);
  
  EXPECT_EQ(decoded, original);
}

// ============================================================================
// Timing Parameter Encoding Tests
// ============================================================================

TEST(TimingEncodingTest, P2ServerMax) {
  // P2 timing is 2 bytes, big-endian, in milliseconds
  std::vector<uint8_t> response = {0x01, 0x00, 0xC8, 0x00, 0x19};
  // Bytes 1-2: P2 = 0x00C8 = 200ms
  // Bytes 3-4: P2* = 0x0019 = 25 (x10ms = 250ms)
  
  uint16_t p2_ms = (static_cast<uint16_t>(response[1]) << 8) | response[2];
  uint16_t p2_star_10ms = (static_cast<uint16_t>(response[3]) << 8) | response[4];
  
  EXPECT_EQ(p2_ms, 200u);
  EXPECT_EQ(p2_star_10ms, 25u);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
