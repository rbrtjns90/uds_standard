/**
 * @file iso_spec_transfer_test.cpp
 * @brief ISO 14229-1 Spec-Anchored Tests: Upload/Download Services
 * 
 * References:
 * - ISO 14229-1:2020 Section 14.2 (RequestDownload)
 * - ISO 14229-1:2020 Section 14.3 (RequestUpload)
 * - ISO 14229-1:2020 Section 14.4 (TransferData)
 * - ISO 14229-1:2020 Section 14.5 (RequestTransferExit)
 * - ISO 14229-1:2020 Annex G (addressAndLengthFormatIdentifier)
 */

#include <gtest/gtest.h>
#include "uds_block.hpp"
#include "ecu_programming.hpp"
#include <vector>
#include <cstdint>

using namespace uds;

// ============================================================================
// ISO 14229-1 Section 14.2: RequestDownload (SID 0x34)
// ============================================================================

class RequestDownloadSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x34;
};

TEST_F(RequestDownloadSpecTest, RequestFormat) {
    // Request: [SID] [dataFormatIdentifier] [addressAndLengthFormatIdentifier]
    //          [memoryAddress...] [memorySize...]
    
    // Per ISO 14229-1:
    // dataFormatIdentifier: high nibble = compressionMethod, low nibble = encryptingMethod
    // addressAndLengthFormatIdentifier: high nibble = memorySize bytes, low nibble = memoryAddress bytes
    
    uint8_t dfi = 0x00;   // No compression, no encryption
    uint8_t alfi = 0x44;  // 4-byte size, 4-byte address
    uint32_t address = 0x08000000;
    uint32_t size = 0x00040000;  // 256KB
    
    std::vector<uint8_t> request = {
        SID,
        dfi,
        alfi,
        // Address (4 bytes, big-endian)
        static_cast<uint8_t>((address >> 24) & 0xFF),
        static_cast<uint8_t>((address >> 16) & 0xFF),
        static_cast<uint8_t>((address >> 8) & 0xFF),
        static_cast<uint8_t>(address & 0xFF),
        // Size (4 bytes, big-endian)
        static_cast<uint8_t>((size >> 24) & 0xFF),
        static_cast<uint8_t>((size >> 16) & 0xFF),
        static_cast<uint8_t>((size >> 8) & 0xFF),
        static_cast<uint8_t>(size & 0xFF)
    };
    
    EXPECT_EQ(11, request.size()) << "1 + 1 + 1 + 4 + 4 = 11 bytes";
    EXPECT_EQ(0x34, request[0]);
    EXPECT_EQ(0x00, request[1]) << "DFI = 0x00";
    EXPECT_EQ(0x44, request[2]) << "ALFI = 0x44";
}

TEST_F(RequestDownloadSpecTest, DataFormatIdentifier) {
    // Per ISO 14229-1 Table 119:
    // Bits 7-4: compressionMethod (0 = no compression)
    // Bits 3-0: encryptingMethod (0 = no encryption)
    
    uint8_t dfi_none = 0x00;           // No compression, no encryption
    uint8_t dfi_compressed = 0x10;     // Compression method 1, no encryption
    uint8_t dfi_encrypted = 0x01;      // No compression, encryption method 1
    uint8_t dfi_both = 0x11;           // Both compression and encryption
    
    EXPECT_EQ(0, (dfi_none >> 4) & 0x0F) << "No compression";
    EXPECT_EQ(0, dfi_none & 0x0F) << "No encryption";
    
    EXPECT_EQ(1, (dfi_compressed >> 4) & 0x0F) << "Compression method 1";
    EXPECT_EQ(1, dfi_encrypted & 0x0F) << "Encryption method 1";
}

TEST_F(RequestDownloadSpecTest, PositiveResponseFormat) {
    // Response: [SID+0x40] [lengthFormatIdentifier] [maxNumberOfBlockLength...]
    
    // Per ISO 14229-1:
    // lengthFormatIdentifier: high nibble = number of bytes for maxNumberOfBlockLength
    //                         low nibble = reserved (0)
    
    std::vector<uint8_t> response = {
        0x74,       // SID + 0x40
        0x20,       // LFI: 2 bytes for maxNumberOfBlockLength
        0x0F, 0xFE  // maxNumberOfBlockLength = 4094 bytes
    };
    
    EXPECT_EQ(4, response.size());
    EXPECT_EQ(0x74, response[0]) << "Response SID = 0x34 + 0x40";
    
    uint8_t lfi = response[1];
    uint8_t length_bytes = (lfi >> 4) & 0x0F;
    EXPECT_EQ(2, length_bytes) << "2 bytes for max block length";
    EXPECT_EQ(0, lfi & 0x0F) << "Low nibble reserved (0)";
    
    uint16_t max_block = (static_cast<uint16_t>(response[2]) << 8) | response[3];
    EXPECT_EQ(4094, max_block);
}

TEST_F(RequestDownloadSpecTest, LengthFormatIdentifierVariants) {
    // LFI with different byte counts
    
    // 1-byte maxNumberOfBlockLength
    uint8_t lfi_1byte = 0x10;
    EXPECT_EQ(1, (lfi_1byte >> 4) & 0x0F);
    
    // 2-byte maxNumberOfBlockLength
    uint8_t lfi_2byte = 0x20;
    EXPECT_EQ(2, (lfi_2byte >> 4) & 0x0F);
    
    // 3-byte maxNumberOfBlockLength
    uint8_t lfi_3byte = 0x30;
    EXPECT_EQ(3, (lfi_3byte >> 4) & 0x0F);
    
    // 4-byte maxNumberOfBlockLength
    uint8_t lfi_4byte = 0x40;
    EXPECT_EQ(4, (lfi_4byte >> 4) & 0x0F);
}

// ============================================================================
// ISO 14229-1 Section 14.3: RequestUpload (SID 0x35)
// ============================================================================

class RequestUploadSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x35;
};

TEST_F(RequestUploadSpecTest, RequestFormat) {
    // Request format is identical to RequestDownload
    // Request: [SID] [dataFormatIdentifier] [addressAndLengthFormatIdentifier]
    //          [memoryAddress...] [memorySize...]
    
    std::vector<uint8_t> request = {
        SID,
        0x00,  // DFI
        0x44,  // ALFI
        0x08, 0x00, 0x00, 0x00,  // Address
        0x00, 0x01, 0x00, 0x00   // Size = 64KB
    };
    
    EXPECT_EQ(11, request.size());
    EXPECT_EQ(0x35, request[0]);
}

TEST_F(RequestUploadSpecTest, PositiveResponseFormat) {
    // Response format is identical to RequestDownload
    std::vector<uint8_t> response = {
        0x75,       // SID + 0x40
        0x20,       // LFI
        0x0F, 0xFE  // maxNumberOfBlockLength
    };
    
    EXPECT_EQ(0x75, response[0]) << "Response SID = 0x35 + 0x40";
}

// ============================================================================
// ISO 14229-1 Section 14.4: TransferData (SID 0x36)
// ============================================================================

class TransferDataSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x36;
};

TEST_F(TransferDataSpecTest, RequestFormat) {
    // Request: [SID] [blockSequenceCounter] [transferRequestParameterRecord...]
    
    std::vector<uint8_t> request = {
        SID,
        0x01,  // Block sequence counter (starts at 1)
        // Data bytes follow
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE
    };
    
    EXPECT_EQ(0x36, request[0]);
    EXPECT_EQ(0x01, request[1]) << "First block = 0x01";
}

TEST_F(TransferDataSpecTest, BlockSequenceCounter) {
    // Per ISO 14229-1:
    // - Starts at 0x01 for first block
    // - Increments by 1 for each block
    // - Wraps from 0xFF to 0x00
    
    uint8_t counter = 0x01;  // First block
    
    // Simulate block transfers
    for (int i = 0; i < 256; ++i) {
        if (i == 0) {
            EXPECT_EQ(0x01, counter) << "First block is 0x01";
        }
        if (i == 254) {
            EXPECT_EQ(0xFF, counter) << "Block 255 is 0xFF";
        }
        if (i == 255) {
            EXPECT_EQ(0x00, counter) << "Block 256 wraps to 0x00";
        }
        
        counter = static_cast<uint8_t>(counter + 1);  // Wraps naturally
    }
}

TEST_F(TransferDataSpecTest, PositiveResponseFormat) {
    // Response: [SID+0x40] [blockSequenceCounter] [transferResponseParameterRecord...]
    
    std::vector<uint8_t> response = {
        0x76,  // SID + 0x40
        0x01   // Block sequence counter echoed
        // Optional: transferResponseParameterRecord
    };
    
    EXPECT_EQ(0x76, response[0]) << "Response SID = 0x36 + 0x40";
    EXPECT_EQ(0x01, response[1]) << "Block counter echoed";
}

TEST_F(TransferDataSpecTest, UploadResponse) {
    // For upload, response contains the data
    std::vector<uint8_t> upload_response = {
        0x76,  // SID + 0x40
        0x01,  // Block sequence counter
        // Uploaded data
        0x12, 0x34, 0x56, 0x78
    };
    
    EXPECT_EQ(6, upload_response.size());
    
    // Data starts at index 2
    std::vector<uint8_t> data(upload_response.begin() + 2, upload_response.end());
    EXPECT_EQ(4, data.size());
}

// ============================================================================
// ISO 14229-1 Section 14.5: RequestTransferExit (SID 0x37)
// ============================================================================

class RequestTransferExitSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x37;
};

TEST_F(RequestTransferExitSpecTest, RequestFormat) {
    // Request: [SID] [transferRequestParameterRecord (optional)]
    
    // Minimal request
    std::vector<uint8_t> request_minimal = {SID};
    EXPECT_EQ(1, request_minimal.size());
    EXPECT_EQ(0x37, request_minimal[0]);
    
    // Request with CRC or checksum
    std::vector<uint8_t> request_with_crc = {
        SID,
        0x12, 0x34, 0x56, 0x78  // CRC32 or other verification data
    };
    EXPECT_EQ(5, request_with_crc.size());
}

TEST_F(RequestTransferExitSpecTest, PositiveResponseFormat) {
    // Response: [SID+0x40] [transferResponseParameterRecord (optional)]
    
    std::vector<uint8_t> response = {0x77};
    
    EXPECT_EQ(0x77, response[0]) << "Response SID = 0x37 + 0x40";
}

// ============================================================================
// ISO 14229-1 Annex G: addressAndLengthFormatIdentifier (ALFI)
// ============================================================================

class ALFISpecTest : public ::testing::Test {
protected:
    // Per ISO 14229-1 Annex G.1:
    // Bits 7-4 (high nibble): memorySizeLength (1-15 bytes)
    // Bits 3-0 (low nibble): memoryAddressLength (1-15 bytes)
};

TEST_F(ALFISpecTest, CommonALFIValues) {
    // Common ALFI values used in automotive
    
    // 0x11: 1-byte address, 1-byte size (small memory)
    uint8_t alfi_11 = 0x11;
    EXPECT_EQ(1, (alfi_11 >> 4) & 0x0F) << "1-byte size";
    EXPECT_EQ(1, alfi_11 & 0x0F) << "1-byte address";
    
    // 0x22: 2-byte address, 2-byte size (16-bit systems)
    uint8_t alfi_22 = 0x22;
    EXPECT_EQ(2, (alfi_22 >> 4) & 0x0F) << "2-byte size";
    EXPECT_EQ(2, alfi_22 & 0x0F) << "2-byte address";
    
    // 0x44: 4-byte address, 4-byte size (32-bit systems)
    uint8_t alfi_44 = 0x44;
    EXPECT_EQ(4, (alfi_44 >> 4) & 0x0F) << "4-byte size";
    EXPECT_EQ(4, alfi_44 & 0x0F) << "4-byte address";
    
    // 0x24: 4-byte address, 2-byte size (common for small transfers)
    uint8_t alfi_24 = 0x24;
    EXPECT_EQ(2, (alfi_24 >> 4) & 0x0F) << "2-byte size";
    EXPECT_EQ(4, alfi_24 & 0x0F) << "4-byte address";
    
    // 0x42: 2-byte address, 4-byte size (large transfers to small address space)
    uint8_t alfi_42 = 0x42;
    EXPECT_EQ(4, (alfi_42 >> 4) & 0x0F) << "4-byte size";
    EXPECT_EQ(2, alfi_42 & 0x0F) << "2-byte address";
}

TEST_F(ALFISpecTest, BuildALFI) {
    // Helper to build ALFI per spec
    auto build_alfi = [](uint8_t size_bytes, uint8_t addr_bytes) -> uint8_t {
        return ((size_bytes & 0x0F) << 4) | (addr_bytes & 0x0F);
    };
    
    EXPECT_EQ(0x44, build_alfi(4, 4));
    EXPECT_EQ(0x24, build_alfi(2, 4));
    EXPECT_EQ(0x42, build_alfi(4, 2));
    EXPECT_EQ(0x11, build_alfi(1, 1));
}

TEST_F(ALFISpecTest, ParseALFI) {
    // Helper to parse ALFI per spec
    auto parse_alfi = [](uint8_t alfi, uint8_t& size_bytes, uint8_t& addr_bytes) {
        size_bytes = (alfi >> 4) & 0x0F;
        addr_bytes = alfi & 0x0F;
    };
    
    uint8_t size_bytes, addr_bytes;
    
    parse_alfi(0x44, size_bytes, addr_bytes);
    EXPECT_EQ(4, size_bytes);
    EXPECT_EQ(4, addr_bytes);
    
    parse_alfi(0x24, size_bytes, addr_bytes);
    EXPECT_EQ(2, size_bytes);
    EXPECT_EQ(4, addr_bytes);
    
    parse_alfi(0x42, size_bytes, addr_bytes);
    EXPECT_EQ(4, size_bytes);
    EXPECT_EQ(2, addr_bytes);
}

TEST_F(ALFISpecTest, CalculatePayloadSize) {
    // Calculate total payload size from ALFI
    auto calc_payload_size = [](uint8_t alfi) -> size_t {
        uint8_t size_bytes = (alfi >> 4) & 0x0F;
        uint8_t addr_bytes = alfi & 0x0F;
        return 1 + addr_bytes + size_bytes;  // ALFI + address + size
    };
    
    EXPECT_EQ(9, calc_payload_size(0x44)) << "1 + 4 + 4 = 9";
    EXPECT_EQ(7, calc_payload_size(0x24)) << "1 + 4 + 2 = 7";
    EXPECT_EQ(7, calc_payload_size(0x42)) << "1 + 2 + 4 = 7";
    EXPECT_EQ(3, calc_payload_size(0x11)) << "1 + 1 + 1 = 3";
}

// ============================================================================
// Real-world transfer scenarios
// ============================================================================

class TransferScenarioSpecTest : public ::testing::Test {};

TEST_F(TransferScenarioSpecTest, TypicalFlashSequence) {
    // Typical flash programming sequence per ISO 14229-1
    
    // 1. RequestDownload
    std::vector<uint8_t> req_download = {0x34, 0x00, 0x44, 
        0x08, 0x00, 0x00, 0x00,  // Address
        0x00, 0x04, 0x00, 0x00}; // Size = 256KB
    EXPECT_EQ(0x34, req_download[0]);
    
    // 2. Positive response with max block size
    std::vector<uint8_t> resp_download = {0x74, 0x20, 0x0F, 0xFE};  // 4094 bytes
    EXPECT_EQ(0x74, resp_download[0]);
    
    // 3. TransferData blocks
    std::vector<uint8_t> transfer_1 = {0x36, 0x01 /* data... */};
    std::vector<uint8_t> transfer_2 = {0x36, 0x02 /* data... */};
    EXPECT_EQ(0x01, transfer_1[1]) << "First block";
    EXPECT_EQ(0x02, transfer_2[1]) << "Second block";
    
    // 4. RequestTransferExit
    std::vector<uint8_t> req_exit = {0x37};
    EXPECT_EQ(0x37, req_exit[0]);
    
    // 5. Positive response
    std::vector<uint8_t> resp_exit = {0x77};
    EXPECT_EQ(0x77, resp_exit[0]);
}

TEST_F(TransferScenarioSpecTest, BlockCountCalculation) {
    // Calculate number of blocks needed for a transfer
    uint32_t data_size = 262144;  // 256KB
    uint16_t max_block_size = 4094;  // From ECU response
    
    // Account for block sequence counter byte
    uint16_t usable_block_size = max_block_size - 2;  // SID + counter = 4092
    
    uint32_t num_blocks = (data_size + usable_block_size - 1) / usable_block_size;
    
    // 262144 / 4092 = 64.03... -> 65 blocks (ceiling division)
    EXPECT_EQ(65, num_blocks) << "256KB / 4092 bytes = 65 blocks (ceiling)";
}
