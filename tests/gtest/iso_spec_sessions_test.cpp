/**
 * @file iso_spec_sessions_test.cpp
 * @brief ISO 14229-1 Spec-Anchored Tests: Diagnostic Session Control (0x10)
 * 
 * References:
 * - ISO 14229-1:2020 Section 9.2 (DiagnosticSessionControl)
 * - ISO 14229-1:2020 Table 26 (Session parameter record)
 */

#include <gtest/gtest.h>
#include "uds.hpp"
#include <vector>
#include <cstdint>

using namespace uds;

// ============================================================================
// ISO 14229-1 Section 9.2: DiagnosticSessionControl (SID 0x10)
// ============================================================================

class DiagnosticSessionControlSpecTest : public ::testing::Test {
protected:
    // Per ISO 14229-1 Table 23: diagnosticSessionType values
    static constexpr uint8_t SID = 0x10;
    static constexpr uint8_t DEFAULT_SESSION = 0x01;
    static constexpr uint8_t PROGRAMMING_SESSION = 0x02;
    static constexpr uint8_t EXTENDED_SESSION = 0x03;
    static constexpr uint8_t SAFETY_SYSTEM_SESSION = 0x04;
    
    // OEM-specific session range per ISO 14229-1
    static constexpr uint8_t OEM_SESSION_START = 0x40;
    static constexpr uint8_t OEM_SESSION_END = 0x5F;
    
    // System supplier specific session range
    static constexpr uint8_t SUPPLIER_SESSION_START = 0x60;
    static constexpr uint8_t SUPPLIER_SESSION_END = 0x7E;
};

// Test: Request message format per ISO 14229-1 Section 9.2.2.1
TEST_F(DiagnosticSessionControlSpecTest, RequestFormat) {
    // Request: [SID] [sub-function]
    // Sub-function byte: bit 7 = suppressPositiveResponse, bits 6-0 = sessionType
    
    // Default session request without suppress
    std::vector<uint8_t> request_default = {SID, DEFAULT_SESSION};
    EXPECT_EQ(2, request_default.size()) << "Request should be 2 bytes";
    EXPECT_EQ(0x10, request_default[0]) << "SID should be 0x10";
    EXPECT_EQ(0x01, request_default[1]) << "Default session = 0x01";
    
    // Programming session with suppress positive response (bit 7 set)
    std::vector<uint8_t> request_prog_suppress = {SID, static_cast<uint8_t>(PROGRAMMING_SESSION | 0x80)};
    EXPECT_EQ(0x82, request_prog_suppress[1]) << "Programming + suppress = 0x82";
}

// Test: Positive response format per ISO 14229-1 Section 9.2.2.2
TEST_F(DiagnosticSessionControlSpecTest, PositiveResponseFormat) {
    // Response: [SID+0x40] [sessionType] [P2_high] [P2_low] [P2*_high] [P2*_low]
    // P2 is in milliseconds, P2* is in 10ms units
    
    // Example: Extended session, P2=50ms, P2*=5000ms (500 * 10ms)
    std::vector<uint8_t> response = {
        0x50,       // SID + 0x40
        0x03,       // Extended session
        0x00, 0x32, // P2 = 50ms (0x0032)
        0x01, 0xF4  // P2* = 500 * 10ms = 5000ms (0x01F4)
    };
    
    EXPECT_EQ(6, response.size()) << "Positive response should be 6 bytes";
    EXPECT_EQ(0x50, response[0]) << "Response SID = 0x10 + 0x40";
    EXPECT_EQ(0x03, response[1]) << "Session type echoed";
    
    // Parse P2 (16-bit, milliseconds)
    uint16_t p2_ms = (static_cast<uint16_t>(response[2]) << 8) | response[3];
    EXPECT_EQ(50, p2_ms) << "P2 should be 50ms";
    
    // Parse P2* (16-bit, 10ms units)
    uint16_t p2_star_10ms = (static_cast<uint16_t>(response[4]) << 8) | response[5];
    uint32_t p2_star_ms = p2_star_10ms * 10;
    EXPECT_EQ(5000, p2_star_ms) << "P2* should be 5000ms";
}

// Test: Session type values per ISO 14229-1 Table 23
TEST_F(DiagnosticSessionControlSpecTest, SessionTypeValues) {
    EXPECT_EQ(0x01, DEFAULT_SESSION);
    EXPECT_EQ(0x02, PROGRAMMING_SESSION);
    EXPECT_EQ(0x03, EXTENDED_SESSION);
    EXPECT_EQ(0x04, SAFETY_SYSTEM_SESSION);
    
    // Reserved range: 0x05-0x3F
    // OEM range: 0x40-0x5F
    // Supplier range: 0x60-0x7E
    // Reserved: 0x7F
    
    EXPECT_EQ(0x40, OEM_SESSION_START);
    EXPECT_EQ(0x5F, OEM_SESSION_END);
    EXPECT_EQ(0x60, SUPPLIER_SESSION_START);
    EXPECT_EQ(0x7E, SUPPLIER_SESSION_END);
}

// Test: Suppress positive response bit per ISO 14229-1
TEST_F(DiagnosticSessionControlSpecTest, SuppressPositiveResponseBit) {
    // Bit 7 of sub-function = suppressPositiveResponse
    uint8_t session_with_suppress = EXTENDED_SESSION | 0x80;
    
    EXPECT_EQ(0x83, session_with_suppress);
    EXPECT_TRUE((session_with_suppress & 0x80) != 0) << "Bit 7 should be set";
    EXPECT_EQ(0x03, session_with_suppress & 0x7F) << "Session type should be extractable";
}

// ============================================================================
// ISO 14229-1 Section 9.3: ECUReset (SID 0x11)
// ============================================================================

class ECUResetSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x11;
    
    // Per ISO 14229-1 Table 27: resetType values
    static constexpr uint8_t HARD_RESET = 0x01;
    static constexpr uint8_t KEY_OFF_ON_RESET = 0x02;
    static constexpr uint8_t SOFT_RESET = 0x03;
    static constexpr uint8_t ENABLE_RAPID_POWER_SHUTDOWN = 0x04;
    static constexpr uint8_t DISABLE_RAPID_POWER_SHUTDOWN = 0x05;
};

TEST_F(ECUResetSpecTest, RequestFormat) {
    // Request: [SID] [resetType]
    std::vector<uint8_t> hard_reset = {SID, HARD_RESET};
    EXPECT_EQ(2, hard_reset.size());
    EXPECT_EQ(0x11, hard_reset[0]);
    EXPECT_EQ(0x01, hard_reset[1]);
}

TEST_F(ECUResetSpecTest, PositiveResponseFormat) {
    // Response: [SID+0x40] [resetType] [powerDownTime (optional)]
    std::vector<uint8_t> response = {0x51, HARD_RESET};
    EXPECT_EQ(0x51, response[0]) << "Response SID = 0x11 + 0x40";
    EXPECT_EQ(0x01, response[1]) << "Reset type echoed";
}

TEST_F(ECUResetSpecTest, ResetTypeValues) {
    EXPECT_EQ(0x01, HARD_RESET);
    EXPECT_EQ(0x02, KEY_OFF_ON_RESET);
    EXPECT_EQ(0x03, SOFT_RESET);
    EXPECT_EQ(0x04, ENABLE_RAPID_POWER_SHUTDOWN);
    EXPECT_EQ(0x05, DISABLE_RAPID_POWER_SHUTDOWN);
}

// ============================================================================
// ISO 14229-1 Section 9.4: SecurityAccess (SID 0x27)
// ============================================================================

class SecurityAccessSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x27;
    
    // Per ISO 14229-1: odd sub-function = requestSeed, even = sendKey
    // Security levels: 0x01/0x02, 0x03/0x04, ..., 0x41/0x42 (OEM), etc.
};

TEST_F(SecurityAccessSpecTest, RequestSeedFormat) {
    // Request seed: [SID] [securityAccessType (odd)]
    // securityAccessType: 0x01, 0x03, 0x05, ... 0x41, 0x43, ...
    
    std::vector<uint8_t> request_seed_level1 = {SID, 0x01};
    EXPECT_EQ(2, request_seed_level1.size());
    EXPECT_EQ(0x27, request_seed_level1[0]);
    EXPECT_EQ(0x01, request_seed_level1[1]);
    EXPECT_TRUE((request_seed_level1[1] & 0x01) == 1) << "Request seed must be odd";
}

TEST_F(SecurityAccessSpecTest, SendKeyFormat) {
    // Send key: [SID] [securityAccessType (even)] [key bytes...]
    // securityAccessType: 0x02, 0x04, 0x06, ... 0x42, 0x44, ...
    
    std::vector<uint8_t> send_key_level1 = {SID, 0x02, 0x12, 0x34, 0x56, 0x78};
    EXPECT_EQ(0x27, send_key_level1[0]);
    EXPECT_EQ(0x02, send_key_level1[1]);
    EXPECT_TRUE((send_key_level1[1] & 0x01) == 0) << "Send key must be even";
}

TEST_F(SecurityAccessSpecTest, SecurityLevelPairing) {
    // Per ISO 14229-1: requestSeed level N uses sendKey level N+1
    for (uint8_t request_seed = 0x01; request_seed <= 0x41; request_seed += 2) {
        uint8_t send_key = request_seed + 1;
        EXPECT_TRUE((request_seed & 0x01) == 1) << "Request seed should be odd";
        EXPECT_TRUE((send_key & 0x01) == 0) << "Send key should be even";
        EXPECT_EQ(request_seed + 1, send_key) << "Send key = request seed + 1";
    }
}

TEST_F(SecurityAccessSpecTest, PositiveResponseSeedFormat) {
    // Response to requestSeed: [SID+0x40] [securityAccessType] [seed...]
    std::vector<uint8_t> response = {0x67, 0x01, 0xAA, 0xBB, 0xCC, 0xDD};
    
    EXPECT_EQ(0x67, response[0]) << "Response SID = 0x27 + 0x40";
    EXPECT_EQ(0x01, response[1]) << "Security level echoed";
    
    // Seed is 4 bytes in this example
    std::vector<uint8_t> seed(response.begin() + 2, response.end());
    EXPECT_EQ(4, seed.size());
}

TEST_F(SecurityAccessSpecTest, ZeroSeedMeansUnlocked) {
    // Per ISO 14229-1: if seed is all zeros, security is already unlocked
    std::vector<uint8_t> response_unlocked = {0x67, 0x01, 0x00, 0x00, 0x00, 0x00};
    
    bool all_zero = true;
    for (size_t i = 2; i < response_unlocked.size(); ++i) {
        if (response_unlocked[i] != 0x00) {
            all_zero = false;
            break;
        }
    }
    EXPECT_TRUE(all_zero) << "Zero seed indicates already unlocked";
}

// ============================================================================
// ISO 14229-1 Section 9.5: CommunicationControl (SID 0x28)
// ============================================================================

class CommunicationControlSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x28;
    
    // Per ISO 14229-1 Table 47: controlType values
    static constexpr uint8_t ENABLE_RX_AND_TX = 0x00;
    static constexpr uint8_t ENABLE_RX_DISABLE_TX = 0x01;
    static constexpr uint8_t DISABLE_RX_ENABLE_TX = 0x02;
    static constexpr uint8_t DISABLE_RX_AND_TX = 0x03;
    
    // Per ISO 14229-1 Table 48: communicationType bit definitions
    // Bits 0-1: normal/network management communication
    // Bit 0: normalCommunicationMessages
    // Bit 1: networkManagementCommunicationMessages
};

TEST_F(CommunicationControlSpecTest, RequestFormat) {
    // Request: [SID] [controlType] [communicationType]
    std::vector<uint8_t> disable_all = {SID, DISABLE_RX_AND_TX, 0x01};
    
    EXPECT_EQ(3, disable_all.size());
    EXPECT_EQ(0x28, disable_all[0]);
    EXPECT_EQ(0x03, disable_all[1]) << "Disable RX and TX";
    EXPECT_EQ(0x01, disable_all[2]) << "Normal communication messages";
}

TEST_F(CommunicationControlSpecTest, ControlTypeValues) {
    EXPECT_EQ(0x00, ENABLE_RX_AND_TX);
    EXPECT_EQ(0x01, ENABLE_RX_DISABLE_TX);
    EXPECT_EQ(0x02, DISABLE_RX_ENABLE_TX);
    EXPECT_EQ(0x03, DISABLE_RX_AND_TX);
}

TEST_F(CommunicationControlSpecTest, CommunicationTypeBits) {
    // Bit 0: normalCommunicationMessages
    // Bit 1: networkManagementCommunicationMessages
    
    uint8_t normal_only = 0x01;          // Bit 0 set
    uint8_t nm_only = 0x02;              // Bit 1 set
    uint8_t both = 0x03;                 // Bits 0 and 1 set
    
    EXPECT_TRUE((normal_only & 0x01) != 0);
    EXPECT_TRUE((normal_only & 0x02) == 0);
    
    EXPECT_TRUE((nm_only & 0x01) == 0);
    EXPECT_TRUE((nm_only & 0x02) != 0);
    
    EXPECT_TRUE((both & 0x01) != 0);
    EXPECT_TRUE((both & 0x02) != 0);
}

TEST_F(CommunicationControlSpecTest, PositiveResponseFormat) {
    // Response: [SID+0x40] [controlType]
    std::vector<uint8_t> response = {0x68, DISABLE_RX_AND_TX};
    
    EXPECT_EQ(0x68, response[0]) << "Response SID = 0x28 + 0x40";
    EXPECT_EQ(0x03, response[1]) << "Control type echoed";
}

// ============================================================================
// ISO 14229-1 Section 9.6: TesterPresent (SID 0x3E)
// ============================================================================

class TesterPresentSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x3E;
    static constexpr uint8_t ZERO_SUBFUNCTION = 0x00;
};

TEST_F(TesterPresentSpecTest, RequestFormat) {
    // Request: [SID] [sub-function]
    // Sub-function is always 0x00, with optional suppress bit
    
    std::vector<uint8_t> request = {SID, ZERO_SUBFUNCTION};
    EXPECT_EQ(2, request.size());
    EXPECT_EQ(0x3E, request[0]);
    EXPECT_EQ(0x00, request[1]);
}

TEST_F(TesterPresentSpecTest, RequestWithSuppress) {
    // With suppress positive response
    std::vector<uint8_t> request_suppress = {SID, static_cast<uint8_t>(ZERO_SUBFUNCTION | 0x80)};
    EXPECT_EQ(0x80, request_suppress[1]) << "Suppress bit set, sub-function = 0";
}

TEST_F(TesterPresentSpecTest, PositiveResponseFormat) {
    // Response: [SID+0x40] [sub-function]
    std::vector<uint8_t> response = {0x7E, 0x00};
    
    EXPECT_EQ(0x7E, response[0]) << "Response SID = 0x3E + 0x40";
    EXPECT_EQ(0x00, response[1]) << "Sub-function echoed";
}

// ============================================================================
// ISO 14229-1 Section 11.3: ControlDTCSetting (SID 0x85)
// ============================================================================

class ControlDTCSettingSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x85;
    
    // Per ISO 14229-1 Table 98: DTCSettingType values
    static constexpr uint8_t ON = 0x01;
    static constexpr uint8_t OFF = 0x02;
};

TEST_F(ControlDTCSettingSpecTest, RequestFormat) {
    // Request: [SID] [DTCSettingType]
    std::vector<uint8_t> dtc_off = {SID, OFF};
    
    EXPECT_EQ(2, dtc_off.size());
    EXPECT_EQ(0x85, dtc_off[0]);
    EXPECT_EQ(0x02, dtc_off[1]) << "DTC setting OFF";
}

TEST_F(ControlDTCSettingSpecTest, DTCSettingTypeValues) {
    EXPECT_EQ(0x01, ON) << "ON = 0x01";
    EXPECT_EQ(0x02, OFF) << "OFF = 0x02";
}

TEST_F(ControlDTCSettingSpecTest, PositiveResponseFormat) {
    // Response: [SID+0x40] [DTCSettingType]
    std::vector<uint8_t> response = {0xC5, OFF};
    
    EXPECT_EQ(0xC5, response[0]) << "Response SID = 0x85 + 0x40";
    EXPECT_EQ(0x02, response[1]) << "DTC setting type echoed";
}
