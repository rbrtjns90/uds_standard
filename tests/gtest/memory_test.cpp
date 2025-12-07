/**
 * @file memory_test.cpp
 * @brief Comprehensive tests for memory functions (uds_memory.cpp)
 */

#include <gtest/gtest.h>
#include "uds.hpp"
#include "uds_memory.hpp"
#include <queue>

using namespace uds;
using namespace uds::memory;

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

class MemoryTest : public ::testing::Test {
protected:
  void SetUp() override { transport_.reset(); }
  MockTransport transport_;
};

// ============================================================================
// CRC32 Tests
// ============================================================================

TEST(CRC32Test, EmptyData) {
  std::vector<uint8_t> data;
  uint32_t result = crc32(data);
  EXPECT_EQ(result, 0x00000000u);
}

TEST(CRC32Test, SimpleData) {
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  uint32_t result = crc32(data);
  EXPECT_NE(result, 0u);
}

TEST(CRC32Test, WithInitial) {
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  uint32_t result = crc32(data, 0x12345678);
  EXPECT_NE(result, 0u);
}

TEST(CRC32Test, DifferentDataDifferentCRC) {
  std::vector<uint8_t> data1 = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> data2 = {0x05, 0x06, 0x07, 0x08};
  
  EXPECT_NE(crc32(data1), crc32(data2));
}

TEST(CRC32Test, SameDataSameCRC) {
  std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
  
  EXPECT_EQ(crc32(data), crc32(data));
}

// ============================================================================
// AddressFormat Tests
// ============================================================================

TEST(AddressFormatTest, DefaultConstruction) {
  AddressFormat fmt;
  EXPECT_EQ(fmt.address_bytes, 4u);
  EXPECT_EQ(fmt.size_bytes, 4u);
}

TEST(AddressFormatTest, CustomConstruction) {
  AddressFormat fmt(2, 2);
  EXPECT_EQ(fmt.address_bytes, 2u);
  EXPECT_EQ(fmt.size_bytes, 2u);
}

TEST(AddressFormatTest, ToFormatByte) {
  AddressFormat fmt(4, 4);
  EXPECT_EQ(fmt.to_format_byte(), 0x44);
  
  AddressFormat fmt2(2, 1);
  EXPECT_EQ(fmt2.to_format_byte(), 0x21);
}

// ============================================================================
// MemoryArea Tests
// ============================================================================

TEST(MemoryAreaTest, EndAddress) {
  MemoryArea area;
  area.start_address = 0x1000;
  area.size = 0x100;
  
  EXPECT_EQ(area.end_address(), 0x1100u);
}

TEST(MemoryAreaTest, ContainsAddressRange) {
  MemoryArea area;
  area.start_address = 0x1000;
  area.size = 0x100;
  
  EXPECT_TRUE(area.contains(0x1000, 1));
  EXPECT_TRUE(area.contains(0x1050, 10));
  EXPECT_TRUE(area.contains(0x10FF, 1));
  EXPECT_FALSE(area.contains(0x0FFF, 1));
  EXPECT_FALSE(area.contains(0x1100, 1));
  EXPECT_FALSE(area.contains(0x1050, 0x100));  // Extends beyond
}

// ============================================================================
// MemoryManager Tests
// ============================================================================

TEST_F(MemoryTest, MemoryManagerConstruction) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  EXPECT_TRUE(mgr.get_all_areas().empty());
}

TEST_F(MemoryTest, DefineArea) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  MemoryArea area;
  area.area_id = 0x0001;
  area.name = "TestArea";
  area.start_address = 0x1000;
  area.size = 0x100;
  
  mgr.define_area(area);
  
  auto retrieved = mgr.get_area(0x0001);
  EXPECT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->name, "TestArea");
}

TEST_F(MemoryTest, GetAreaByName) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  MemoryArea area;
  area.area_id = 0x0001;
  area.name = "TestArea";
  area.start_address = 0x1000;
  area.size = 0x100;
  
  mgr.define_area(area);
  
  auto retrieved = mgr.get_area_by_name("TestArea");
  EXPECT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->area_id, 0x0001);
}

TEST_F(MemoryTest, GetAreaByNameNotFound) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  auto retrieved = mgr.get_area_by_name("NonExistent");
  EXPECT_FALSE(retrieved.has_value());
}

TEST_F(MemoryTest, FindAreaForAddress) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  MemoryArea area;
  area.area_id = 0x0001;
  area.start_address = 0x1000;
  area.size = 0x100;
  
  mgr.define_area(area);
  
  auto found = mgr.find_area_for_address(0x1050);
  EXPECT_TRUE(found.has_value());
  
  auto not_found = mgr.find_area_for_address(0x2000);
  EXPECT_FALSE(not_found.has_value());
}

TEST_F(MemoryTest, GetAllAreas) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  MemoryArea area1, area2;
  area1.area_id = 0x0001;
  area2.area_id = 0x0002;
  
  mgr.define_area(area1);
  mgr.define_area(area2);
  
  auto areas = mgr.get_all_areas();
  EXPECT_EQ(areas.size(), 2u);
}

TEST_F(MemoryTest, ClearAreas) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  MemoryArea area;
  area.area_id = 0x0001;
  mgr.define_area(area);
  
  mgr.clear_areas();
  
  EXPECT_TRUE(mgr.get_all_areas().empty());
}

// ============================================================================
// Memory Read Tests
// ============================================================================

TEST_F(MemoryTest, ReadMemory) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  transport_.queue_response({0x63, 0xDE, 0xAD, 0xBE, 0xEF});
  
  auto result = mgr.read(0x1000, 4);
  
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.data.size(), 4u);
  EXPECT_EQ(result.address, 0x1000u);
}

TEST_F(MemoryTest, ReadMemoryFailure) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  transport_.queue_response({0x7F, 0x23, 0x31});  // RequestOutOfRange
  
  auto result = mgr.read(0x1000, 4);
  
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.nrc, NegativeResponseCode::RequestOutOfRange);
}

TEST_F(MemoryTest, ReadMemorySecurityDenied) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  transport_.queue_response({0x7F, 0x23, 0x33});  // SecurityAccessDenied
  
  auto result = mgr.read(0x1000, 4);
  
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.nrc, NegativeResponseCode::SecurityAccessDenied);
}

TEST_F(MemoryTest, ReadMemoryConditionsNotCorrect) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  transport_.queue_response({0x7F, 0x23, 0x22});  // ConditionsNotCorrect
  
  auto result = mgr.read(0x1000, 4);
  
  EXPECT_FALSE(result.ok);
}

// ============================================================================
// Memory Write Tests
// ============================================================================

TEST_F(MemoryTest, WriteMemory) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  transport_.queue_response({0x7D, 0x44});
  
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  auto result = mgr.write(0x1000, data);
  
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.bytes_written, 4u);
}

TEST_F(MemoryTest, WriteMemoryFailure) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  transport_.queue_response({0x7F, 0x3D, 0x72});  // GeneralProgrammingFailure
  
  std::vector<uint8_t> data = {0x01, 0x02};
  auto result = mgr.write(0x1000, data);
  
  EXPECT_FALSE(result.ok);
}

// ============================================================================
// Memory Area Read/Write Tests
// ============================================================================

TEST_F(MemoryTest, ReadAreaSuccess) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  MemoryArea area;
  area.area_id = 0x0001;
  area.start_address = 0x1000;
  area.size = 0x100;
  area.is_readable = true;
  mgr.define_area(area);
  
  transport_.queue_response({0x63, 0xAB, 0xCD});
  
  auto result = mgr.read_area(0x0001, 0, 2);
  
  EXPECT_TRUE(result.ok);
}

TEST_F(MemoryTest, ReadAreaNotDefined) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  auto result = mgr.read_area(0x9999, 0, 2);
  
  EXPECT_FALSE(result.ok);
  EXPECT_NE(result.error_message.find("not defined"), std::string::npos);
}

TEST_F(MemoryTest, ReadAreaNotReadable) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  MemoryArea area;
  area.area_id = 0x0001;
  area.is_readable = false;
  mgr.define_area(area);
  
  auto result = mgr.read_area(0x0001, 0, 2);
  
  EXPECT_FALSE(result.ok);
  EXPECT_NE(result.error_message.find("not readable"), std::string::npos);
}

TEST_F(MemoryTest, ReadAreaBeyondBoundary) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  MemoryArea area;
  area.area_id = 0x0001;
  area.size = 0x10;
  area.is_readable = true;
  mgr.define_area(area);
  
  auto result = mgr.read_area(0x0001, 0x08, 0x10);  // Extends beyond
  
  EXPECT_FALSE(result.ok);
}

TEST_F(MemoryTest, WriteAreaSuccess) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  MemoryArea area;
  area.area_id = 0x0001;
  area.start_address = 0x1000;
  area.size = 0x100;
  area.is_writable = true;
  mgr.define_area(area);
  
  transport_.queue_response({0x7D, 0x44});
  
  auto result = mgr.write_area(0x0001, 0, {0x01, 0x02});
  
  EXPECT_TRUE(result.ok);
}

TEST_F(MemoryTest, WriteAreaNotWritable) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  MemoryArea area;
  area.area_id = 0x0001;
  area.is_writable = false;
  mgr.define_area(area);
  
  auto result = mgr.write_area(0x0001, 0, {0x01});
  
  EXPECT_FALSE(result.ok);
}

// ============================================================================
// Block Read/Write Tests
// ============================================================================

TEST_F(MemoryTest, ReadBlocks) {
  Client client(transport_);
  MemoryManager mgr(client);
  mgr.set_max_block_size(4);
  
  // Queue responses for 2 blocks
  transport_.queue_response({0x63, 0x01, 0x02, 0x03, 0x04});
  transport_.queue_response({0x63, 0x05, 0x06, 0x07, 0x08});
  
  auto result = mgr.read_blocks(0x1000, 8, 4);
  
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.data.size(), 8u);
}

TEST_F(MemoryTest, ReadBlocksWithProgress) {
  Client client(transport_);
  MemoryManager mgr(client);
  mgr.set_max_block_size(4);
  
  transport_.queue_response({0x63, 0x01, 0x02, 0x03, 0x04});
  transport_.queue_response({0x63, 0x05, 0x06, 0x07, 0x08});
  
  int progress_calls = 0;
  auto result = mgr.read_blocks(0x1000, 8, 4, [&](uint64_t, uint64_t) {
    progress_calls++;
  });
  
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(progress_calls, 2);
}

TEST_F(MemoryTest, WriteBlocks) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  transport_.queue_response({0x7D, 0x44});
  transport_.queue_response({0x7D, 0x44});
  
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  auto result = mgr.write_blocks(0x1000, data, 4);
  
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.bytes_written, 8u);
}

// ============================================================================
// Verify and Compare Tests
// ============================================================================

TEST_F(MemoryTest, VerifySuccess) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  transport_.queue_response({0x63, 0xDE, 0xAD, 0xBE, 0xEF});
  
  std::vector<uint8_t> expected = {0xDE, 0xAD, 0xBE, 0xEF};
  bool verified = mgr.verify(0x1000, expected);
  
  EXPECT_TRUE(verified);
}

TEST_F(MemoryTest, VerifyFailure) {
  Client client(transport_);
  MemoryManager mgr(client);
  
  transport_.queue_response({0x63, 0xDE, 0xAD, 0xBE, 0xEF});
  
  std::vector<uint8_t> expected = {0x01, 0x02, 0x03, 0x04};
  bool verified = mgr.verify(0x1000, expected);
  
  EXPECT_FALSE(verified);
}

TEST_F(MemoryTest, CalculateCRC32) {
  Client client(transport_);
  MemoryManager mgr(client);
  mgr.set_max_block_size(256);
  
  transport_.queue_response({0x63, 0x01, 0x02, 0x03, 0x04});
  
  auto crc = mgr.calculate_crc32(0x1000, 4);
  
  EXPECT_TRUE(crc.has_value());
}

TEST_F(MemoryTest, CompareSuccess) {
  Client client(transport_);
  MemoryManager mgr(client);
  mgr.set_max_block_size(256);
  
  transport_.queue_response({0x63, 0x01, 0x02, 0x03, 0x04});
  transport_.queue_response({0x63, 0x01, 0x02, 0x03, 0x04});
  
  bool same = mgr.compare(0x1000, 0x2000, 4);
  
  EXPECT_TRUE(same);
}

// ============================================================================
// Common Memory Maps Tests
// ============================================================================

TEST(CommonMapsTest, CreateAutomotiveECUMap) {
  auto areas = common_maps::create_automotive_ecu_map();
  
  EXPECT_FALSE(areas.empty());
  
  // Check for expected areas
  bool has_boot = false, has_app = false, has_ram = false;
  for (const auto& area : areas) {
    if (area.name == "Bootloader") has_boot = true;
    if (area.name == "Application") has_app = true;
    if (area.name == "RAM") has_ram = true;
  }
  
  EXPECT_TRUE(has_boot);
  EXPECT_TRUE(has_app);
  EXPECT_TRUE(has_ram);
}

TEST(CommonMapsTest, CreateBCMMap) {
  auto areas = common_maps::create_bcm_map();
  
  EXPECT_FALSE(areas.empty());
  
  // Should have IO_Config area
  bool has_io_config = false;
  for (const auto& area : areas) {
    if (area.name == "IO_Config") has_io_config = true;
  }
  
  EXPECT_TRUE(has_io_config);
}

TEST(CommonMapsTest, CreateECMMap) {
  auto areas = common_maps::create_ecm_map();
  
  EXPECT_FALSE(areas.empty());
  
  // Should have fuel and timing maps
  bool has_fuel = false, has_timing = false;
  for (const auto& area : areas) {
    if (area.name == "Fuel_Maps") has_fuel = true;
    if (area.name == "Timing_Maps") has_timing = true;
  }
  
  EXPECT_TRUE(has_fuel);
  EXPECT_TRUE(has_timing);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST(MemoryUtilTest, FormatAddress) {
  std::string addr = format_address(0x12345678, 8);
  EXPECT_EQ(addr, "0x12345678");
}

TEST(MemoryUtilTest, FormatAddressShort) {
  std::string addr = format_address(0x1234, 4);
  EXPECT_EQ(addr, "0x1234");
}

TEST(MemoryUtilTest, FormatSizeBytes) {
  std::string size = format_size(512);
  EXPECT_NE(size.find("bytes"), std::string::npos);
}

TEST(MemoryUtilTest, FormatSizeKB) {
  std::string size = format_size(2048);
  EXPECT_NE(size.find("KB"), std::string::npos);
}

TEST(MemoryUtilTest, FormatSizeMB) {
  std::string size = format_size(2 * 1024 * 1024);
  EXPECT_NE(size.find("MB"), std::string::npos);
}

TEST(MemoryUtilTest, FormatSizeGB) {
  std::string size = format_size(2ULL * 1024 * 1024 * 1024);
  EXPECT_NE(size.find("GB"), std::string::npos);
}

TEST(MemoryUtilTest, HexDump) {
  std::vector<uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello"
  std::string dump = hex_dump(data, 0x1000, 16);
  
  EXPECT_FALSE(dump.empty());
  EXPECT_NE(dump.find("1000"), std::string::npos);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
