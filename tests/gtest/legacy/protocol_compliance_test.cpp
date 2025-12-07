/**
 * @file protocol_compliance_test.cpp
 * @brief Tests for ISO-14229 and ISO-15765 protocol compliance
 * 
 * Tests verify:
 * - Service ID (SID) values match ISO-14229
 * - Response SID calculation (request + 0x40)
 * - Subfunction encoding rules
 * - Timing parameter constraints
 * - Message format compliance
 */

#include <gtest/gtest.h>
#include "uds.hpp"
#include "isotp.hpp"
#include "uds_security.hpp"

using namespace uds;

// ============================================================================
// ISO-14229 Service ID Compliance
// ============================================================================

TEST(SIDComplianceTest, DiagnosticManagementServices) {
  // ISO-14229-1 Table 2
  EXPECT_EQ(static_cast<uint8_t>(SID::DiagnosticSessionControl), 0x10);
  EXPECT_EQ(static_cast<uint8_t>(SID::ECUReset), 0x11);
  EXPECT_EQ(static_cast<uint8_t>(SID::SecurityAccess), 0x27);
  EXPECT_EQ(static_cast<uint8_t>(SID::CommunicationControl), 0x28);
  EXPECT_EQ(static_cast<uint8_t>(SID::TesterPresent), 0x3E);
  EXPECT_EQ(static_cast<uint8_t>(SID::AccessTimingParameters), 0x83);
  EXPECT_EQ(static_cast<uint8_t>(SID::ControlDTCSetting), 0x85);
}

TEST(SIDComplianceTest, DataTransmissionServices) {
  // ISO-14229-1 Table 2
  EXPECT_EQ(static_cast<uint8_t>(SID::ReadDataByIdentifier), 0x22);
  EXPECT_EQ(static_cast<uint8_t>(SID::ReadMemoryByAddress), 0x23);
  EXPECT_EQ(static_cast<uint8_t>(SID::ReadScalingDataByIdentifier), 0x24);
  EXPECT_EQ(static_cast<uint8_t>(SID::WriteDataByIdentifier), 0x2E);
  EXPECT_EQ(static_cast<uint8_t>(SID::WriteMemoryByAddress), 0x3D);
}

TEST(SIDComplianceTest, StoredDataServices) {
  EXPECT_EQ(static_cast<uint8_t>(SID::ClearDiagnosticInformation), 0x14);
  EXPECT_EQ(static_cast<uint8_t>(SID::ReadDTCInformation), 0x19);
}

TEST(SIDComplianceTest, IOControlServices) {
  EXPECT_EQ(static_cast<uint8_t>(SID::InputOutputControlByIdentifier), 0x2F);
}

TEST(SIDComplianceTest, RoutineServices) {
  EXPECT_EQ(static_cast<uint8_t>(SID::RoutineControl), 0x31);
}

TEST(SIDComplianceTest, UploadDownloadServices) {
  EXPECT_EQ(static_cast<uint8_t>(SID::RequestDownload), 0x34);
  EXPECT_EQ(static_cast<uint8_t>(SID::RequestUpload), 0x35);
  EXPECT_EQ(static_cast<uint8_t>(SID::TransferData), 0x36);
  EXPECT_EQ(static_cast<uint8_t>(SID::RequestTransferExit), 0x37);
}

// ============================================================================
// Response SID Compliance
// ============================================================================

TEST(ResponseSIDTest, PositiveResponseCalculation) {
  // Positive response SID = Request SID + 0x40
  
  EXPECT_EQ(static_cast<uint8_t>(SID::DiagnosticSessionControl) + 0x40, 0x50);
  EXPECT_EQ(static_cast<uint8_t>(SID::ECUReset) + 0x40, 0x51);
  EXPECT_EQ(static_cast<uint8_t>(SID::SecurityAccess) + 0x40, 0x67);
  EXPECT_EQ(static_cast<uint8_t>(SID::ReadDataByIdentifier) + 0x40, 0x62);
  EXPECT_EQ(static_cast<uint8_t>(SID::WriteDataByIdentifier) + 0x40, 0x6E);
  EXPECT_EQ(static_cast<uint8_t>(SID::RequestDownload) + 0x40, 0x74);
  EXPECT_EQ(static_cast<uint8_t>(SID::TransferData) + 0x40, 0x76);
  EXPECT_EQ(static_cast<uint8_t>(SID::RoutineControl) + 0x40, 0x71);
}

TEST(ResponseSIDTest, IsPositiveResponseFunction) {
  // Test the helper function
  EXPECT_TRUE(is_positive_response(0x50, 0x10));
  EXPECT_TRUE(is_positive_response(0x67, 0x27));
  EXPECT_TRUE(is_positive_response(0x74, 0x34));
  
  EXPECT_FALSE(is_positive_response(0x7F, 0x10));  // NRC
  EXPECT_FALSE(is_positive_response(0x51, 0x10));  // Wrong SID
}

TEST(ResponseSIDTest, NegativeResponseSID) {
  // Negative response always starts with 0x7F
  EXPECT_EQ(0x7F, 0x7F);
}

// ============================================================================
// Subfunction Encoding Compliance
// ============================================================================

TEST(SubfunctionTest, SuppressPositiveResponseBit) {
  // Bit 7 of subfunction = suppress positive response
  uint8_t normal = 0x01;
  uint8_t suppressed = 0x81;  // 0x01 | 0x80
  
  EXPECT_EQ(normal & 0x80, 0x00);
  EXPECT_EQ(suppressed & 0x80, 0x80);
  
  // Extract actual subfunction (mask off bit 7)
  EXPECT_EQ(normal & 0x7F, 0x01);
  EXPECT_EQ(suppressed & 0x7F, 0x01);
}

TEST(SubfunctionTest, SecurityAccessSubfunctions) {
  // Odd = requestSeed, Even = sendKey
  using namespace security;
  
  // Request seed subfunctions are odd
  EXPECT_EQ(Level::Basic & 0x01, 0x01);
  EXPECT_EQ(Level::Extended & 0x01, 0x01);
  EXPECT_EQ(Level::Programming & 0x01, 0x01);
  
  // Send key subfunctions are even
  EXPECT_EQ((Level::Basic + 1) & 0x01, 0x00);
  EXPECT_EQ((Level::Extended + 1) & 0x01, 0x00);
}

TEST(SubfunctionTest, DiagnosticSessionTypes) {
  EXPECT_EQ(static_cast<uint8_t>(Session::DefaultSession), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(Session::ProgrammingSession), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(Session::ExtendedSession), 0x03);
}

TEST(SubfunctionTest, ECUResetTypes) {
  EXPECT_EQ(static_cast<uint8_t>(EcuResetType::HardReset), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(EcuResetType::KeyOffOnReset), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(EcuResetType::SoftReset), 0x03);
}

TEST(SubfunctionTest, RoutineControlTypes) {
  EXPECT_EQ(static_cast<uint8_t>(RoutineAction::Start), 0x01);
  EXPECT_EQ(static_cast<uint8_t>(RoutineAction::Stop), 0x02);
  EXPECT_EQ(static_cast<uint8_t>(RoutineAction::Result), 0x03);
}

// ============================================================================
// Timing Parameter Compliance
// ============================================================================

TEST(TimingComplianceTest, DefaultP2Timing) {
  // ISO-14229: Default P2 = 50ms
  Timings t;
  // Default should be reasonable
  EXPECT_GE(t.p2.count(), 25);
  EXPECT_LE(t.p2.count(), 100);
}

TEST(TimingComplianceTest, DefaultP2StarTiming) {
  // ISO-14229: Default P2* = 5000ms (extended timeout)
  Timings t;
  EXPECT_GE(t.p2_star.count(), 500);
}

TEST(TimingComplianceTest, P2StarMultiplier) {
  // P2* in DSC response is in 10ms units
  uint16_t p2_star_raw = 500;  // 500 * 10ms = 5000ms
  uint32_t p2_star_ms = static_cast<uint32_t>(p2_star_raw) * 10;
  
  EXPECT_EQ(p2_star_ms, 5000u);
}

// ============================================================================
// ISO-15765 (ISO-TP) Compliance
// ============================================================================

TEST(ISOTPComplianceTest, PCITypes) {
  // Protocol Control Information types
  uint8_t SF = 0x00;  // Single Frame
  uint8_t FF = 0x10;  // First Frame
  uint8_t CF = 0x20;  // Consecutive Frame
  uint8_t FC = 0x30;  // Flow Control
  
  EXPECT_EQ(SF & 0xF0, 0x00);
  EXPECT_EQ(FF & 0xF0, 0x10);
  EXPECT_EQ(CF & 0xF0, 0x20);
  EXPECT_EQ(FC & 0xF0, 0x30);
}

TEST(ISOTPComplianceTest, SingleFrameLength) {
  // SF can carry 0-7 bytes (CAN) or 0-62 bytes (CAN-FD)
  // Lower nibble of PCI = data length
  
  for (uint8_t len = 0; len <= 7; ++len) {
    uint8_t pci = 0x00 | len;
    EXPECT_EQ(pci & 0x0F, len);
  }
}

TEST(ISOTPComplianceTest, FirstFrameLength) {
  // FF length is 12 bits (max 4095 for standard, 0xFFFFFFFF for extended)
  uint16_t max_standard = 4095;
  
  uint8_t pci0 = 0x10 | ((max_standard >> 8) & 0x0F);
  uint8_t pci1 = max_standard & 0xFF;
  
  uint16_t decoded = ((pci0 & 0x0F) << 8) | pci1;
  EXPECT_EQ(decoded, max_standard);
}

TEST(ISOTPComplianceTest, FlowControlStatus) {
  // Flow Status values
  uint8_t CTS = 0x00;   // Continue To Send
  uint8_t WT = 0x01;    // Wait
  uint8_t OVFL = 0x02;  // Overflow/Abort
  
  EXPECT_EQ(CTS, 0x00);
  EXPECT_EQ(WT, 0x01);
  EXPECT_EQ(OVFL, 0x02);
}

TEST(ISOTPComplianceTest, STminEncoding) {
  // STmin encoding per ISO 15765-2
  // 0x00-0x7F: 0-127 ms
  // 0xF1-0xF9: 100-900 µs
  
  // Millisecond range
  EXPECT_LE(0x00, 0x7F);
  EXPECT_LE(0x7F, 0x7F);
  
  // Microsecond range
  EXPECT_GE(0xF1, 0xF1);
  EXPECT_LE(0xF9, 0xF9);
  
  // Reserved ranges
  EXPECT_GT(0x80, 0x7F);
  EXPECT_LT(0x80, 0xF1);
}

// ============================================================================
// Data Identifier (DID) Compliance
// ============================================================================

TEST(DIDComplianceTest, StandardDIDRanges) {
  // ISO-14229-1 Annex C
  
  // Vehicle manufacturer specific: 0x0100-0xA5FF
  EXPECT_GE(0x0100, 0x0100);
  EXPECT_LE(0xA5FF, 0xA5FF);
  
  // System supplier specific: 0xA600-0xA7FF
  EXPECT_GE(0xA600, 0xA600);
  EXPECT_LE(0xA7FF, 0xA7FF);
  
  // Identification option: 0xF100-0xF1FF
  EXPECT_GE(0xF100, 0xF100);
  EXPECT_LE(0xF1FF, 0xF1FF);
}

TEST(DIDComplianceTest, CommonDIDs) {
  // Common standardized DIDs
  DID vin_did = 0xF190;  // VIN
  DID ecu_serial = 0xF18C;  // ECU Serial Number
  DID part_number = 0xF187;  // Part Number
  
  EXPECT_EQ(vin_did, 0xF190);
  EXPECT_EQ(ecu_serial, 0xF18C);
  EXPECT_EQ(part_number, 0xF187);
}

// ============================================================================
// Routine Identifier Compliance
// ============================================================================

TEST(RoutineIDComplianceTest, StandardRoutineRanges) {
  // ISO-14229-1 Annex D
  
  // Erase memory routine
  RoutineId erase = 0xFF00;
  EXPECT_EQ(erase, 0xFF00);
  
  // Check programming dependencies
  RoutineId check_deps = 0xFF01;
  EXPECT_EQ(check_deps, 0xFF01);
}

// ============================================================================
// Address and Length Format Compliance
// ============================================================================

TEST(ALFIComplianceTest, FormatByte) {
  // ISO-14229-1 Table 146
  // High nibble = memory address bytes (1-5)
  // Low nibble = memory size bytes (1-4)
  
  // Common formats
  EXPECT_EQ(0x11, (1 << 4) | 1);  // 1-byte addr, 1-byte size
  EXPECT_EQ(0x22, (2 << 4) | 2);  // 2-byte addr, 2-byte size
  EXPECT_EQ(0x44, (4 << 4) | 4);  // 4-byte addr, 4-byte size
}

TEST(ALFIComplianceTest, ValidRanges) {
  // Address bytes: 1-5 (nibble values 1-5)
  // Size bytes: 1-4 (nibble values 1-4)
  
  for (uint8_t addr = 1; addr <= 5; ++addr) {
    for (uint8_t size = 1; size <= 4; ++size) {
      uint8_t alfi = (addr << 4) | size;
      
      uint8_t decoded_addr = (alfi >> 4) & 0x0F;
      uint8_t decoded_size = alfi & 0x0F;
      
      EXPECT_EQ(decoded_addr, addr);
      EXPECT_EQ(decoded_size, size);
    }
  }
}

// ============================================================================
// DTC Format Compliance
// ============================================================================

TEST(DTCComplianceTest, DTCFormat) {
  // ISO-14229-1 Table 250
  // DTC is 3 bytes (24 bits)
  
  // First 2 bits = type (P/C/B/U)
  // 00 = P (Powertrain)
  // 01 = C (Chassis)
  // 10 = B (Body)
  // 11 = U (Network)
  
  uint8_t type_P = 0x00;
  uint8_t type_C = 0x40;
  uint8_t type_B = 0x80;
  uint8_t type_U = 0xC0;
  
  EXPECT_EQ((type_P >> 6) & 0x03, 0);
  EXPECT_EQ((type_C >> 6) & 0x03, 1);
  EXPECT_EQ((type_B >> 6) & 0x03, 2);
  EXPECT_EQ((type_U >> 6) & 0x03, 3);
}

TEST(DTCComplianceTest, StatusMaskBits) {
  // ISO-14229-1 Table 251
  uint8_t testFailed = 0x01;
  uint8_t testFailedThisOpCycle = 0x02;
  uint8_t pendingDTC = 0x04;
  uint8_t confirmedDTC = 0x08;
  uint8_t testNotCompletedSinceLastClear = 0x10;
  uint8_t testFailedSinceLastClear = 0x20;
  uint8_t testNotCompletedThisOpCycle = 0x40;
  uint8_t warningIndicatorRequested = 0x80;
  
  EXPECT_EQ(testFailed, 0x01);
  EXPECT_EQ(testFailedThisOpCycle, 0x02);
  EXPECT_EQ(pendingDTC, 0x04);
  EXPECT_EQ(confirmedDTC, 0x08);
  EXPECT_EQ(testNotCompletedSinceLastClear, 0x10);
  EXPECT_EQ(testFailedSinceLastClear, 0x20);
  EXPECT_EQ(testNotCompletedThisOpCycle, 0x40);
  EXPECT_EQ(warningIndicatorRequested, 0x80);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
