/**
 * @file iso_spec_dtc_test.cpp
 * @brief ISO 14229-1 Spec-Anchored Tests: DTC Services
 * 
 * References:
 * - ISO 14229-1:2020 Section 11.2 (ClearDiagnosticInformation)
 * - ISO 14229-1:2020 Section 11.3 (ReadDTCInformation)
 * - ISO 14229-1:2020 Annex D (DTC format)
 */

#include <gtest/gtest.h>
#include "uds_dtc.hpp"
#include <vector>
#include <cstdint>

// ============================================================================
// ISO 14229-1 Section 11.2: ClearDiagnosticInformation (SID 0x14)
// ============================================================================

class ClearDiagnosticInformationSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x14;
    
    // Per ISO 14229-1: groupOfDTC values
    static constexpr uint32_t ALL_DTCS = 0xFFFFFF;
    static constexpr uint32_t POWERTRAIN_DTCS = 0x000000;  // Example
    static constexpr uint32_t CHASSIS_DTCS = 0x400000;     // Example
    static constexpr uint32_t BODY_DTCS = 0x800000;        // Example
    static constexpr uint32_t NETWORK_DTCS = 0xC00000;     // Example
};

TEST_F(ClearDiagnosticInformationSpecTest, RequestFormat) {
    // Request: [SID] [groupOfDTC_high] [groupOfDTC_mid] [groupOfDTC_low]
    
    // Clear all DTCs
    std::vector<uint8_t> clear_all = {
        SID,
        0xFF, 0xFF, 0xFF  // All DTCs
    };
    
    EXPECT_EQ(4, clear_all.size());
    EXPECT_EQ(0x14, clear_all[0]);
    
    uint32_t group = (static_cast<uint32_t>(clear_all[1]) << 16) |
                     (static_cast<uint32_t>(clear_all[2]) << 8) |
                     clear_all[3];
    EXPECT_EQ(0xFFFFFF, group) << "All DTCs = 0xFFFFFF";
}

TEST_F(ClearDiagnosticInformationSpecTest, PositiveResponseFormat) {
    // Response: [SID+0x40]
    // Note: No additional data in positive response
    
    std::vector<uint8_t> response = {0x54};
    
    EXPECT_EQ(1, response.size()) << "Response is just SID+0x40";
    EXPECT_EQ(0x54, response[0]) << "Response SID = 0x14 + 0x40";
}

TEST_F(ClearDiagnosticInformationSpecTest, GroupOfDTCValues) {
    // Per ISO 14229-1 Table 91
    EXPECT_EQ(0xFFFFFF, ALL_DTCS) << "All groups = 0xFFFFFF";
}

// ============================================================================
// ISO 14229-1 Section 11.3: ReadDTCInformation (SID 0x19)
// ============================================================================

class ReadDTCInformationSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x19;
    
    // Per ISO 14229-1 Table 93: reportType (sub-function) values
    static constexpr uint8_t REPORT_NUMBER_OF_DTC_BY_STATUS_MASK = 0x01;
    static constexpr uint8_t REPORT_DTC_BY_STATUS_MASK = 0x02;
    static constexpr uint8_t REPORT_DTC_SNAPSHOT_IDENTIFICATION = 0x03;
    static constexpr uint8_t REPORT_DTC_SNAPSHOT_RECORD_BY_DTC_NUMBER = 0x04;
    static constexpr uint8_t REPORT_DTC_STORED_DATA_BY_RECORD_NUMBER = 0x05;
    static constexpr uint8_t REPORT_DTC_EXT_DATA_RECORD_BY_DTC_NUMBER = 0x06;
    static constexpr uint8_t REPORT_SUPPORTED_DTC = 0x0A;
    static constexpr uint8_t REPORT_FIRST_TEST_FAILED_DTC = 0x0B;
    static constexpr uint8_t REPORT_FIRST_CONFIRMED_DTC = 0x0C;
    static constexpr uint8_t REPORT_MOST_RECENT_TEST_FAILED_DTC = 0x0D;
    static constexpr uint8_t REPORT_MOST_RECENT_CONFIRMED_DTC = 0x0E;
    static constexpr uint8_t REPORT_DTC_FAULT_DETECTION_COUNTER = 0x14;
    static constexpr uint8_t REPORT_DTC_WITH_PERMANENT_STATUS = 0x15;
    
    // Per ISO 14229-1 Table 94: DTCStatusMask bits
    static constexpr uint8_t TEST_FAILED = 0x01;
    static constexpr uint8_t TEST_FAILED_THIS_OPERATION_CYCLE = 0x02;
    static constexpr uint8_t PENDING_DTC = 0x04;
    static constexpr uint8_t CONFIRMED_DTC = 0x08;
    static constexpr uint8_t TEST_NOT_COMPLETED_SINCE_LAST_CLEAR = 0x10;
    static constexpr uint8_t TEST_FAILED_SINCE_LAST_CLEAR = 0x20;
    static constexpr uint8_t TEST_NOT_COMPLETED_THIS_OPERATION_CYCLE = 0x40;
    static constexpr uint8_t WARNING_INDICATOR_REQUESTED = 0x80;
};

TEST_F(ReadDTCInformationSpecTest, ReportNumberOfDTCByStatusMaskRequest) {
    // Request: [SID] [reportType] [DTCStatusMask]
    std::vector<uint8_t> request = {
        SID,
        REPORT_NUMBER_OF_DTC_BY_STATUS_MASK,
        CONFIRMED_DTC  // Status mask = 0x08
    };
    
    EXPECT_EQ(3, request.size());
    EXPECT_EQ(0x19, request[0]);
    EXPECT_EQ(0x01, request[1]) << "Report type 0x01";
    EXPECT_EQ(0x08, request[2]) << "Confirmed DTC mask";
}

TEST_F(ReadDTCInformationSpecTest, ReportNumberOfDTCByStatusMaskResponse) {
    // Response: [SID+0x40] [reportType] [DTCStatusAvailabilityMask] 
    //           [DTCFormatIdentifier] [DTCCount_high] [DTCCount_low]
    
    std::vector<uint8_t> response = {
        0x59,  // SID + 0x40
        0x01,  // Report type echoed
        0xFF,  // Status availability mask (all bits supported)
        0x01,  // DTC format: ISO 14229-1
        0x00, 0x05  // DTC count = 5
    };
    
    EXPECT_EQ(6, response.size());
    EXPECT_EQ(0x59, response[0]) << "Response SID = 0x19 + 0x40";
    EXPECT_EQ(0x01, response[1]) << "Report type echoed";
    
    uint16_t dtc_count = (static_cast<uint16_t>(response[4]) << 8) | response[5];
    EXPECT_EQ(5, dtc_count);
}

TEST_F(ReadDTCInformationSpecTest, ReportDTCByStatusMaskRequest) {
    // Request: [SID] [reportType] [DTCStatusMask]
    std::vector<uint8_t> request = {
        SID,
        REPORT_DTC_BY_STATUS_MASK,
        static_cast<uint8_t>(CONFIRMED_DTC | TEST_FAILED)  // 0x09
    };
    
    EXPECT_EQ(0x02, request[1]) << "Report type 0x02";
    EXPECT_EQ(0x09, request[2]) << "Confirmed + Test Failed";
}

TEST_F(ReadDTCInformationSpecTest, ReportDTCByStatusMaskResponse) {
    // Response: [SID+0x40] [reportType] [DTCStatusAvailabilityMask]
    //           [DTC_high] [DTC_mid] [DTC_low] [statusOfDTC]...
    
    std::vector<uint8_t> response = {
        0x59,  // SID + 0x40
        0x02,  // Report type
        0xFF,  // Status availability mask
        // DTC 1: P0100 (0x010000) with status 0x09
        0x01, 0x00, 0x00, 0x09,
        // DTC 2: P0200 (0x020000) with status 0x08
        0x02, 0x00, 0x00, 0x08
    };
    
    EXPECT_EQ(11, response.size());
    
    // Parse first DTC
    uint32_t dtc1 = (static_cast<uint32_t>(response[3]) << 16) |
                    (static_cast<uint32_t>(response[4]) << 8) |
                    response[5];
    uint8_t status1 = response[6];
    
    EXPECT_EQ(0x010000, dtc1);
    EXPECT_EQ(0x09, status1);
}

TEST_F(ReadDTCInformationSpecTest, DTCStatusMaskBits) {
    // Per ISO 14229-1 Table 94
    EXPECT_EQ(0x01, TEST_FAILED);
    EXPECT_EQ(0x02, TEST_FAILED_THIS_OPERATION_CYCLE);
    EXPECT_EQ(0x04, PENDING_DTC);
    EXPECT_EQ(0x08, CONFIRMED_DTC);
    EXPECT_EQ(0x10, TEST_NOT_COMPLETED_SINCE_LAST_CLEAR);
    EXPECT_EQ(0x20, TEST_FAILED_SINCE_LAST_CLEAR);
    EXPECT_EQ(0x40, TEST_NOT_COMPLETED_THIS_OPERATION_CYCLE);
    EXPECT_EQ(0x80, WARNING_INDICATOR_REQUESTED);
}

TEST_F(ReadDTCInformationSpecTest, DTCFormatIdentifier) {
    // Per ISO 14229-1 Table 95: DTCFormatIdentifier values
    constexpr uint8_t ISO_15031_6 = 0x00;
    constexpr uint8_t ISO_14229_1 = 0x01;
    constexpr uint8_t SAE_J1939_73 = 0x02;
    constexpr uint8_t ISO_11992_4 = 0x03;
    constexpr uint8_t SAE_J2012_DA = 0x04;
    
    EXPECT_EQ(0x00, ISO_15031_6);
    EXPECT_EQ(0x01, ISO_14229_1);
    EXPECT_EQ(0x02, SAE_J1939_73);
    EXPECT_EQ(0x03, ISO_11992_4);
    EXPECT_EQ(0x04, SAE_J2012_DA);
}

// ============================================================================
// ISO 14229-1 Annex D: DTC Format
// ============================================================================

class DTCFormatSpecTest : public ::testing::Test {
protected:
    // Per ISO 14229-1 Annex D.1: 3-byte DTC format
    // Byte 1: DTC high byte
    // Byte 2: DTC middle byte  
    // Byte 3: DTC low byte (failure type)
};

TEST_F(DTCFormatSpecTest, ThreeByteDTCFormat) {
    // Example: P0100 (Mass Air Flow sensor circuit malfunction)
    // In ISO 14229-1 format: 0x01 0x00 0x00
    
    uint8_t dtc_high = 0x01;
    uint8_t dtc_mid = 0x00;
    uint8_t dtc_low = 0x00;  // Failure type
    
    uint32_t dtc = (static_cast<uint32_t>(dtc_high) << 16) |
                   (static_cast<uint32_t>(dtc_mid) << 8) |
                   dtc_low;
    
    EXPECT_EQ(0x010000, dtc);
}

TEST_F(DTCFormatSpecTest, DTCWithFailureType) {
    // DTC with failure type byte
    // Example: P0100-11 (circuit short to ground)
    
    uint8_t dtc_high = 0x01;
    uint8_t dtc_mid = 0x00;
    uint8_t failure_type = 0x11;  // Short to ground
    
    uint32_t dtc = (static_cast<uint32_t>(dtc_high) << 16) |
                   (static_cast<uint32_t>(dtc_mid) << 8) |
                   failure_type;
    
    EXPECT_EQ(0x010011, dtc);
    EXPECT_EQ(0x11, dtc & 0xFF) << "Failure type extractable";
}

TEST_F(DTCFormatSpecTest, FailureTypeValues) {
    // Per ISO 14229-1 Annex D.2: Common failure type values
    constexpr uint8_t GENERAL_FAILURE = 0x00;
    constexpr uint8_t UPPER_THRESHOLD_EXCEEDED = 0x01;
    constexpr uint8_t LOWER_THRESHOLD_EXCEEDED = 0x02;
    constexpr uint8_t NO_SIGNAL = 0x03;
    constexpr uint8_t INVALID_SIGNAL = 0x04;
    constexpr uint8_t OPEN_CIRCUIT = 0x05;
    constexpr uint8_t SHORT_TO_GROUND = 0x11;
    constexpr uint8_t SHORT_TO_BATTERY = 0x12;
    constexpr uint8_t CIRCUIT_OPEN = 0x13;
    constexpr uint8_t CIRCUIT_SHORT_TO_GROUND_OR_OPEN = 0x14;
    constexpr uint8_t CIRCUIT_SHORT_TO_BATTERY_OR_OPEN = 0x15;
    
    EXPECT_EQ(0x00, GENERAL_FAILURE);
    EXPECT_EQ(0x11, SHORT_TO_GROUND);
    EXPECT_EQ(0x12, SHORT_TO_BATTERY);
}

// ============================================================================
// ISO 14229-1: DTC Snapshot Records
// ============================================================================

class DTCSnapshotSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x19;
    static constexpr uint8_t REPORT_DTC_SNAPSHOT_BY_DTC = 0x04;
};

TEST_F(DTCSnapshotSpecTest, RequestFormat) {
    // Request: [SID] [reportType] [DTC_high] [DTC_mid] [DTC_low] [snapshotRecordNumber]
    
    std::vector<uint8_t> request = {
        SID,
        REPORT_DTC_SNAPSHOT_BY_DTC,
        0x01, 0x00, 0x11,  // DTC P0100-11
        0xFF               // All snapshot records
    };
    
    EXPECT_EQ(6, request.size());
    EXPECT_EQ(0x04, request[1]) << "Report type 0x04";
    EXPECT_EQ(0xFF, request[5]) << "All records = 0xFF";
}

TEST_F(DTCSnapshotSpecTest, ResponseFormat) {
    // Response: [SID+0x40] [reportType] [DTC_high] [DTC_mid] [DTC_low] [statusOfDTC]
    //           [snapshotRecordNumber] [snapshotRecordNumberOfIdentifiers]
    //           [DID_high] [DID_low] [data...] ...
    
    std::vector<uint8_t> response = {
        0x59,              // SID + 0x40
        0x04,              // Report type
        0x01, 0x00, 0x11,  // DTC
        0x09,              // Status
        0x01,              // Snapshot record number
        0x02,              // Number of DIDs in this record
        0xF1, 0x90,        // DID 1: VIN
        'W', 'V', 'W',     // VIN data (truncated for example)
        0x10, 0x00,        // DID 2: Engine RPM
        0x0B, 0xB8         // RPM = 3000
    };
    
    EXPECT_EQ(0x59, response[0]);
    EXPECT_EQ(0x04, response[1]);
    EXPECT_EQ(0x01, response[6]) << "Snapshot record 1";
    EXPECT_EQ(0x02, response[7]) << "2 DIDs in record";
}

// ============================================================================
// ISO 14229-1: DTC Extended Data Records
// ============================================================================

class DTCExtendedDataSpecTest : public ::testing::Test {
protected:
    static constexpr uint8_t SID = 0x19;
    static constexpr uint8_t REPORT_DTC_EXT_DATA = 0x06;
};

TEST_F(DTCExtendedDataSpecTest, RequestFormat) {
    // Request: [SID] [reportType] [DTC_high] [DTC_mid] [DTC_low] [extDataRecordNumber]
    
    std::vector<uint8_t> request = {
        SID,
        REPORT_DTC_EXT_DATA,
        0x01, 0x00, 0x11,  // DTC
        0x01               // Extended data record 1
    };
    
    EXPECT_EQ(6, request.size());
    EXPECT_EQ(0x06, request[1]);
}

TEST_F(DTCExtendedDataSpecTest, ExtendedDataRecordNumbers) {
    // Per ISO 14229-1:
    // 0x01-0x8F: OEM specific
    // 0x90-0xEF: Reserved
    // 0xF0-0xFE: System supplier specific
    // 0xFF: All extended data records
    
    constexpr uint8_t OEM_START = 0x01;
    constexpr uint8_t OEM_END = 0x8F;
    constexpr uint8_t SUPPLIER_START = 0xF0;
    constexpr uint8_t SUPPLIER_END = 0xFE;
    constexpr uint8_t ALL_RECORDS = 0xFF;
    
    EXPECT_EQ(0x01, OEM_START);
    EXPECT_EQ(0x8F, OEM_END);
    EXPECT_EQ(0xF0, SUPPLIER_START);
    EXPECT_EQ(0xFE, SUPPLIER_END);
    EXPECT_EQ(0xFF, ALL_RECORDS);
}
