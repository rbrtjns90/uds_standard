/**
 * @file block_transfer_test.cpp
 * @brief Tests for block transfer functions (uds_block.cpp)
 */

#include <gtest/gtest.h>
#include "uds.hpp"
#include "uds_block.hpp"
#include <queue>

using namespace uds;
using namespace uds::block;

// Mock Transport
class MockTransport : public Transport {
public:
  void set_address(const Address& addr) override { addr_ = addr; }
  const Address& address() const override { return addr_; }
  
  bool request_response(const std::vector<uint8_t>& tx,
                        std::vector<uint8_t>& rx,
                        std::chrono::milliseconds) override {
    last_request_ = tx;
    if (!responses_.empty()) { rx = responses_.front(); responses_.pop(); return true; }
    return false;
  }
  
  void queue_response(const std::vector<uint8_t>& r) { responses_.push(r); }
  const std::vector<uint8_t>& last_request() const { return last_request_; }
  void reset() { while (!responses_.empty()) responses_.pop(); last_request_.clear(); }

private:
  Address addr_;
  std::queue<std::vector<uint8_t>> responses_;
  std::vector<uint8_t> last_request_;
};

class BlockTransferTest : public ::testing::Test {
protected:
  void SetUp() override { transport_.reset(); }
  MockTransport transport_;
};

// ============================================================================
// CRC32 Tests
// ============================================================================

TEST(CRC32Test, EmptyData) {
  std::vector<uint8_t> data;
  uint32_t crc = calculate_crc32(data);
  EXPECT_EQ(crc, 0x00000000u);
}

TEST(CRC32Test, SimpleData) {
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  uint32_t crc = calculate_crc32(data);
  EXPECT_NE(crc, 0u);
}

TEST(CRC32Test, KnownValue) {
  // "123456789" has known CRC32 = 0xCBF43926
  std::vector<uint8_t> data = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  uint32_t crc = calculate_crc32(data);
  EXPECT_EQ(crc, 0xCBF43926u);
}

TEST(CRC32Test, WithInitialValue) {
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  uint32_t crc1 = calculate_crc32(data, 0xFFFFFFFF);
  uint32_t crc2 = calculate_crc32(data, 0x00000000);
  EXPECT_NE(crc1, crc2);
}

TEST(CRC32Test, DifferentDataDifferentCRC) {
  std::vector<uint8_t> data1 = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> data2 = {0x05, 0x06, 0x07, 0x08};
  EXPECT_NE(calculate_crc32(data1), calculate_crc32(data2));
}

TEST(CRC32Test, SameDataSameCRC) {
  std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
  EXPECT_EQ(calculate_crc32(data), calculate_crc32(data));
}

// ============================================================================
// BlockTransferManager Tests
// ============================================================================

TEST_F(BlockTransferTest, ManagerConstruction) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  EXPECT_TRUE(true);  // Just verify construction works
}

TEST_F(BlockTransferTest, SetAddressBytes) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  
  mgr.set_address_bytes(2);
  mgr.set_length_bytes(2);
  
  // Verify encoding uses correct sizes
  auto encoded = mgr.encode_address_and_length(0x1234, 0x0100);
  EXPECT_EQ(encoded[0], 0x22);  // 2 bytes addr, 2 bytes length
}

TEST_F(BlockTransferTest, EncodeAddressAndLength4Bytes) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  
  mgr.set_address_bytes(4);
  mgr.set_length_bytes(4);
  
  auto encoded = mgr.encode_address_and_length(0x12345678, 0x00001000);
  
  EXPECT_EQ(encoded[0], 0x44);  // Format byte
  EXPECT_EQ(encoded.size(), 9u);  // 1 + 4 + 4
  
  // Address bytes (big-endian)
  EXPECT_EQ(encoded[1], 0x12);
  EXPECT_EQ(encoded[2], 0x34);
  EXPECT_EQ(encoded[3], 0x56);
  EXPECT_EQ(encoded[4], 0x78);
  
  // Length bytes (big-endian)
  EXPECT_EQ(encoded[5], 0x00);
  EXPECT_EQ(encoded[6], 0x00);
  EXPECT_EQ(encoded[7], 0x10);
  EXPECT_EQ(encoded[8], 0x00);
}

TEST_F(BlockTransferTest, EncodeAddressAndLength2Bytes) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  
  mgr.set_address_bytes(2);
  mgr.set_length_bytes(2);
  
  auto encoded = mgr.encode_address_and_length(0x1234, 0x0100);
  
  EXPECT_EQ(encoded[0], 0x22);  // Format byte
  EXPECT_EQ(encoded.size(), 5u);  // 1 + 2 + 2
}

TEST_F(BlockTransferTest, RequestDownload) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  
  // Response: [0x74][lengthFormatId][maxBlockLength]
  transport_.queue_response({0x74, 0x20, 0x00, 0x80});  // Max block = 128
  
  auto result = mgr.request_download(0x00010000, 0x1000, 0x00);
  
  EXPECT_TRUE(result.ok);
  EXPECT_GT(result.max_block_length, 0u);
}

TEST_F(BlockTransferTest, RequestDownloadFailure) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  
  transport_.queue_response({0x7F, 0x34, 0x70});  // UploadDownloadNotAccepted
  
  auto result = mgr.request_download(0x00010000, 0x1000, 0x00);
  
  EXPECT_FALSE(result.ok);
}

TEST_F(BlockTransferTest, RequestUpload) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  
  transport_.queue_response({0x75, 0x20, 0x00, 0x80});
  
  auto result = mgr.request_upload(0x00010000, 0x1000, 0x00);
  
  EXPECT_TRUE(result.ok);
}

TEST_F(BlockTransferTest, TransferDataSingleBlock) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  
  transport_.queue_response({0x76, 0x01});  // Block counter 1
  
  std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
  auto result = mgr.transfer_data(0x01, data);
  
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.block_counter, 0x01);
}

TEST_F(BlockTransferTest, TransferDataWrongSequence) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  
  transport_.queue_response({0x7F, 0x36, 0x73});  // WrongBlockSequenceCounter
  
  std::vector<uint8_t> data = {0x01, 0x02};
  auto result = mgr.transfer_data(0x01, data);
  
  EXPECT_FALSE(result.ok);
}

TEST_F(BlockTransferTest, RequestTransferExit) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  
  transport_.queue_response({0x77});
  
  auto result = mgr.request_transfer_exit();
  
  EXPECT_TRUE(result.ok);
}

TEST_F(BlockTransferTest, RequestTransferExitWithCRC) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  
  transport_.queue_response({0x77, 0x12, 0x34, 0x56, 0x78});
  
  std::vector<uint8_t> param = {0x12, 0x34, 0x56, 0x78};
  auto result = mgr.request_transfer_exit(param);
  
  EXPECT_TRUE(result.ok);
}

// ============================================================================
// Full Download Transfer Tests
// ============================================================================

TEST_F(BlockTransferTest, DownloadDataComplete) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  mgr.set_max_block_size(64);
  
  // Queue RequestDownload response
  transport_.queue_response({0x74, 0x20, 0x00, 0x40});  // Max 64 bytes
  
  // Queue TransferData responses
  transport_.queue_response({0x76, 0x01});
  transport_.queue_response({0x76, 0x02});
  
  // Queue TransferExit response
  transport_.queue_response({0x77});
  
  std::vector<uint8_t> data(100, 0xAA);  // 100 bytes
  auto result = mgr.download_data(0x00010000, data, 0x00);
  
  EXPECT_TRUE(result.ok);
}

TEST_F(BlockTransferTest, DownloadDataWithProgress) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  mgr.set_max_block_size(64);
  
  transport_.queue_response({0x74, 0x20, 0x00, 0x40});
  transport_.queue_response({0x76, 0x01});
  transport_.queue_response({0x77});
  
  int progress_calls = 0;
  std::vector<uint8_t> data(50, 0xBB);
  
  auto result = mgr.download_data(0x00010000, data, 0x00, 
    [&](uint64_t, uint64_t) { progress_calls++; });
  
  EXPECT_TRUE(result.ok);
  EXPECT_GT(progress_calls, 0);
}

// ============================================================================
// Full Upload Transfer Tests
// ============================================================================

TEST_F(BlockTransferTest, UploadDataComplete) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  
  // Queue RequestUpload response
  transport_.queue_response({0x75, 0x20, 0x00, 0x40});
  
  // Queue TransferData response with data
  transport_.queue_response({0x76, 0x01, 0xDE, 0xAD, 0xBE, 0xEF});
  
  // Queue TransferExit response
  transport_.queue_response({0x77});
  
  auto result = mgr.upload_data(0x00010000, 4, 0x00);
  
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.data.size(), 4u);
}

// ============================================================================
// Block Counter Tests
// ============================================================================

TEST_F(BlockTransferTest, BlockCounterWraparound) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  
  // Block counter should wrap from 0xFF to 0x00
  mgr.reset_block_counter();
  
  for (int i = 0; i < 256; i++) {
    uint8_t expected = (i + 1) & 0xFF;
    if (expected == 0) expected = 1;  // Skip 0
    EXPECT_EQ(mgr.next_block_counter(), expected);
  }
}

// ============================================================================
// Checksum Verification Tests
// ============================================================================

TEST_F(BlockTransferTest, VerifyChecksumSuccess) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  uint32_t expected_crc = calculate_crc32(data);
  
  EXPECT_TRUE(mgr.verify_checksum(data, expected_crc));
}

TEST_F(BlockTransferTest, VerifyChecksumFailure) {
  Client client(transport_);
  BlockTransferManager mgr(client);
  
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  
  EXPECT_FALSE(mgr.verify_checksum(data, 0xDEADBEEF));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
