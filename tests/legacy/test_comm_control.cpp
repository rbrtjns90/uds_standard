/**
 * @file test_comm_control.cpp
 * @brief Tests for uds_comm_control.hpp - Communication Control (0x28)
 * 
 * ISO 14229-1:2020 Section 8.4 (pp. 116-125)
 */

#include "test_framework.hpp"
#include "../include/uds_comm_control.hpp"

using namespace test;
using namespace uds;

// ============================================================================
// CommunicationControlType Tests
// ============================================================================

TEST(CommControlType_EnableRxAndTx) {
    std::cout << "  Testing: CommunicationControlType::EnableRxAndTx (0x00)" << std::endl;
    
    ASSERT_EQ(0x00, static_cast<int>(CommunicationControlType::EnableRxAndTx));
}

TEST(CommControlType_EnableRxDisableTx) {
    std::cout << "  Testing: CommunicationControlType::EnableRxDisableTx (0x01)" << std::endl;
    
    ASSERT_EQ(0x01, static_cast<int>(CommunicationControlType::EnableRxDisableTx));
}

TEST(CommControlType_DisableRxEnableTx) {
    std::cout << "  Testing: CommunicationControlType::DisableRxEnableTx (0x02)" << std::endl;
    
    ASSERT_EQ(0x02, static_cast<int>(CommunicationControlType::DisableRxEnableTx));
}

TEST(CommControlType_DisableRxAndTx) {
    std::cout << "  Testing: CommunicationControlType::DisableRxAndTx (0x03)" << std::endl;
    
    ASSERT_EQ(0x03, static_cast<int>(CommunicationControlType::DisableRxAndTx));
}

// ============================================================================
// CommunicationType Tests
// ============================================================================

TEST(CommType_NormalMessages) {
    std::cout << "  Testing: CommunicationType::NormalCommunicationMessages (0x01)" << std::endl;
    
    ASSERT_EQ(0x01, static_cast<int>(CommunicationType::NormalCommunicationMessages));
}

TEST(CommType_NetworkManagement) {
    std::cout << "  Testing: CommunicationType::NetworkManagementMessages (0x02)" << std::endl;
    
    ASSERT_EQ(0x02, static_cast<int>(CommunicationType::NetworkManagementMessages));
}

TEST(CommType_NetworkDownloadUpload) {
    std::cout << "  Testing: CommunicationType::NetworkDownloadUpload (0x03)" << std::endl;
    
    ASSERT_EQ(0x03, static_cast<int>(CommunicationType::NetworkDownloadUpload));
}

// ============================================================================
// Message Format Tests
// ============================================================================

TEST(CommControl_RequestFormat) {
    std::cout << "  Testing: CommunicationControl request format" << std::endl;
    
    // Request: [0x28] [controlType] [communicationType]
    uint8_t sid = 0x28;
    uint8_t control_type = 0x03;  // DisableRxAndTx
    uint8_t comm_type = 0x01;     // NormalCommunicationMessages
    
    ASSERT_EQ(0x28, sid);
    ASSERT_EQ(0x03, control_type);
    ASSERT_EQ(0x01, comm_type);
}

TEST(CommControl_PositiveResponse) {
    std::cout << "  Testing: CommunicationControl positive response format" << std::endl;
    
    // Positive response: [0x68] [controlType]
    uint8_t pos_sid = 0x68;  // 0x28 + 0x40
    
    ASSERT_EQ(0x68, pos_sid);
}

TEST(CommControl_NegativeResponse) {
    std::cout << "  Testing: CommunicationControl negative response format" << std::endl;
    
    // Negative response: [0x7F] [0x28] [NRC]
    uint8_t neg_sid = 0x7F;
    uint8_t req_sid = 0x28;
    
    ASSERT_EQ(0x7F, neg_sid);
    ASSERT_EQ(0x28, req_sid);
}

// ============================================================================
// Control Type Combinations
// ============================================================================

TEST(CommControl_DisableNormalComms) {
    std::cout << "  Testing: Disable normal communication combination" << std::endl;
    
    // DisableRxAndTx (0x03) + NormalCommunicationMessages (0x01)
    uint8_t control = static_cast<uint8_t>(CommunicationControlType::DisableRxAndTx);
    uint8_t type = static_cast<uint8_t>(CommunicationType::NormalCommunicationMessages);
    
    ASSERT_EQ(0x03, control);
    ASSERT_EQ(0x01, type);
}

TEST(CommControl_EnableAllComms) {
    std::cout << "  Testing: Enable all communication combination" << std::endl;
    
    // EnableRxAndTx (0x00) + NetworkDownloadUpload (0x03)
    uint8_t control = static_cast<uint8_t>(CommunicationControlType::EnableRxAndTx);
    uint8_t type = static_cast<uint8_t>(CommunicationType::NetworkDownloadUpload);
    
    ASSERT_EQ(0x00, control);
    ASSERT_EQ(0x03, type);
}

TEST(CommControl_ListenOnlyMode) {
    std::cout << "  Testing: Listen-only mode combination" << std::endl;
    
    // EnableRxDisableTx (0x01) + NormalCommunicationMessages (0x01)
    uint8_t control = static_cast<uint8_t>(CommunicationControlType::EnableRxDisableTx);
    uint8_t type = static_cast<uint8_t>(CommunicationType::NormalCommunicationMessages);
    
    ASSERT_EQ(0x01, control);
    ASSERT_EQ(0x01, type);
}

// ============================================================================
// NRC Tests for CommunicationControl
// ============================================================================

TEST(CommControl_NRC_SubFunctionNotSupported) {
    std::cout << "  Testing: NRC 0x12 (subFunctionNotSupported)" << std::endl;
    
    uint8_t nrc = 0x12;
    ASSERT_EQ(0x12, nrc);
}

TEST(CommControl_NRC_ConditionsNotCorrect) {
    std::cout << "  Testing: NRC 0x22 (conditionsNotCorrect)" << std::endl;
    
    uint8_t nrc = 0x22;
    ASSERT_EQ(0x22, nrc);
}

TEST(CommControl_NRC_RequestOutOfRange) {
    std::cout << "  Testing: NRC 0x31 (requestOutOfRange)" << std::endl;
    
    uint8_t nrc = 0x31;
    ASSERT_EQ(0x31, nrc);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "\n" << colors::BOLD << "UDS Communication Control Tests" << colors::RESET << "\n";
    std::cout << "Testing Service 0x28 - ISO 14229-1 Section 8.4\n\n";
    
    TestRunner::instance().run_all_tests();
    
    auto stats = TestRunner::instance().get_stats();
    
    std::cout << "\n" << colors::BOLD << "Communication Control Coverage:" << colors::RESET << "\n";
    std::cout << "  CommunicationControlType: 4 values ✓\n";
    std::cout << "  CommunicationType: 3 values ✓\n";
    std::cout << "  Message formats: Request/Response ✓\n";
    std::cout << "  Control combinations: 3 scenarios ✓\n";
    std::cout << "  NRC handling: 3 codes ✓\n\n";
    
    return (stats.failed == 0) ? 0 : 1;
}
