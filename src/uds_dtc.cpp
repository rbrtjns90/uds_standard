#include "uds_dtc.hpp"
#include <sstream>
#include <iomanip>
#include <cctype>

namespace uds {
namespace dtc {

// ============================================================================
// DTC Code Parsing/Encoding
// ============================================================================

uint32_t parse_dtc_code(const uint8_t* bytes) {
  return (static_cast<uint32_t>(bytes[0]) << 16) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         static_cast<uint32_t>(bytes[2]);
}

std::vector<uint8_t> encode_dtc_code(uint32_t dtc_code) {
  return {
    static_cast<uint8_t>((dtc_code >> 16) & 0xFF),
    static_cast<uint8_t>((dtc_code >> 8) & 0xFF),
    static_cast<uint8_t>(dtc_code & 0xFF)
  };
}

std::string format_dtc_code(uint32_t dtc_code) {
  // DTC format: First 2 bits determine type (P/C/B/U)
  // Remaining bits are the numeric code
  char type_char;
  uint8_t first_byte = (dtc_code >> 16) & 0xFF;
  uint8_t type_bits = (first_byte >> 6) & 0x03;
  
  switch (type_bits) {
    case 0: type_char = 'P'; break;  // Powertrain
    case 1: type_char = 'C'; break;  // Chassis
    case 2: type_char = 'B'; break;  // Body
    case 3: type_char = 'U'; break;  // Network
    default: type_char = '?'; break;
  }
  
  // Extract the numeric portion
  uint16_t numeric = ((first_byte & 0x3F) << 8) | ((dtc_code >> 8) & 0xFF);
  uint8_t last_byte = dtc_code & 0xFF;
  
  std::ostringstream oss;
  oss << type_char << std::hex << std::uppercase 
      << std::setfill('0') << std::setw(2) << (numeric >> 8)
      << std::setw(2) << (numeric & 0xFF)
      << std::setw(2) << static_cast<int>(last_byte);
  
  // Simplified format: just show as hex
  oss.str("");
  oss << type_char << std::hex << std::uppercase 
      << std::setfill('0') << std::setw(4) << (dtc_code & 0xFFFF);
  
  return oss.str();
}

uint32_t parse_dtc_string(const std::string& dtc_string) {
  if (dtc_string.length() < 5) {
    return 0;
  }
  
  uint8_t type_bits = 0;
  switch (std::toupper(dtc_string[0])) {
    case 'P': type_bits = 0; break;
    case 'C': type_bits = 1; break;
    case 'B': type_bits = 2; break;
    case 'U': type_bits = 3; break;
    default: return 0;
  }
  
  // Parse hex digits
  uint32_t numeric = 0;
  for (size_t i = 1; i < dtc_string.length() && i < 5; ++i) {
    char c = dtc_string[i];
    uint8_t digit = 0;
    if (c >= '0' && c <= '9') digit = c - '0';
    else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
    else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
    else return 0;
    numeric = (numeric << 4) | digit;
  }
  
  return (static_cast<uint32_t>(type_bits) << 22) | numeric;
}

std::string describe_dtc_status(uint8_t status) {
  std::ostringstream oss;
  bool first = true;
  
  auto add_flag = [&](const char* name) {
    if (!first) oss << ", ";
    oss << name;
    first = false;
  };
  
  if (status & StatusMask::TestFailed) add_flag("TestFailed");
  if (status & StatusMask::TestFailedThisOperationCycle) add_flag("TestFailedThisCycle");
  if (status & StatusMask::PendingDTC) add_flag("Pending");
  if (status & StatusMask::ConfirmedDTC) add_flag("Confirmed");
  if (status & StatusMask::TestNotCompletedSinceLastClear) add_flag("NotCompletedSinceClear");
  if (status & StatusMask::TestFailedSinceLastClear) add_flag("FailedSinceClear");
  if (status & StatusMask::TestNotCompletedThisOperationCycle) add_flag("NotCompletedThisCycle");
  if (status & StatusMask::WarningIndicatorRequested) add_flag("WarningIndicator");
  
  if (first) return "None";
  return oss.str();
}

const char* severity_name(DTCSeverity severity) {
  switch (severity) {
    case DTCSeverity::NoSeverityAvailable: return "No Severity Available";
    case DTCSeverity::MaintenanceOnly:     return "Maintenance Only";
    case DTCSeverity::CheckAtNextHalt:     return "Check At Next Halt";
    case DTCSeverity::CheckImmediately:    return "Check Immediately";
    default:                               return "Unknown";
  }
}

const char* subfunction_name(ReadDTCSubFunction sf) {
  switch (sf) {
    case ReadDTCSubFunction::ReportNumberOfDTCByStatusMask:
      return "ReportNumberOfDTCByStatusMask";
    case ReadDTCSubFunction::ReportDTCByStatusMask:
      return "ReportDTCByStatusMask";
    case ReadDTCSubFunction::ReportDTCSnapshotIdentification:
      return "ReportDTCSnapshotIdentification";
    case ReadDTCSubFunction::ReportDTCSnapshotRecordByDTCNumber:
      return "ReportDTCSnapshotRecordByDTCNumber";
    case ReadDTCSubFunction::ReportDTCExtDataRecordByDTCNumber:
      return "ReportDTCExtDataRecordByDTCNumber";
    case ReadDTCSubFunction::ReportSupportedDTC:
      return "ReportSupportedDTC";
    case ReadDTCSubFunction::ReportFirstTestFailedDTC:
      return "ReportFirstTestFailedDTC";
    case ReadDTCSubFunction::ReportFirstConfirmedDTC:
      return "ReportFirstConfirmedDTC";
    case ReadDTCSubFunction::ReportMostRecentTestFailedDTC:
      return "ReportMostRecentTestFailedDTC";
    case ReadDTCSubFunction::ReportMostRecentConfirmedDTC:
      return "ReportMostRecentConfirmedDTC";
    case ReadDTCSubFunction::ReportDTCWithPermanentStatus:
      return "ReportDTCWithPermanentStatus";
    default:
      return "Unknown";
  }
}

// ============================================================================
// ReadDTCInformation Implementation
// ============================================================================

PositiveOrNegative read_dtc_information(Client& client,
                                        ReadDTCSubFunction sub_function,
                                        const std::vector<uint8_t>& record) {
  return client.read_dtc_information(static_cast<uint8_t>(sub_function), record);
}

Result<DTCCountResponse> get_dtc_count(Client& client, uint8_t status_mask) {
  auto result = read_dtc_information(client,
                                     ReadDTCSubFunction::ReportNumberOfDTCByStatusMask,
                                     {status_mask});
  
  if (!result.ok) {
    return Result<DTCCountResponse>::error(result.nrc);
  }
  
  // Response: [subFunction][statusAvailabilityMask][DTCFormatIdentifier][DTCCount(2)]
  if (result.payload.size() < 4) {
    return Result<DTCCountResponse>::error();
  }
  
  DTCCountResponse response;
  response.status_availability_mask = result.payload[1];
  response.format = static_cast<DTCFormatIdentifier>(result.payload[2]);
  response.dtc_count = (static_cast<uint16_t>(result.payload[3]) << 8);
  if (result.payload.size() > 4) {
    response.dtc_count |= result.payload[4];
  }
  
  return Result<DTCCountResponse>::success(response);
}

Result<DTCListResponse> read_dtcs_by_status(Client& client, uint8_t status_mask) {
  auto result = read_dtc_information(client,
                                     ReadDTCSubFunction::ReportDTCByStatusMask,
                                     {status_mask});
  
  if (!result.ok) {
    return Result<DTCListResponse>::error(result.nrc);
  }
  
  // Response: [subFunction][statusAvailabilityMask][DTC(3)+Status(1)]...
  DTCListResponse response;
  
  if (result.payload.size() >= 2) {
    response.status_availability_mask = result.payload[1];
  }
  
  // Parse DTC records (4 bytes each: 3 bytes DTC + 1 byte status)
  size_t offset = 2;
  while (offset + 4 <= result.payload.size()) {
    DTCRecord dtc;
    dtc.code = parse_dtc_code(&result.payload[offset]);
    dtc.status = result.payload[offset + 3];
    response.dtcs.push_back(dtc);
    offset += 4;
  }
  
  return Result<DTCListResponse>::success(response);
}

Result<DTCListResponse> read_supported_dtcs(Client& client) {
  auto result = read_dtc_information(client,
                                     ReadDTCSubFunction::ReportSupportedDTC,
                                     {});
  
  if (!result.ok) {
    return Result<DTCListResponse>::error(result.nrc);
  }
  
  DTCListResponse response;
  
  if (result.payload.size() >= 2) {
    response.status_availability_mask = result.payload[1];
  }
  
  size_t offset = 2;
  while (offset + 4 <= result.payload.size()) {
    DTCRecord dtc;
    dtc.code = parse_dtc_code(&result.payload[offset]);
    dtc.status = result.payload[offset + 3];
    response.dtcs.push_back(dtc);
    offset += 4;
  }
  
  return Result<DTCListResponse>::success(response);
}

Result<DTCListResponse> read_first_test_failed_dtc(Client& client) {
  auto result = read_dtc_information(client,
                                     ReadDTCSubFunction::ReportFirstTestFailedDTC,
                                     {});
  
  if (!result.ok) {
    return Result<DTCListResponse>::error(result.nrc);
  }
  
  DTCListResponse response;
  
  if (result.payload.size() >= 2) {
    response.status_availability_mask = result.payload[1];
  }
  
  if (result.payload.size() >= 6) {
    DTCRecord dtc;
    dtc.code = parse_dtc_code(&result.payload[2]);
    dtc.status = result.payload[5];
    response.dtcs.push_back(dtc);
  }
  
  return Result<DTCListResponse>::success(response);
}

Result<DTCListResponse> read_first_confirmed_dtc(Client& client) {
  auto result = read_dtc_information(client,
                                     ReadDTCSubFunction::ReportFirstConfirmedDTC,
                                     {});
  
  if (!result.ok) {
    return Result<DTCListResponse>::error(result.nrc);
  }
  
  DTCListResponse response;
  
  if (result.payload.size() >= 2) {
    response.status_availability_mask = result.payload[1];
  }
  
  if (result.payload.size() >= 6) {
    DTCRecord dtc;
    dtc.code = parse_dtc_code(&result.payload[2]);
    dtc.status = result.payload[5];
    response.dtcs.push_back(dtc);
  }
  
  return Result<DTCListResponse>::success(response);
}

Result<DTCListResponse> read_most_recent_test_failed_dtc(Client& client) {
  auto result = read_dtc_information(client,
                                     ReadDTCSubFunction::ReportMostRecentTestFailedDTC,
                                     {});
  
  if (!result.ok) {
    return Result<DTCListResponse>::error(result.nrc);
  }
  
  DTCListResponse response;
  
  if (result.payload.size() >= 2) {
    response.status_availability_mask = result.payload[1];
  }
  
  if (result.payload.size() >= 6) {
    DTCRecord dtc;
    dtc.code = parse_dtc_code(&result.payload[2]);
    dtc.status = result.payload[5];
    response.dtcs.push_back(dtc);
  }
  
  return Result<DTCListResponse>::success(response);
}

Result<DTCListResponse> read_most_recent_confirmed_dtc(Client& client) {
  auto result = read_dtc_information(client,
                                     ReadDTCSubFunction::ReportMostRecentConfirmedDTC,
                                     {});
  
  if (!result.ok) {
    return Result<DTCListResponse>::error(result.nrc);
  }
  
  DTCListResponse response;
  
  if (result.payload.size() >= 2) {
    response.status_availability_mask = result.payload[1];
  }
  
  if (result.payload.size() >= 6) {
    DTCRecord dtc;
    dtc.code = parse_dtc_code(&result.payload[2]);
    dtc.status = result.payload[5];
    response.dtcs.push_back(dtc);
  }
  
  return Result<DTCListResponse>::success(response);
}

Result<DTCListResponse> read_permanent_dtcs(Client& client) {
  auto result = read_dtc_information(client,
                                     ReadDTCSubFunction::ReportDTCWithPermanentStatus,
                                     {});
  
  if (!result.ok) {
    return Result<DTCListResponse>::error(result.nrc);
  }
  
  DTCListResponse response;
  
  if (result.payload.size() >= 2) {
    response.status_availability_mask = result.payload[1];
  }
  
  size_t offset = 2;
  while (offset + 4 <= result.payload.size()) {
    DTCRecord dtc;
    dtc.code = parse_dtc_code(&result.payload[offset]);
    dtc.status = result.payload[offset + 3];
    response.dtcs.push_back(dtc);
    offset += 4;
  }
  
  return Result<DTCListResponse>::success(response);
}

Result<DTCSeverityListResponse> read_dtcs_by_severity(Client& client,
                                                       uint8_t severity_mask,
                                                       uint8_t status_mask) {
  auto result = read_dtc_information(client,
                                     ReadDTCSubFunction::ReportDTCBySeverityMaskRecord,
                                     {severity_mask, status_mask});
  
  if (!result.ok) {
    return Result<DTCSeverityListResponse>::error(result.nrc);
  }
  
  DTCSeverityListResponse response;
  
  if (result.payload.size() >= 2) {
    response.status_availability_mask = result.payload[1];
  }
  
  // Parse DTC records with severity (6 bytes each)
  size_t offset = 2;
  while (offset + 6 <= result.payload.size()) {
    DTCWithSeverity dtc;
    dtc.severity = static_cast<DTCSeverity>(result.payload[offset]);
    dtc.functional_unit = result.payload[offset + 1];
    dtc.code = parse_dtc_code(&result.payload[offset + 2]);
    dtc.status = result.payload[offset + 5];
    response.dtcs.push_back(dtc);
    offset += 6;
  }
  
  return Result<DTCSeverityListResponse>::success(response);
}

Result<DTCSnapshotResponse> read_dtc_snapshot(Client& client,
                                               uint32_t dtc_code,
                                               uint8_t record_number) {
  auto dtc_bytes = encode_dtc_code(dtc_code);
  dtc_bytes.push_back(record_number);
  
  auto result = read_dtc_information(client,
                                     ReadDTCSubFunction::ReportDTCSnapshotRecordByDTCNumber,
                                     dtc_bytes);
  
  if (!result.ok) {
    return Result<DTCSnapshotResponse>::error(result.nrc);
  }
  
  DTCSnapshotResponse response;
  
  // Parse snapshot records
  // Format varies by ECU implementation
  // Basic parsing: [subFunction][DTC(3)][status][recordNumber][data...]
  if (result.payload.size() >= 6) {
    DTCSnapshot snapshot;
    snapshot.dtc_code = parse_dtc_code(&result.payload[1]);
    snapshot.snapshot_record_number = result.payload[5];
    if (result.payload.size() > 6) {
      snapshot.snapshot_data.assign(result.payload.begin() + 6, result.payload.end());
    }
    response.snapshots.push_back(snapshot);
  }
  
  return Result<DTCSnapshotResponse>::success(response);
}

Result<DTCExtendedDataResponse> read_dtc_extended_data(Client& client,
                                                        uint32_t dtc_code,
                                                        uint8_t record_number) {
  auto dtc_bytes = encode_dtc_code(dtc_code);
  dtc_bytes.push_back(record_number);
  
  auto result = read_dtc_information(client,
                                     ReadDTCSubFunction::ReportDTCExtDataRecordByDTCNumber,
                                     dtc_bytes);
  
  if (!result.ok) {
    return Result<DTCExtendedDataResponse>::error(result.nrc);
  }
  
  DTCExtendedDataResponse response;
  
  // Parse extended data records
  // Format: [subFunction][DTC(3)][status][recordNumber][data...]
  if (result.payload.size() >= 5) {
    response.dtc_code = parse_dtc_code(&result.payload[1]);
    response.status = result.payload[4];
    
    if (result.payload.size() > 5) {
      DTCExtendedData record;
      record.dtc_code = response.dtc_code;
      record.extended_data_record_number = result.payload[5];
      if (result.payload.size() > 6) {
        record.extended_data.assign(result.payload.begin() + 6, result.payload.end());
      }
      response.records.push_back(record);
    }
  }
  
  return Result<DTCExtendedDataResponse>::success(response);
}

// ============================================================================
// ClearDiagnosticInformation Implementation
// ============================================================================

Result<void> clear_all_dtcs(Client& client) {
  return clear_dtc_group(client, Group::AllDTCs);
}

Result<void> clear_dtc_group(Client& client, uint32_t group_of_dtc) {
  auto group_bytes = encode_dtc_code(group_of_dtc);
  auto result = client.clear_diagnostic_information(group_bytes);
  
  if (!result.ok) {
    return Result<void>::error(result.nrc);
  }
  
  return Result<void>::success();
}

Result<void> clear_powertrain_dtcs(Client& client) {
  return clear_dtc_group(client, Group::Powertrain);
}

Result<void> clear_chassis_dtcs(Client& client) {
  return clear_dtc_group(client, Group::Chassis);
}

Result<void> clear_body_dtcs(Client& client) {
  return clear_dtc_group(client, Group::Body);
}

Result<void> clear_network_dtcs(Client& client) {
  return clear_dtc_group(client, Group::Network);
}

// ============================================================================
// ControlDTCSetting Implementation
// ============================================================================

Result<void> enable_dtc_setting(Client& client) {
  auto result = client.control_dtc_setting(static_cast<uint8_t>(DTCSettingType::On));
  
  if (!result.ok) {
    return Result<void>::error(result.nrc);
  }
  
  return Result<void>::success();
}

Result<void> disable_dtc_setting(Client& client) {
  auto result = client.control_dtc_setting(static_cast<uint8_t>(DTCSettingType::Off));
  
  if (!result.ok) {
    return Result<void>::error(result.nrc);
  }
  
  return Result<void>::success();
}

bool is_dtc_setting_enabled(const Client& client) {
  return client.is_dtc_setting_enabled();
}

// ============================================================================
// RAII Guards
// ============================================================================

DTCSettingGuard::DTCSettingGuard(Client& client)
    : client_(client), saved_state_(client.is_dtc_setting_enabled()) {}

DTCSettingGuard::~DTCSettingGuard() {
  if (saved_state_) {
    enable_dtc_setting(client_);
  } else {
    disable_dtc_setting(client_);
  }
}

FlashDTCGuard::FlashDTCGuard(Client& client) : client_(client) {
  auto result = disable_dtc_setting(client_);
  active_ = result.ok;
}

FlashDTCGuard::~FlashDTCGuard() {
  if (active_) {
    enable_dtc_setting(client_);
  }
}

} // namespace dtc
} // namespace uds
