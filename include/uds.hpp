#ifndef UDS_HPP
#define UDS_HPP

/**
 * @file uds.hpp
 * @brief Unified Diagnostic Services (UDS) – ISO 14229-1:2013 C++ Implementation
 *
 * This header provides a practical, open-source oriented interface for ISO 14229 style
 * diagnostics over CAN using ISO-TP (ISO 15765-2). The content is original and
 * paraphrased for software implementation purposes.
 *
 * ============================================================================
 * ISO 14229-1:2013 REFERENCE GUIDE
 * ============================================================================
 *
 * DOCUMENT STRUCTURE (for PDF page references):
 * - Section 1-3: Scope, References, Terms (pp. 1-4)
 * - Section 4: Conventions (p. 5)
 * - Section 5: Document Overview (p. 6)
 * - Section 6: Application Layer Services (pp. 7-14)
 * - Section 7: Application Layer Protocol (pp. 15-28)
 * - Section 8: Service Description Conventions (pp. 29-34)
 * - Section 9: Diagnostic and Communication Management (pp. 35-105)
 *   - 9.2: DiagnosticSessionControl (0x10) - p. 36
 *   - 9.3: ECUReset (0x11) - p. 43
 *   - 9.4: SecurityAccess (0x27) - p. 47
 *   - 9.5: CommunicationControl (0x28) - p. 53
 *   - 9.6: TesterPresent (0x3E) - p. 58
 *   - 9.7: AccessTimingParameter (0x83) - p. 61
 *   - 9.8: SecuredDataTransmission (0x84) - p. 66
 *   - 9.9: ControlDTCSetting (0x85) - p. 71
 *   - 9.10: ResponseOnEvent (0x86) - p. 75
 *   - 9.11: LinkControl (0x87) - p. 99
 * - Section 10: Data Transmission Functional Unit (pp. 106-173)
 *   - 10.2: ReadDataByIdentifier (0x22) - p. 106
 *   - 10.3: ReadMemoryByAddress (0x23) - p. 113
 *   - 10.4: ReadScalingDataByIdentifier (0x24) - p. 119
 *   - 10.5: ReadDataByPeriodicIdentifier (0x2A) - p. 126
 *   - 10.6: DynamicallyDefineDataIdentifier (0x2C) - p. 140
 *   - 10.7: WriteDataByIdentifier (0x2E) - p. 162
 *   - 10.8: WriteMemoryByAddress (0x3D) - p. 167
 * - Section 11: Stored Data Transmission (pp. 174-244)
 *   - 11.2: ClearDiagnosticInformation (0x14) - p. 175
 *   - 11.3: ReadDTCInformation (0x19) - p. 178
 * - Section 12: InputOutput Control (pp. 245-258)
 *   - 12.2: InputOutputControlByIdentifier (0x2F) - p. 245
 * - Section 13: Routine Functional Unit (pp. 259-269)
 *   - 13.2: RoutineControl (0x31) - p. 260
 * - Section 14: Upload Download (pp. 270-302)
 *   - 14.2: RequestDownload (0x34) - p. 270
 *   - 14.3: RequestUpload (0x35) - p. 275
 *   - 14.4: TransferData (0x36) - p. 280
 *   - 14.5: RequestTransferExit (0x37) - p. 285
 *   - 14.6: RequestFileTransfer (0x38) - p. 295
 * - Section 15: Non-volatile Memory Programming (pp. 303-324)
 * - Annex A: Negative Response Codes (p. 325)
 * - Annex B-J: Data Parameters, DIDs, Routines (pp. 333-391)
 *
 * TIMING PARAMETERS (Section 7.2, pp. 16-18):
 * - P2server_max: Max time for initial response (default 50ms)
 * - P2*server_max: Max time for response after NRC 0x78 (default 5000ms)
 * - S3server: Session timeout (default 5000ms)
 *
 * MESSAGE FORMAT (Section 7.2, pp. 16-17):
 * - Request:  [SID] [Sub-function] [Data...]
 * - Positive: [SID+0x40] [Sub-function echo] [Data...]
 * - Negative: [0x7F] [SID] [NRC]
 *
 * High-level layout (mirroring the standard structure):
 * 1) Concepts, addressing, PDUs, timings
 * 2) Service identifiers and sub-functions
 * 3) Negative response handling
 * 4) Service request/response models
 * 5) Client API and transport abstraction
 */

#include <cstdint>
#include <vector>
#include <string>
#include <chrono>
#include <functional>

namespace uds {

// ============================================================================
// 1) Concepts, addressing, PDUs, timings
//    ISO 14229-1:2013 Section 5-6 (pp. 6-14), Section 7 (pp. 15-28)
// ============================================================================

/**
 * @brief Addressing type for diagnostic communication
 * 
 * ISO 14229-1:2013 Section 6.2 (p. 8):
 * - Physical addressing: Point-to-point communication with single ECU
 * - Functional addressing: Broadcast to all ECUs (e.g., for TesterPresent)
 * 
 * Physical addressing uses specific CAN IDs (e.g., 0x7E0 -> 0x7E8)
 * Functional addressing uses broadcast ID (e.g., 0x7DF)
 */
enum class AddressType : uint8_t {
  Physical,   ///< Point-to-point (ISO 14229-1 Section 6.2)
  Functional  ///< Broadcast (ISO 14229-1 Section 6.2)
};

/**
 * @brief Addressing container for CAN communication
 * 
 * ISO 14229-1:2013 Section 6.2 (pp. 8-9):
 * Maps to CAN IDs per ISO 15765-2 (11-bit or 29-bit, normal/extended addressing)
 * 
 * Common OBD-II addressing:
 * - Tester -> ECU: 0x7E0-0x7E7 (physical) or 0x7DF (functional)
 * - ECU -> Tester: 0x7E8-0x7EF
 */
struct Address {
  AddressType type{AddressType::Physical};
  uint32_t tx_can_id{0}; ///< Tester->ECU CAN ID
  uint32_t rx_can_id{0}; ///< ECU->Tester CAN ID
};

/**
 * @brief UDS Protocol Data Unit (A_PDU)
 * 
 * ISO 14229-1:2013 Section 7.2 (pp. 16-17):
 * Raw service data unit bytes after ISO-TP reassembly.
 * First byte is always SID (request) or SID+0x40 (positive) or 0x7F (negative)
 */
struct PDU {
  std::vector<uint8_t> bytes; ///< First byte = SID or 0x7F for negative response
};

/**
 * @brief UDS Timing parameters
 * 
 * ISO 14229-1:2013 Section 7.2 Table 2 (pp. 17-18):
 * 
 * P2server_max (p2): Maximum time for server to start response
 *   - Default: 50ms, Range: 0-65535ms
 *   - After request, client waits this long for response
 * 
 * P2*server_max (p2_star): Extended response time after NRC 0x78
 *   - Default: 5000ms, Range: 0-655350ms (10ms resolution)
 *   - Used when server sends "ResponsePending" (0x78)
 * 
 * S3server: Session timeout (not shown here, typically 5000ms)
 *   - Time before server reverts to defaultSession
 */
struct Timings {
  std::chrono::milliseconds p2{std::chrono::milliseconds(50)};      ///< P2server_max (Table 2, p. 17)
  std::chrono::milliseconds p2_star{std::chrono::milliseconds(5000)}; ///< P2*server_max (Table 2, p. 17)
  std::chrono::milliseconds req_gap{std::chrono::milliseconds(0)};    ///< Minimum inter-request gap
};

// ============================================================================
// 2) Service identifiers (SID) and common sub-functions
//    ISO 14229-1:2013 Section 9-14 (pp. 35-302), Table 1 (p. 7)
//    Positive responses use SID + 0x40 (Section 8.3, p. 33)
// ============================================================================

/**
 * @brief UDS Service Identifiers (SID)
 * 
 * ISO 14229-1:2013 Table 1 (p. 7) - Service identifier assignments:
 * - 0x00-0x0F: Reserved
 * - 0x10-0x3E: Diagnostic and communication management
 * - 0x83-0x88: Remote activation of diagnostic
 * - 0xBA-0xBE: Reserved for ISO 14229-1
 * 
 * Positive response SID = Request SID + 0x40 (Section 8.3, p. 33)
 * Negative response always starts with 0x7F (Section 8.4, p. 34)
 */
enum class SID : uint8_t {
  // === Diagnostic Session Management (Section 9.2-9.3) ===
  DiagnosticSessionControl      = 0x10,  ///< Section 9.2 (p. 36) - Session control
  ECUReset                      = 0x11,  ///< Section 9.3 (p. 43) - ECU reset types
  
  // === DTC Services (Section 11.2-11.3) ===
  ClearDiagnosticInformation    = 0x14,  ///< Section 11.2 (p. 175) - Clear DTCs
  ReadDTCInformation            = 0x19,  ///< Section 11.3 (p. 178) - Read DTCs
  
  // === Data Services (Section 10.2-10.8) ===
  ReadDataByIdentifier          = 0x22,  ///< Section 10.2 (p. 106) - Read DID
  ReadMemoryByAddress           = 0x23,  ///< Section 10.3 (p. 113) - Read memory
  ReadScalingDataByIdentifier   = 0x24,  ///< Section 10.4 (p. 119) - Scaling info
  
  // === Security (Section 9.4-9.5) ===
  SecurityAccess                = 0x27,  ///< Section 9.4 (p. 47) - Seed/key auth
  CommunicationControl          = 0x28,  ///< Section 9.5 (p. 53) - Comm control
  Authentication                = 0x29,  ///< ISO 14229-1:2013 only - PKI auth
  
  // === Periodic/Dynamic Data (Section 10.5-10.6) ===
  ReadDataByPeriodicIdentifier  = 0x2A,  ///< Section 10.5 (p. 126) - Periodic DIDs
  DynamicallyDefineDataIdentifier = 0x2C, ///< Section 10.6 (p. 140) - Dynamic DIDs
  
  // === Keep-Alive ===
  TesterPresent                 = 0x3E,  ///< Section 9.6 (p. 58) - Session keep-alive

  // === Remote Activation (Section 9.7-9.11) ===
  AccessTimingParameters        = 0x83,  ///< Section 9.7 (p. 61) - Read/modify timing params
  SecuredDataTransmission       = 0x84,  ///< Section 9.8 (p. 66) - Encrypted data
  ControlDTCSetting             = 0x85,  ///< Section 9.9 (p. 71) - DTC on/off
  ResponseOnEvent               = 0x86,  ///< Section 9.10 (p. 75) - Event triggers
  LinkControl                   = 0x87,  ///< Section 9.11 (p. 99) - Baudrate control

  // === Write Services (Section 10.7-10.8) ===
  WriteDataByIdentifier         = 0x2E,  ///< Section 10.7 (p. 162) - Write DID
  InputOutputControlByIdentifier = 0x2F, ///< Section 12.2 (p. 245) - I/O control
  WriteMemoryByAddress          = 0x3D,  ///< Section 10.8 (p. 167) - Write memory

  // === Routine Control (Section 13.2) ===
  RoutineControl                = 0x31,  ///< Section 13.2 (p. 260) - Start/stop routines

  // === Upload/Download (Section 14.2-14.5) ===
  RequestDownload               = 0x34,  ///< Section 14.2 (p. 270) - Init download
  RequestUpload                 = 0x35,  ///< Section 14.3 (p. 275) - Init upload
  TransferData                  = 0x36,  ///< Section 14.4 (p. 280) - Data blocks
  RequestTransferExit           = 0x37   ///< Section 14.5 (p. 285) - End transfer
};

// ============================================================================
// Sub-function definitions for each service
// ISO 14229-1:2013 Section 9-14 - Service-specific sub-functions
// ============================================================================

/**
 * @brief DiagnosticSessionControl (0x10) sub-functions
 * 
 * ISO 14229-1:2013 Section 9.2.2.2 Table 25 (p. 39):
 * - 0x01: defaultSession - Normal operation mode
 * - 0x02: programmingSession - Flash programming mode
 * - 0x03: extendedDiagnosticSession - Extended diagnostics
 * - 0x04: safetySystemDiagnosticSession - Safety-critical access
 * - 0x05-0x3F: Reserved
 * - 0x40-0x5F: Vehicle manufacturer specific
 * - 0x60-0x7E: System supplier specific
 */
enum class Session : uint8_t {
  DefaultSession      = 0x01,  ///< Table 25 - Normal operation
  ProgrammingSession  = 0x02,  ///< Table 25 - Flash programming (requires security)
  ExtendedSession     = 0x03,  ///< Table 25 - Extended diagnostic functions
  SafetySystemSession = 0x04   ///< Table 25 - Safety system diagnostics
};

/**
 * @brief ECUReset (0x11) sub-functions
 * 
 * ISO 14229-1:2013 Section 9.3.2.2 Table 33 (p. 44):
 * - 0x01: hardReset - Power-on reset equivalent
 * - 0x02: keyOffOnReset - Ignition cycle simulation
 * - 0x03: softReset - Application restart
 * - 0x04: enableRapidPowerShutDown - Fast shutdown enable
 * - 0x05: disableRapidPowerShutDown - Fast shutdown disable
 */
enum class EcuResetType : uint8_t {
  HardReset             = 0x01,  ///< Table 33 - Full power cycle
  KeyOffOnReset         = 0x02,  ///< Table 33 - Ignition cycle
  SoftReset             = 0x03,  ///< Table 33 - Software restart
  EnableRapidPowerShut  = 0x04,  ///< Table 33 - Enable fast shutdown
  DisableRapidPowerShut = 0x05   ///< Table 33 - Disable fast shutdown
};

/**
 * @brief CommunicationControl (0x28) sub-functions
 * 
 * ISO 14229-1:2013 Section 9.5.2.2 Table 54 (p. 54):
 * Controls transmission and reception of messages on the network.
 * Critical for flash programming to prevent bus interference.
 * 
 * Bit 7 (suppressPosRspMsgIndicationBit) can be set to suppress positive response.
 */
enum class CommunicationControlType : uint8_t {
  EnableRxAndTx                      = 0x00,  ///< Table 54 - Normal operation
  EnableRxDisableTx                  = 0x01,  ///< Table 54 - Listen only
  DisableRxEnableTx                  = 0x02,  ///< Table 54 - Transmit only
  DisableRxAndTx                     = 0x03,  ///< Table 54 - Silent mode
  EnableRxAndTxWithEnhancedAddrInfo  = 0x04,  ///< Table 54 - With subnet
  EnableRxDisableTxWithEnhancedAddrInfo = 0x05,
  DisableRxEnableTxWithEnhancedAddrInfo = 0x06,
  DisableRxAndTxWithEnhancedAddrInfo = 0x07
  // 0x08–0x3F: ISO Reserved
  // 0x40–0x5F: Vehicle manufacturer specific
  // 0x60–0x7E: System supplier specific
};

/**
 * @brief CommunicationControl (0x28) communication type parameter
 * 
 * ISO 14229-1:2013 Section 9.5.2.3 Table 55 (p. 55):
 * Bitfield specifying which message types are affected.
 */
enum class CommunicationType : uint8_t {
  NormalCommunicationMessages = 0x01,  ///< Bit 0 - Application messages
  NetworkManagementMessages   = 0x02,  ///< Bit 1 - NM messages
  NetworkDownloadUpload       = 0x03   ///< Bits 0+1 - Both types
  // Bits 2-3: ISO Reserved
  // Bits 4-7: Vehicle manufacturer specific
};

/**
 * @brief RoutineControl (0x31) sub-functions
 * 
 * ISO 14229-1:2013 Section 13.2.2.2 Table 379 (p. 262):
 * - 0x01: startRoutine - Begin routine execution
 * - 0x02: stopRoutine - Abort routine execution
 * - 0x03: requestRoutineResults - Get routine output
 */
enum class RoutineAction : uint8_t {
  Start = 0x01,   ///< Table 379 - Start routine
  Stop  = 0x02,   ///< Table 379 - Stop routine
  Result= 0x03    ///< Table 379 - Request results
};

/**
 * @brief ControlDTCSetting (0x85) sub-functions
 * 
 * ISO 14229-1:2013 Section 9.9.2.2 Table 87 (p. 72):
 * - 0x01: on - Enable DTC status bit updates
 * - 0x02: off - Disable DTC status bit updates
 * 
 * CRITICAL: Must disable DTC setting during flash programming to prevent
 * false DTCs from being stored due to interrupted communication.
 */
enum class DTCSettingType : uint8_t {
  On  = 0x01,  ///< Table 87 - Enable DTC logging
  Off = 0x02   ///< Table 87 - Disable DTC logging (for programming)
  // 0x03–0x3F: ISO Reserved
  // 0x40–0x5F: Vehicle manufacturer specific
  // 0x60–0x7E: System supplier specific
};

/**
 * @brief AccessTimingParameters (0x83) sub-functions
 * 
 * ISO 14229-1:2013 Section 9.7.2.2 Table 74 (p. 63):
 * Allows reading and modifying P2/P2* timing parameters.
 */
enum class AccessTimingParametersType : uint8_t {
  ReadExtendedTimingParameterSet = 0x01,      ///< Read extended set
  SetTimingParametersToDefaultValues = 0x02,  ///< Reset to defaults
  ReadCurrentlyActiveTimingParameters = 0x03, ///< Read current values
  SetTimingParametersToGivenValues = 0x04     ///< Set custom values
};

// ReadDataByPeriodicIdentifier (0x2A) transmission modes
enum class PeriodicTransmissionMode : uint8_t {
  SendAtSlowRate       = 0x01,  // Slow rate (typically > 1 Hz)
  SendAtMediumRate     = 0x02,  // Medium rate
  SendAtFastRate       = 0x03,  // Fast rate (typically 10+ Hz)
  StopSending          = 0x04   // Stop periodic transmission
  // 0x05–0x7F Reserved
  // 0x80–0xFF OEM-specific rates
};

// DynamicallyDefineDataIdentifier (0x2C) sub‑functions
enum class DDDISubFunction : uint8_t {
  DefineByIdentifier  = 0x01,  // Define by DID references
  DefineByMemoryAddress = 0x02, // Define by memory address
  ClearDynamicallyDefinedDataIdentifier = 0x03 // Clear a DDDI
  // 0x04–0xFF Reserved/OEM-specific
};

// ================================================================
// 3) Negative response handling
// ================================================================

enum class NegativeResponseCode : uint8_t {
  GeneralReject                    = 0x10,
  ServiceNotSupported              = 0x11,
  SubFunctionNotSupported          = 0x12,
  IncorrectMessageLengthOrFormat   = 0x13,
  ResponseTooLong                  = 0x14,
  BusyRepeatRequest                = 0x21,
  ConditionsNotCorrect             = 0x22,
  RequestSequenceError             = 0x24,
  RequestOutOfRange                = 0x31,
  SecurityAccessDenied             = 0x33,
  InvalidKey                       = 0x35,
  ExceededNumberOfAttempts         = 0x36,
  RequiredTimeDelayNotExpired      = 0x37,
  UploadDownloadNotAccepted        = 0x70,
  TransferDataSuspended            = 0x71,
  GeneralProgrammingFailure        = 0x72,
  WrongBlockSequenceCounter        = 0x73,
  RequestCorrectlyReceived_ResponsePending = 0x78, // RCR-RP
  SubFunctionNotSupportedInActiveSession   = 0x7E,
  ServiceNotSupportedInActiveSession       = 0x7F
};

// Detect positive response SID (request SID + 0x40)
constexpr uint8_t kPositiveResponseOffset = 0x40;

inline bool is_positive_response(uint8_t sid_rx, uint8_t sid_req) {
  return sid_rx == static_cast<uint8_t>(sid_req) + kPositiveResponseOffset;
}

// ================================================================
// 4) Service request/response models (selected common services)
//    Each builder encodes the request; the parser extracts key fields
//    from a positive response or detects a negative response.
// ================================================================

using DID = uint16_t;      // Data Identifier
using RoutineId = uint16_t;// Routine Identifier
using BlockCounter = uint8_t;

struct NegativeResponse {
  SID original_sid{};             // service that was rejected
  NegativeResponseCode code{};    // NRC
};

struct PositiveOrNegative {
  bool ok{false};
  NegativeResponse nrc{};             // valid if ok==false and bytes[0]==0x7F
  std::vector<uint8_t> payload;       // positive response payload (after SID)
};

// ------------------------- DiagnosticSessionControl (0x10)
struct DSC_Request { Session session; };

// Positive response typically echoes session and may include timing (S3 or P2/P2*)
struct DSC_Response { Session session; std::vector<uint8_t> params; };

// ------------------------- ECUReset (0x11)
struct ECUReset_Request { EcuResetType type; };
struct ECUReset_Response { EcuResetType type; std::vector<uint8_t> powerdown_time; };

// ------------------------- TesterPresent (0x3E)
struct TesterPresent_Request { bool suppress_response{true}; };

// ------------------------- SecurityAccess (0x27)
struct SecurityAccess_RequestSeed { uint8_t level; };
struct SecurityAccess_SendKey  { uint8_t level; std::vector<uint8_t> key; };
struct SecurityAccess_SeedResp { std::vector<uint8_t> seed; };

// ------------------------- Read/WriteDataByIdentifier (0x22/0x2E)
struct ReadDID_Request { DID did; };
struct ReadDID_Response { DID did; std::vector<uint8_t> data; };

struct WriteDID_Request { DID did; std::vector<uint8_t> data; };
struct WriteDID_Response { DID did; };

// ------------------------- DynamicallyDefineDataIdentifier (0x2C)
// Define by source DID - references other DIDs and optionally position+size
struct DDDI_SourceByDID {
  DID source_did;
  uint8_t position{0};   // byte position in source DID (1-based, 0 = full DID)
  uint8_t mem_size{0};   // number of bytes to extract (0 = all remaining)
};

// Define by memory address
struct DDDI_SourceByMemory {
  uint8_t address_and_length_format_id; // ALFID
  std::vector<uint8_t> memory_address;
  std::vector<uint8_t> memory_size;
};

struct DDDI_DefineByIdentifier_Request {
  DID dynamic_did;  // The new dynamically defined DID to create
  std::vector<DDDI_SourceByDID> sources; // List of source DIDs
};

struct DDDI_DefineByMemoryAddress_Request {
  DID dynamic_did;  // The new dynamically defined DID to create
  std::vector<DDDI_SourceByMemory> sources; // List of memory sources
};

struct DDDI_Clear_Request {
  DID dynamic_did;  // The DID to clear
};

// ------------------------- ReadDataByPeriodicIdentifier (0x2A)
// Periodic identifiers use single-byte identifiers (not 16-bit DIDs)
using PeriodicDID = uint8_t;

struct PeriodicData_Request {
  PeriodicTransmissionMode mode;
  std::vector<PeriodicDID> identifiers;  // List of periodic DIDs to start/stop
};

// Periodic data message structure (ECU-initiated)
struct PeriodicDataMessage {
  PeriodicDID identifier;
  std::vector<uint8_t> data;
};

// ------------------------- RoutineControl (0x31)
struct RoutineControl_Request {
  RoutineAction action;
  RoutineId id;
  std::vector<uint8_t> optRecord; // optional data record
};
struct RoutineControl_Response { RoutineAction action; RoutineId id; std::vector<uint8_t> resultRecord; };

// ------------------------- Read/Clear DTC Information (0x19/0x14)
using DTC = uint32_t; // typically 24 bits used; store in 32 for convenience

struct ReadDTC_Request { uint8_t subFunction; std::vector<uint8_t> record; };
struct ReadDTC_Response { uint8_t subFunction; std::vector<uint8_t> payload; };

struct ClearDTC_Request { std::vector<uint8_t> groupOfDTC; };

// ------------------------- RequestDownload/Upload & Transfer (0x34/0x35/0x36/0x37)
struct RequestDownload_Request {
  uint8_t dataFormatId{0x00}; // default: uncompressed/unencrypted
  std::vector<uint8_t> memoryAddress; // length‑prefixed by builder
  std::vector<uint8_t> memorySize;    // length‑prefixed by builder
};
struct RequestDownload_Response { uint8_t lengthFormatId{0}; std::vector<uint8_t> maxNumberOfBlockLength; };

struct TransferData_Request { BlockCounter block; std::vector<uint8_t> data; };
struct TransferData_Response { BlockCounter block; std::vector<uint8_t> data; };

// ================================================================
// 5) Transport abstraction + Client API
// ================================================================

// ISO‑TP/transport abstraction: a minimal, blocking request‑response channel.
// Implementations must handle segmentation, flow control, timeouts, etc.
class Transport {
public:
  virtual ~Transport() = default;
  virtual void set_address(const Address&) = 0;
  virtual const Address& address() const = 0;

  // Send a complete UDS SDU and wait for the full response SDU.
  // Returns true on success; false on timeout or transport error.
  virtual bool request_response(const std::vector<uint8_t>& tx,
                                std::vector<uint8_t>& rx,
                                std::chrono::milliseconds timeout) = 0;
  
  // Optional: receive unsolicited messages (for periodic data)
  // Returns true if a message was received, false on timeout
  // Default implementation returns false (not supported)
  virtual bool recv_unsolicited(std::vector<uint8_t>& rx,
                                std::chrono::milliseconds timeout) {
    (void)rx; (void)timeout;
    return false;
  }
};

// Helper: encode/decode building blocks
namespace codec {
  // append big‑endian integers
  inline void be16(std::vector<uint8_t>& v, uint16_t x){ v.push_back(uint8_t(x>>8)); v.push_back(uint8_t(x)); }
  inline void be24(std::vector<uint8_t>& v, uint32_t x){ v.push_back(uint8_t(x>>16)); v.push_back(uint8_t(x>>8)); v.push_back(uint8_t(x)); }
  inline void be32(std::vector<uint8_t>& v, uint32_t x){ v.push_back(uint8_t(x>>24)); v.push_back(uint8_t(x>>16)); v.push_back(uint8_t(x>>8)); v.push_back(uint8_t(x)); }
}

// UDS client: synchronous helpers for common services
class Client {
public:
  Client(Transport& t, Timings timings = {}) : t_(t), timings_(timings) {}

  // Core exchange primitive with NRC parsing
  PositiveOrNegative exchange(SID sid, const std::vector<uint8_t>& req_payload,
                              std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

  // --------- Selected service helpers (encode request, parse positive response)
  PositiveOrNegative diagnostic_session_control(Session s);
  PositiveOrNegative ecu_reset(EcuResetType type);
  PositiveOrNegative tester_present(bool suppress_response = true);

  PositiveOrNegative security_access_request_seed(uint8_t level);
  PositiveOrNegative security_access_send_key(uint8_t level, const std::vector<uint8_t>& key);

  PositiveOrNegative read_data_by_identifier(DID did);
  PositiveOrNegative read_scaling_data_by_identifier(DID did);
  PositiveOrNegative write_data_by_identifier(DID did, const std::vector<uint8_t>& data);

  PositiveOrNegative dynamically_define_data_identifier_by_did(DID dynamic_did, const std::vector<DDDI_SourceByDID>& sources);
  PositiveOrNegative dynamically_define_data_identifier_by_memory(DID dynamic_did, const std::vector<DDDI_SourceByMemory>& sources);
  PositiveOrNegative clear_dynamically_defined_data_identifier(DID dynamic_did);

  PositiveOrNegative read_memory_by_address(uint32_t address, uint32_t size);
  PositiveOrNegative read_memory_by_address(const std::vector<uint8_t>& addr,
                                            const std::vector<uint8_t>& size);
  PositiveOrNegative write_memory_by_address(uint32_t address, const std::vector<uint8_t>& data);
  PositiveOrNegative write_memory_by_address(const std::vector<uint8_t>& addr,
                                             const std::vector<uint8_t>& size,
                                             const std::vector<uint8_t>& data);

  PositiveOrNegative routine_control(RoutineAction action, RoutineId id, const std::vector<uint8_t>& record = {});

  PositiveOrNegative clear_diagnostic_information(const std::vector<uint8_t>& group_of_dtc);
  PositiveOrNegative read_dtc_information(uint8_t subFunction, const std::vector<uint8_t>& record = {});

  PositiveOrNegative request_download(uint8_t dfi,
                                      const std::vector<uint8_t>& addr,
                                      const std::vector<uint8_t>& size);
  PositiveOrNegative request_upload(uint8_t dfi,
                                    const std::vector<uint8_t>& addr,
                                    const std::vector<uint8_t>& size);
  PositiveOrNegative transfer_data(BlockCounter block, const std::vector<uint8_t>& data);
  PositiveOrNegative request_transfer_exit(const std::vector<uint8_t>& opt = {});

  PositiveOrNegative communication_control(uint8_t subFunction, uint8_t communicationType);
  PositiveOrNegative control_dtc_setting(uint8_t settingType);
  PositiveOrNegative access_timing_parameters(AccessTimingParametersType type, const std::vector<uint8_t>& record = {});

  // ReadDataByPeriodicIdentifier (0x2A) - ECU-initiated streaming
  PositiveOrNegative read_data_by_periodic_identifier(PeriodicTransmissionMode mode,
                                                      const std::vector<PeriodicDID>& identifiers);
  
  // Helper to start periodic transmission
  PositiveOrNegative start_periodic_transmission(PeriodicTransmissionMode rate,
                                                 const std::vector<PeriodicDID>& identifiers) {
    return read_data_by_periodic_identifier(rate, identifiers);
  }
  
  // Helper to stop periodic transmission
  PositiveOrNegative stop_periodic_transmission(const std::vector<PeriodicDID>& identifiers) {
    return read_data_by_periodic_identifier(PeriodicTransmissionMode::StopSending, identifiers);
  }
  
  // Receive periodic data (non-blocking with timeout)
  // Returns true if periodic message received and parsed
  bool receive_periodic_data(PeriodicDataMessage& msg, std::chrono::milliseconds timeout);

  // Accessors
  void set_timings(const Timings& t) { timings_ = t; }
  const Timings& timings() const { return timings_; }

  // Communication state management
  struct CommunicationState {
    bool rx_enabled{true};
    bool tx_enabled{true};
    uint8_t active_comm_type{0x01}; // default: normal messages
  };
  
  const CommunicationState& communication_state() const { return comm_state_; }
  void reset_communication_state() { comm_state_ = CommunicationState{}; }
  
  // DTC setting state
  bool is_dtc_setting_enabled() const { return dtc_setting_enabled_; }
  void reset_dtc_setting_state() { dtc_setting_enabled_ = true; }

private:
  Transport& t_;
  Timings timings_{};
  CommunicationState comm_state_{};
  bool dtc_setting_enabled_{true}; // Default: DTC setting is ON
};

} // namespace uds

#endif // UDS_HPP
