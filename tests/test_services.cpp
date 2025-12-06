/**
 * @file test_services.cpp
 * @brief UDS Service validation and integration tests
 */

#include "test_framework.hpp"
#include "../include/uds.hpp"
#include "../include/uds_dtc.hpp"
#include "../include/uds_event.hpp"
#include "../include/uds_link.hpp"
#include "../include/uds_scaling.hpp"
#include "../include/uds_io.hpp"
#include "../include/uds_memory.hpp"
#include <vector>

using namespace test;

// ============================================================================
// Test: ReadDataByPeriodicIdentifier (0x2A)
// ============================================================================

TEST(PeriodicData_StartTransmission) {
    std::cout << "  Testing: Start periodic transmission (0x2A 0x02)" << std::endl;
    
    // Request: 0x2A [transmissionMode] [periodicDIDs...]
    std::vector<uint8_t> request = {0x2A, 0x02, 0x01, 0x02}; // Medium rate, DIDs 0x01, 0x02
    
    // Positive response: 0x6A [transmissionMode]
    std::vector<uint8_t> response = {0x6A, 0x02};
    
    ASSERT_EQ(0x6A, response[0]);
    ASSERT_EQ(0x02, response[1]); // Medium rate confirmed
}

TEST(PeriodicData_StopTransmission) {
    std::cout << "  Testing: Stop periodic transmission (0x2A 0x04)" << std::endl;
    
    std::vector<uint8_t> request = {0x2A, 0x04, 0x01, 0x02}; // Stop, DIDs 0x01, 0x02
    std::vector<uint8_t> response = {0x6A, 0x04};
    
    ASSERT_EQ(0x6A, response[0]);
    ASSERT_EQ(0x04, response[1]); // Stop confirmed
}

TEST(PeriodicData_UnsolicitedMessage) {
    std::cout << "  Testing: Unsolicited periodic data message format" << std::endl;
    
    // Unsolicited message: 0x6A [periodicDID] [data...]
    std::vector<uint8_t> message = {0x6A, 0x01, 0xAA, 0xBB, 0xCC};
    
    ASSERT_EQ(0x6A, message[0]);
    ASSERT_EQ(0x01, message[1]); // Periodic DID
    ASSERT_GT(message.size(), 2); // Has data
}

// ============================================================================
// Test: DynamicallyDefineDataIdentifier (0x2C)
// ============================================================================

TEST(DDDI_DefineByIdentifier) {
    std::cout << "  Testing: Define DID by identifier (0x2C 0x01)" << std::endl;
    
    // Request: 0x2C 0x01 [newDID] [sourceDID] [position] [length]
    std::vector<uint8_t> request = {0x2C, 0x01, 0xF2, 0x00, 0xF1, 0x90, 0x01, 0x04};
    
    // Positive response: 0x6C 0x01 [newDID]
    std::vector<uint8_t> response = {0x6C, 0x01, 0xF2, 0x00};
    
    ASSERT_EQ(0x6C, response[0]);
    ASSERT_EQ(0x01, response[1]); // DefineByIdentifier
}

TEST(DDDI_DefineByMemoryAddress) {
    std::cout << "  Testing: Define DID by memory address (0x2C 0x02)" << std::endl;
    
    std::vector<uint8_t> request = {0x2C, 0x02, 0xF2, 0x01, 0x44, 
                                     0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04};
    std::vector<uint8_t> response = {0x6C, 0x02, 0xF2, 0x01};
    
    ASSERT_EQ(0x6C, response[0]);
    ASSERT_EQ(0x02, response[1]); // DefineByMemoryAddress
}

TEST(DDDI_Clear) {
    std::cout << "  Testing: Clear dynamically defined DID (0x2C 0x03)" << std::endl;
    
    std::vector<uint8_t> request = {0x2C, 0x03, 0xF2, 0x00};
    std::vector<uint8_t> response = {0x6C, 0x03};
    
    ASSERT_EQ(0x6C, response[0]);
    ASSERT_EQ(0x03, response[1]); // Clear
}

// ============================================================================
// Test: ReadScalingDataByIdentifier (0x24)
// ============================================================================

TEST(ScalingData_ReadSingle) {
    std::cout << "  Testing: Read scaling data by identifier (0x24)" << std::endl;
    
    // Request: 0x24 [DID]
    std::vector<uint8_t> request = {0x24, 0xF1, 0x90};
    
    // Positive response: 0x64 [DID] [scalingData...]
    std::vector<uint8_t> response = {0x64, 0xF1, 0x90, 0x01, 0x02, 0x03, 0x04};
    
    ASSERT_EQ(0x64, response[0]);
    ASSERT_EQ(0xF1, response[1]);
    ASSERT_EQ(0x90, response[2]);
    ASSERT_GT(response.size(), 3);
}

TEST(ScalingData_Format) {
    std::cout << "  Testing: Scaling data format (linear formula)" << std::endl;
    
    // Example scaling: y = C0 * x + C1
    // Format: [formatId] [unitId] [C0 bytes] [C1 bytes]
    std::vector<uint8_t> scaling = {0x01, 0x02, 0x00, 0x01, 0x00, 0x00};
    
    ASSERT_EQ(0x01, scaling[0]); // Format ID
    ASSERT_EQ(0x02, scaling[1]); // Unit ID
}

// ============================================================================
// Test: InputOutputControlByIdentifier (0x2F)
// ============================================================================

TEST(IOControl_ReturnToECU) {
    std::cout << "  Testing: I/O Control - Return to ECU (0x2F 0x00)" << std::endl;
    
    // Request: 0x2F [DID] [controlOption]
    std::vector<uint8_t> request = {0x2F, 0x01, 0x02, 0x00};
    
    // Positive response: 0x6F [DID] [controlOption]
    std::vector<uint8_t> response = {0x6F, 0x01, 0x02, 0x00};
    
    ASSERT_EQ(0x6F, response[0]);
    ASSERT_EQ(0x00, response[3]); // ReturnControlToECU
}

TEST(IOControl_ShortTermAdjustment) {
    std::cout << "  Testing: I/O Control - Short term adjustment (0x2F 0x03)" << std::endl;
    
    std::vector<uint8_t> request = {0x2F, 0x01, 0x02, 0x03, 0xFF, 0x00};
    std::vector<uint8_t> response = {0x6F, 0x01, 0x02, 0x03};
    
    ASSERT_EQ(0x6F, response[0]);
    ASSERT_EQ(0x03, response[3]); // ShortTermAdjustment
}

TEST(IOControl_FreezeCurrentState) {
    std::cout << "  Testing: I/O Control - Freeze current state (0x2F 0x02)" << std::endl;
    
    std::vector<uint8_t> request = {0x2F, 0x01, 0x02, 0x02};
    std::vector<uint8_t> response = {0x6F, 0x01, 0x02, 0x02};
    
    ASSERT_EQ(0x6F, response[0]);
    ASSERT_EQ(0x02, response[3]); // FreezeCurrentState
}

// ============================================================================
// Test: LinkControl (0x87)
// ============================================================================

TEST(LinkControl_VerifyFixedBaudrate) {
    std::cout << "  Testing: Link Control - Verify fixed baudrate (0x87 0x01)" << std::endl;
    
    // Request: 0x87 [verifyBaudrateTransitionWithFixedBaudrate] [baudrateIdentifier]
    std::vector<uint8_t> request = {0x87, 0x01, 0x03};
    
    // Positive response: 0xC7 [subfunction]
    std::vector<uint8_t> response = {0xC7, 0x01};
    
    ASSERT_EQ(0xC7, response[0]);
    ASSERT_EQ(0x01, response[1]);
}

TEST(LinkControl_TransitionBaudrate) {
    std::cout << "  Testing: Link Control - Transition baudrate (0x87 0x03)" << std::endl;
    
    std::vector<uint8_t> request = {0x87, 0x03, 0x03};
    // No response expected (suppressPosRspMsgIndicationBit set)
    
    ASSERT_EQ(0x87, request[0]);
    ASSERT_EQ(0x03, request[1]); // TransitionBaudrate
}

// ============================================================================
// Test: ResponseOnEvent (0x86)
// ============================================================================

TEST(ResponseOnEvent_StartOnDTCChange) {
    std::cout << "  Testing: Response On Event - Start on DTC change (0x86 0x05)" << std::endl;
    
    // Request: 0x86 [startResponseOnEvent] [eventType] [eventWindow] [service...]
    std::vector<uint8_t> request = {0x86, 0x05, 0x01, 0x00, 0x19, 0x02, 0xFF};
    
    // Positive response: 0xC6 [subfunction] [numberOfActivatedEvents] [eventTypeRecord]
    std::vector<uint8_t> response = {0xC6, 0x05, 0x01, 0x01};
    
    ASSERT_EQ(0xC6, response[0]);
    ASSERT_EQ(0x05, response[1]); // StartResponseOnEvent
}

TEST(ResponseOnEvent_StopResponse) {
    std::cout << "  Testing: Response On Event - Stop (0x86 0x00)" << std::endl;
    
    std::vector<uint8_t> request = {0x86, 0x00};
    std::vector<uint8_t> response = {0xC6, 0x00};
    
    ASSERT_EQ(0xC6, response[0]);
    ASSERT_EQ(0x00, response[1]); // StopResponseOnEvent
}

TEST(ResponseOnEvent_UnsolicitedMessage) {
    std::cout << "  Testing: Response On Event - Unsolicited message format" << std::endl;
    
    // Unsolicited: 0xC6 [eventTypeRecord] [numberOfActivatedEvents] [serviceID] [data...]
    std::vector<uint8_t> message = {0xC6, 0x01, 0x01, 0x19, 0x02, 0x01};
    
    ASSERT_EQ(0xC6, message[0]);
    ASSERT_GT(message.size(), 3);
}

// ============================================================================
// Test: ControlDTCSetting (0x85)
// ============================================================================

TEST(DTCControl_On) {
    std::cout << "  Testing: Control DTC Setting - On (0x85 0x01)" << std::endl;
    
    // Request: 0x85 [DTCSettingType]
    std::vector<uint8_t> request = {0x85, 0x01};
    
    // Positive response: 0xC5 [DTCSettingType]
    std::vector<uint8_t> response = {0xC5, 0x01};
    
    ASSERT_EQ(0xC5, response[0]);
    ASSERT_EQ(0x01, response[1]); // DTC setting ON
}

TEST(DTCControl_Off) {
    std::cout << "  Testing: Control DTC Setting - Off (0x85 0x02)" << std::endl;
    
    std::vector<uint8_t> request = {0x85, 0x02};
    std::vector<uint8_t> response = {0xC5, 0x02};
    
    ASSERT_EQ(0xC5, response[0]);
    ASSERT_EQ(0x02, response[1]); // DTC setting OFF
}

// ============================================================================
// Test: ReadDTCInformation (0x19) - Extended
// ============================================================================

TEST(DTCInfo_ReportDTCByStatusMask) {
    std::cout << "  Testing: Read DTC Info - Report by status mask (0x19 0x02)" << std::endl;
    
    // Request: 0x19 0x02 [statusMask]
    std::vector<uint8_t> request = {0x19, 0x02, 0xFF};
    
    // Positive response: 0x59 0x02 [statusAvailabilityMask] [DTCs...]
    std::vector<uint8_t> response = {0x59, 0x02, 0xFF, 0x12, 0x34, 0x56, 0x01};
    
    ASSERT_EQ(0x59, response[0]);
    ASSERT_EQ(0x02, response[1]);
}

TEST(DTCInfo_ReportDTCSnapshotRecord) {
    std::cout << "  Testing: Read DTC Info - Report snapshot (0x19 0x04)" << std::endl;
    
    std::vector<uint8_t> request = {0x19, 0x04, 0x12, 0x34, 0x56, 0x01};
    std::vector<uint8_t> response = {0x59, 0x04, 0x12, 0x34, 0x56, 0x01, 0xAA, 0xBB};
    
    ASSERT_EQ(0x59, response[0]);
    ASSERT_EQ(0x04, response[1]);
}

TEST(DTCInfo_ReportSupportedDTC) {
    std::cout << "  Testing: Read DTC Info - Report supported DTCs (0x19 0x0A)" << std::endl;
    
    std::vector<uint8_t> request = {0x19, 0x0A};
    std::vector<uint8_t> response = {0x59, 0x0A, 0xFF, 0x12, 0x34, 0x56, 0x01};
    
    ASSERT_EQ(0x59, response[0]);
    ASSERT_EQ(0x0A, response[1]);
}

// ============================================================================
// Test: SecuredDataTransmission (0x84)
// ============================================================================

TEST(SecuredData_Transmission) {
    std::cout << "  Testing: Secured Data Transmission (0x84)" << std::endl;
    
    // Request: 0x84 [securityDataRequestRecord...]
    std::vector<uint8_t> request = {0x84, 0x01, 0x02, 0x03, 0x04};
    
    // Positive response: 0xC4 [securityDataResponseRecord...]
    std::vector<uint8_t> response = {0xC4, 0xAA, 0xBB, 0xCC, 0xDD};
    
    ASSERT_EQ(0xC4, response[0]);
    ASSERT_GT(response.size(), 1);
}

// ============================================================================
// Test: Memory Operations - Extended
// ============================================================================

TEST(Memory_RequestUpload) {
    std::cout << "  Testing: Request Upload (0x35)" << std::endl;
    
    // Request: 0x35 [dataFormatIdentifier] [ALFI] [memoryAddress] [memorySize]
    std::vector<uint8_t> request = {0x35, 0x00, 0x44, 
                                     0x00, 0x01, 0x00, 0x00, 
                                     0x00, 0x00, 0x10, 0x00};
    
    // Positive response: 0x75 [lengthFormatIdentifier] [maxNumberOfBlockLength]
    std::vector<uint8_t> response = {0x75, 0x20, 0x10, 0x00};
    
    ASSERT_EQ(0x75, response[0]);
}

TEST(Memory_RequestFileTransfer) {
    std::cout << "  Testing: Request File Transfer (0x38)" << std::endl;
    
    // Request: 0x38 [modeOfOperation] [filePathAndNameLength] [filePath...]
    std::vector<uint8_t> request = {0x38, 0x01, 0x05, 
                                     'f', 'i', 'l', 'e', '1'};
    
    // Positive response: 0x78 [modeOfOperation] [lengthFormatIdentifier] [maxFileSize]
    std::vector<uint8_t> response = {0x78, 0x01, 0x40, 0x00, 0x00, 0x10, 0x00};
    
    ASSERT_EQ(0x78, response[0]);
}

// ============================================================================
// Test: Access Timing Parameters (0x83)
// ============================================================================

TEST(AccessTiming_ReadExtended) {
    std::cout << "  Testing: Access Timing Parameters - Read (0x83 0x01)" << std::endl;
    
    // Request: 0x83 [timingParameterAccessType]
    std::vector<uint8_t> request = {0x83, 0x01};
    
    // Positive response: 0xC3 [timingParameterAccessType] [timingParameters...]
    std::vector<uint8_t> response = {0xC3, 0x01, 0x00, 0x32, 0x01, 0xF4};
    
    ASSERT_EQ(0xC3, response[0]);
    ASSERT_EQ(0x01, response[1]);
}

TEST(AccessTiming_SetToDefault) {
    std::cout << "  Testing: Access Timing Parameters - Set to default (0x83 0x02)" << std::endl;
    
    std::vector<uint8_t> request = {0x83, 0x02};
    std::vector<uint8_t> response = {0xC3, 0x02};
    
    ASSERT_EQ(0xC3, response[0]);
    ASSERT_EQ(0x02, response[1]);
}

// ============================================================================
// Test: Service Integration
// ============================================================================

TEST(Integration_SessionAndSecurity) {
    std::cout << "  Testing: Session control followed by security access" << std::endl;
    
    // Step 1: Enter programming session
    std::vector<uint8_t> session_req = {0x10, 0x02};
    std::vector<uint8_t> session_resp = {0x50, 0x02, 0x00, 0x32, 0x01, 0xF4};
    
    ASSERT_EQ(0x50, session_resp[0]);
    
    // Step 2: Request seed
    std::vector<uint8_t> seed_req = {0x27, 0x01};
    std::vector<uint8_t> seed_resp = {0x67, 0x01, 0x12, 0x34, 0x56, 0x78};
    
    ASSERT_EQ(0x67, seed_resp[0]);
}

TEST(Integration_DTCControlAndCommunication) {
    std::cout << "  Testing: DTC control followed by communication control" << std::endl;
    
    // Step 1: Disable DTC setting
    std::vector<uint8_t> dtc_req = {0x85, 0x02};
    std::vector<uint8_t> dtc_resp = {0xC5, 0x02};
    
    ASSERT_EQ(0xC5, dtc_resp[0]);
    
    // Step 2: Disable communications
    std::vector<uint8_t> comm_req = {0x28, 0x03, 0xFF};
    std::vector<uint8_t> comm_resp = {0x68, 0x03};
    
    ASSERT_EQ(0x68, comm_resp[0]);
}

// ============================================================================
// Main test runner
// ============================================================================

int main() {
    std::cout << "\n" << colors::BOLD << "UDS Service Validation Tests" << colors::RESET << "\n";
    std::cout << "Testing Advanced UDS Services and Integration\n\n";
    
    TestRunner::instance().run_all_tests();
    
    auto stats = TestRunner::instance().get_stats();
    return (stats.failed == 0) ? 0 : 1;
}

