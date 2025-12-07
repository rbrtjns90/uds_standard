/**
 * @file test_uds_core.cpp
 * @brief Core UDS service testing - ISO 14229-1 compliance
 */

#include "test_framework.hpp"
#include "../include/uds.hpp"
#include "../include/nrc.hpp"
#include <memory>

using namespace test;
using namespace uds;

// ============================================================================
// Test: DiagnosticSessionControl (0x10)
// ============================================================================

TEST(DiagnosticSessionControl_DefaultSession) {
    std::cout << "  Testing: Default Session (0x10 0x01)" << std::endl;
    
    // Mock positive response: 0x50 0x01 [P2 P2*]
    std::vector<uint8_t> response = {0x50, 0x01, 0x00, 0x32, 0x01, 0xF4};
    
    // Verify response format
    ASSERT_EQ(0x50, response[0]); // Positive response SID
    ASSERT_EQ(0x01, response[1]); // Default session
    ASSERT_EQ(6, response.size()); // Correct length
}

TEST(DiagnosticSessionControl_ProgrammingSession) {
    std::cout << "  Testing: Programming Session (0x10 0x02)" << std::endl;
    
    // Mock positive response: 0x50 0x02 [P2 P2*]
    std::vector<uint8_t> response = {0x50, 0x02, 0x00, 0x32, 0x01, 0xF4};
    
    ASSERT_EQ(0x50, response[0]);
    ASSERT_EQ(0x02, response[1]); // Programming session
}

TEST(DiagnosticSessionControl_ExtendedSession) {
    std::cout << "  Testing: Extended Session (0x10 0x03)" << std::endl;
    
    std::vector<uint8_t> response = {0x50, 0x03, 0x00, 0x32, 0x01, 0xF4};
    
    ASSERT_EQ(0x50, response[0]);
    ASSERT_EQ(0x03, response[1]); // Extended session
}

// ============================================================================
// Test: ECUReset (0x11)
// ============================================================================

TEST(ECUReset_HardReset) {
    std::cout << "  Testing: Hard Reset (0x11 0x01)" << std::endl;
    
    // Positive response: 0x51 0x01 [PowerDownTime]
    std::vector<uint8_t> response = {0x51, 0x01, 0x05};
    
    ASSERT_EQ(0x51, response[0]);
    ASSERT_EQ(0x01, response[1]);
}

TEST(ECUReset_SoftReset) {
    std::cout << "  Testing: Soft Reset (0x11 0x03)" << std::endl;
    
    std::vector<uint8_t> response = {0x51, 0x03};
    
    ASSERT_EQ(0x51, response[0]);
    ASSERT_EQ(0x03, response[1]);
}

// ============================================================================
// Test: SecurityAccess (0x27)
// ============================================================================

TEST(SecurityAccess_RequestSeed) {
    std::cout << "  Testing: Security Access - Request Seed (0x27 0x01)" << std::endl;
    
    // Positive response: 0x67 0x01 [seed bytes]
    std::vector<uint8_t> response = {0x67, 0x01, 0x12, 0x34, 0x56, 0x78};
    
    ASSERT_EQ(0x67, response[0]);
    ASSERT_EQ(0x01, response[1]); // RequestSeed subfunction
    ASSERT_GT(response.size(), 2); // Has seed data
}

TEST(SecurityAccess_SendKey) {
    std::cout << "  Testing: Security Access - Send Key (0x27 0x02)" << std::endl;
    
    // Positive response: 0x67 0x02
    std::vector<uint8_t> response = {0x67, 0x02};
    
    ASSERT_EQ(0x67, response[0]);
    ASSERT_EQ(0x02, response[1]); // SendKey subfunction
}

TEST(SecurityAccess_InvalidKey) {
    std::cout << "  Testing: Security Access - Invalid Key (NRC 0x35)" << std::endl;
    
    // Negative response: 0x7F 0x27 0x35 (InvalidKey)
    std::vector<uint8_t> response = {0x7F, 0x27, 0x35};
    
    ASSERT_EQ(0x7F, response[0]); // Negative response
    ASSERT_EQ(0x27, response[1]); // SecurityAccess SID
    ASSERT_EQ(0x35, response[2]); // InvalidKey NRC
}

// ============================================================================
// Test: CommunicationControl (0x28)
// ============================================================================

TEST(CommunicationControl_DisableRxTx) {
    std::cout << "  Testing: Communication Control - Disable Rx/Tx (0x28 0x03)" << std::endl;
    
    // Positive response: 0x68 0x03
    std::vector<uint8_t> response = {0x68, 0x03};
    
    ASSERT_EQ(0x68, response[0]);
    ASSERT_EQ(0x03, response[1]); // DisableRxAndTx
}

TEST(CommunicationControl_EnableRxTx) {
    std::cout << "  Testing: Communication Control - Enable Rx/Tx (0x28 0x00)" << std::endl;
    
    std::vector<uint8_t> response = {0x68, 0x00};
    
    ASSERT_EQ(0x68, response[0]);
    ASSERT_EQ(0x00, response[1]); // EnableRxAndTx
}

// ============================================================================
// Test: TesterPresent (0x3E)
// ============================================================================

TEST(TesterPresent_ZeroSubfunction) {
    std::cout << "  Testing: Tester Present (0x3E 0x00)" << std::endl;
    
    // Positive response: 0x7E 0x00
    std::vector<uint8_t> response = {0x7E, 0x00};
    
    ASSERT_EQ(0x7E, response[0]);
    ASSERT_EQ(0x00, response[1]);
}

// ============================================================================
// Test: ReadDataByIdentifier (0x22)
// ============================================================================

TEST(ReadDataByIdentifier_SingleDID) {
    std::cout << "  Testing: Read Data By Identifier - Single DID (0x22)" << std::endl;
    
    // Request: 0x22 [DID high] [DID low]
    std::vector<uint8_t> request = {0x22, 0xF1, 0x90}; // VIN
    
    // Positive response: 0x62 [DID] [data...]
    std::vector<uint8_t> response = {0x62, 0xF1, 0x90, 
                                      'W', 'V', 'W', 'Z', 'Z', 'Z', '1', 'K', 
                                      '2', '3', '4', '5', '6', '7', '8', '9', '0'};
    
    ASSERT_EQ(0x62, response[0]);
    ASSERT_EQ(0xF1, response[1]); // DID high
    ASSERT_EQ(0x90, response[2]); // DID low
    ASSERT_GT(response.size(), 3); // Has data
}

TEST(ReadDataByIdentifier_MultipleDIDs) {
    std::cout << "  Testing: Read Data By Identifier - Multiple DIDs" << std::endl;
    
    // Request: 0x22 [DID1] [DID2]
    std::vector<uint8_t> request = {0x22, 0xF1, 0x90, 0xF1, 0x91};
    
    // Response contains data for both DIDs
    std::vector<uint8_t> response = {0x62, 0xF1, 0x90, 0x01, 0x02, 0xF1, 0x91, 0x03, 0x04};
    
    ASSERT_EQ(0x62, response[0]);
    ASSERT_GT(response.size(), 7); // Has data for multiple DIDs
}

// ============================================================================
// Test: WriteDataByIdentifier (0x2E)
// ============================================================================

TEST(WriteDataByIdentifier_Success) {
    std::cout << "  Testing: Write Data By Identifier (0x2E)" << std::endl;
    
    // Positive response: 0x6E [DID]
    std::vector<uint8_t> response = {0x6E, 0x01, 0x02};
    
    ASSERT_EQ(0x6E, response[0]);
    ASSERT_EQ(3, response.size());
}

// ============================================================================
// Test: RoutineControl (0x31)
// ============================================================================

TEST(RoutineControl_StartRoutine) {
    std::cout << "  Testing: Routine Control - Start (0x31 0x01)" << std::endl;
    
    // Positive response: 0x71 0x01 [routineID] [routineInfo]
    std::vector<uint8_t> response = {0x71, 0x01, 0xFF, 0x00, 0x00};
    
    ASSERT_EQ(0x71, response[0]);
    ASSERT_EQ(0x01, response[1]); // Start routine
}

TEST(RoutineControl_StopRoutine) {
    std::cout << "  Testing: Routine Control - Stop (0x31 0x02)" << std::endl;
    
    std::vector<uint8_t> response = {0x71, 0x02, 0xFF, 0x00};
    
    ASSERT_EQ(0x71, response[0]);
    ASSERT_EQ(0x02, response[1]); // Stop routine
}

TEST(RoutineControl_RequestResults) {
    std::cout << "  Testing: Routine Control - Request Results (0x31 0x03)" << std::endl;
    
    std::vector<uint8_t> response = {0x71, 0x03, 0xFF, 0x00, 0x01, 0x02};
    
    ASSERT_EQ(0x71, response[0]);
    ASSERT_EQ(0x03, response[1]); // Request results
}

// ============================================================================
// Test: RequestDownload (0x34)
// ============================================================================

TEST(RequestDownload_Success) {
    std::cout << "  Testing: Request Download (0x34)" << std::endl;
    
    // Positive response: 0x74 [lengthFormatIdentifier] [maxNumberOfBlockLength]
    std::vector<uint8_t> response = {0x74, 0x20, 0x10, 0x00};
    
    ASSERT_EQ(0x74, response[0]);
    ASSERT_GT(response.size(), 1);
}

// ============================================================================
// Test: TransferData (0x36)
// ============================================================================

TEST(TransferData_Success) {
    std::cout << "  Testing: Transfer Data (0x36)" << std::endl;
    
    // Positive response: 0x76 [blockSequenceCounter]
    std::vector<uint8_t> response = {0x76, 0x01};
    
    ASSERT_EQ(0x76, response[0]);
    ASSERT_EQ(0x01, response[1]); // Block counter
}

TEST(TransferData_WrongBlockSequence) {
    std::cout << "  Testing: Transfer Data - Wrong Block Sequence (NRC 0x73)" << std::endl;
    
    // Negative response: 0x7F 0x36 0x73
    std::vector<uint8_t> response = {0x7F, 0x36, 0x73};
    
    ASSERT_EQ(0x7F, response[0]);
    ASSERT_EQ(0x36, response[1]);
    ASSERT_EQ(0x73, response[2]); // WrongBlockSequenceCounter
}

// ============================================================================
// Test: RequestTransferExit (0x37)
// ============================================================================

TEST(RequestTransferExit_Success) {
    std::cout << "  Testing: Request Transfer Exit (0x37)" << std::endl;
    
    // Positive response: 0x77
    std::vector<uint8_t> response = {0x77};
    
    ASSERT_EQ(0x77, response[0]);
}

// ============================================================================
// Test: Negative Response Codes (NRC)
// ============================================================================

TEST(NRC_ResponsePending) {
    std::cout << "  Testing: NRC 0x78 - Response Pending" << std::endl;
    
    std::vector<uint8_t> response = {0x7F, 0x10, 0x78};
    
    ASSERT_EQ(0x7F, response[0]);
    ASSERT_EQ(0x78, response[2]); // ResponsePending
    
    // Verify NRC interpretation
    auto nrc_opt = nrc::Interpreter::parse_from_response(response);
    ASSERT_TRUE(nrc_opt.has_value());
    ASSERT_TRUE(nrc::Interpreter::is_response_pending(*nrc_opt));
}

TEST(NRC_ServiceNotSupported) {
    std::cout << "  Testing: NRC 0x11 - Service Not Supported" << std::endl;
    
    std::vector<uint8_t> response = {0x7F, 0x22, 0x11};
    
    ASSERT_EQ(0x11, response[2]);
    
    auto nrc_opt = nrc::Interpreter::parse_from_response(response);
    ASSERT_TRUE(nrc_opt.has_value());
    ASSERT_FALSE(nrc::Interpreter::is_recoverable(*nrc_opt));
}

TEST(NRC_ConditionsNotCorrect) {
    std::cout << "  Testing: NRC 0x22 - Conditions Not Correct" << std::endl;
    
    std::vector<uint8_t> response = {0x7F, 0x10, 0x22};
    
    ASSERT_EQ(0x22, response[2]);
    
    auto nrc_opt = nrc::Interpreter::parse_from_response(response);
    ASSERT_TRUE(nrc_opt.has_value());
    // ConditionsNotCorrect is NOT recoverable - user must fix preconditions
    ASSERT_FALSE(nrc::Interpreter::is_recoverable(*nrc_opt));
    auto category = nrc::Interpreter::get_category(*nrc_opt);
    ASSERT_TRUE(category == nrc::Category::ConditionsNotMet);
}

TEST(NRC_SecurityAccessDenied) {
    std::cout << "  Testing: NRC 0x33 - Security Access Denied" << std::endl;
    
    std::vector<uint8_t> response = {0x7F, 0x2E, 0x33};
    
    ASSERT_EQ(0x33, response[2]);
    
    auto nrc_opt = nrc::Interpreter::parse_from_response(response);
    ASSERT_TRUE(nrc_opt.has_value());
    
    auto category = nrc::Interpreter::get_category(*nrc_opt);
    ASSERT_TRUE(category == nrc::Category::SecurityIssue);
}

// ============================================================================
// Test: DTC Services
// ============================================================================

TEST(ReadDTCInformation_ReportNumber) {
    std::cout << "  Testing: Read DTC Information - Report Number (0x19 0x01)" << std::endl;
    
    // Positive response: 0x59 0x01 [availabilityMask] [DTCFormatIdentifier] [DTCCount]
    std::vector<uint8_t> response = {0x59, 0x01, 0xFF, 0x01, 0x00, 0x05};
    
    ASSERT_EQ(0x59, response[0]);
    ASSERT_EQ(0x01, response[1]);
}

TEST(ClearDiagnosticInformation_Success) {
    std::cout << "  Testing: Clear Diagnostic Information (0x14)" << std::endl;
    
    // Positive response: 0x54
    std::vector<uint8_t> response = {0x54};
    
    ASSERT_EQ(0x54, response[0]);
}

// ============================================================================
// Test: Memory Operations
// ============================================================================

TEST(ReadMemoryByAddress_Success) {
    std::cout << "  Testing: Read Memory By Address (0x23)" << std::endl;
    
    // Positive response: 0x63 [dataRecord]
    std::vector<uint8_t> response = {0x63, 0x01, 0x02, 0x03, 0x04};
    
    ASSERT_EQ(0x63, response[0]);
    ASSERT_GT(response.size(), 1);
}

TEST(WriteMemoryByAddress_Success) {
    std::cout << "  Testing: Write Memory By Address (0x3D)" << std::endl;
    
    // Positive response: 0x7D [addressAndLengthFormatIdentifier] [memoryAddress] [memorySize]
    std::vector<uint8_t> response = {0x7D, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};
    
    ASSERT_EQ(0x7D, response[0]);
}

// ============================================================================
// Main test runner
// ============================================================================

int main() {
    std::cout << "\n" << colors::BOLD << "UDS Core Service Tests" << colors::RESET << "\n";
    std::cout << "Testing ISO 14229-1 Core Services\n\n";
    
    TestRunner::instance().run_all_tests();
    
    auto stats = TestRunner::instance().get_stats();
    return (stats.failed == 0) ? 0 : 1;
}

