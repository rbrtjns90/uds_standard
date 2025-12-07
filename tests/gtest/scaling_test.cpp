/**
 * @file scaling_test.cpp
 * @brief Google Test suite for scaling/data interpretation functionality
 */

#include <gtest/gtest.h>
#include "uds_scaling.hpp"

using namespace uds;
using namespace uds::scaling;

// ============================================================================
// Scaling Format Tests
// ============================================================================

TEST(ScalingFormatTest, FormatValues) {
  EXPECT_EQ(static_cast<uint8_t>(ScalingFormat::UnscaledUnsigned), 0x00);
  EXPECT_EQ(static_cast<uint8_t>(ScalingFormat::UnscaledSigned), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(ScalingFormat::LinearUnsigned), 0x10);
  EXPECT_EQ(static_cast<uint8_t>(ScalingFormat::LinearSigned), 0x11);
  EXPECT_EQ(static_cast<uint8_t>(ScalingFormat::BitMappedReported), 0x20);
  EXPECT_EQ(static_cast<uint8_t>(ScalingFormat::ASCII), 0x30);
}

// ============================================================================
// Unit Tests
// ============================================================================

TEST(ScalingUnitTest, UnitValues) {
  EXPECT_EQ(static_cast<uint8_t>(Unit::NoUnit), 0x00);
  EXPECT_EQ(static_cast<uint8_t>(Unit::Percent), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(Unit::DegreeCelsius), 0x03);
  EXPECT_EQ(static_cast<uint8_t>(Unit::Volt), 0x07);
  EXPECT_EQ(static_cast<uint8_t>(Unit::Ampere), 0x08);
  EXPECT_EQ(static_cast<uint8_t>(Unit::Meter), 0x0D);
  EXPECT_EQ(static_cast<uint8_t>(Unit::KilometersPerHour), 0x10);
  EXPECT_EQ(static_cast<uint8_t>(Unit::RevolutionsPerMinute), 0x12);
}

TEST(ScalingUnitTest, UnitSymbol) {
  EXPECT_STREQ(unit_symbol(Unit::NoUnit), "");
  EXPECT_STREQ(unit_symbol(Unit::Percent), "%");
  EXPECT_STREQ(unit_symbol(Unit::Volt), "V");
  EXPECT_STREQ(unit_symbol(Unit::Ampere), "A");
}

TEST(ScalingUnitTest, UnitName) {
  EXPECT_STREQ(unit_name(Unit::NoUnit), "No Unit");
  EXPECT_STREQ(unit_name(Unit::Percent), "Percent");
  EXPECT_STREQ(unit_name(Unit::DegreeCelsius), "Degree Celsius");
}

// ============================================================================
// Linear Scaling Tests
// ============================================================================

TEST(LinearScalingTest, SimpleScaling) {
  std::vector<uint8_t> raw = {0x64};  // 100
  double result = apply_linear_scaling(raw, 0.1, 0.0, false);
  EXPECT_DOUBLE_EQ(result, 10.0);
}

TEST(LinearScalingTest, ScalingWithOffset) {
  std::vector<uint8_t> raw = {0x50};  // 80
  double result = apply_linear_scaling(raw, 1.0, -40.0, false);
  EXPECT_DOUBLE_EQ(result, 40.0);
}

TEST(LinearScalingTest, TemperatureConversion) {
  // Typical coolant temp: raw 0-255, -40 to 215°C
  std::vector<uint8_t> raw0 = {0x00};
  std::vector<uint8_t> raw40 = {0x28};
  std::vector<uint8_t> raw140 = {0x8C};
  
  EXPECT_DOUBLE_EQ(apply_linear_scaling(raw0, 1.0, -40.0, false), -40.0);
  EXPECT_DOUBLE_EQ(apply_linear_scaling(raw40, 1.0, -40.0, false), 0.0);
  EXPECT_DOUBLE_EQ(apply_linear_scaling(raw140, 1.0, -40.0, false), 100.0);
}

// ============================================================================
// Byte Conversion Tests
// ============================================================================

TEST(ByteConversionTest, Unsigned8Bit) {
  std::vector<uint8_t> data = {0xFF};
  uint64_t value = bytes_to_uint(data);
  EXPECT_EQ(value, 255);
}

TEST(ByteConversionTest, Unsigned16BitBigEndian) {
  std::vector<uint8_t> data = {0x12, 0x34};
  uint64_t value = bytes_to_uint(data);
  EXPECT_EQ(value, 0x1234);
}

TEST(ByteConversionTest, Unsigned32BitBigEndian) {
  std::vector<uint8_t> data = {0x12, 0x34, 0x56, 0x78};
  uint64_t value = bytes_to_uint(data);
  EXPECT_EQ(value, 0x12345678);
}

TEST(ByteConversionTest, Signed8BitPositive) {
  std::vector<uint8_t> data = {0x7F};  // 127
  int64_t value = bytes_to_int(data, true);
  EXPECT_EQ(value, 127);
}

TEST(ByteConversionTest, Signed8BitNegative) {
  std::vector<uint8_t> data = {0xFF};  // -1
  int64_t value = bytes_to_int(data, true);
  EXPECT_EQ(value, -1);
}

TEST(ByteConversionTest, Signed16BitNegative) {
  std::vector<uint8_t> data = {0xFF, 0xFE};  // -2
  int64_t value = bytes_to_int(data, true);
  EXPECT_EQ(value, -2);
}

// ============================================================================
// ASCII Conversion Tests
// ============================================================================

TEST(ASCIIConversionTest, SimpleASCII) {
  std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
  std::string result = bytes_to_ascii(data);
  EXPECT_EQ(result, "Hello");
}

TEST(ASCIIConversionTest, VINFormat) {
  std::vector<uint8_t> data = {'W', 'V', 'W', 'Z', 'Z', 'Z', '3', 'C', 'Z', 'W', 'E', '1', '2', '3', '4', '5', '6'};
  std::string result = bytes_to_ascii(data);
  EXPECT_EQ(result.length(), 17);
}

// ============================================================================
// Scaling Info Structure Tests
// ============================================================================

TEST(ScalingInfoTest, DefaultValues) {
  ScalingInfo info;
  
  EXPECT_EQ(info.did, 0);
  EXPECT_EQ(info.format, ScalingFormat::UnscaledUnsigned);
  EXPECT_EQ(info.data_length, 0);
  EXPECT_FALSE(info.is_signed);
}

TEST(ScalingInfoTest, LinearScalingInfo) {
  ScalingInfo info;
  info.did = 0xF40C;  // Engine RPM
  info.format = ScalingFormat::LinearUnsigned;
  info.unit = Unit::RevolutionsPerMinute;
  info.data_length = 2;
  info.linear = LinearScaling{0.25, 0.0, 0};
  
  EXPECT_EQ(info.did, 0xF40C);
  EXPECT_EQ(info.data_length, 2);
  EXPECT_TRUE(info.linear.has_value());
  EXPECT_DOUBLE_EQ(info.linear->coefficient, 0.25);
}

// ============================================================================
// Result Type Tests
// ============================================================================

TEST(ScalingResultTest, SuccessResult) {
  auto result = Result<ScalingInfo>::success(ScalingInfo{});
  EXPECT_TRUE(result.ok);
}

TEST(ScalingResultTest, ErrorResult) {
  auto result = Result<ScalingInfo>::error();
  EXPECT_FALSE(result.ok);
}

// ============================================================================
// Format Name Tests
// ============================================================================

TEST(FormatNameTest, GetFormatName) {
  EXPECT_STREQ(scaling_format_name(ScalingFormat::UnscaledUnsigned), "Unscaled Unsigned");
  EXPECT_STREQ(scaling_format_name(ScalingFormat::LinearUnsigned), "Linear Unsigned");
  EXPECT_STREQ(scaling_format_name(ScalingFormat::ASCII), "ASCII");
}

// ============================================================================
// Helper Function Tests
// ============================================================================

TEST(HelperFunctionTest, IsLinearFormat) {
  EXPECT_TRUE(is_linear_format(ScalingFormat::LinearUnsigned));
  EXPECT_TRUE(is_linear_format(ScalingFormat::LinearSigned));
  EXPECT_FALSE(is_linear_format(ScalingFormat::ASCII));
  EXPECT_FALSE(is_linear_format(ScalingFormat::BitMappedReported));
}

TEST(HelperFunctionTest, IsTextFormat) {
  EXPECT_TRUE(is_text_format(ScalingFormat::ASCII));
  EXPECT_FALSE(is_text_format(ScalingFormat::LinearUnsigned));
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
