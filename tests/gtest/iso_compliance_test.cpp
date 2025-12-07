/**
 * @file iso_compliance_test.cpp
 * @brief ISO 14229-1 Spec-Anchored Compliance Tests
 * 
 * These tests verify that our implementation matches the exact byte-level
 * encoding specified in ISO 14229-1. They are anchored to specific examples
 * and diagrams from the standard, NOT derived from our own implementation.
 * 
 * Purpose: Catch spec-level bugs that internal consistency tests cannot detect.
 * 
 * References:
 * - ISO 14229-1:2020 Section 14.2 (RequestDownload)
 * - ISO 14229-1:2020 Section 14.3 (RequestUpload)
 * - ISO 14229-1:2020 Annex G (addressAndLengthFormatIdentifier)
 */

#include <gtest/gtest.h>
#include "uds_block.hpp"
#include "ecu_programming.hpp"
#include <vector>
#include <cstdint>

using namespace uds;
using namespace uds::block;

// ============================================================================
// ISO 14229-1 addressAndLengthFormatIdentifier (ALFI) Tests
// 
// Per ISO 14229-1:2020 Annex G.1:
//   Bits 7-4 (high nibble): memorySizeLength (number of bytes for memorySize)
//   Bits 3-0 (low nibble):  memoryAddressLength (number of bytes for memoryAddress)
//
// Example from spec: ALFI = 0x44 means:
//   - memorySize uses 4 bytes (high nibble = 4)
//   - memoryAddress uses 4 bytes (low nibble = 4)
// ============================================================================

class ALFIComplianceTest : public ::testing::Test {
protected:
    // Helper to build ALFI byte per ISO 14229-1 spec
    static uint8_t build_alfi_per_spec(uint8_t memory_size_bytes, uint8_t memory_address_bytes) {
        // ISO 14229-1: high nibble = memorySizeLength, low nibble = memoryAddressLength
        return ((memory_size_bytes & 0x0F) << 4) | (memory_address_bytes & 0x0F);
    }
    
    // Helper to parse ALFI byte per ISO 14229-1 spec
    static void parse_alfi_per_spec(uint8_t alfi, uint8_t& memory_size_bytes, uint8_t& memory_address_bytes) {
        // ISO 14229-1: high nibble = memorySizeLength, low nibble = memoryAddressLength
        memory_size_bytes = (alfi >> 4) & 0x0F;
        memory_address_bytes = alfi & 0x0F;
    }
};

// Test: ALFI 0x44 per ISO spec (4-byte size, 4-byte address)
TEST_F(ALFIComplianceTest, ALFI_0x44_PerSpec) {
    // ISO 14229-1 example: ALFI = 0x44
    // High nibble (4) = 4 bytes for memorySize
    // Low nibble (4) = 4 bytes for memoryAddress
    
    uint8_t alfi = 0x44;
    uint8_t size_bytes, addr_bytes;
    parse_alfi_per_spec(alfi, size_bytes, addr_bytes);
    
    EXPECT_EQ(4, size_bytes) << "ALFI 0x44: high nibble should indicate 4-byte memorySize";
    EXPECT_EQ(4, addr_bytes) << "ALFI 0x44: low nibble should indicate 4-byte memoryAddress";
}

// Test: ALFI 0x24 per ISO spec (2-byte size, 4-byte address)
TEST_F(ALFIComplianceTest, ALFI_0x24_PerSpec) {
    // ALFI = 0x24
    // High nibble (2) = 2 bytes for memorySize
    // Low nibble (4) = 4 bytes for memoryAddress
    
    uint8_t alfi = 0x24;
    uint8_t size_bytes, addr_bytes;
    parse_alfi_per_spec(alfi, size_bytes, addr_bytes);
    
    EXPECT_EQ(2, size_bytes) << "ALFI 0x24: high nibble should indicate 2-byte memorySize";
    EXPECT_EQ(4, addr_bytes) << "ALFI 0x24: low nibble should indicate 4-byte memoryAddress";
}

// Test: ALFI 0x42 per ISO spec (4-byte size, 2-byte address)
TEST_F(ALFIComplianceTest, ALFI_0x42_PerSpec) {
    // ALFI = 0x42
    // High nibble (4) = 4 bytes for memorySize
    // Low nibble (2) = 2 bytes for memoryAddress
    
    uint8_t alfi = 0x42;
    uint8_t size_bytes, addr_bytes;
    parse_alfi_per_spec(alfi, size_bytes, addr_bytes);
    
    EXPECT_EQ(4, size_bytes) << "ALFI 0x42: high nibble should indicate 4-byte memorySize";
    EXPECT_EQ(2, addr_bytes) << "ALFI 0x42: low nibble should indicate 2-byte memoryAddress";
}

// Test: Build ALFI for asymmetric case
TEST_F(ALFIComplianceTest, BuildALFI_Asymmetric) {
    // Want: 3-byte memorySize, 4-byte memoryAddress
    // Per spec: ALFI = (3 << 4) | 4 = 0x34
    
    uint8_t alfi = build_alfi_per_spec(3, 4);
    EXPECT_EQ(0x34, alfi) << "3-byte size + 4-byte address should produce ALFI 0x34";
}

// ============================================================================
// Verify our implementation matches ISO spec for ALFI
// ============================================================================

TEST(ALFIImplementationTest, EncodeAddressAndLength_MatchesSpec) {
    // Create a mock client for BlockTransferManager
    // We'll test the encode_address_and_length function
    
    // Per ISO 14229-1, for RequestDownload with:
    //   memoryAddress = 0x08004000 (4 bytes)
    //   memorySize = 0x00010000 (4 bytes, 64KB)
    //   ALFI should be 0x44 (high=4 for size, low=4 for address)
    //
    // Expected payload after SID and dataFormatIdentifier:
    //   [ALFI=0x44] [addr3] [addr2] [addr1] [addr0] [size3] [size2] [size1] [size0]
    //   [0x44] [0x08] [0x00] [0x40] [0x00] [0x00] [0x01] [0x00] [0x00]
    
    // NOTE: This test will FAIL if our implementation has the nibbles swapped!
    
    // Test using ECUProgrammer::encode_address_and_size which takes explicit ALFI
    uint32_t address = 0x08004000;
    uint32_t size = 0x00010000;
    uint8_t alfi = 0x44;  // Per spec: high=4 (size bytes), low=4 (addr bytes)
    
    auto encoded = ECUProgrammer::encode_address_and_size(address, size, alfi);
    
    // Expected: [ALFI] [4 addr bytes] [4 size bytes] = 9 bytes total
    ASSERT_EQ(9, encoded.size()) << "ALFI 0x44 should produce 1 + 4 + 4 = 9 bytes";
    
    // Verify ALFI byte
    EXPECT_EQ(0x44, encoded[0]) << "First byte should be ALFI";
    
    // Per ISO spec with ALFI 0x44:
    //   - Low nibble (4) = address bytes come first, 4 bytes
    //   - High nibble (4) = size bytes come second, 4 bytes
    
    // Verify address bytes (should be at positions 1-4)
    EXPECT_EQ(0x08, encoded[1]) << "Address byte 3 (MSB)";
    EXPECT_EQ(0x00, encoded[2]) << "Address byte 2";
    EXPECT_EQ(0x40, encoded[3]) << "Address byte 1";
    EXPECT_EQ(0x00, encoded[4]) << "Address byte 0 (LSB)";
    
    // Verify size bytes (should be at positions 5-8)
    EXPECT_EQ(0x00, encoded[5]) << "Size byte 3 (MSB)";
    EXPECT_EQ(0x01, encoded[6]) << "Size byte 2";
    EXPECT_EQ(0x00, encoded[7]) << "Size byte 1";
    EXPECT_EQ(0x00, encoded[8]) << "Size byte 0 (LSB)";
}

TEST(ALFIImplementationTest, EncodeAddressAndLength_Asymmetric) {
    // Test with different address and size byte counts
    // ALFI = 0x24: high=2 (size bytes), low=4 (addr bytes)
    
    uint32_t address = 0x08004000;
    uint32_t size = 0x1000;  // 4KB, fits in 2 bytes
    uint8_t alfi = 0x24;
    
    auto encoded = ECUProgrammer::encode_address_and_size(address, size, alfi);
    
    // Expected: [ALFI] [4 addr bytes] [2 size bytes] = 7 bytes total
    ASSERT_EQ(7, encoded.size()) << "ALFI 0x24 should produce 1 + 4 + 2 = 7 bytes";
    
    EXPECT_EQ(0x24, encoded[0]) << "First byte should be ALFI";
    
    // Address (4 bytes)
    EXPECT_EQ(0x08, encoded[1]);
    EXPECT_EQ(0x00, encoded[2]);
    EXPECT_EQ(0x40, encoded[3]);
    EXPECT_EQ(0x00, encoded[4]);
    
    // Size (2 bytes)
    EXPECT_EQ(0x10, encoded[5]) << "Size high byte";
    EXPECT_EQ(0x00, encoded[6]) << "Size low byte";
}

// ============================================================================
// ISO 14229-1 lengthFormatIdentifier (LFI) Tests
// 
// Per ISO 14229-1:2020 Section 14.2.2.2 (RequestDownload positive response):
//   Bits 7-4 (high nibble): maxNumberOfBlockLength size in bytes
//   Bits 3-0 (low nibble):  reserved, should be 0
//
// Example: LFI = 0x20 means maxNumberOfBlockLength is 2 bytes
// ============================================================================

class LFIComplianceTest : public ::testing::Test {
protected:
    // Helper to build LFI byte per ISO 14229-1 spec
    static uint8_t build_lfi_per_spec(uint8_t max_block_length_bytes) {
        // ISO 14229-1: high nibble = length, low nibble = 0 (reserved)
        return (max_block_length_bytes & 0x0F) << 4;
    }
    
    // Helper to parse LFI byte per ISO 14229-1 spec
    static uint8_t parse_lfi_per_spec(uint8_t lfi) {
        // ISO 14229-1: high nibble = length
        return (lfi >> 4) & 0x0F;
    }
};

TEST_F(LFIComplianceTest, LFI_0x20_PerSpec) {
    // LFI = 0x20: high nibble = 2, meaning 2-byte maxNumberOfBlockLength
    uint8_t lfi = 0x20;
    uint8_t length_bytes = parse_lfi_per_spec(lfi);
    
    EXPECT_EQ(2, length_bytes) << "LFI 0x20: high nibble should indicate 2-byte length";
}

TEST_F(LFIComplianceTest, LFI_0x30_PerSpec) {
    // LFI = 0x30: high nibble = 3, meaning 3-byte maxNumberOfBlockLength
    uint8_t lfi = 0x30;
    uint8_t length_bytes = parse_lfi_per_spec(lfi);
    
    EXPECT_EQ(3, length_bytes) << "LFI 0x30: high nibble should indicate 3-byte length";
}

TEST_F(LFIComplianceTest, LFI_0x40_PerSpec) {
    // LFI = 0x40: high nibble = 4, meaning 4-byte maxNumberOfBlockLength
    uint8_t lfi = 0x40;
    uint8_t length_bytes = parse_lfi_per_spec(lfi);
    
    EXPECT_EQ(4, length_bytes) << "LFI 0x40: high nibble should indicate 4-byte length";
}

TEST_F(LFIComplianceTest, BuildLFI) {
    // Build LFI for 2-byte maxNumberOfBlockLength
    uint8_t lfi = build_lfi_per_spec(2);
    EXPECT_EQ(0x20, lfi) << "2-byte length should produce LFI 0x20";
    
    lfi = build_lfi_per_spec(3);
    EXPECT_EQ(0x30, lfi) << "3-byte length should produce LFI 0x30";
}

// ============================================================================
// Verify our implementation matches ISO spec for LFI parsing
// ============================================================================

TEST(LFIImplementationTest, ParseMaxBlockLength_MatchesSpec) {
    // Simulate RequestDownload positive response payload (after SID):
    // [LFI] [maxNumberOfBlockLength bytes...]
    
    // Example: LFI = 0x20 (2-byte length), maxNumberOfBlockLength = 0x0FFA (4090 bytes)
    // Per ISO spec, we should read HIGH nibble of LFI to get byte count
    
    std::vector<uint8_t> response_lfi_0x20 = {0x20, 0x0F, 0xFA};
    uint32_t max_block = ECUProgrammer::parse_max_block_length(response_lfi_0x20);
    
    // If implementation uses HIGH nibble correctly: 2 bytes -> 0x0FFA = 4090
    // If implementation uses LOW nibble (bug): 0 bytes -> would fail or return 0
    EXPECT_EQ(4090, max_block) << "LFI 0x20 with 0x0FFA should parse to 4090";
}

TEST(LFIImplementationTest, ParseMaxBlockLength_3Bytes) {
    // LFI = 0x30 (3-byte length), maxNumberOfBlockLength = 0x00FFFF (65535 bytes)
    std::vector<uint8_t> response_lfi_0x30 = {0x30, 0x00, 0xFF, 0xFF};
    uint32_t max_block = ECUProgrammer::parse_max_block_length(response_lfi_0x30);
    
    EXPECT_EQ(65535, max_block) << "LFI 0x30 with 0x00FFFF should parse to 65535";
}

TEST(LFIImplementationTest, ParseMaxBlockLength_4Bytes) {
    // LFI = 0x40 (4-byte length), maxNumberOfBlockLength = 0x00100000 (1MB)
    std::vector<uint8_t> response_lfi_0x40 = {0x40, 0x00, 0x10, 0x00, 0x00};
    uint32_t max_block = ECUProgrammer::parse_max_block_length(response_lfi_0x40);
    
    EXPECT_EQ(0x00100000, max_block) << "LFI 0x40 with 0x00100000 should parse to 1MB";
}

// ============================================================================
// Real-world ECU trace validation
// These test cases are based on actual UDS traces from known-good implementations
// ============================================================================

TEST(RealWorldTraceTest, RequestDownload_TypicalECU) {
    // Typical RequestDownload request for flashing:
    // SID=0x34, DFI=0x00, ALFI=0x44, Address=0x08000000, Size=0x00040000 (256KB)
    //
    // Expected raw bytes (excluding SID):
    // [DFI=0x00] [ALFI=0x44] [0x08 0x00 0x00 0x00] [0x00 0x04 0x00 0x00]
    
    uint32_t address = 0x08000000;
    uint32_t size = 0x00040000;
    uint8_t alfi = 0x44;
    
    auto encoded = ECUProgrammer::encode_address_and_size(address, size, alfi);
    
    // Verify the complete encoding matches what a real ECU expects
    std::vector<uint8_t> expected = {
        0x44,                    // ALFI
        0x08, 0x00, 0x00, 0x00,  // Address (4 bytes, big-endian)
        0x00, 0x04, 0x00, 0x00   // Size (4 bytes, big-endian)
    };
    
    ASSERT_EQ(expected.size(), encoded.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(expected[i], encoded[i]) << "Mismatch at byte " << i;
    }
}

TEST(RealWorldTraceTest, RequestDownloadResponse_TypicalECU) {
    // Typical RequestDownload positive response:
    // SID+0x40=0x74, LFI=0x20, maxNumberOfBlockLength=0x0FFE (4094 bytes)
    //
    // Payload (excluding SID): [0x20] [0x0F] [0xFE]
    
    std::vector<uint8_t> response_payload = {0x20, 0x0F, 0xFE};
    uint32_t max_block = ECUProgrammer::parse_max_block_length(response_payload);
    
    EXPECT_EQ(4094, max_block) << "Should parse maxNumberOfBlockLength as 4094";
}

// ============================================================================
// Edge cases that would reveal nibble swap bugs
// ============================================================================

TEST(NibbleSwapDetectionTest, ALFI_DifferentAddressAndSizeBytes) {
    // This test specifically catches the nibble swap bug
    // If address_bytes != size_bytes, swapping nibbles produces wrong encoding
    
    // ALFI = 0x42: per spec, high=4 (size bytes), low=2 (addr bytes)
    uint8_t alfi = 0x42;
    uint32_t address = 0x8000;      // 2-byte address
    uint32_t size = 0x00010000;     // 4-byte size (64KB)
    
    auto encoded = ECUProgrammer::encode_address_and_size(address, size, alfi);
    
    // Per ISO spec with ALFI 0x42:
    //   Low nibble = 2 -> 2 address bytes
    //   High nibble = 4 -> 4 size bytes
    // Total: 1 + 2 + 4 = 7 bytes
    
    ASSERT_EQ(7, encoded.size()) << "ALFI 0x42 should produce 7 bytes";
    
    EXPECT_EQ(0x42, encoded[0]) << "ALFI byte";
    
    // Address (2 bytes): 0x8000
    EXPECT_EQ(0x80, encoded[1]) << "Address high byte";
    EXPECT_EQ(0x00, encoded[2]) << "Address low byte";
    
    // Size (4 bytes): 0x00010000
    EXPECT_EQ(0x00, encoded[3]) << "Size byte 3";
    EXPECT_EQ(0x01, encoded[4]) << "Size byte 2";
    EXPECT_EQ(0x00, encoded[5]) << "Size byte 1";
    EXPECT_EQ(0x00, encoded[6]) << "Size byte 0";
}

TEST(NibbleSwapDetectionTest, LFI_NonZeroLowNibble) {
    // Per spec, LFI low nibble should be 0 (reserved)
    // But if an ECU sends non-zero low nibble, we should still use HIGH nibble
    
    // LFI = 0x2F: high nibble = 2 (2-byte length), low nibble = F (reserved, ignore)
    std::vector<uint8_t> response = {0x2F, 0x10, 0x00};  // 2 bytes = 4096
    uint32_t max_block = ECUProgrammer::parse_max_block_length(response);
    
    // Should use HIGH nibble (2), not low nibble (F=15)
    EXPECT_EQ(4096, max_block) << "Should use high nibble for length, ignoring low nibble";
}
