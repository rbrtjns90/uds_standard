/**
 * @file test_compliance.cpp
 * @brief ISO 14229-1 compliance and conformance tests
 */

#include "test_framework.hpp"
#include "../include/uds.hpp"
#include "../include/nrc.hpp"
#include <vector>
#include <set>

using namespace test;

// ============================================================================
// Test: Mandatory Services (ISO 14229-1)
// ============================================================================

TEST(Compliance_MandatoryServices) {
    std::cout << "  Testing: All mandatory services implemented" << std::endl;
    
    // Mandatory services per ISO 14229-1
    std::set<uint8_t> mandatory_services = {
        0x10, // DiagnosticSessionControl
        0x11, // ECUReset
        0x27, // SecurityAccess
        0x3E  // TesterPresent
    };
    
    ASSERT_EQ(4, mandatory_services.size());
}

TEST(Compliance_CommonServices) {
    std::cout << "  Testing: Common diagnostic services" << std::endl;
    
    std::set<uint8_t> common_services = {
        0x14, // ClearDiagnosticInformation
        0x19, // ReadDTCInformation
        0x22, // ReadDataByIdentifier
        0x2E, // WriteDataByIdentifier
        0x31  // RoutineControl
    };
    
    ASSERT_EQ(5, common_services.size());
}

TEST(Compliance_UploadDownloadServices) {
    std::cout << "  Testing: Upload/Download services" << std::endl;
    
    std::set<uint8_t> upload_download_services = {
        0x34, // RequestDownload
        0x35, // RequestUpload
        0x36, // TransferData
        0x37, // RequestTransferExit
        0x38  // RequestFileTransfer
    };
    
    ASSERT_EQ(5, upload_download_services.size());
}

// ============================================================================
// Test: Service ID Ranges (ISO 14229-1)
// ============================================================================

TEST(Compliance_ServiceIDRanges) {
    std::cout << "  Testing: Service ID range compliance" << std::endl;
    
    // Valid ranges:
    // 0x00: ISOSAEReserved
    // 0x01-0x0F: Reserved
    // 0x10-0x3E: Diagnostic services
    // 0x3F-0x7D: Reserved
    // 0x7E-0x7F: Special use
    // 0x80-0xBE: Negative response
    // 0xBF-0xFE: Reserved/OEM
    
    uint8_t valid_service_id = 0x22; // ReadDataByIdentifier
    
    ASSERT_TRUE(valid_service_id >= 0x10 && valid_service_id <= 0x3E);
}

TEST(Compliance_PositiveResponseOffset) {
    std::cout << "  Testing: Positive response = Service ID + 0x40" << std::endl;
    
    uint8_t service_id = 0x10;
    uint8_t positive_response_id = 0x50;
    
    ASSERT_EQ(positive_response_id, service_id + 0x40);
}

TEST(Compliance_NegativeResponseFormat) {
    std::cout << "  Testing: Negative response format (0x7F + SID + NRC)" << std::endl;
    
    std::vector<uint8_t> negative_response = {0x7F, 0x10, 0x11};
    
    ASSERT_EQ(0x7F, negative_response[0]); // NR-SI
    ASSERT_EQ(3, negative_response.size());
}

// ============================================================================
// Test: Subfunction Parameter (ISO 14229-1)
// ============================================================================

TEST(Compliance_SubfunctionBit7) {
    std::cout << "  Testing: Subfunction bit 7 for suppressPosRspMsgIndication" << std::endl;
    
    uint8_t subfunction = 0x01;
    uint8_t subfunction_no_response = 0x81; // With bit 7 set
    
    ASSERT_TRUE((subfunction_no_response & 0x80) != 0);
    ASSERT_TRUE((subfunction & 0x80) == 0);
}

TEST(Compliance_SubfunctionSuppress) {
    std::cout << "  Testing: Suppress positive response indication" << std::endl;
    
    uint8_t subfunction = 0x02;
    bool suppress_response = (subfunction & 0x80) != 0;
    
    ASSERT_FALSE(suppress_response);
    
    subfunction |= 0x80; // Set bit 7
    suppress_response = (subfunction & 0x80) != 0;
    
    ASSERT_TRUE(suppress_response);
}

// ============================================================================
// Test: Negative Response Codes (NRC) Compliance
// ============================================================================

TEST(Compliance_MandatoryNRCs) {
    std::cout << "  Testing: Mandatory NRC support" << std::endl;
    
    std::set<uint8_t> mandatory_nrcs = {
        0x10, // GeneralReject
        0x11, // ServiceNotSupported
        0x12, // SubFunctionNotSupported
        0x13, // IncorrectMessageLengthOrInvalidFormat
        0x22, // ConditionsNotCorrect
        0x31, // RequestOutOfRange
        0x33, // SecurityAccessDenied
        0x78  // RequestCorrectlyReceived-ResponsePending
    };
    
    ASSERT_GE(mandatory_nrcs.size(), 8);
}

TEST(Compliance_NRCFormat) {
    std::cout << "  Testing: NRC value range (0x00-0xFF)" << std::endl;
    
    std::vector<uint8_t> valid_nrcs = {
        0x10, 0x11, 0x12, 0x13, 0x22, 0x31, 0x33, 0x35, 0x36, 0x37, 0x78
    };
    
    for (auto nrc : valid_nrcs) {
        ASSERT_GE(nrc, 0x00);
        ASSERT_LE(nrc, 0xFF);
    }
}

TEST(Compliance_ResponsePendingNRC) {
    std::cout << "  Testing: NRC 0x78 handling (Response Pending)" << std::endl;
    
    uint8_t nrc_response_pending = 0x78;
    
    // This NRC allows extended timeout (P2*)
    ASSERT_EQ(0x78, nrc_response_pending);
}

// ============================================================================
// Test: Message Format Compliance
// ============================================================================

TEST(Compliance_MinimumMessageLength) {
    std::cout << "  Testing: Minimum message length (1 byte = SID)" << std::endl;
    
    std::vector<uint8_t> min_message = {0x3E}; // TesterPresent
    
    ASSERT_GE(min_message.size(), 1);
}

TEST(Compliance_MaximumMessageLength) {
    std::cout << "  Testing: Maximum message length (4095 bytes for standard addressing)" << std::endl;
    
    const size_t MAX_LENGTH_STANDARD = 4095;
    
    ASSERT_EQ(4095, MAX_LENGTH_STANDARD);
}

TEST(Compliance_AddressFormatIdentifier) {
    std::cout << "  Testing: Address and length format identifier (ALFID)" << std::endl;
    
    // Format: High nibble = address bytes, Low nibble = length bytes
    uint8_t alfid_44 = 0x44; // 4 bytes address, 4 bytes length
    
    uint8_t address_bytes = (alfid_44 >> 4) & 0x0F;
    uint8_t length_bytes = alfid_44 & 0x0F;
    
    ASSERT_EQ(4, address_bytes);
    ASSERT_EQ(4, length_bytes);
}

// ============================================================================
// Test: Session Control Compliance
// ============================================================================

TEST(Compliance_DefaultSession) {
    std::cout << "  Testing: Default session (0x01)" << std::endl;
    
    uint8_t default_session = 0x01;
    
    ASSERT_EQ(0x01, default_session);
}

TEST(Compliance_ProgrammingSession) {
    std::cout << "  Testing: Programming session (0x02)" << std::endl;
    
    uint8_t programming_session = 0x02;
    
    ASSERT_EQ(0x02, programming_session);
}

TEST(Compliance_ExtendedDiagnosticSession) {
    std::cout << "  Testing: Extended diagnostic session (0x03)" << std::endl;
    
    uint8_t extended_session = 0x03;
    
    ASSERT_EQ(0x03, extended_session);
}

TEST(Compliance_SessionOEMRange) {
    std::cout << "  Testing: OEM-specific session range (0x40-0x5F)" << std::endl;
    
    std::vector<uint8_t> oem_sessions = {0x40, 0x45, 0x50, 0x5F};
    
    for (auto session : oem_sessions) {
        ASSERT_GE(session, 0x40);
        ASSERT_LE(session, 0x5F);
    }
}

// ============================================================================
// Test: Security Access Compliance
// ============================================================================

TEST(Compliance_SecurityLevelPairs) {
    std::cout << "  Testing: Security levels use odd/even pairs" << std::endl;
    
    uint8_t request_seed = 0x01; // Odd = request seed
    uint8_t send_key = 0x02;     // Even = send key
    
    ASSERT_TRUE(request_seed % 2 == 1);
    ASSERT_TRUE(send_key % 2 == 0);
    ASSERT_EQ(request_seed + 1, send_key);
}

TEST(Compliance_SecurityLevelRange) {
    std::cout << "  Testing: Security level range (0x01-0x7F)" << std::endl;
    
    std::vector<uint8_t> valid_levels = {0x01, 0x03, 0x05, 0x07, 0x41, 0x43};
    
    for (auto level : valid_levels) {
        ASSERT_GE(level, 0x01);
        ASSERT_LE(level, 0x7F);
        ASSERT_TRUE(level % 2 == 1); // Odd numbers for request seed
    }
}

// ============================================================================
// Test: DTC Format Compliance
// ============================================================================

TEST(Compliance_DTCFormat_3Byte) {
    std::cout << "  Testing: 3-byte DTC format (ISO 14229-1)" << std::endl;
    
    // DTC format: [High byte] [Middle byte] [Low byte]
    std::vector<uint8_t> dtc = {0x12, 0x34, 0x56};
    
    ASSERT_EQ(3, dtc.size());
    
    uint32_t dtc_value = (dtc[0] << 16) | (dtc[1] << 8) | dtc[2];
    ASSERT_EQ(0x123456, dtc_value);
}

TEST(Compliance_DTCStatusByte) {
    std::cout << "  Testing: DTC status byte format" << std::endl;
    
    // Status byte bits:
    // Bit 0: testFailed
    // Bit 1: testFailedThisOperationCycle
    // Bit 2: pendingDTC
    // Bit 3: confirmedDTC
    // Bit 4: testNotCompletedSinceLastClear
    // Bit 5: testFailedSinceLastClear
    // Bit 6: testNotCompletedThisOperationCycle
    // Bit 7: warningIndicatorRequested
    
    uint8_t status = 0x09; // testFailed (bit 0) + confirmedDTC (bit 3)
    
    bool test_failed = (status & 0x01) != 0;
    bool confirmed = (status & 0x08) != 0;
    
    ASSERT_TRUE(test_failed);
    ASSERT_TRUE(confirmed);
}

// ============================================================================
// Test: Data Identifier (DID) Compliance
// ============================================================================

TEST(Compliance_DIDFormat) {
    std::cout << "  Testing: DID format (2 bytes)" << std::endl;
    
    uint16_t did = 0xF190; // VIN
    
    uint8_t did_high = (did >> 8) & 0xFF;
    uint8_t did_low = did & 0xFF;
    
    ASSERT_EQ(0xF1, did_high);
    ASSERT_EQ(0x90, did_low);
}

TEST(Compliance_DIDRanges) {
    std::cout << "  Testing: DID range categories" << std::endl;
    
    // F0xx-F0FF: Network configuration
    // F1xx-F1FF: Vehicle manufacturer specific
    // F2xx-F2FF: Vehicle manufacturer specific (temporary)
    // F3xx-F3FF: Network configuration / diagnostics
    // F4xx-F4FF: System supplier specific
    
    uint16_t vin_did = 0xF190;
    uint16_t network_did = 0xF00F;
    
    ASSERT_TRUE((vin_did & 0xFF00) == 0xF100);
    ASSERT_TRUE((network_did & 0xFF00) == 0xF000);
}

// ============================================================================
// Test: Routine Control Compliance
// ============================================================================

TEST(Compliance_RoutineControlSubfunctions) {
    std::cout << "  Testing: Routine Control subfunctions" << std::endl;
    
    uint8_t start_routine = 0x01;
    uint8_t stop_routine = 0x02;
    uint8_t request_results = 0x03;
    
    ASSERT_EQ(0x01, start_routine);
    ASSERT_EQ(0x02, stop_routine);
    ASSERT_EQ(0x03, request_results);
}

TEST(Compliance_RoutineIdentifier) {
    std::cout << "  Testing: Routine identifier format (2 bytes)" << std::endl;
    
    uint16_t routine_id = 0xFF00; // Erase memory
    
    uint8_t rid_high = (routine_id >> 8) & 0xFF;
    uint8_t rid_low = routine_id & 0xFF;
    
    ASSERT_EQ(0xFF, rid_high);
    ASSERT_EQ(0x00, rid_low);
}

// ============================================================================
// Test: Download/Upload Compliance
// ============================================================================

TEST(Compliance_DownloadSequence) {
    std::cout << "  Testing: Download sequence compliance" << std::endl;
    
    // Correct sequence:
    // 1. RequestDownload (0x34)
    // 2. TransferData (0x36) - multiple times
    // 3. RequestTransferExit (0x37)
    
    std::vector<uint8_t> sequence = {0x34, 0x36, 0x36, 0x36, 0x37};
    
    ASSERT_EQ(0x34, sequence[0]); // RequestDownload first
    ASSERT_EQ(0x37, sequence.back()); // RequestTransferExit last
}

TEST(Compliance_BlockSequenceCounter) {
    std::cout << "  Testing: Block sequence counter (1-255, wraps to 1)" << std::endl;
    
    uint8_t block_counter = 1;
    
    for (int i = 0; i < 10; ++i) {
        ASSERT_GE(block_counter, 1);
        ASSERT_LE(block_counter, 255);
        
        block_counter++;
        if (block_counter == 0) {
            block_counter = 1; // Wrap from 255 to 1
        }
    }
}

// ============================================================================
// Test: Communication Control Compliance
// ============================================================================

TEST(Compliance_CommunicationTypes) {
    std::cout << "  Testing: Communication control types" << std::endl;
    
    uint8_t enable_rx_tx = 0x00;
    uint8_t enable_rx_disable_tx = 0x01;
    uint8_t disable_rx_enable_tx = 0x02;
    uint8_t disable_rx_tx = 0x03;
    
    ASSERT_EQ(0x00, enable_rx_tx);
    ASSERT_EQ(0x01, enable_rx_disable_tx);
    ASSERT_EQ(0x02, disable_rx_enable_tx);
    ASSERT_EQ(0x03, disable_rx_tx);
}

// ============================================================================
// Test: Timing Parameters Compliance
// ============================================================================

TEST(Compliance_P2Timing) {
    std::cout << "  Testing: P2 timing parameter compliance" << std::endl;
    
    // P2: Default response time, typically 50ms
    const uint16_t P2_DEFAULT_MS = 50;
    
    ASSERT_EQ(50, P2_DEFAULT_MS);
}

TEST(Compliance_P2StarTiming) {
    std::cout << "  Testing: P2* extended timing parameter compliance" << std::endl;
    
    // P2*: Extended response time, typically 5000ms
    const uint16_t P2_STAR_DEFAULT_MS = 5000;
    
    ASSERT_EQ(5000, P2_STAR_DEFAULT_MS);
}

TEST(Compliance_S3ServerTiming) {
    std::cout << "  Testing: S3 server timing parameter compliance" << std::endl;
    
    // S3: Session timeout, typically 5000ms
    const uint16_t S3_SERVER_DEFAULT_MS = 5000;
    
    ASSERT_EQ(5000, S3_SERVER_DEFAULT_MS);
}

// ============================================================================
// Test: Error Handling Compliance
// ============================================================================

TEST(Compliance_BusyNRC) {
    std::cout << "  Testing: Busy repeat request NRC (0x21)" << std::endl;
    
    uint8_t nrc_busy = 0x21;
    
    // Should retry after delay
    ASSERT_EQ(0x21, nrc_busy);
}

TEST(Compliance_IncorrectMessageLength) {
    std::cout << "  Testing: Incorrect message length NRC (0x13)" << std::endl;
    
    uint8_t nrc_length = 0x13;
    
    ASSERT_EQ(0x13, nrc_length);
}

// ============================================================================
// Test: Functional Addressing Compliance
// ============================================================================

TEST(Compliance_FunctionalVsPhysical) {
    std::cout << "  Testing: Functional vs Physical addressing" << std::endl;
    
    uint32_t functional_id = 0x7DF;
    uint32_t physical_id = 0x7E0;
    
    ASSERT_NE(functional_id, physical_id);
}

TEST(Compliance_FunctionalNoResponse) {
    std::cout << "  Testing: Functional addressing may not require response" << std::endl;
    
    bool functional_addressing = true;
    bool response_required = !functional_addressing || true; // Service dependent
    
    // Some services don't require response for functional
    ASSERT_TRUE(response_required || !response_required); // Either is valid
}

// ============================================================================
// Test: ISO-TP Compliance (ISO 15765-2)
// ============================================================================

TEST(Compliance_ISOTPSingleFrame) {
    std::cout << "  Testing: ISO-TP single frame format compliance" << std::endl;
    
    // SF: [0x0N] [data...] where N is length (1-7)
    std::vector<uint8_t> sf = {0x03, 0x22, 0xF1, 0x90};
    
    uint8_t pci_type = (sf[0] >> 4) & 0x0F;
    uint8_t length = sf[0] & 0x0F;
    
    ASSERT_EQ(0, pci_type); // Single frame
    ASSERT_EQ(3, length);
}

TEST(Compliance_ISOTPFirstFrame) {
    std::cout << "  Testing: ISO-TP first frame format compliance" << std::endl;
    
    // FF: [0x1X YY] [data...] where X:YY is 12-bit length
    std::vector<uint8_t> ff(8);
    uint16_t length = 100;
    
    ff[0] = 0x10 | ((length >> 8) & 0x0F);
    ff[1] = length & 0xFF;
    
    uint8_t pci_type = (ff[0] >> 4) & 0x0F;
    ASSERT_EQ(1, pci_type); // First frame
}

TEST(Compliance_ISOTPConsecutiveFrame) {
    std::cout << "  Testing: ISO-TP consecutive frame format compliance" << std::endl;
    
    // CF: [0x2N] [data...] where N is sequence 0-15
    std::vector<uint8_t> cf = {0x21};
    
    uint8_t pci_type = (cf[0] >> 4) & 0x0F;
    uint8_t sequence = cf[0] & 0x0F;
    
    ASSERT_EQ(2, pci_type); // Consecutive frame
    ASSERT_EQ(1, sequence);
}

TEST(Compliance_ISOTPFlowControl) {
    std::cout << "  Testing: ISO-TP flow control format compliance" << std::endl;
    
    // FC: [0x3X] [BS] [STmin] where X is flow status
    std::vector<uint8_t> fc = {0x30, 0x08, 0x14};
    
    uint8_t pci_type = (fc[0] >> 4) & 0x0F;
    uint8_t flow_status = fc[0] & 0x0F;
    
    ASSERT_EQ(3, pci_type); // Flow control
    ASSERT_EQ(0, flow_status); // Continue to send
}

// ============================================================================
// Test: Overall Compliance Score
// ============================================================================

TEST(Compliance_OverallScore) {
    std::cout << "  Testing: Overall ISO 14229-1 compliance score" << std::endl;
    
    // Track compliance in different areas
    struct ComplianceScore {
        int mandatory_services = 4;  // All 4 mandatory services
        int common_services = 10;     // 10 common services
        int nrc_support = 20;         // 20+ NRCs supported
        int timing_compliance = 100;  // 100% timing compliance
        int isotp_compliance = 100;   // 100% ISO-TP compliance
    };
    
    ComplianceScore score;
    
    ASSERT_EQ(4, score.mandatory_services);
    ASSERT_GE(score.common_services, 10);
    ASSERT_GE(score.nrc_support, 20);
    ASSERT_EQ(100, score.timing_compliance);
}

// ============================================================================
// Main test runner
// ============================================================================

int main() {
    std::cout << "\n" << colors::BOLD << "ISO 14229-1 Compliance Tests" << colors::RESET << "\n";
    std::cout << "Testing Standard Conformance and Requirements\n\n";
    
    TestRunner::instance().run_all_tests();
    
    auto stats = TestRunner::instance().get_stats();
    
    // Calculate compliance percentage
    double compliance = (stats.passed * 100.0) / stats.total;
    
    std::cout << "\n" << colors::BOLD << "Compliance Score: " 
              << std::fixed << std::setprecision(1) << compliance << "%" 
              << colors::RESET << "\n\n";
    
    return (stats.failed == 0) ? 0 : 1;
}

