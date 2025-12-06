#ifndef UDS_DTC_HPP
#define UDS_DTC_HPP

/**
 * @file uds_dtc.hpp
 * @brief DTC Management Services - ISO 14229-1:2013 Section 11
 * 
 * This header provides comprehensive DTC (Diagnostic Trouble Code) management:
 * - ReadDTCInformation (0x19) - Section 11.3 (p. 178)
 * - ClearDiagnosticInformation (0x14) - Section 11.2 (p. 175)
 * - ControlDTCSetting (0x85) - Section 9.9 (p. 71)
 * 
 * ============================================================================
 * ISO 14229-1:2013 DTC REFERENCE
 * ============================================================================
 * 
 * DTC Format (Annex D, p. 353):
 * - 3 bytes (24 bits): [High][Mid][Low] representing fault code
 * - First nibble indicates system: P=Powertrain, C=Chassis, B=Body, U=Network
 * - Status byte (Annex D.2, Table D.3) indicates current state
 * 
 * DTC Status Byte Bits (Annex D.2, Table D.3):
 *   Bit 0: testFailed - DTC test failed at time of request
 *   Bit 1: testFailedThisOperationCycle - Failed this ignition cycle
 *   Bit 2: pendingDTC - Pending confirmation
 *   Bit 3: confirmedDTC - Confirmed/stored DTC
 *   Bit 4: testNotCompletedSinceLastClear - Test incomplete since clear
 *   Bit 5: testFailedSinceLastClear - Failed since last clear
 *   Bit 6: testNotCompletedThisOperationCycle - Test incomplete this cycle
 *   Bit 7: warningIndicatorRequested - MIL lamp requested
 * 
 * ReadDTCInformation (0x19) Sub-functions (Section 11.3.2.2):
 *   0x01: reportNumberOfDTCByStatusMask
 *   0x02: reportDTCByStatusMask
 *   0x03: reportDTCSnapshotIdentification
 *   0x04: reportDTCSnapshotRecordByDTCNumber
 *   0x05: reportDTCStoredDataByRecordNumber
 *   0x06: reportDTCExtDataRecordByDTCNumber
 *   0x07: reportNumberOfDTCBySeverityMaskRecord
 *   0x08: reportDTCBySeverityMaskRecord
 *   0x09: reportSeverityInformationOfDTC
 *   0x0A: reportSupportedDTC
 *   0x0B: reportFirstTestFailedDTC
 *   0x0C: reportFirstConfirmedDTC
 *   0x0D: reportMostRecentTestFailedDTC
 *   0x0E: reportMostRecentConfirmedDTC
 *   0x0F: reportMirrorMemoryDTCByStatusMask
 *   ... (see Table 271 for complete list)
 * 
 * Usage:
 * @code
 *   // Read all confirmed DTCs
 *   auto result = uds::dtc::read_dtcs_by_status(client, 0x08); // Confirmed
 *   if (result.ok) {
 *     for (const auto& dtc : result.value.dtcs) {
 *       printf("DTC: %06X Status: %02X\n", dtc.code, dtc.status);
 *     }
 *   }
 *   
 *   // Clear all DTCs
 *   uds::dtc::clear_all_dtcs(client);
 * @endcode
 */

#include "uds.hpp"
#include <optional>

namespace uds {
namespace dtc {

// ============================================================================
// DTC Status Mask Bits
// ISO 14229-1:2013 Annex D.2, Table D.3
// ============================================================================

/**
 * @brief DTC Status Byte bit definitions
 * 
 * ISO 14229-1:2013 Annex D.2, Table D.3:
 * Each bit represents a specific test/status condition.
 * Multiple bits can be set simultaneously.
 */
namespace StatusMask {
  constexpr uint8_t TestFailed                         = 0x01;  ///< Bit 0 - Test failed now
  constexpr uint8_t TestFailedThisOperationCycle       = 0x02;  ///< Bit 1 - Failed this cycle
  constexpr uint8_t PendingDTC                         = 0x04;  ///< Bit 2 - Pending confirmation
  constexpr uint8_t ConfirmedDTC                       = 0x08;  ///< Bit 3 - Confirmed/stored
  constexpr uint8_t TestNotCompletedSinceLastClear     = 0x10;  ///< Bit 4 - Incomplete since clear
  constexpr uint8_t TestFailedSinceLastClear           = 0x20;  ///< Bit 5 - Failed since clear
  constexpr uint8_t TestNotCompletedThisOperationCycle = 0x40;  ///< Bit 6 - Incomplete this cycle
  constexpr uint8_t WarningIndicatorRequested          = 0x80;  ///< Bit 7 - MIL lamp on
  
  constexpr uint8_t AllDTCs = 0xFF;  ///< Match any status
}

// ============================================================================
// ReadDTCInformation (0x19) Sub-functions
// ISO 14229-1:2013 Section 11.3.2.2
// ============================================================================

/**
 * @brief ReadDTCInformation (0x19) sub-function values
 * 
 * ISO 14229-1:2013 Section 11.3.2.2
 */
enum class ReadDTCSubFunction : uint8_t {
  ReportNumberOfDTCByStatusMask           = 0x01,  ///< Table 271 - Count DTCs by status
  ReportDTCByStatusMask                   = 0x02,  ///< Table 271 - List DTCs by status
  ReportDTCSnapshotIdentification         = 0x03,
  ReportDTCSnapshotRecordByDTCNumber      = 0x04,
  ReportDTCStoredDataByRecordNumber       = 0x05,
  ReportDTCExtDataRecordByDTCNumber       = 0x06,
  ReportNumberOfDTCBySeverityMaskRecord   = 0x07,
  ReportDTCBySeverityMaskRecord           = 0x08,
  ReportSeverityInformationOfDTC          = 0x09,
  ReportSupportedDTC                      = 0x0A,
  ReportFirstTestFailedDTC                = 0x0B,
  ReportFirstConfirmedDTC                 = 0x0C,
  ReportMostRecentTestFailedDTC           = 0x0D,
  ReportMostRecentConfirmedDTC            = 0x0E,
  ReportMirrorMemoryDTCByStatusMask       = 0x0F,
  ReportMirrorMemoryDTCExtDataRecordByDTCNumber = 0x10,
  ReportNumberOfMirrorMemoryDTCByStatusMask     = 0x11,
  ReportNumberOfEmissionsOBDDTCByStatusMask     = 0x12,
  ReportEmissionsOBDDTCByStatusMask             = 0x13,
  ReportDTCFaultDetectionCounter                = 0x14,
  ReportDTCWithPermanentStatus                  = 0x15,
  ReportDTCExtDataRecordByRecordNumber          = 0x16,
  ReportUserDefMemoryDTCByStatusMask            = 0x17,
  ReportUserDefMemoryDTCSnapshotRecordByDTCNumber = 0x18,
  ReportUserDefMemoryDTCExtDataRecordByDTCNumber  = 0x19,
  ReportWWHOBDDTCByMaskRecord                     = 0x42,
  ReportWWHOBDDTCWithPermanentStatus              = 0x55
};

// ============================================================================
// DTC Severity Levels
// ============================================================================

enum class DTCSeverity : uint8_t {
  NoSeverityAvailable           = 0x00,
  MaintenanceOnly               = 0x20,
  CheckAtNextHalt               = 0x40,
  CheckImmediately              = 0x80
};

// ============================================================================
// DTC Format Identifier
// ============================================================================

enum class DTCFormatIdentifier : uint8_t {
  ISO15031_6DTCFormat           = 0x00,  // SAE J2012-DA
  ISO14229_1DTCFormat           = 0x01,  // UDS format
  SAEJ1939_73DTCFormat          = 0x02,
  ISO11992_4DTCFormat           = 0x03
};

// ============================================================================
// DTC Data Structures
// ============================================================================

// Single DTC with status
struct DTCRecord {
  uint32_t code{0};           // 3-byte DTC code (stored in lower 24 bits)
  uint8_t status{0};          // Status byte
  
  // Convenience accessors for status bits
  bool test_failed() const { return status & StatusMask::TestFailed; }
  bool test_failed_this_cycle() const { return status & StatusMask::TestFailedThisOperationCycle; }
  bool is_pending() const { return status & StatusMask::PendingDTC; }
  bool is_confirmed() const { return status & StatusMask::ConfirmedDTC; }
  bool warning_indicator() const { return status & StatusMask::WarningIndicatorRequested; }
};

// DTC with severity information
struct DTCWithSeverity : DTCRecord {
  DTCSeverity severity{DTCSeverity::NoSeverityAvailable};
  uint8_t functional_unit{0};
};

// DTC snapshot (freeze frame) record
struct DTCSnapshot {
  uint32_t dtc_code{0};
  uint8_t snapshot_record_number{0};
  std::vector<uint8_t> snapshot_data;
};

// DTC extended data record
struct DTCExtendedData {
  uint32_t dtc_code{0};
  uint8_t extended_data_record_number{0};
  std::vector<uint8_t> extended_data;
};

// Response structures
struct DTCCountResponse {
  uint8_t status_availability_mask{0};
  DTCFormatIdentifier format{DTCFormatIdentifier::ISO14229_1DTCFormat};
  uint16_t dtc_count{0};
};

struct DTCListResponse {
  uint8_t status_availability_mask{0};
  std::vector<DTCRecord> dtcs;
};

struct DTCSeverityListResponse {
  uint8_t status_availability_mask{0};
  std::vector<DTCWithSeverity> dtcs;
};

struct DTCSnapshotResponse {
  std::vector<DTCSnapshot> snapshots;
};

struct DTCExtendedDataResponse {
  uint32_t dtc_code{0};
  uint8_t status{0};
  std::vector<DTCExtendedData> records;
};

// ============================================================================
// Result Type
// ============================================================================

template<typename T>
struct Result {
  bool ok{false};
  T value{};
  NegativeResponse nrc{};
  
  static Result success(const T& v) {
    Result r; r.ok = true; r.value = v; return r;
  }
  
  static Result error(const NegativeResponse& n) {
    Result r; r.ok = false; r.nrc = n; return r;
  }
  
  static Result error() {
    Result r; r.ok = false; return r;
  }
};

template<>
struct Result<void> {
  bool ok{false};
  NegativeResponse nrc{};
  
  static Result success() {
    Result r; r.ok = true; return r;
  }
  
  static Result error(const NegativeResponse& n) {
    Result r; r.ok = false; r.nrc = n; return r;
  }
  
  static Result error() {
    Result r; r.ok = false; return r;
  }
};

// ============================================================================
// ReadDTCInformation API (Service 0x19)
// ============================================================================

/**
 * Get number of DTCs matching status mask
 * 
 * @param client UDS client instance
 * @param status_mask DTC status mask (use StatusMask constants)
 * @return Result with DTCCountResponse on success
 */
Result<DTCCountResponse> get_dtc_count(Client& client, uint8_t status_mask);

/**
 * Read DTCs by status mask
 * 
 * @param client UDS client instance
 * @param status_mask DTC status mask (use StatusMask constants)
 * @return Result with DTCListResponse on success
 */
Result<DTCListResponse> read_dtcs_by_status(Client& client, uint8_t status_mask);

/**
 * Read all supported DTCs (regardless of status)
 */
Result<DTCListResponse> read_supported_dtcs(Client& client);

/**
 * Read first test-failed DTC
 */
Result<DTCListResponse> read_first_test_failed_dtc(Client& client);

/**
 * Read first confirmed DTC
 */
Result<DTCListResponse> read_first_confirmed_dtc(Client& client);

/**
 * Read most recent test-failed DTC
 */
Result<DTCListResponse> read_most_recent_test_failed_dtc(Client& client);

/**
 * Read most recent confirmed DTC
 */
Result<DTCListResponse> read_most_recent_confirmed_dtc(Client& client);

/**
 * Read DTCs with permanent status (emissions-related)
 */
Result<DTCListResponse> read_permanent_dtcs(Client& client);

/**
 * Read DTCs by severity mask
 * 
 * @param client UDS client instance
 * @param severity_mask Severity mask
 * @param status_mask DTC status mask
 * @return Result with DTCSeverityListResponse on success
 */
Result<DTCSeverityListResponse> read_dtcs_by_severity(Client& client,
                                                       uint8_t severity_mask,
                                                       uint8_t status_mask);

/**
 * Read DTC snapshot (freeze frame) data
 * 
 * @param client UDS client instance
 * @param dtc_code DTC code (3 bytes)
 * @param record_number Snapshot record number (0xFF for all)
 * @return Result with DTCSnapshotResponse on success
 */
Result<DTCSnapshotResponse> read_dtc_snapshot(Client& client,
                                               uint32_t dtc_code,
                                               uint8_t record_number = 0xFF);

/**
 * Read DTC extended data
 * 
 * @param client UDS client instance
 * @param dtc_code DTC code (3 bytes)
 * @param record_number Extended data record number (0xFF for all)
 * @return Result with DTCExtendedDataResponse on success
 */
Result<DTCExtendedDataResponse> read_dtc_extended_data(Client& client,
                                                        uint32_t dtc_code,
                                                        uint8_t record_number = 0xFF);

/**
 * Generic ReadDTCInformation request
 * 
 * @param client UDS client instance
 * @param sub_function Sub-function code
 * @param record Additional record bytes
 * @return Raw response payload
 */
PositiveOrNegative read_dtc_information(Client& client,
                                        ReadDTCSubFunction sub_function,
                                        const std::vector<uint8_t>& record = {});

// ============================================================================
// ClearDiagnosticInformation API (Service 0x14)
// ============================================================================

/**
 * Clear all DTCs
 */
Result<void> clear_all_dtcs(Client& client);

/**
 * Clear DTCs by group
 * 
 * @param client UDS client instance
 * @param group_of_dtc 3-byte DTC group mask
 * @return Result indicating success/failure
 */
Result<void> clear_dtc_group(Client& client, uint32_t group_of_dtc);

/**
 * Clear powertrain DTCs (P-codes)
 */
Result<void> clear_powertrain_dtcs(Client& client);

/**
 * Clear chassis DTCs (C-codes)
 */
Result<void> clear_chassis_dtcs(Client& client);

/**
 * Clear body DTCs (B-codes)
 */
Result<void> clear_body_dtcs(Client& client);

/**
 * Clear network DTCs (U-codes)
 */
Result<void> clear_network_dtcs(Client& client);

// ============================================================================
// ControlDTCSetting API (Service 0x85)
// ============================================================================

/**
 * Enable DTC setting (normal operation)
 */
Result<void> enable_dtc_setting(Client& client);

/**
 * Disable DTC setting (suppress DTC logging)
 */
Result<void> disable_dtc_setting(Client& client);

/**
 * Check if DTC setting is currently enabled
 */
bool is_dtc_setting_enabled(const Client& client);

// ============================================================================
// RAII Guards
// ============================================================================

/**
 * RAII guard that automatically restores DTC setting on destruction.
 */
class DTCSettingGuard {
public:
  explicit DTCSettingGuard(Client& client);
  ~DTCSettingGuard();
  
  DTCSettingGuard(const DTCSettingGuard&) = delete;
  DTCSettingGuard& operator=(const DTCSettingGuard&) = delete;
  DTCSettingGuard(DTCSettingGuard&&) = delete;
  DTCSettingGuard& operator=(DTCSettingGuard&&) = delete;

private:
  Client& client_;
  bool saved_state_;
};

/**
 * RAII guard for flash programming that disables DTC setting.
 */
class FlashDTCGuard {
public:
  explicit FlashDTCGuard(Client& client);
  ~FlashDTCGuard();
  
  bool is_active() const { return active_; }
  
  FlashDTCGuard(const FlashDTCGuard&) = delete;
  FlashDTCGuard& operator=(const FlashDTCGuard&) = delete;
  FlashDTCGuard(FlashDTCGuard&&) = delete;
  FlashDTCGuard& operator=(FlashDTCGuard&&) = delete;

private:
  Client& client_;
  bool active_{false};
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Parse DTC code from 3 bytes
 */
uint32_t parse_dtc_code(const uint8_t* bytes);

/**
 * Encode DTC code to 3 bytes
 */
std::vector<uint8_t> encode_dtc_code(uint32_t dtc_code);

/**
 * Format DTC code as string (e.g., "P0123", "C0456", "B0789", "U0ABC")
 */
std::string format_dtc_code(uint32_t dtc_code);

/**
 * Parse DTC code from string (e.g., "P0123")
 */
uint32_t parse_dtc_string(const std::string& dtc_string);

/**
 * Get human-readable description of DTC status
 */
std::string describe_dtc_status(uint8_t status);

/**
 * Get human-readable name for severity level
 */
const char* severity_name(DTCSeverity severity);

/**
 * Get human-readable name for sub-function
 */
const char* subfunction_name(ReadDTCSubFunction sf);

// ============================================================================
// DTC Group Constants
// ============================================================================

namespace Group {
  constexpr uint32_t AllDTCs       = 0xFFFFFF;
  constexpr uint32_t Powertrain    = 0x000000;  // P-codes
  constexpr uint32_t Chassis       = 0x400000;  // C-codes
  constexpr uint32_t Body          = 0x800000;  // B-codes
  constexpr uint32_t Network       = 0xC00000;  // U-codes
}

} // namespace dtc
} // namespace uds

#endif // UDS_DTC_HPP
