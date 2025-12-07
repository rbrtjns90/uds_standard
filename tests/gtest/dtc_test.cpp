/**
 * @file dtc_test.cpp
 * @brief Google Test suite for DTC management functionality
 */

#include <gtest/gtest.h>
#include "uds_dtc.hpp"

using namespace uds;
using namespace uds::dtc;

// ============================================================================
// DTC Code Parsing Tests
// ============================================================================

TEST(DTCCodeTest, ParseDTCCode) {
  uint8_t bytes[] = {0x01, 0x23, 0x45};
  uint32_t code = parse_dtc_code(bytes);
  EXPECT_EQ(code, 0x012345);
}

TEST(DTCCodeTest, EncodeDTCCode) {
  auto bytes = encode_dtc_code(0xABCDEF);
  
  ASSERT_EQ(bytes.size(), 3);
  EXPECT_EQ(bytes[0], 0xAB);
  EXPECT_EQ(bytes[1], 0xCD);
  EXPECT_EQ(bytes[2], 0xEF);
}

TEST(DTCCodeTest, RoundTrip) {
  uint32_t original = 0x123456;
  auto encoded = encode_dtc_code(original);
  uint32_t decoded = parse_dtc_code(encoded.data());
  EXPECT_EQ(decoded, original);
}

// ============================================================================
// DTC Status Mask Tests
// ============================================================================

TEST(DTCStatusTest, StatusMaskValues) {
  EXPECT_EQ(StatusMask::TestFailed, 0x01);
  EXPECT_EQ(StatusMask::TestFailedThisOperationCycle, 0x02);
  EXPECT_EQ(StatusMask::PendingDTC, 0x04);
  EXPECT_EQ(StatusMask::ConfirmedDTC, 0x08);
  EXPECT_EQ(StatusMask::TestNotCompletedSinceLastClear, 0x10);
  EXPECT_EQ(StatusMask::TestFailedSinceLastClear, 0x20);
  EXPECT_EQ(StatusMask::TestNotCompletedThisOperationCycle, 0x40);
  EXPECT_EQ(StatusMask::WarningIndicatorRequested, 0x80);
  EXPECT_EQ(StatusMask::AllDTCs, 0xFF);
}

TEST(DTCStatusTest, DTCRecordStatusAccessors) {
  DTCRecord dtc;
  dtc.status = 0x00;
  
  EXPECT_FALSE(dtc.test_failed());
  EXPECT_FALSE(dtc.is_pending());
  EXPECT_FALSE(dtc.is_confirmed());
  EXPECT_FALSE(dtc.warning_indicator());
  
  dtc.status = StatusMask::ConfirmedDTC | StatusMask::WarningIndicatorRequested;
  
  EXPECT_FALSE(dtc.test_failed());
  EXPECT_FALSE(dtc.is_pending());
  EXPECT_TRUE(dtc.is_confirmed());
  EXPECT_TRUE(dtc.warning_indicator());
}

// ============================================================================
// DTC Group Tests
// ============================================================================

TEST(DTCGroupTest, GroupConstants) {
  EXPECT_EQ(Group::AllDTCs, 0xFFFFFF);
  EXPECT_EQ(Group::Powertrain, 0x000000);
  EXPECT_EQ(Group::Chassis, 0x400000);
  EXPECT_EQ(Group::Body, 0x800000);
  EXPECT_EQ(Group::Network, 0xC00000);
}

// ============================================================================
// DTC Severity Tests
// ============================================================================

TEST(DTCSeverityTest, SeverityValues) {
  EXPECT_EQ(static_cast<uint8_t>(DTCSeverity::NoSeverityAvailable), 0x00);
  EXPECT_EQ(static_cast<uint8_t>(DTCSeverity::MaintenanceOnly), 0x20);
  EXPECT_EQ(static_cast<uint8_t>(DTCSeverity::CheckAtNextHalt), 0x40);
  EXPECT_EQ(static_cast<uint8_t>(DTCSeverity::CheckImmediately), 0x80);
}

TEST(DTCSeverityTest, SeverityName) {
  EXPECT_STREQ(severity_name(DTCSeverity::NoSeverityAvailable), "No Severity Available");
  EXPECT_STREQ(severity_name(DTCSeverity::MaintenanceOnly), "Maintenance Only");
  EXPECT_STREQ(severity_name(DTCSeverity::CheckAtNextHalt), "Check At Next Halt");
  EXPECT_STREQ(severity_name(DTCSeverity::CheckImmediately), "Check Immediately");
}

// ============================================================================
// DTC Sub-function Tests
// ============================================================================

TEST(DTCSubFunctionTest, SubFunctionValues) {
  EXPECT_EQ(static_cast<uint8_t>(ReadDTCSubFunction::ReportNumberOfDTCByStatusMask), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(ReadDTCSubFunction::ReportDTCByStatusMask), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(ReadDTCSubFunction::ReportSupportedDTC), 0x0A);
  EXPECT_EQ(static_cast<uint8_t>(ReadDTCSubFunction::ReportFirstTestFailedDTC), 0x0B);
  EXPECT_EQ(static_cast<uint8_t>(ReadDTCSubFunction::ReportFirstConfirmedDTC), 0x0C);
  EXPECT_EQ(static_cast<uint8_t>(ReadDTCSubFunction::ReportDTCWithPermanentStatus), 0x15);
}

TEST(DTCSubFunctionTest, SubFunctionName) {
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportNumberOfDTCByStatusMask), 
               "ReportNumberOfDTCByStatusMask");
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportDTCByStatusMask), 
               "ReportDTCByStatusMask");
}

// ============================================================================
// DTC Status Description Tests
// ============================================================================

TEST(DTCDescriptionTest, DescribeStatus) {
  std::string desc = describe_dtc_status(0x00);
  EXPECT_EQ(desc, "None");
  
  desc = describe_dtc_status(StatusMask::ConfirmedDTC);
  EXPECT_NE(desc.find("Confirmed"), std::string::npos);
  
  desc = describe_dtc_status(StatusMask::PendingDTC | StatusMask::TestFailed);
  EXPECT_NE(desc.find("Pending"), std::string::npos);
  EXPECT_NE(desc.find("TestFailed"), std::string::npos);
}

// ============================================================================
// DTC Format Tests
// ============================================================================

TEST(DTCFormatTest, FormatIdentifierValues) {
  EXPECT_EQ(static_cast<uint8_t>(DTCFormatIdentifier::ISO15031_6DTCFormat), 0x00);
  EXPECT_EQ(static_cast<uint8_t>(DTCFormatIdentifier::ISO14229_1DTCFormat), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(DTCFormatIdentifier::SAEJ1939_73DTCFormat), 0x02);
}

// ============================================================================
// Result Type Tests
// ============================================================================

TEST(DTCResultTest, SuccessResult) {
  auto result = Result<DTCCountResponse>::success(DTCCountResponse{});
  EXPECT_TRUE(result.ok);
}

TEST(DTCResultTest, ErrorResult) {
  auto result = Result<DTCCountResponse>::error();
  EXPECT_FALSE(result.ok);
}

TEST(DTCResultTest, VoidSuccessResult) {
  auto result = Result<void>::success();
  EXPECT_TRUE(result.ok);
}

TEST(DTCResultTest, VoidErrorResult) {
  auto result = Result<void>::error();
  EXPECT_FALSE(result.ok);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
