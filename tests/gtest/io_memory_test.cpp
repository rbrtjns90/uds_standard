/**
 * @file io_memory_test.cpp
 * @brief Comprehensive Google Test suite for I/O Control, Memory, and Block Transfer
 */

#include <gtest/gtest.h>
#include "uds_io.hpp"
#include "uds_memory.hpp"
#include "uds_block.hpp"

using namespace uds;

// ############################################################################
// I/O CONTROL TESTS (uds_io.hpp)
// ############################################################################

// ============================================================================
// Control Option Tests
// ============================================================================

TEST(IOControlTest, ControlOptionValues) {
  using namespace io;
  
  EXPECT_EQ(static_cast<uint8_t>(ControlOption::ReturnControlToECU), 0x00);
  EXPECT_EQ(static_cast<uint8_t>(ControlOption::ResetToDefault), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(ControlOption::FreezeCurrentState), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(ControlOption::ShortTermAdjustment), 0x03);
}

TEST(IOControlTest, ControlOptionNames) {
  using namespace io;
  
  EXPECT_STREQ(control_option_name(ControlOption::ReturnControlToECU), "ReturnControlToECU");
  EXPECT_STREQ(control_option_name(ControlOption::ResetToDefault), "ResetToDefault");
  EXPECT_STREQ(control_option_name(ControlOption::FreezeCurrentState), "FreezeCurrentState");
  EXPECT_STREQ(control_option_name(ControlOption::ShortTermAdjustment), "ShortTermAdjustment");
}

TEST(IOControlTest, ControlOptionOEMRange) {
  using namespace io;
  
  // OEM-specific options are 0x04-0xFF
  auto oem_opt = static_cast<ControlOption>(0x10);
  EXPECT_STREQ(control_option_name(oem_opt), "VehicleManufacturerSpecific");
}

// ============================================================================
// I/O Status Tests
// ============================================================================

TEST(IOControlTest, IOStatusValues) {
  using namespace io;
  
  EXPECT_EQ(static_cast<uint8_t>(IOStatus::Idle), 0x00);
  EXPECT_EQ(static_cast<uint8_t>(IOStatus::Active), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(IOStatus::Pending), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(IOStatus::Failed), 0x03);
  EXPECT_EQ(static_cast<uint8_t>(IOStatus::TimedOut), 0x04);
  EXPECT_EQ(static_cast<uint8_t>(IOStatus::SecurityDenied), 0x05);
}

// ============================================================================
// I/O Control Request Tests
// ============================================================================

TEST(IOControlTest, RequestDefaultConstruction) {
  using namespace io;
  
  IOControlRequest request;
  
  // Vectors should be empty by default
  EXPECT_TRUE(request.control_enable_mask.empty());
  EXPECT_TRUE(request.control_state.empty());
}

TEST(IOControlTest, RequestWithDIDAndOption) {
  using namespace io;
  
  IOControlRequest request(0xF100, ControlOption::FreezeCurrentState);
  
  EXPECT_EQ(request.data_identifier, 0xF100);
  EXPECT_EQ(request.control_option, ControlOption::FreezeCurrentState);
}

TEST(IOControlTest, RequestWithState) {
  using namespace io;
  
  std::vector<uint8_t> state = {0x01, 0x00};
  IOControlRequest request(0xF200, ControlOption::ShortTermAdjustment, state);
  
  EXPECT_EQ(request.data_identifier, 0xF200);
  EXPECT_EQ(request.control_option, ControlOption::ShortTermAdjustment);
  EXPECT_EQ(request.control_state, state);
}

// ============================================================================
// I/O Control Response Tests
// ============================================================================

TEST(IOControlTest, ResponseDefaultConstruction) {
  using namespace io;
  
  IOControlResponse response;
  
  // Vector should be empty by default
  EXPECT_TRUE(response.control_status.empty());
}

TEST(IOControlTest, ResponseIsValid) {
  using namespace io;
  
  IOControlResponse response;
  response.data_identifier = 0xF100;
  
  EXPECT_TRUE(response.is_valid());
}

// ============================================================================
// I/O Control Result Tests
// ============================================================================

TEST(IOControlTest, ResultDefaultConstruction) {
  using namespace io;
  
  IOControlResult result;
  
  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(result.error_message.empty());
}

// ============================================================================
// I/O Identifier Info Tests
// ============================================================================

TEST(IOControlTest, IdentifierInfoDefaultConstruction) {
  using namespace io;
  
  IOIdentifierInfo info;
  
  // String should be empty by default
  EXPECT_TRUE(info.name.empty());
  EXPECT_TRUE(info.description.empty());
}

TEST(IOControlTest, IdentifierInfoConstruction) {
  using namespace io;
  
  IOIdentifierInfo info(0xF100, "Headlight Low", 1);
  
  EXPECT_EQ(info.did, 0xF100);
  EXPECT_EQ(info.name, "Headlight Low");
  EXPECT_EQ(info.data_length, 1);
}

// ============================================================================
// Common I/O DID Constants Tests
// ============================================================================

TEST(IOControlTest, CommonIODIDsEngine) {
  using namespace io::common_io;
  
  EXPECT_EQ(THROTTLE_ACTUATOR, 0xF000);
  EXPECT_EQ(IDLE_AIR_CONTROL, 0xF001);
  EXPECT_EQ(EGR_VALVE, 0xF002);
  EXPECT_EQ(FUEL_INJECTOR_1, 0xF010);
  EXPECT_EQ(FUEL_INJECTOR_2, 0xF011);
  EXPECT_EQ(IGNITION_COIL_1, 0xF020);
  EXPECT_EQ(FUEL_PUMP_RELAY, 0xF030);
  EXPECT_EQ(COOLING_FAN_RELAY, 0xF031);
  EXPECT_EQ(AC_COMPRESSOR_CLUTCH, 0xF032);
}

TEST(IOControlTest, CommonIODIDsLighting) {
  using namespace io::common_io;
  
  EXPECT_EQ(HEADLIGHT_LOW, 0xF100);
  EXPECT_EQ(HEADLIGHT_HIGH, 0xF101);
  EXPECT_EQ(TURN_SIGNAL_LEFT, 0xF102);
  EXPECT_EQ(TURN_SIGNAL_RIGHT, 0xF103);
  EXPECT_EQ(BRAKE_LIGHT, 0xF104);
  EXPECT_EQ(REVERSE_LIGHT, 0xF105);
  EXPECT_EQ(INTERIOR_LIGHT, 0xF108);
}

TEST(IOControlTest, CommonIODIDsBody) {
  using namespace io::common_io;
  
  EXPECT_EQ(DOOR_LOCK_DRIVER, 0xF200);
  EXPECT_EQ(WINDOW_DRIVER, 0xF210);
  EXPECT_EQ(SUNROOF, 0xF220);
  EXPECT_EQ(TRUNK_RELEASE, 0xF221);
  EXPECT_EQ(HORN, 0xF230);
  EXPECT_EQ(WIPER_FRONT, 0xF240);
}

TEST(IOControlTest, CommonIODIDsHVAC) {
  using namespace io::common_io;
  
  EXPECT_EQ(BLOWER_MOTOR, 0xF300);
  EXPECT_EQ(AC_CLUTCH, 0xF301);
  EXPECT_EQ(HEATER_VALVE, 0xF302);
  EXPECT_EQ(BLEND_DOOR, 0xF303);
}

TEST(IOControlTest, CommonIODIDsInstrument) {
  using namespace io::common_io;
  
  EXPECT_EQ(SPEEDOMETER, 0xF400);
  EXPECT_EQ(TACHOMETER, 0xF401);
  EXPECT_EQ(FUEL_GAUGE, 0xF402);
  EXPECT_EQ(WARNING_LAMP_MIL, 0xF410);
  EXPECT_EQ(WARNING_LAMP_ABS, 0xF411);
  EXPECT_EQ(WARNING_LAMP_AIRBAG, 0xF412);
}

// ############################################################################
// MEMORY TESTS (uds_memory.hpp)
// ############################################################################

// ============================================================================
// Address Format Tests
// ============================================================================

TEST(MemoryTest, AddressFormatDefault) {
  using namespace memory;
  
  AddressFormat fmt;
  
  EXPECT_EQ(fmt.address_bytes, 4);
  EXPECT_EQ(fmt.size_bytes, 4);
}

TEST(MemoryTest, AddressFormatConstruction) {
  using namespace memory;
  
  AddressFormat fmt(2, 2);
  
  EXPECT_EQ(fmt.address_bytes, 2);
  EXPECT_EQ(fmt.size_bytes, 2);
}

TEST(MemoryTest, AddressFormatToFormatByte) {
  using namespace memory;
  
  AddressFormat fmt_11(1, 1);
  EXPECT_EQ(fmt_11.to_format_byte(), 0x11);
  
  AddressFormat fmt_22(2, 2);
  EXPECT_EQ(fmt_22.to_format_byte(), 0x22);
  
  AddressFormat fmt_44(4, 4);
  EXPECT_EQ(fmt_44.to_format_byte(), 0x44);
  
  AddressFormat fmt_42(4, 2);
  EXPECT_EQ(fmt_42.to_format_byte(), 0x42);
}

TEST(MemoryTest, AddressFormatFromFormatByte) {
  using namespace memory;
  
  auto fmt = AddressFormat::from_format_byte(0x24);
  
  EXPECT_EQ(fmt.address_bytes, 2);
  EXPECT_EQ(fmt.size_bytes, 4);
}

TEST(MemoryTest, AddressFormatRoundTrip) {
  using namespace memory;
  
  AddressFormat original(3, 2);
  uint8_t byte = original.to_format_byte();
  auto parsed = AddressFormat::from_format_byte(byte);
  
  EXPECT_EQ(parsed.address_bytes, original.address_bytes);
  EXPECT_EQ(parsed.size_bytes, original.size_bytes);
}

// ============================================================================
// Memory Access Level Tests
// ============================================================================

TEST(MemoryTest, MemoryAccessLevelValues) {
  using namespace memory;
  
  EXPECT_EQ(static_cast<uint8_t>(MemoryAccessLevel::Public), 0x00);
  EXPECT_EQ(static_cast<uint8_t>(MemoryAccessLevel::Level1), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(MemoryAccessLevel::Level2), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(MemoryAccessLevel::Level3), 0x03);
  EXPECT_EQ(static_cast<uint8_t>(MemoryAccessLevel::Programming), 0x10);
  EXPECT_EQ(static_cast<uint8_t>(MemoryAccessLevel::Extended), 0x20);
  EXPECT_EQ(static_cast<uint8_t>(MemoryAccessLevel::OEM), 0x40);
}

// ============================================================================
// Memory Type Tests
// ============================================================================

TEST(MemoryTest, MemoryTypeValues) {
  using namespace memory;
  
  EXPECT_EQ(static_cast<uint8_t>(MemoryType::RAM), 0x00);
  EXPECT_EQ(static_cast<uint8_t>(MemoryType::ROM), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(MemoryType::EEPROM), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(MemoryType::Flash), 0x03);
  EXPECT_EQ(static_cast<uint8_t>(MemoryType::NVM), 0x04);
  EXPECT_EQ(static_cast<uint8_t>(MemoryType::Calibration), 0x10);
  EXPECT_EQ(static_cast<uint8_t>(MemoryType::Code), 0x20);
  EXPECT_EQ(static_cast<uint8_t>(MemoryType::Data), 0x30);
  EXPECT_EQ(static_cast<uint8_t>(MemoryType::Reserved), 0xFF);
}

// ============================================================================
// Memory Area Tests
// ============================================================================

TEST(MemoryTest, MemoryAreaDefault) {
  using namespace memory;
  
  MemoryArea area;
  
  EXPECT_EQ(area.area_id, 0);
  EXPECT_TRUE(area.name.empty());
  EXPECT_EQ(area.start_address, 0u);
  EXPECT_EQ(area.size, 0u);
  EXPECT_EQ(area.type, MemoryType::RAM);
  EXPECT_EQ(area.access, MemoryAccessLevel::Public);
  EXPECT_TRUE(area.is_readable);
  EXPECT_TRUE(area.is_writable);
  EXPECT_FALSE(area.is_erasable);
  EXPECT_EQ(area.erase_block_size, 0u);
  EXPECT_EQ(area.write_block_size, 1u);
}

TEST(MemoryTest, MemoryAreaContains) {
  using namespace memory;
  
  MemoryArea area;
  area.start_address = 0x1000;
  area.size = 0x1000;  // 0x1000 - 0x1FFF
  
  // Inside range
  EXPECT_TRUE(area.contains(0x1000, 1));
  EXPECT_TRUE(area.contains(0x1000, 0x1000));
  EXPECT_TRUE(area.contains(0x1500, 0x100));
  EXPECT_TRUE(area.contains(0x1FFF, 1));
  
  // Outside range
  EXPECT_FALSE(area.contains(0x0FFF, 1));  // Before
  EXPECT_FALSE(area.contains(0x2000, 1));  // After
  EXPECT_FALSE(area.contains(0x1F00, 0x200));  // Extends past end
  EXPECT_FALSE(area.contains(0x0F00, 0x200));  // Starts before
}

TEST(MemoryTest, MemoryAreaEndAddress) {
  using namespace memory;
  
  MemoryArea area;
  area.start_address = 0x10000;
  area.size = 0x8000;
  
  EXPECT_EQ(area.end_address(), 0x18000u);
}

TEST(MemoryTest, MemoryAreaFlashConfig) {
  using namespace memory;
  
  MemoryArea flash;
  flash.area_id = 1;
  flash.name = "Application Flash";
  flash.start_address = 0x00010000;
  flash.size = 0x00100000;  // 1MB
  flash.type = MemoryType::Flash;
  flash.access = MemoryAccessLevel::Programming;
  flash.is_readable = true;
  flash.is_writable = true;
  flash.is_erasable = true;
  flash.erase_block_size = 4096;
  flash.write_block_size = 256;
  
  EXPECT_EQ(flash.name, "Application Flash");
  EXPECT_EQ(flash.type, MemoryType::Flash);
  EXPECT_TRUE(flash.is_erasable);
  EXPECT_EQ(flash.erase_block_size, 4096u);
  EXPECT_TRUE(flash.contains(0x00010000, 0x00100000));
}

// ============================================================================
// Memory Read/Write Result Tests
// ============================================================================

TEST(MemoryTest, MemoryReadResultDefault) {
  using namespace memory;
  
  MemoryReadResult result;
  
  EXPECT_FALSE(result.ok);
  EXPECT_TRUE(result.data.empty());
  EXPECT_EQ(result.address, 0u);
  EXPECT_TRUE(result.error_message.empty());
}

TEST(MemoryTest, MemoryWriteResultDefault) {
  using namespace memory;
  
  MemoryWriteResult result;
  
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.address, 0u);
  EXPECT_EQ(result.bytes_written, 0u);
  EXPECT_TRUE(result.error_message.empty());
}

// ############################################################################
// BLOCK TRANSFER TESTS (uds_block.hpp)
// ############################################################################

// ============================================================================
// Transfer Config Tests
// ============================================================================

TEST(BlockTransferTest, TransferConfigDefault) {
  using namespace block;
  
  TransferConfig config;
  
  EXPECT_EQ(config.block_size, 256u);
  EXPECT_EQ(config.max_retries, 3u);
  EXPECT_EQ(config.retry_delay_ms, 100u);
  EXPECT_TRUE(config.verify_blocks);
  EXPECT_TRUE(config.use_crc);
  EXPECT_EQ(config.timeout_ms, 5000u);
}

TEST(BlockTransferTest, TransferConfigFast) {
  using namespace block;
  
  auto config = TransferConfig::fast();
  
  EXPECT_EQ(config.block_size, 4096u);
  EXPECT_FALSE(config.verify_blocks);
  EXPECT_EQ(config.max_retries, 1u);
}

TEST(BlockTransferTest, TransferConfigReliable) {
  using namespace block;
  
  auto config = TransferConfig::reliable();
  
  EXPECT_EQ(config.block_size, 256u);
  EXPECT_TRUE(config.verify_blocks);
  EXPECT_EQ(config.max_retries, 5u);
  EXPECT_EQ(config.retry_delay_ms, 200u);
}

TEST(BlockTransferTest, TransferConfigConservative) {
  using namespace block;
  
  auto config = TransferConfig::conservative();
  
  EXPECT_EQ(config.block_size, 64u);
  EXPECT_TRUE(config.verify_blocks);
  EXPECT_EQ(config.max_retries, 10u);
  EXPECT_EQ(config.retry_delay_ms, 500u);
  EXPECT_EQ(config.timeout_ms, 10000u);
}

// ============================================================================
// Transfer State Tests
// ============================================================================

TEST(BlockTransferTest, TransferStateValues) {
  using namespace block;
  
  EXPECT_NE(static_cast<int>(TransferState::Idle), static_cast<int>(TransferState::Preparing));
  EXPECT_NE(static_cast<int>(TransferState::Transferring), static_cast<int>(TransferState::Verifying));
  EXPECT_NE(static_cast<int>(TransferState::Completed), static_cast<int>(TransferState::Failed));
  EXPECT_NE(static_cast<int>(TransferState::Failed), static_cast<int>(TransferState::Cancelled));
}

// ============================================================================
// Transfer Progress Tests
// ============================================================================

TEST(BlockTransferTest, TransferProgressDefault) {
  using namespace block;
  
  TransferProgress progress;
  
  EXPECT_EQ(progress.state, TransferState::Idle);
  EXPECT_EQ(progress.total_bytes, 0u);
  EXPECT_EQ(progress.transferred_bytes, 0u);
  EXPECT_EQ(progress.current_block, 0u);
  EXPECT_EQ(progress.total_blocks, 0u);
  EXPECT_EQ(progress.retry_count, 0u);
  EXPECT_EQ(progress.total_retries, 0u);
}

TEST(BlockTransferTest, TransferProgressPercentage) {
  using namespace block;
  
  TransferProgress progress;
  progress.total_bytes = 1000;
  progress.transferred_bytes = 250;
  
  EXPECT_FLOAT_EQ(progress.percentage(), 25.0f);
}

TEST(BlockTransferTest, TransferProgressPercentageZeroTotal) {
  using namespace block;
  
  TransferProgress progress;
  progress.total_bytes = 0;
  progress.transferred_bytes = 0;
  
  EXPECT_FLOAT_EQ(progress.percentage(), 0.0f);
}

TEST(BlockTransferTest, TransferProgressPercentageComplete) {
  using namespace block;
  
  TransferProgress progress;
  progress.total_bytes = 10000;
  progress.transferred_bytes = 10000;
  
  EXPECT_FLOAT_EQ(progress.percentage(), 100.0f);
}

// ============================================================================
// Cancellation Token Tests
// ============================================================================

TEST(BlockTransferTest, CancellationTokenDefault) {
  using namespace block;
  
  CancellationToken token;
  
  EXPECT_FALSE(token.is_cancelled());
}

TEST(BlockTransferTest, CancellationTokenCancel) {
  using namespace block;
  
  CancellationToken token;
  token.cancel();
  
  EXPECT_TRUE(token.is_cancelled());
}

TEST(BlockTransferTest, CancellationTokenReset) {
  using namespace block;
  
  CancellationToken token;
  token.cancel();
  EXPECT_TRUE(token.is_cancelled());
  
  token.reset();
  EXPECT_FALSE(token.is_cancelled());
}

// ============================================================================
// Transfer Result Tests
// ============================================================================

TEST(BlockTransferTest, TransferResultDefault) {
  using namespace block;
  
  TransferResult result;
  
  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.final_state, TransferState::Idle);
  EXPECT_EQ(result.bytes_transferred, 0u);
  EXPECT_EQ(result.blocks_transferred, 0u);
  EXPECT_EQ(result.total_retries, 0u);
  EXPECT_EQ(result.duration.count(), 0);
  EXPECT_FALSE(result.crc32.has_value());
  EXPECT_TRUE(result.error_message.empty());
}

TEST(BlockTransferTest, TransferResultBytesPerSecond) {
  using namespace block;
  
  TransferResult result;
  result.bytes_transferred = 10000;
  result.duration = std::chrono::milliseconds(1000);
  
  EXPECT_DOUBLE_EQ(result.bytes_per_second(), 10000.0);
}

TEST(BlockTransferTest, TransferResultBytesPerSecondZeroDuration) {
  using namespace block;
  
  TransferResult result;
  result.bytes_transferred = 10000;
  result.duration = std::chrono::milliseconds(0);
  
  EXPECT_DOUBLE_EQ(result.bytes_per_second(), 0.0);
}

// ============================================================================
// Block Sequence Counter Tests
// ============================================================================

TEST(BlockTransferTest, SequenceCounterStart) {
  // First TransferData uses sequence counter 0x01
  uint8_t first_seq = 0x01;
  EXPECT_EQ(first_seq, 1);
}

TEST(BlockTransferTest, SequenceCounterIncrement) {
  uint8_t seq = 0x01;
  for (int i = 2; i <= 255; i++) {
    seq++;
    EXPECT_EQ(seq, i);
  }
}

TEST(BlockTransferTest, SequenceCounterWrap) {
  // Block sequence counter wraps from 0xFF to 0x00 (not 0x01!)
  uint8_t seq = 0xFF;
  seq++;
  EXPECT_EQ(seq, 0x00);
}

// ============================================================================
// CRC32 Tests
// ============================================================================

TEST(BlockTransferTest, CRC32Calculation) {
  using namespace block;
  
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  uint32_t crc = calculate_crc32(data);
  
  // CRC should be non-zero for non-empty data
  EXPECT_NE(crc, 0u);
}

TEST(BlockTransferTest, CRC32Consistency) {
  using namespace block;
  
  std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
  
  uint32_t crc1 = calculate_crc32(data);
  uint32_t crc2 = calculate_crc32(data);
  
  // Same data should produce same CRC
  EXPECT_EQ(crc1, crc2);
}

TEST(BlockTransferTest, CRC32DifferentData) {
  using namespace block;
  
  std::vector<uint8_t> data1 = {0x01, 0x02, 0x03, 0x04};
  std::vector<uint8_t> data2 = {0x01, 0x02, 0x03, 0x05};  // One byte different
  
  uint32_t crc1 = calculate_crc32(data1);
  uint32_t crc2 = calculate_crc32(data2);
  
  // Different data should produce different CRC
  EXPECT_NE(crc1, crc2);
}

TEST(BlockTransferTest, CRC32WithInitialValue) {
  using namespace block;
  
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  
  uint32_t crc_default = calculate_crc32(data);
  uint32_t crc_custom = calculate_crc32(data, 0x12345678);
  
  // Different initial values should produce different results
  EXPECT_NE(crc_default, crc_custom);
}

// ============================================================================
// Address/Size Encoding Tests
// ============================================================================

TEST(BlockTransferTest, AddressEncoding1Byte) {
  uint8_t address = 0x80;
  std::vector<uint8_t> encoded = {address};
  
  EXPECT_EQ(encoded.size(), 1u);
  EXPECT_EQ(encoded[0], 0x80);
}

TEST(BlockTransferTest, AddressEncoding2Bytes) {
  uint16_t address = 0x1234;
  std::vector<uint8_t> encoded;
  encoded.push_back((address >> 8) & 0xFF);
  encoded.push_back(address & 0xFF);
  
  EXPECT_EQ(encoded.size(), 2u);
  EXPECT_EQ(encoded[0], 0x12);
  EXPECT_EQ(encoded[1], 0x34);
}

TEST(BlockTransferTest, AddressEncoding4Bytes) {
  uint32_t address = 0x12345678;
  std::vector<uint8_t> encoded;
  encoded.push_back((address >> 24) & 0xFF);
  encoded.push_back((address >> 16) & 0xFF);
  encoded.push_back((address >> 8) & 0xFF);
  encoded.push_back(address & 0xFF);
  
  EXPECT_EQ(encoded.size(), 4u);
  EXPECT_EQ(encoded[0], 0x12);
  EXPECT_EQ(encoded[1], 0x34);
  EXPECT_EQ(encoded[2], 0x56);
  EXPECT_EQ(encoded[3], 0x78);
}

// ============================================================================
// Data Format Identifier Tests
// ============================================================================

TEST(BlockTransferTest, DataFormatIdentifier) {
  // dataFormatIdentifier encoding:
  // High nibble: compressionMethod (0=none)
  // Low nibble: encryptingMethod (0=none)
  
  uint8_t no_compression_no_encryption = 0x00;
  uint8_t compression_only = 0x10;
  uint8_t encryption_only = 0x01;
  uint8_t both = 0x11;
  
  EXPECT_EQ(no_compression_no_encryption >> 4, 0);  // No compression
  EXPECT_EQ(no_compression_no_encryption & 0x0F, 0);  // No encryption
  
  EXPECT_EQ(compression_only >> 4, 1);  // Compression method 1
  EXPECT_EQ(compression_only & 0x0F, 0);  // No encryption
  
  EXPECT_EQ(encryption_only >> 4, 0);  // No compression
  EXPECT_EQ(encryption_only & 0x0F, 1);  // Encryption method 1
  
  EXPECT_EQ(both >> 4, 1);  // Compression method 1
  EXPECT_EQ(both & 0x0F, 1);  // Encryption method 1
}

// ============================================================================
// Block Size Calculation Tests
// ============================================================================

TEST(BlockTransferTest, BlockCountCalculation) {
  // Calculate number of blocks for a transfer
  auto calc_blocks = [](uint64_t total_size, uint32_t block_size) -> uint32_t {
    return static_cast<uint32_t>((total_size + block_size - 1) / block_size);
  };
  
  EXPECT_EQ(calc_blocks(1000, 256), 4u);   // 1000 / 256 = 3.9 -> 4 blocks
  EXPECT_EQ(calc_blocks(1024, 256), 4u);   // Exact fit
  EXPECT_EQ(calc_blocks(1025, 256), 5u);   // One byte over
  EXPECT_EQ(calc_blocks(100000, 4096), 25u);  // Large transfer
  EXPECT_EQ(calc_blocks(1, 256), 1u);      // Single byte
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
