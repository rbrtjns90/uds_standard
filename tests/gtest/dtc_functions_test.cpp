/**
 * @file dtc_functions_test.cpp
 * @brief Comprehensive tests for DTC functions (uds_dtc.cpp)
 */

#include <gtest/gtest.h>
#include "uds.hpp"
#include "uds_dtc.hpp"
#include <queue>

using namespace uds;
using namespace uds::dtc;

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

class DTCTest : public ::testing::Test {
protected:
  void SetUp() override { transport_.reset(); }
  MockTransport transport_;
};

// ============================================================================
// DTC Code Parsing/Encoding Tests
// ============================================================================

TEST(DTCCodeTest, ParseDTCCode) {
  uint8_t bytes[] = {0x12, 0x34, 0x56};
  uint32_t code = parse_dtc_code(bytes);
  EXPECT_EQ(code, 0x123456u);
}

TEST(DTCCodeTest, EncodeDTCCode) {
  auto bytes = encode_dtc_code(0x123456);
  ASSERT_EQ(bytes.size(), 3u);
  EXPECT_EQ(bytes[0], 0x12);
  EXPECT_EQ(bytes[1], 0x34);
  EXPECT_EQ(bytes[2], 0x56);
}

TEST(DTCCodeTest, FormatDTCCodePowertrain) {
  // P-code: first 2 bits = 00
  std::string formatted = format_dtc_code(0x001234);
  EXPECT_EQ(formatted[0], 'P');
}

TEST(DTCCodeTest, FormatDTCCodeChassis) {
  // C-code: first 2 bits = 01
  std::string formatted = format_dtc_code(0x401234);
  EXPECT_EQ(formatted[0], 'C');
}

TEST(DTCCodeTest, FormatDTCCodeBody) {
  // B-code: first 2 bits = 10
  std::string formatted = format_dtc_code(0x801234);
  EXPECT_EQ(formatted[0], 'B');
}

TEST(DTCCodeTest, FormatDTCCodeNetwork) {
  // U-code: first 2 bits = 11
  std::string formatted = format_dtc_code(0xC01234);
  EXPECT_EQ(formatted[0], 'U');
}

TEST(DTCCodeTest, ParseDTCStringPowertrain) {
  uint32_t code = parse_dtc_string("P1234");
  EXPECT_NE(code, 0u);
}

TEST(DTCCodeTest, ParseDTCStringChassis) {
  uint32_t code = parse_dtc_string("C1234");
  EXPECT_NE(code, 0u);
}

TEST(DTCCodeTest, ParseDTCStringBody) {
  uint32_t code = parse_dtc_string("B1234");
  EXPECT_NE(code, 0u);
}

TEST(DTCCodeTest, ParseDTCStringNetwork) {
  uint32_t code = parse_dtc_string("U1234");
  EXPECT_NE(code, 0u);
}

TEST(DTCCodeTest, ParseDTCStringInvalid) {
  EXPECT_EQ(parse_dtc_string("X1234"), 0u);
  EXPECT_EQ(parse_dtc_string("P12"), 0u);  // Too short
  EXPECT_EQ(parse_dtc_string("PZZZZ"), 0u);  // Invalid hex
}

TEST(DTCCodeTest, ParseDTCStringLowercase) {
  uint32_t code = parse_dtc_string("p1234");
  EXPECT_NE(code, 0u);
}

// ============================================================================
// DTC Status Description Tests
// ============================================================================

TEST(DTCStatusTest, DescribeTestFailed) {
  std::string desc = describe_dtc_status(StatusMask::TestFailed);
  EXPECT_NE(desc.find("TestFailed"), std::string::npos);
}

TEST(DTCStatusTest, DescribeConfirmed) {
  std::string desc = describe_dtc_status(StatusMask::ConfirmedDTC);
  EXPECT_NE(desc.find("Confirmed"), std::string::npos);
}

TEST(DTCStatusTest, DescribePending) {
  std::string desc = describe_dtc_status(StatusMask::PendingDTC);
  EXPECT_NE(desc.find("Pending"), std::string::npos);
}

TEST(DTCStatusTest, DescribeWarningIndicator) {
  std::string desc = describe_dtc_status(StatusMask::WarningIndicatorRequested);
  EXPECT_NE(desc.find("WarningIndicator"), std::string::npos);
}

TEST(DTCStatusTest, DescribeMultiple) {
  std::string desc = describe_dtc_status(StatusMask::TestFailed | StatusMask::ConfirmedDTC);
  EXPECT_NE(desc.find("TestFailed"), std::string::npos);
  EXPECT_NE(desc.find("Confirmed"), std::string::npos);
}

TEST(DTCStatusTest, DescribeNone) {
  std::string desc = describe_dtc_status(0x00);
  EXPECT_EQ(desc, "None");
}

// ============================================================================
// Severity Name Tests
// ============================================================================

TEST(DTCSeverityTest, SeverityNames) {
  EXPECT_STREQ(severity_name(DTCSeverity::NoSeverityAvailable), "No Severity Available");
  EXPECT_STREQ(severity_name(DTCSeverity::MaintenanceOnly), "Maintenance Only");
  EXPECT_STREQ(severity_name(DTCSeverity::CheckAtNextHalt), "Check At Next Halt");
  EXPECT_STREQ(severity_name(DTCSeverity::CheckImmediately), "Check Immediately");
}

// ============================================================================
// SubFunction Name Tests
// ============================================================================

TEST(DTCSubFunctionTest, SubFunctionNames) {
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportNumberOfDTCByStatusMask), 
               "ReportNumberOfDTCByStatusMask");
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportDTCByStatusMask), 
               "ReportDTCByStatusMask");
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportSupportedDTC), 
               "ReportSupportedDTC");
}

// ============================================================================
// DTCRecord Tests
// ============================================================================

TEST(DTCRecordTest, StatusAccessors) {
  DTCRecord rec;
  rec.status = StatusMask::TestFailed | StatusMask::ConfirmedDTC;
  
  EXPECT_TRUE(rec.test_failed());
  EXPECT_TRUE(rec.is_confirmed());
  EXPECT_FALSE(rec.is_pending());
  EXPECT_FALSE(rec.warning_indicator());
}

// ============================================================================
// Get DTC Count Tests
// ============================================================================

TEST_F(DTCTest, GetDTCCount) {
  Client client(transport_);
  // Response: [subFunc][statusAvailMask][formatId][countHigh][countLow]
  transport_.queue_response({0x59, 0x01, 0xFF, 0x01, 0x00, 0x05});
  
  auto result = get_dtc_count(client, StatusMask::AllDTCs);
  
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.value.dtc_count, 5u);
  EXPECT_EQ(result.value.status_availability_mask, 0xFF);
}

TEST_F(DTCTest, GetDTCCountShortResponse) {
  Client client(transport_);
  transport_.queue_response({0x59, 0x01, 0xFF});  // Too short
  
  auto result = get_dtc_count(client, StatusMask::AllDTCs);
  
  EXPECT_FALSE(result.ok);
}

TEST_F(DTCTest, GetDTCCountNegativeResponse) {
  Client client(transport_);
  transport_.queue_response({0x7F, 0x19, 0x12});  // SubFunctionNotSupported
  
  auto result = get_dtc_count(client, StatusMask::AllDTCs);
  
  EXPECT_FALSE(result.ok);
}

// ============================================================================
// Read DTCs By Status Tests
// ============================================================================

TEST_F(DTCTest, ReadDTCsByStatus) {
  Client client(transport_);
  // Response: [subFunc][statusAvailMask][DTC1(3)][status1][DTC2(3)][status2]
  transport_.queue_response({0x59, 0x02, 0xFF, 
                             0x12, 0x34, 0x56, 0x08,
                             0xAB, 0xCD, 0xEF, 0x04});
  
  auto result = read_dtcs_by_status(client, StatusMask::AllDTCs);
  
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.value.dtcs.size(), 2u);
  EXPECT_EQ(result.value.dtcs[0].code, 0x123456u);
  EXPECT_EQ(result.value.dtcs[0].status, 0x08);
  EXPECT_EQ(result.value.dtcs[1].code, 0xABCDEFu);
}

TEST_F(DTCTest, ReadDTCsByStatusEmpty) {
  Client client(transport_);
  transport_.queue_response({0x59, 0x02, 0xFF});  // No DTCs
  
  auto result = read_dtcs_by_status(client, StatusMask::AllDTCs);
  
  EXPECT_TRUE(result.ok);
  EXPECT_TRUE(result.value.dtcs.empty());
}

// ============================================================================
// Read Supported DTCs Tests
// ============================================================================

TEST_F(DTCTest, ReadSupportedDTCs) {
  Client client(transport_);
  transport_.queue_response({0x59, 0x0A, 0xFF, 0x12, 0x34, 0x56, 0x00});
  
  auto result = read_supported_dtcs(client);
  
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.value.dtcs.size(), 1u);
}

// ============================================================================
// Read First/Most Recent DTC Tests
// ============================================================================

TEST_F(DTCTest, ReadFirstTestFailedDTC) {
  Client client(transport_);
  transport_.queue_response({0x59, 0x0B, 0xFF, 0x12, 0x34, 0x56, 0x01});
  
  auto result = read_first_test_failed_dtc(client);
  
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.value.dtcs.size(), 1u);
}

TEST_F(DTCTest, ReadFirstConfirmedDTC) {
  Client client(transport_);
  transport_.queue_response({0x59, 0x0C, 0xFF, 0x12, 0x34, 0x56, 0x08});
  
  auto result = read_first_confirmed_dtc(client);
  
  EXPECT_TRUE(result.ok);
}

TEST_F(DTCTest, ReadMostRecentTestFailedDTC) {
  Client client(transport_);
  transport_.queue_response({0x59, 0x0D, 0xFF, 0x12, 0x34, 0x56, 0x01});
  
  auto result = read_most_recent_test_failed_dtc(client);
  
  EXPECT_TRUE(result.ok);
}

TEST_F(DTCTest, ReadMostRecentConfirmedDTC) {
  Client client(transport_);
  transport_.queue_response({0x59, 0x0E, 0xFF, 0x12, 0x34, 0x56, 0x08});
  
  auto result = read_most_recent_confirmed_dtc(client);
  
  EXPECT_TRUE(result.ok);
}

// ============================================================================
// Read Permanent DTCs Tests
// ============================================================================

TEST_F(DTCTest, ReadPermanentDTCs) {
  Client client(transport_);
  transport_.queue_response({0x59, 0x15, 0xFF, 0x12, 0x34, 0x56, 0x08});
  
  auto result = read_permanent_dtcs(client);
  
  EXPECT_TRUE(result.ok);
}

// ============================================================================
// Read DTCs By Severity Tests
// ============================================================================

TEST_F(DTCTest, ReadDTCsBySeverity) {
  Client client(transport_);
  // Response: [subFunc][statusAvailMask][severity][funcUnit][DTC(3)][status]
  transport_.queue_response({0x59, 0x08, 0xFF, 0x80, 0x01, 0x12, 0x34, 0x56, 0x08});
  
  auto result = read_dtcs_by_severity(client, 0x80, StatusMask::AllDTCs);
  
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.value.dtcs.size(), 1u);
  EXPECT_EQ(result.value.dtcs[0].severity, DTCSeverity::CheckImmediately);
}

// ============================================================================
// Read DTC Snapshot Tests
// ============================================================================

TEST_F(DTCTest, ReadDTCSnapshot) {
  Client client(transport_);
  transport_.queue_response({0x59, 0x04, 0x12, 0x34, 0x56, 0x08, 0x01, 0xAB, 0xCD});
  
  auto result = read_dtc_snapshot(client, 0x123456, 0xFF);
  
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.value.snapshots.size(), 1u);
}

// ============================================================================
// Read DTC Extended Data Tests
// ============================================================================

TEST_F(DTCTest, ReadDTCExtendedData) {
  Client client(transport_);
  transport_.queue_response({0x59, 0x06, 0x12, 0x34, 0x56, 0x08, 0x01, 0xDE, 0xAD});
  
  auto result = read_dtc_extended_data(client, 0x123456, 0xFF);
  
  EXPECT_TRUE(result.ok);
}

// ============================================================================
// Clear DTC Tests
// ============================================================================

TEST_F(DTCTest, ClearAllDTCs) {
  Client client(transport_);
  transport_.queue_response({0x54});
  
  auto result = clear_all_dtcs(client);
  
  EXPECT_TRUE(result.ok);
}

TEST_F(DTCTest, ClearDTCGroup) {
  Client client(transport_);
  transport_.queue_response({0x54});
  
  auto result = clear_dtc_group(client, Group::Powertrain);
  
  EXPECT_TRUE(result.ok);
}

TEST_F(DTCTest, ClearPowertrainDTCs) {
  Client client(transport_);
  transport_.queue_response({0x54});
  
  auto result = clear_powertrain_dtcs(client);
  
  EXPECT_TRUE(result.ok);
}

TEST_F(DTCTest, ClearChassisDTCs) {
  Client client(transport_);
  transport_.queue_response({0x54});
  
  auto result = clear_chassis_dtcs(client);
  
  EXPECT_TRUE(result.ok);
}

TEST_F(DTCTest, ClearBodyDTCs) {
  Client client(transport_);
  transport_.queue_response({0x54});
  
  auto result = clear_body_dtcs(client);
  
  EXPECT_TRUE(result.ok);
}

TEST_F(DTCTest, ClearNetworkDTCs) {
  Client client(transport_);
  transport_.queue_response({0x54});
  
  auto result = clear_network_dtcs(client);
  
  EXPECT_TRUE(result.ok);
}

// ============================================================================
// Control DTC Setting Tests
// ============================================================================

TEST_F(DTCTest, EnableDTCSetting) {
  Client client(transport_);
  transport_.queue_response({0xC5, 0x01});
  
  auto result = enable_dtc_setting(client);
  
  EXPECT_TRUE(result.ok);
}

TEST_F(DTCTest, DisableDTCSetting) {
  Client client(transport_);
  transport_.queue_response({0xC5, 0x02});
  
  auto result = disable_dtc_setting(client);
  
  EXPECT_TRUE(result.ok);
}

TEST_F(DTCTest, IsDTCSettingEnabled) {
  Client client(transport_);
  
  EXPECT_TRUE(is_dtc_setting_enabled(client));  // Default is enabled
}

// ============================================================================
// RAII Guard Tests
// ============================================================================

TEST_F(DTCTest, DTCSettingGuard) {
  Client client(transport_);
  
  // Queue responses for disable and enable
  transport_.queue_response({0xC5, 0x02});  // For potential disable in guard
  transport_.queue_response({0xC5, 0x01});  // For restore on destruction
  
  {
    DTCSettingGuard guard(client);
    // Guard should save state
  }
  // Guard destructor should restore state
}

TEST_F(DTCTest, FlashDTCGuard) {
  Client client(transport_);
  
  transport_.queue_response({0xC5, 0x02});  // For disable
  transport_.queue_response({0xC5, 0x01});  // For enable on destruction
  
  {
    FlashDTCGuard guard(client);
    EXPECT_TRUE(guard.is_active());
  }
}

// ============================================================================
// Additional Sub-Function Name Tests
// ============================================================================

TEST(DTCSubFunctionTest, AllSubFunctionNames) {
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportNumberOfDTCByStatusMask), "ReportNumberOfDTCByStatusMask");
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportDTCByStatusMask), "ReportDTCByStatusMask");
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportDTCSnapshotIdentification), "ReportDTCSnapshotIdentification");
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportDTCSnapshotRecordByDTCNumber), "ReportDTCSnapshotRecordByDTCNumber");
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportDTCExtDataRecordByDTCNumber), "ReportDTCExtDataRecordByDTCNumber");
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportSupportedDTC), "ReportSupportedDTC");
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportFirstTestFailedDTC), "ReportFirstTestFailedDTC");
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportFirstConfirmedDTC), "ReportFirstConfirmedDTC");
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportMostRecentTestFailedDTC), "ReportMostRecentTestFailedDTC");
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportMostRecentConfirmedDTC), "ReportMostRecentConfirmedDTC");
  EXPECT_STREQ(subfunction_name(ReadDTCSubFunction::ReportDTCWithPermanentStatus), "ReportDTCWithPermanentStatus");
  EXPECT_STREQ(subfunction_name(static_cast<ReadDTCSubFunction>(0xFF)), "Unknown");
}

// ============================================================================
// Additional DTC Severity Name Tests
// ============================================================================

TEST(DTCSeverityTest, UnknownSeverity) {
  EXPECT_STREQ(severity_name(static_cast<DTCSeverity>(0xFF)), "Unknown");
}

// ============================================================================
// Additional DTC Type Tests
// ============================================================================

TEST(DTCCodeTest, UnknownType) {
  // Test unknown DTC type character - format_dtc_code handles unknown types
  std::string formatted = format_dtc_code(0xFFFFFF);  // Unknown type
  EXPECT_FALSE(formatted.empty());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
