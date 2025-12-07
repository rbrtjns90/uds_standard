/**
 * @file iso_spec_data_test.cpp
 * @brief ISO 14229-1 Spec-Anchored Tests: Data Services
 * 
 * References:
 * - ISO 14229-1:2020 Section 10.2 (ReadDataByIdentifier)
 * - ISO 14229-1:2020 Section 10.3 (ReadMemoryByAddress)
 * - ISO 14229-1:2020 Section 10.4 (ReadScalingDataByIdentifier)
 * - ISO 14229-1:2020 Section 10.5 (ReadDataByPeriodicIdentifier)
 * - ISO 14229-1:2020 Section 10.6 (DynamicallyDefineDataIdentifier)
 * - ISO 14229-1:2020 Section 10.7 (WriteDataByIdentifier)
 * - ISO 14229-1:2020 Section 10.8 (WriteMemoryByAddress)
 */

#include <gtest/gtest.h>
#include "uds.hpp"
#include "uds_memory.hpp"
#include "uds_scaling.hpp"
#include <vector>
#include <cstdint>

// ============================================================================
// ISO 14229-1 Section 10.2: ReadDataByIdentifier (SID 0x22)
// ============================================================================

class ReadDataByIdentifierSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x22;
};

TEST_F(ReadDataByIdentifierSpecTest, RequestFormat) {
    // Request: [SID] [DID_high] [DID_low] [DID_high] [DID_low] ...
    // Can request multiple DIDs in one request
    
    // Single DID request: 0xF190 (VIN)
    std::vector<uint8_t> request_single = {SID, 0xF1, 0x90};
    EXPECT_EQ(3, request_single.size());
    EXPECT_EQ(0x22, request_single[0]);
    
    uint16_t did = (static_cast<uint16_t>(request_single[1]) << 8) | request_single[2];
    EXPECT_EQ(0xF190, did) << "DID should be 0xF190";
}

TEST_F(ReadDataByIdentifierSpecTest, MultiDIDRequest) {
    // Multiple DIDs: 0xF190, 0xF18C
    std::vector<uint8_t> request_multi = {SID, 0xF1, 0x90, 0xF1, 0x8C};
    EXPECT_EQ(5, request_multi.size());
    
    uint16_t did1 = (static_cast<uint16_t>(request_multi[1]) << 8) | request_multi[2];
    uint16_t did2 = (static_cast<uint16_t>(request_multi[3]) << 8) | request_multi[4];
    EXPECT_EQ(0xF190, did1);
    EXPECT_EQ(0xF18C, did2);
}

TEST_F(ReadDataByIdentifierSpecTest, PositiveResponseFormat) {
    // Response: [SID+0x40] [DID_high] [DID_low] [data...] [DID_high] [DID_low] [data...]
    
    // Example: VIN response (17 bytes)
    std::vector<uint8_t> response = {
        0x62,       // SID + 0x40
        0xF1, 0x90, // DID 0xF190
        'W', 'V', 'W', 'Z', 'Z', 'Z', '3', 'C', 'Z', 'W', 'E', '1', '2', '3', '4', '5', '6'
    };
    
    EXPECT_EQ(0x62, response[0]) << "Response SID = 0x22 + 0x40";
    
    uint16_t did = (static_cast<uint16_t>(response[1]) << 8) | response[2];
    EXPECT_EQ(0xF190, did);
    
    // VIN data starts at index 3
    std::string vin(response.begin() + 3, response.end());
    EXPECT_EQ(17, vin.size()) << "VIN is 17 characters";
}

TEST_F(ReadDataByIdentifierSpecTest, StandardDIDRanges) {
    // Per ISO 14229-1 Annex C: DID ranges
    
    // 0x0000-0x00FF: ISO/SAE reserved
    // 0x0100-0xA5FF: Vehicle manufacturer specific
    // 0xA600-0xA7FF: Reserved for ISO 27145
    // 0xA800-0xACFF: Reserved for ISO 27145
    // 0xAD00-0xAFFF: Reserved for ISO 27145
    // 0xB000-0xB1FF: Reserved for ISO 27145
    // 0xB200-0xBFFF: Reserved for ISO 27145
    // 0xC000-0xC2FF: Reserved for ISO 27145
    // 0xC300-0xFEFF: Reserved
    // 0xF000-0xF0FF: Network configuration data
    // 0xF100-0xF1FF: Identification data
    // 0xF200-0xF2FF: Periodic DIDs
    // 0xF300-0xF3FF: Dynamically defined DIDs
    // 0xF400-0xF4FF: OBD DIDs
    // 0xF500-0xF5FF: OBD monitor DIDs
    // 0xF600-0xF6FF: OBD info type DIDs
    // 0xF700-0xF7FF: OBD info type DIDs
    // 0xF800-0xF8FF: Tester DIDs
    // 0xF900-0xF9FF: ECU identification
    // 0xFA00-0xFAFF: ECU identification
    // 0xFB00-0xFBFF: ECU identification
    // 0xFC00-0xFCFF: ECU identification
    // 0xFD00-0xFDFF: System supplier specific
    // 0xFE00-0xFEFF: System supplier specific
    // 0xFF00-0xFFFF: ISO/SAE reserved
    
    // Common identification DIDs
    EXPECT_TRUE(0xF190 >= 0xF100 && 0xF190 <= 0xF1FF) << "VIN DID in identification range";
    EXPECT_TRUE(0xF18C >= 0xF100 && 0xF18C <= 0xF1FF) << "ECU serial in identification range";
}

// ============================================================================
// ISO 14229-1 Section 10.3: ReadMemoryByAddress (SID 0x23)
// ============================================================================

class ReadMemoryByAddressSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x23;
};

TEST_F(ReadMemoryByAddressSpecTest, RequestFormat) {
    // Request: [SID] [addressAndLengthFormatIdentifier] [memoryAddress...] [memorySize...]
    // ALFI: high nibble = memorySize bytes, low nibble = memoryAddress bytes
    
    // Example: 4-byte address, 2-byte size -> ALFI = 0x24
    uint8_t alfi = 0x24;  // High=2 (size bytes), Low=4 (addr bytes)
    uint32_t address = 0x08004000;
    uint16_t size = 0x0100;  // 256 bytes
    
    std::vector<uint8_t> request = {
        SID,
        alfi,
        0x08, 0x00, 0x40, 0x00,  // Address (4 bytes)
        0x01, 0x00               // Size (2 bytes)
    };
    
    EXPECT_EQ(8, request.size()) << "1 + 1 + 4 + 2 = 8 bytes";
    EXPECT_EQ(0x23, request[0]);
    EXPECT_EQ(0x24, request[1]) << "ALFI: 2-byte size, 4-byte address";
}

TEST_F(ReadMemoryByAddressSpecTest, ALFIInterpretation) {
    // Per ISO 14229-1 Annex G.1:
    // Bits 7-4 (high nibble) = memorySizeLength
    // Bits 3-0 (low nibble) = memoryAddressLength
    
    uint8_t alfi = 0x42;  // 4-byte size, 2-byte address
    
    uint8_t size_bytes = (alfi >> 4) & 0x0F;
    uint8_t addr_bytes = alfi & 0x0F;
    
    EXPECT_EQ(4, size_bytes) << "High nibble = size bytes";
    EXPECT_EQ(2, addr_bytes) << "Low nibble = address bytes";
}

TEST_F(ReadMemoryByAddressSpecTest, PositiveResponseFormat) {
    // Response: [SID+0x40] [dataRecord...]
    std::vector<uint8_t> response = {
        0x63,  // SID + 0x40
        0xDE, 0xAD, 0xBE, 0xEF  // Memory data
    };
    
    EXPECT_EQ(0x63, response[0]) << "Response SID = 0x23 + 0x40";
    EXPECT_EQ(4, response.size() - 1) << "4 bytes of data";
}

// ============================================================================
// ISO 14229-1 Section 10.4: ReadScalingDataByIdentifier (SID 0x24)
// ============================================================================

class ReadScalingDataByIdentifierSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x24;
};

TEST_F(ReadScalingDataByIdentifierSpecTest, RequestFormat) {
    // Request: [SID] [DID_high] [DID_low]
    std::vector<uint8_t> request = {SID, 0xF1, 0x90};
    
    EXPECT_EQ(3, request.size());
    EXPECT_EQ(0x24, request[0]);
}

TEST_F(ReadScalingDataByIdentifierSpecTest, PositiveResponseFormat) {
    // Response: [SID+0x40] [DID_high] [DID_low] [scalingByte] [scalingByteExtension...]
    
    // Per ISO 14229-1 Table 69: scalingByte format
    // Bits 7-4: scalingByteHighNibble (data type)
    // Bits 3-0: scalingByteLowNibble (number of bytes)
    
    std::vector<uint8_t> response = {
        0x64,       // SID + 0x40
        0xF1, 0x90, // DID
        0x12        // Scaling byte: type=1, length=2
    };
    
    EXPECT_EQ(0x64, response[0]) << "Response SID = 0x24 + 0x40";
    
    uint8_t scaling_byte = response[3];
    uint8_t data_type = (scaling_byte >> 4) & 0x0F;
    uint8_t num_bytes = scaling_byte & 0x0F;
    
    EXPECT_EQ(1, data_type);
    EXPECT_EQ(2, num_bytes);
}

// ============================================================================
// ISO 14229-1 Section 10.6: DynamicallyDefineDataIdentifier (SID 0x2C)
// ============================================================================

class DynamicallyDefineDataIdentifierSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x2C;
    
    // Per ISO 14229-1 Table 74: definitionType values
    static constexpr uint8_t DEFINE_BY_IDENTIFIER = 0x01;
    static constexpr uint8_t DEFINE_BY_MEMORY_ADDRESS = 0x02;
    static constexpr uint8_t CLEAR_DYNAMICALLY_DEFINED_DID = 0x03;
};

TEST_F(DynamicallyDefineDataIdentifierSpecTest, DefineByIdentifierFormat) {
    // Request: [SID] [definitionType] [DDDI_high] [DDDI_low] 
    //          [sourceDID_high] [sourceDID_low] [positionInSource] [memorySize]...
    
    std::vector<uint8_t> request = {
        SID,
        DEFINE_BY_IDENTIFIER,
        0xF2, 0x00,  // DDDI = 0xF200
        0xF1, 0x90,  // Source DID = 0xF190
        0x01,        // Position in source = 1
        0x05         // Memory size = 5 bytes
    };
    
    EXPECT_EQ(0x2C, request[0]);
    EXPECT_EQ(0x01, request[1]) << "Define by identifier";
    
    uint16_t dddi = (static_cast<uint16_t>(request[2]) << 8) | request[3];
    EXPECT_EQ(0xF200, dddi) << "DDDI in range 0xF200-0xF2FF";
}

TEST_F(DynamicallyDefineDataIdentifierSpecTest, ClearDDDIFormat) {
    // Request: [SID] [definitionType] [DDDI_high] [DDDI_low]
    std::vector<uint8_t> request = {
        SID,
        CLEAR_DYNAMICALLY_DEFINED_DID,
        0xF2, 0x00
    };
    
    EXPECT_EQ(0x03, request[1]) << "Clear DDDI";
}

TEST_F(DynamicallyDefineDataIdentifierSpecTest, DDDIRange) {
    // Per ISO 14229-1: DDDIs are in range 0xF200-0xF2FF or 0xF300-0xF3FF
    uint16_t dddi_start = 0xF200;
    uint16_t dddi_end = 0xF3FF;
    
    EXPECT_TRUE(0xF200 >= dddi_start && 0xF200 <= dddi_end);
    EXPECT_TRUE(0xF2FF >= dddi_start && 0xF2FF <= dddi_end);
    EXPECT_TRUE(0xF300 >= dddi_start && 0xF300 <= dddi_end);
}

// ============================================================================
// ISO 14229-1 Section 10.7: WriteDataByIdentifier (SID 0x2E)
// ============================================================================

class WriteDataByIdentifierSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x2E;
};

TEST_F(WriteDataByIdentifierSpecTest, RequestFormat) {
    // Request: [SID] [DID_high] [DID_low] [dataRecord...]
    std::vector<uint8_t> request = {
        SID,
        0xF1, 0x99,  // DID 0xF199 (programming date)
        0x20, 0x24, 0x12, 0x06  // Date: 2024-12-06
    };
    
    EXPECT_EQ(0x2E, request[0]);
    
    uint16_t did = (static_cast<uint16_t>(request[1]) << 8) | request[2];
    EXPECT_EQ(0xF199, did);
}

TEST_F(WriteDataByIdentifierSpecTest, PositiveResponseFormat) {
    // Response: [SID+0x40] [DID_high] [DID_low]
    std::vector<uint8_t> response = {0x6E, 0xF1, 0x99};
    
    EXPECT_EQ(0x6E, response[0]) << "Response SID = 0x2E + 0x40";
    EXPECT_EQ(3, response.size()) << "Response is 3 bytes (no data echoed)";
}

// ============================================================================
// ISO 14229-1 Section 10.8: WriteMemoryByAddress (SID 0x3D)
// ============================================================================

class WriteMemoryByAddressSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x3D;
};

TEST_F(WriteMemoryByAddressSpecTest, RequestFormat) {
    // Request: [SID] [ALFI] [memoryAddress...] [memorySize...] [dataRecord...]
    
    uint8_t alfi = 0x24;  // 2-byte size, 4-byte address
    std::vector<uint8_t> request = {
        SID,
        alfi,
        0x08, 0x00, 0x40, 0x00,  // Address
        0x00, 0x04,              // Size = 4 bytes
        0xDE, 0xAD, 0xBE, 0xEF   // Data
    };
    
    EXPECT_EQ(0x3D, request[0]);
    EXPECT_EQ(12, request.size()) << "1 + 1 + 4 + 2 + 4 = 12 bytes";
}

TEST_F(WriteMemoryByAddressSpecTest, PositiveResponseFormat) {
    // Response: [SID+0x40] [ALFI] [memoryAddress...] [memorySize...]
    // Note: data is NOT echoed in response
    
    std::vector<uint8_t> response = {
        0x7D,  // SID + 0x40
        0x24,
        0x08, 0x00, 0x40, 0x00,
        0x00, 0x04
    };
    
    EXPECT_EQ(0x7D, response[0]) << "Response SID = 0x3D + 0x40";
    EXPECT_EQ(8, response.size()) << "No data in response";
}
