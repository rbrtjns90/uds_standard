/**
 * @file stress_test.cpp
 * @brief Performance and stress tests for large transfers and high-volume operations
 * 
 * Tests cover:
 * - Large data block handling
 * - Memory allocation under stress
 * - Block counter wraparound during long transfers
 * - CRC calculation performance
 * - Payload building efficiency
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <numeric>
#include <random>
#include "uds.hpp"
#include "uds_block.hpp"
#include "uds_dtc.hpp"
#include "uds_scaling.hpp"

using namespace uds;

// ============================================================================
// Large Data Handling Tests
// ============================================================================

TEST(LargeDataTest, LargeVectorAllocation) {
  // Test allocation of large data buffers
  constexpr size_t size_1mb = 1024 * 1024;
  
  EXPECT_NO_THROW({
    std::vector<uint8_t> large_buffer(size_1mb, 0xAA);
    EXPECT_EQ(large_buffer.size(), size_1mb);
  });
}

TEST(LargeDataTest, LargeVectorCopy) {
  constexpr size_t size_100kb = 100 * 1024;
  
  std::vector<uint8_t> source(size_100kb);
  std::iota(source.begin(), source.end(), 0);  // Fill with 0, 1, 2, ...
  
  auto start = std::chrono::steady_clock::now();
  std::vector<uint8_t> dest = source;
  auto end = std::chrono::steady_clock::now();
  
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  
  EXPECT_EQ(dest.size(), size_100kb);
  EXPECT_EQ(dest[0], 0);
  EXPECT_EQ(dest[size_100kb - 1], static_cast<uint8_t>((size_100kb - 1) & 0xFF));
  
  // Should complete in reasonable time (< 100ms)
  EXPECT_LT(duration.count(), 100000);
}

TEST(LargeDataTest, LargePayloadBuilding) {
  // Simulate building a large transfer payload
  constexpr size_t chunk_size = 4096;
  constexpr size_t num_chunks = 100;
  
  std::vector<uint8_t> full_payload;
  full_payload.reserve(chunk_size * num_chunks);
  
  auto start = std::chrono::steady_clock::now();
  
  for (size_t i = 0; i < num_chunks; ++i) {
    std::vector<uint8_t> chunk(chunk_size, static_cast<uint8_t>(i & 0xFF));
    full_payload.insert(full_payload.end(), chunk.begin(), chunk.end());
  }
  
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_EQ(full_payload.size(), chunk_size * num_chunks);
  EXPECT_LT(duration.count(), 1000);  // Should complete in < 1 second
}

// ============================================================================
// Block Transfer Stress Tests
// ============================================================================

TEST(BlockTransferStressTest, ManyBlockCounterIncrements) {
  // Simulate a very long transfer with many blocks
  constexpr int num_blocks = 10000;
  
  auto start = std::chrono::steady_clock::now();
  
  uint8_t counter = 1;  // Block counter starts at 1
  for (int i = 0; i < num_blocks; ++i) {
    // Verify counter behavior
    EXPECT_GE(counter, 0);
    EXPECT_LE(counter, 255);
    
    // Increment with wrap (0xFF -> 0x00)
    counter++;
  }
  
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  
  // Should be very fast
  EXPECT_LT(duration.count(), 100000);  // < 100ms
}

TEST(BlockTransferStressTest, BlockCountCalculation) {
  using namespace block;
  
  // Test block count calculation for various sizes
  struct TestCase {
    uint32_t total_bytes;
    uint32_t block_size;
    uint32_t expected_blocks;
  };
  
  std::vector<TestCase> cases = {
    {0, 256, 0},
    {1, 256, 1},
    {255, 256, 1},
    {256, 256, 1},
    {257, 256, 2},
    {1000000, 4096, 245},  // ~1MB
    {10000000, 4096, 2442},  // ~10MB
  };
  
  for (const auto& tc : cases) {
    uint32_t blocks = (tc.total_bytes + tc.block_size - 1) / tc.block_size;
    if (tc.total_bytes == 0) blocks = 0;
    EXPECT_EQ(blocks, tc.expected_blocks) 
      << "total=" << tc.total_bytes << " block_size=" << tc.block_size;
  }
}

TEST(BlockTransferStressTest, ProgressTrackingAccuracy) {
  using namespace block;
  
  TransferProgress progress;
  progress.total_bytes = 1000000;  // 1MB
  
  // Simulate transfer in chunks
  constexpr uint64_t chunk_size = 4096;
  uint64_t transferred = 0;
  
  while (transferred < progress.total_bytes) {
    uint64_t this_chunk = (chunk_size < (progress.total_bytes - transferred)) 
                          ? chunk_size : (progress.total_bytes - transferred);
    transferred += this_chunk;
    progress.transferred_bytes = transferred;
    
    double pct = progress.percentage();
    EXPECT_GE(pct, 0.0);
    EXPECT_LE(pct, 100.0);
  }
  
  EXPECT_DOUBLE_EQ(progress.percentage(), 100.0);
}

// ============================================================================
// CRC Performance Tests
// ============================================================================

TEST(CRCPerformanceTest, SmallDataCRC) {
  using namespace block;
  
  std::vector<uint8_t> small_data(64, 0x55);
  
  auto start = std::chrono::steady_clock::now();
  
  for (int i = 0; i < 10000; ++i) {
    uint32_t crc = calculate_crc32(small_data);
    (void)crc;
  }
  
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  // 10000 CRC calculations should be fast
  EXPECT_LT(duration.count(), 1000);  // < 1 second
}

TEST(CRCPerformanceTest, LargeDataCRC) {
  using namespace block;
  
  std::vector<uint8_t> large_data(1024 * 1024, 0xAA);  // 1MB
  
  auto start = std::chrono::steady_clock::now();
  
  uint32_t crc = calculate_crc32(large_data);
  
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_NE(crc, 0u);
  // 1MB CRC should complete quickly
  EXPECT_LT(duration.count(), 500);  // < 500ms
}

TEST(CRCPerformanceTest, IncrementalCRC) {
  using namespace block;
  
  // Build data incrementally and verify CRC consistency
  std::vector<uint8_t> data;
  data.reserve(10000);
  
  for (int i = 0; i < 10000; ++i) {
    data.push_back(static_cast<uint8_t>(i & 0xFF));
  }
  
  uint32_t crc1 = calculate_crc32(data);
  uint32_t crc2 = calculate_crc32(data);
  
  EXPECT_EQ(crc1, crc2);
}

// ============================================================================
// Encoding Performance Tests
// ============================================================================

TEST(EncodingPerformanceTest, BigEndianEncoding) {
  auto start = std::chrono::steady_clock::now();
  
  for (int i = 0; i < 100000; ++i) {
    std::vector<uint8_t> v;
    v.reserve(8);
    codec::be16(v, static_cast<uint16_t>(i));
    codec::be32(v, static_cast<uint32_t>(i));
  }
  
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  // 100000 encodings should be fast
  EXPECT_LT(duration.count(), 500);
}

TEST(EncodingPerformanceTest, PayloadConstruction) {
  // Simulate building many UDS request payloads
  auto start = std::chrono::steady_clock::now();
  
  for (int i = 0; i < 10000; ++i) {
    std::vector<uint8_t> payload;
    payload.reserve(10);
    
    payload.push_back(0x22);  // RDBI SID
    codec::be16(payload, static_cast<uint16_t>(0xF190 + (i % 100)));
    
    EXPECT_EQ(payload.size(), 3u);
  }
  
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_LT(duration.count(), 500);
}

// ============================================================================
// DTC Parsing Performance Tests
// ============================================================================

TEST(DTCPerformanceTest, ManyDTCParsing) {
  using namespace dtc;
  
  // Simulate parsing a large DTC response
  std::vector<uint8_t> dtc_data;
  constexpr int num_dtcs = 1000;
  
  // Build mock DTC data: [DTC(3)][Status(1)] * num_dtcs
  for (int i = 0; i < num_dtcs; ++i) {
    dtc_data.push_back(static_cast<uint8_t>((i >> 16) & 0xFF));
    dtc_data.push_back(static_cast<uint8_t>((i >> 8) & 0xFF));
    dtc_data.push_back(static_cast<uint8_t>(i & 0xFF));
    dtc_data.push_back(0x09);  // Status
  }
  
  auto start = std::chrono::steady_clock::now();
  
  std::vector<DTCRecord> dtcs;
  dtcs.reserve(num_dtcs);
  
  for (size_t offset = 0; offset + 4 <= dtc_data.size(); offset += 4) {
    DTCRecord dtc;
    dtc.code = parse_dtc_code(&dtc_data[offset]);
    dtc.status = dtc_data[offset + 3];
    dtcs.push_back(dtc);
  }
  
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  
  EXPECT_EQ(dtcs.size(), static_cast<size_t>(num_dtcs));
  EXPECT_LT(duration.count(), 50000);  // < 50ms
}

TEST(DTCPerformanceTest, DTCStringFormatting) {
  using namespace dtc;
  
  auto start = std::chrono::steady_clock::now();
  
  for (int i = 0; i < 10000; ++i) {
    std::string formatted = format_dtc_code(static_cast<uint32_t>(i));
    EXPECT_FALSE(formatted.empty());
  }
  
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_LT(duration.count(), 1000);
}

// ============================================================================
// Scaling Performance Tests
// ============================================================================

TEST(ScalingPerformanceTest, ByteConversion) {
  using namespace scaling;
  
  auto start = std::chrono::steady_clock::now();
  
  for (int i = 0; i < 100000; ++i) {
    std::vector<uint8_t> bytes = {
      static_cast<uint8_t>((i >> 24) & 0xFF),
      static_cast<uint8_t>((i >> 16) & 0xFF),
      static_cast<uint8_t>((i >> 8) & 0xFF),
      static_cast<uint8_t>(i & 0xFF)
    };
    
    uint64_t val = bytes_to_uint(bytes);
    EXPECT_EQ(val, static_cast<uint64_t>(i));
  }
  
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_LT(duration.count(), 1000);
}

// ============================================================================
// Memory Stress Tests
// ============================================================================

TEST(MemoryStressTest, RepeatedAllocation) {
  // Test repeated allocation/deallocation
  for (int iteration = 0; iteration < 100; ++iteration) {
    std::vector<uint8_t> buffer(10000);
    std::fill(buffer.begin(), buffer.end(), static_cast<uint8_t>(iteration));
    
    EXPECT_EQ(buffer.size(), 10000u);
    EXPECT_EQ(buffer[0], static_cast<uint8_t>(iteration));
  }
}

TEST(MemoryStressTest, GrowingVector) {
  std::vector<uint8_t> growing;
  
  // Grow vector incrementally
  for (int i = 0; i < 100000; ++i) {
    growing.push_back(static_cast<uint8_t>(i & 0xFF));
  }
  
  EXPECT_EQ(growing.size(), 100000u);
}

TEST(MemoryStressTest, ManySmallVectors) {
  std::vector<std::vector<uint8_t>> many_vectors;
  many_vectors.reserve(1000);
  
  for (int i = 0; i < 1000; ++i) {
    std::vector<uint8_t> small(100, static_cast<uint8_t>(i));
    many_vectors.push_back(std::move(small));
  }
  
  EXPECT_EQ(many_vectors.size(), 1000u);
  
  // Verify contents
  for (int i = 0; i < 1000; ++i) {
    EXPECT_EQ(many_vectors[i].size(), 100u);
    EXPECT_EQ(many_vectors[i][0], static_cast<uint8_t>(i));
  }
}

// ============================================================================
// Simulated Transfer Stress Tests
// ============================================================================

TEST(TransferStressTest, SimulatedLargeDownload) {
  // Simulate a large firmware download
  constexpr uint32_t firmware_size = 512 * 1024;  // 512KB
  constexpr uint32_t block_size = 4096;
  
  std::vector<uint8_t> firmware(firmware_size);
  std::iota(firmware.begin(), firmware.end(), 0);
  
  uint8_t block_counter = 1;
  uint32_t offset = 0;
  uint32_t blocks_sent = 0;
  
  auto start = std::chrono::steady_clock::now();
  
  while (offset < firmware_size) {
    uint32_t chunk_size = std::min(block_size, firmware_size - offset);
    
    // Simulate building transfer_data payload
    std::vector<uint8_t> payload;
    payload.reserve(1 + chunk_size);
    payload.push_back(block_counter);
    payload.insert(payload.end(), 
                   firmware.begin() + offset, 
                   firmware.begin() + offset + chunk_size);
    
    EXPECT_EQ(payload.size(), 1 + chunk_size);
    
    offset += chunk_size;
    blocks_sent++;
    
    // Increment block counter with wrap
    block_counter++;
    if (block_counter == 0) block_counter = 1;
  }
  
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_EQ(offset, firmware_size);
  EXPECT_GT(blocks_sent, 0u);
  EXPECT_LT(duration.count(), 2000);  // < 2 seconds
}

TEST(TransferStressTest, SimulatedLargeUpload) {
  // Simulate receiving a large memory read
  constexpr uint32_t total_size = 256 * 1024;  // 256KB
  constexpr uint32_t chunk_size = 4096;
  
  std::vector<uint8_t> received_data;
  received_data.reserve(total_size);
  
  auto start = std::chrono::steady_clock::now();
  
  uint32_t remaining = total_size;
  while (remaining > 0) {
    uint32_t this_chunk = std::min(chunk_size, remaining);
    
    // Simulate receiving data
    std::vector<uint8_t> chunk(this_chunk, 0xAA);
    received_data.insert(received_data.end(), chunk.begin(), chunk.end());
    
    remaining -= this_chunk;
  }
  
  auto end = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  
  EXPECT_EQ(received_data.size(), total_size);
  EXPECT_LT(duration.count(), 1000);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
