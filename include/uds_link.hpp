#ifndef UDS_LINK_HPP
#define UDS_LINK_HPP

/*
  LinkControl (Service 0x87) - Communication Link Parameter Control
  
  ISO 14229-1:2013 Section 9.11 (p. 99)
  
  This header provides types and functions for the LinkControl service
  defined in ISO 14229-1. This service allows the tester to control
  communication link parameters, primarily for baudrate transitions
  during flash programming.
  
  Message Format:
  Request:  [0x87] [linkControlType] [linkBaudrateRecord (optional)]
  Response: [0xC7] [linkControlType]
  
  The service operates in two phases:
  1. Verify: Check if the ECU supports the requested baudrate
  2. Transition: Actually change to the new baudrate
  
  Supported link control types:
  - 0x01: VerifyBaudrateTransitionWithFixedBaudrate
  - 0x02: VerifyBaudrateTransitionWithSpecificBaudrate
  - 0x03: TransitionBaudrate (with suppressPosRspMsgIndicationBit)
  - 0x04-0x3F: Reserved
  - 0x40-0x5F: Vehicle manufacturer specific
  - 0x60-0x7E: System supplier specific
  
  Standard Fixed Baudrate Identifiers (Table C.1, p. 450):
  - 0x01: CAN 125 kbit/s
  - 0x02: CAN 250 kbit/s
  - 0x03: CAN 500 kbit/s
  - 0x04: CAN 1000 kbit/s
  - 0x10-0x1F: Additional standard rates
  - 0x20-0x7F: Reserved
  - 0x80-0xFF: Vehicle manufacturer specific
  
  Usage Example (Section 8.9.3, p. 181):
    // Verify ECU supports 500kbps
    auto verify_result = uds::link::verify_fixed_baudrate(client, 0x03); // 500kbps
    if (verify_result.ok) {
      // Transition to 500kbps
      uds::link::transition_baudrate(client);
      // Reconfigure local CAN interface to 500kbps
    }
    
  RAII Usage:
    {
      TemporaryBaudrateGuard guard(client, FixedBaudrate::CAN_500kbps);
      // ... perform high-speed operations ...
    } // Automatically restores original baudrate
*/

#include "uds.hpp"
#include <optional>

namespace uds {
namespace link {

// ============================================================================
// Link Control Types (Sub-functions for LinkControl 0x87)
// ============================================================================

enum class LinkControlType : uint8_t {
  VerifyBaudrateTransitionWithFixedBaudrate    = 0x01,
  VerifyBaudrateTransitionWithSpecificBaudrate = 0x02,
  TransitionBaudrate                           = 0x03
  // 0x04-0x3F: Reserved
  // 0x40-0x5F: Vehicle manufacturer specific
  // 0x60-0x7E: System supplier specific
};

// ============================================================================
// Standard Fixed Baudrate Identifiers (ISO 14229-1 Table C.1)
// ============================================================================

enum class FixedBaudrate : uint8_t {
  // CAN baudrates
  CAN_125kbps   = 0x01,
  CAN_250kbps   = 0x02,
  CAN_500kbps   = 0x03,
  CAN_1Mbps     = 0x04,
  
  // Additional standard rates (OEM-specific mappings)
  Rate_9600     = 0x10,
  Rate_19200    = 0x11,
  Rate_38400    = 0x12,
  Rate_57600    = 0x13,
  Rate_115200   = 0x14,
  
  // Programming baudrates (common OEM values)
  Programming_High = 0x20,
  Programming_Max  = 0x21
  
  // 0x80-0xFF: Vehicle manufacturer specific
};

// ============================================================================
// Link Control Request/Response Structures
// ============================================================================

struct LinkRequest {
  LinkControlType control_type{LinkControlType::VerifyBaudrateTransitionWithFixedBaudrate};
  
  // For fixed baudrate (control_type = 0x01)
  std::optional<uint8_t> baudrate_id;
  
  // For specific baudrate (control_type = 0x02)
  // Baudrate encoded as 3 bytes (high, mid, low) in bps
  std::optional<uint32_t> specific_baudrate_bps;
};

struct LinkResponse {
  LinkControlType control_type{};
  
  // For verify responses, may contain additional info
  std::vector<uint8_t> link_baudrate_record;
};

// ============================================================================
// Result Type for Link Operations
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
// Link Control API
// ============================================================================

/**
 * Verify baudrate transition with fixed baudrate identifier
 * 
 * @param client UDS client instance
 * @param baudrate_id Fixed baudrate identifier (see FixedBaudrate enum)
 * @return Result with LinkResponse on success
 */
Result<LinkResponse> verify_fixed_baudrate(Client& client, uint8_t baudrate_id);

/**
 * Verify baudrate transition with fixed baudrate identifier (enum version)
 */
Result<LinkResponse> verify_fixed_baudrate(Client& client, FixedBaudrate baudrate);

/**
 * Verify baudrate transition with specific baudrate value
 * 
 * @param client UDS client instance
 * @param baudrate_bps Baudrate in bits per second
 * @return Result with LinkResponse on success
 */
Result<LinkResponse> verify_specific_baudrate(Client& client, uint32_t baudrate_bps);

/**
 * Transition to the previously verified baudrate
 * 
 * Note: After calling this, the ECU will immediately switch baudrates.
 * The caller must reconfigure their local CAN interface to match.
 * This function uses suppressPosRsp since no response is expected
 * (the ECU switches baudrate before it could respond).
 * 
 * @param client UDS client instance
 * @return Result indicating success (request sent) or failure
 */
Result<void> transition_baudrate(Client& client);

/**
 * Generic link control request
 * 
 * @param client UDS client instance
 * @param request Link control request parameters
 * @return Result with LinkResponse on success
 */
Result<LinkResponse> link_control(Client& client, const LinkRequest& request);

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * Verify and prepare for high-speed programming baudrate
 * 
 * @param client UDS client instance
 * @param target_baudrate_bps Target baudrate in bps (0 = use OEM default)
 * @return Result indicating if transition is possible
 */
Result<void> prepare_programming_baudrate(Client& client, uint32_t target_baudrate_bps = 0);

/**
 * Get human-readable name for link control type
 */
const char* link_control_type_name(LinkControlType type);

/**
 * Get human-readable name for fixed baudrate
 */
const char* fixed_baudrate_name(FixedBaudrate baudrate);

/**
 * Convert specific baudrate to 3-byte encoding
 */
std::vector<uint8_t> encode_baudrate(uint32_t baudrate_bps);

/**
 * Decode 3-byte baudrate to uint32_t
 */
uint32_t decode_baudrate(const std::vector<uint8_t>& bytes);

// ============================================================================
// RAII Guard for Temporary Baudrate Changes
// ============================================================================

/**
 * RAII guard that manages temporary baudrate transitions.
 * 
 * This guard:
 * 1. Verifies and transitions to a new baudrate on construction
 * 2. Stores the original baudrate
 * 3. Transitions back to the original baudrate on destruction
 * 
 * Note: The caller is responsible for reconfiguring their local CAN
 * interface after construction and before destruction.
 * 
 * Usage:
 *   {
 *     TemporaryBaudrateGuard guard(client, FixedBaudrate::CAN_500kbps);
 *     if (guard.is_active()) {
 *       // Reconfigure local CAN to 500kbps
 *       // ... perform high-speed operations ...
 *     }
 *   } // Guard transitions back to original baudrate
 *   // Reconfigure local CAN back to original
 */
class TemporaryBaudrateGuard {
public:
  /**
   * Construct guard with fixed baudrate
   * 
   * @param client UDS client instance
   * @param target_baudrate Target fixed baudrate
   * @param original_baudrate Original baudrate to restore (0 = don't restore)
   */
  TemporaryBaudrateGuard(Client& client, 
                         FixedBaudrate target_baudrate,
                         FixedBaudrate original_baudrate = FixedBaudrate::CAN_500kbps);
  
  /**
   * Construct guard with specific baudrate
   * 
   * @param client UDS client instance
   * @param target_baudrate_bps Target baudrate in bps
   * @param original_baudrate_bps Original baudrate to restore (0 = don't restore)
   */
  TemporaryBaudrateGuard(Client& client,
                         uint32_t target_baudrate_bps,
                         uint32_t original_baudrate_bps = 0);
  
  ~TemporaryBaudrateGuard();
  
  /**
   * Check if the baudrate transition was successful
   */
  bool is_active() const { return active_; }
  
  /**
   * Get the target baudrate that was set
   */
  uint32_t target_baudrate() const { return target_baudrate_bps_; }
  
  /**
   * Get the original baudrate that will be restored
   */
  uint32_t original_baudrate() const { return original_baudrate_bps_; }
  
  // Non-copyable, non-movable
  TemporaryBaudrateGuard(const TemporaryBaudrateGuard&) = delete;
  TemporaryBaudrateGuard& operator=(const TemporaryBaudrateGuard&) = delete;
  TemporaryBaudrateGuard(TemporaryBaudrateGuard&&) = delete;
  TemporaryBaudrateGuard& operator=(TemporaryBaudrateGuard&&) = delete;

private:
  Client& client_;
  uint32_t target_baudrate_bps_{0};
  uint32_t original_baudrate_bps_{0};
  bool active_{false};
  bool use_fixed_baudrate_{false};
  uint8_t original_fixed_id_{0};
};

} // namespace link
} // namespace uds

#endif // UDS_LINK_HPP
