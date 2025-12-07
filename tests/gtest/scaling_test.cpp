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
// Additional Scaling Format Name Tests
// ============================================================================

TEST(FormatNameTest, AllFormatNames) {
  EXPECT_STREQ(scaling_format_name(ScalingFormat::UnscaledUnsigned), "Unscaled Unsigned");
  EXPECT_STREQ(scaling_format_name(ScalingFormat::UnscaledSigned), "Unscaled Signed");
  EXPECT_STREQ(scaling_format_name(ScalingFormat::LinearUnsigned), "Linear Unsigned");
  EXPECT_STREQ(scaling_format_name(ScalingFormat::LinearSigned), "Linear Signed");
  EXPECT_STREQ(scaling_format_name(ScalingFormat::BitMappedReported), "Bit-Mapped");
  EXPECT_STREQ(scaling_format_name(ScalingFormat::ASCII), "ASCII");
  EXPECT_STREQ(scaling_format_name(ScalingFormat::FormulaUnsigned), "Formula Unsigned");
  EXPECT_STREQ(scaling_format_name(ScalingFormat::FormulaSigned), "Formula Signed");
  EXPECT_STREQ(scaling_format_name(ScalingFormat::UnitFormat), "Unit Format");
  EXPECT_STREQ(scaling_format_name(ScalingFormat::StateEncoded), "State Encoded");
  EXPECT_STREQ(scaling_format_name(ScalingFormat::OEMSpecific), "OEM Specific");
  EXPECT_STREQ(scaling_format_name(static_cast<ScalingFormat>(0xFF)), "Unknown");
}

// ============================================================================
// Additional Unit Name Tests
// ============================================================================

TEST(UnitNameTest, AllUnitNames) {
  EXPECT_STREQ(unit_name(Unit::NoUnit), "No Unit");
  EXPECT_STREQ(unit_name(Unit::Percent), "Percent");
  EXPECT_STREQ(unit_name(Unit::PerMille), "Per Mille");
  EXPECT_STREQ(unit_name(Unit::DegreeCelsius), "Degree Celsius");
  EXPECT_STREQ(unit_name(Unit::Kelvin), "Kelvin");
  EXPECT_STREQ(unit_name(Unit::KiloPascal), "Kilopascal");
  EXPECT_STREQ(unit_name(Unit::Bar), "Bar");
  EXPECT_STREQ(unit_name(Unit::Volt), "Volt");
  EXPECT_STREQ(unit_name(Unit::Ampere), "Ampere");
  EXPECT_STREQ(unit_name(Unit::Ohm), "Ohm");
  EXPECT_STREQ(unit_name(Unit::Kilogram), "Kilogram");
  EXPECT_STREQ(unit_name(Unit::Gram), "Gram");
  EXPECT_STREQ(unit_name(Unit::Milligram), "Milligram");
  EXPECT_STREQ(unit_name(Unit::Meter), "Meter");
  EXPECT_STREQ(unit_name(Unit::Centimeter), "Centimeter");
  EXPECT_STREQ(unit_name(Unit::Millimeter), "Millimeter");
  EXPECT_STREQ(unit_name(Unit::KilometersPerHour), "Kilometers per Hour");
  EXPECT_STREQ(unit_name(Unit::MetersPerSecond), "Meters per Second");
  EXPECT_STREQ(unit_name(Unit::RevolutionsPerMinute), "Revolutions per Minute");
  EXPECT_STREQ(unit_name(Unit::Hertz), "Hertz");
  EXPECT_STREQ(unit_name(Unit::Kilohertz), "Kilohertz");
  EXPECT_STREQ(unit_name(Unit::Second), "Second");
  EXPECT_STREQ(unit_name(Unit::Millisecond), "Millisecond");
  EXPECT_STREQ(unit_name(Unit::Microsecond), "Microsecond");
  EXPECT_STREQ(unit_name(Unit::Degree), "Degree");
  EXPECT_STREQ(unit_name(Unit::Radian), "Radian");
  EXPECT_STREQ(unit_name(Unit::LiterPerHour), "Liter per Hour");
  EXPECT_STREQ(unit_name(Unit::GramPerSecond), "Gram per Second");
  EXPECT_STREQ(unit_name(Unit::Newton), "Newton");
  EXPECT_STREQ(unit_name(Unit::NewtonMeter), "Newton Meter");
  EXPECT_STREQ(unit_name(Unit::Watt), "Watt");
  EXPECT_STREQ(unit_name(Unit::Kilowatt), "Kilowatt");
  EXPECT_STREQ(unit_name(static_cast<Unit>(0xFF)), "Unknown");
}

// ============================================================================
// Additional Unit Symbol Tests
// ============================================================================

TEST(UnitSymbolTest, AllUnitSymbols) {
  EXPECT_STREQ(unit_symbol(Unit::NoUnit), "");
  EXPECT_STREQ(unit_symbol(Unit::Percent), "%");
  EXPECT_STREQ(unit_symbol(Unit::Volt), "V");
  EXPECT_STREQ(unit_symbol(Unit::Ampere), "A");
  EXPECT_STREQ(unit_symbol(Unit::Ohm), "Ω");
  EXPECT_STREQ(unit_symbol(Unit::Meter), "m");
  EXPECT_STREQ(unit_symbol(Unit::KilometersPerHour), "km/h");
  EXPECT_STREQ(unit_symbol(Unit::RevolutionsPerMinute), "rpm");
  EXPECT_STREQ(unit_symbol(Unit::Second), "s");
  EXPECT_STREQ(unit_symbol(Unit::Millisecond), "ms");
  EXPECT_STREQ(unit_symbol(Unit::DegreeCelsius), "°C");
}

// ============================================================================
// Linear Scaling with ScalingInfo Tests
// ============================================================================

TEST(LinearScalingTest, ApplyWithScalingInfo) {
  ScalingInfo info;
  info.format = ScalingFormat::LinearUnsigned;
  info.linear = LinearScaling{0.5, -40.0, 1};
  info.is_signed = false;
  
  std::vector<uint8_t> raw = {0x50};  // 80
  double result = apply_linear_scaling(raw, info);
  EXPECT_DOUBLE_EQ(result, 0.0);  // 80 * 0.5 - 40 = 0
}

TEST(LinearScalingTest, ApplyWithoutLinearInfo) {
  ScalingInfo info;
  info.format = ScalingFormat::UnscaledUnsigned;
  info.is_signed = false;
  // No linear scaling set
  
  std::vector<uint8_t> raw = {0x64};  // 100
  double result = apply_linear_scaling(raw, info);
  EXPECT_DOUBLE_EQ(result, 100.0);  // Raw value returned
}

// ============================================================================
// Bit-Mapped Scaling Tests
// ============================================================================

TEST(BitMappedTest, ApplyBitMappedScaling) {
  ScalingInfo info;
  info.format = ScalingFormat::BitMappedReported;
  
  BitMappedScaling bm;
  bm.bits.push_back({0, "Bit 0", true});
  bm.bits.push_back({1, "Bit 1", true});
  bm.bits.push_back({2, "Bit 2", false});  // active_low
  info.bit_mapped = bm;
  
  std::vector<uint8_t> raw = {0x03};  // bits 0 and 1 set
  auto results = apply_bit_mapped_scaling(raw, info);
  
  EXPECT_EQ(results.size(), 3u);
  EXPECT_TRUE(results[0].second);   // Bit 0 active
  EXPECT_TRUE(results[1].second);   // Bit 1 active
  EXPECT_TRUE(results[2].second);   // Bit 2 active (active_low, bit not set)
}

TEST(BitMappedTest, ApplyBitMappedScalingNoInfo) {
  ScalingInfo info;
  info.format = ScalingFormat::BitMappedReported;
  // No bit_mapped info set
  
  std::vector<uint8_t> raw = {0xFF};
  auto results = apply_bit_mapped_scaling(raw, info);
  
  EXPECT_TRUE(results.empty());
}

// ============================================================================
// State-Encoded Scaling Tests
// ============================================================================

TEST(StateEncodedTest, ApplyStateEncodedScaling) {
  ScalingInfo info;
  info.format = ScalingFormat::StateEncoded;
  
  StateEncodedScaling se;
  se.states.push_back({0x00, "Off"});
  se.states.push_back({0x01, "On"});
  se.states.push_back({0x02, "Error"});
  info.state_encoded = se;
  
  EXPECT_EQ(apply_state_encoded_scaling(0x00, info), "Off");
  EXPECT_EQ(apply_state_encoded_scaling(0x01, info), "On");
  EXPECT_EQ(apply_state_encoded_scaling(0x02, info), "Error");
  EXPECT_EQ(apply_state_encoded_scaling(0xFF, info), "");  // Unknown state
}

TEST(StateEncodedTest, ApplyStateEncodedScalingNoInfo) {
  ScalingInfo info;
  info.format = ScalingFormat::StateEncoded;
  // No state_encoded info set
  
  EXPECT_EQ(apply_state_encoded_scaling(0x00, info), "");
}

// ============================================================================
// Parse Scaling Info Tests
// ============================================================================

TEST(ParseScalingTest, EmptyPayload) {
  std::vector<uint8_t> payload;
  auto info = parse_scaling_info(0xF190, payload);
  EXPECT_EQ(info.did, 0xF190);
  EXPECT_TRUE(info.raw_scaling_bytes.empty());
}

TEST(ParseScalingTest, UnscaledUnsigned) {
  std::vector<uint8_t> payload = {0x00, 0x02};  // Unscaled unsigned, 2 bytes
  auto info = parse_scaling_info(0xF190, payload);
  EXPECT_EQ(info.format, ScalingFormat::UnscaledUnsigned);
  EXPECT_FALSE(info.is_signed);
  EXPECT_EQ(info.data_length, 2u);
}

TEST(ParseScalingTest, UnscaledSigned) {
  std::vector<uint8_t> payload = {0x01, 0x02};  // Unscaled signed, 2 bytes
  auto info = parse_scaling_info(0xF190, payload);
  EXPECT_EQ(info.format, ScalingFormat::UnscaledSigned);
  EXPECT_TRUE(info.is_signed);
}

TEST(ParseScalingTest, ASCIIFormat) {
  std::vector<uint8_t> payload = {0x30, 0x11};  // ASCII, 17 bytes
  auto info = parse_scaling_info(0xF190, payload);
  EXPECT_EQ(info.format, ScalingFormat::ASCII);
}

TEST(ParseScalingTest, LinearUnsignedWithCoefficient) {
  // Linear format: [format][decimals][coef 4 bytes][offset 4 bytes]
  std::vector<uint8_t> payload = {
    0x10,                   // LinearUnsigned
    0x02,                   // 2 decimal places
    0x00, 0x00, 0x00, 0x64, // coefficient = 100 (1.00 after decimals)
    0x00, 0x00, 0x00, 0x00  // offset = 0
  };
  auto info = parse_scaling_info(0xF40C, payload);
  EXPECT_EQ(info.format, ScalingFormat::LinearUnsigned);
  EXPECT_TRUE(info.linear.has_value());
}

TEST(ParseScalingTest, LinearSignedWithOffset) {
  std::vector<uint8_t> payload = {
    0x11,                   // LinearSigned
    0x01,                   // 1 decimal place
    0x00, 0x00, 0x00, 0x0A, // coefficient = 10 (1.0)
    0xFF, 0xFF, 0xFF, 0xD8  // offset = -40 (-4.0)
  };
  auto info = parse_scaling_info(0xF405, payload);
  EXPECT_EQ(info.format, ScalingFormat::LinearSigned);
  EXPECT_TRUE(info.is_signed);
}

TEST(ParseScalingTest, UnitFormat) {
  std::vector<uint8_t> payload = {0x50, 0x03, 0x02};  // Unit format (0x50), DegreeCelsius, 2 bytes
  auto info = parse_scaling_info(0xF405, payload);
  EXPECT_EQ(info.format, ScalingFormat::UnitFormat);
  EXPECT_EQ(info.unit, Unit::DegreeCelsius);
  EXPECT_EQ(info.data_length, 2u);
}

TEST(ParseScalingTest, BitMappedReported) {
  std::vector<uint8_t> payload = {0x20, 0xFF};  // BitMapped
  auto info = parse_scaling_info(0xF400, payload);
  EXPECT_EQ(info.format, ScalingFormat::BitMappedReported);
}

TEST(ParseScalingTest, StateEncoded) {
  std::vector<uint8_t> payload = {0x60, 0x01};  // StateEncoded (0x60)
  auto info = parse_scaling_info(0xF401, payload);
  EXPECT_EQ(info.format, ScalingFormat::StateEncoded);
}

TEST(ParseScalingTest, ASCIIWithText) {
  std::vector<uint8_t> payload = {0x30, 'T', 'E', 'S', 'T'};
  auto info = parse_scaling_info(0xF190, payload);
  EXPECT_EQ(info.format, ScalingFormat::ASCII);
  EXPECT_EQ(info.text_value, "TEST");
}

TEST(ParseScalingTest, UnknownFormat) {
  std::vector<uint8_t> payload = {0xFF, 0x01, 0x02};  // Unknown format
  auto info = parse_scaling_info(0xF000, payload);
  EXPECT_EQ(static_cast<uint8_t>(info.format), 0xFF);
}


// ============================================================================
// Null Terminator Tests
// ============================================================================

TEST(ASCIIConversionTest, NullTerminated) {
  std::vector<uint8_t> data = {'A', 'B', 'C', 0x00, 'D', 'E'};
  std::string result = bytes_to_ascii(data);
  EXPECT_EQ(result, "ABC");
}

TEST(ASCIIConversionTest, TrailingSpaces) {
  std::vector<uint8_t> data = {'T', 'E', 'S', 'T', ' ', ' ', ' '};
  std::string result = bytes_to_ascii(data);
  EXPECT_EQ(result, "TEST");
}

TEST(ASCIIConversionTest, NonPrintable) {
  std::vector<uint8_t> data = {'A', 0x01, 'B', 0x02, 'C'};
  std::string result = bytes_to_ascii(data);
  EXPECT_EQ(result, "ABC");
}

// ============================================================================
// Empty Data Tests
// ============================================================================

TEST(ByteConversionTest, EmptyData) {
  std::vector<uint8_t> data;
  EXPECT_EQ(bytes_to_uint(data), 0u);
  EXPECT_EQ(bytes_to_int(data, true), 0);
  EXPECT_EQ(bytes_to_int(data, false), 0);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
