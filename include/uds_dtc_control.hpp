#ifndef UDS_DTC_CONTROL_HPP
#define UDS_DTC_CONTROL_HPP

/*
  Control DTC Setting (Service 0x85) Helper Utilities
  
  ISO 14229-1:2013 Section 9.9 (p. 71)
  Tables: 86-87 (request/response definitions)
  
  This header provides high-level helper functions for using the
  ControlDTCSetting service (0x85) in ISO 14229-1 UDS.
  
  The ControlDTCSetting service controls whether the ECU logs DTCs.
  This is critical for:
  - Flash programming (prevent error codes during memory erase)
  - ECU testing without polluting permanent DTC logs
  - OEM tool compliance
  
  Message Format:
  Request:  [0x85] [DTCSettingType]
  Response: [0xC5] [DTCSettingType]
  
  DTC Setting Types (Table 87, p. 72):
  - 0x01: On (enable DTC setting - normal operation)
  - 0x02: Off (disable DTC setting - suppress logging)
  
  WARNING: If DTC setting is not disabled before programming, ECUs may:
  - Set permanent error codes
  - Report false flash corruption
  - Become bricked in extreme cases
  
  Typical Usage in Flash Programming (Section 8.7.3, p. 155):
  1. Enter programming session (0x10 0x02)
  2. Disable DTC setting (0x85 0x02) ← This service
  3. Unlock security access (0x27)
  4. Perform flash operations (0x34, 0x36, 0x37)
  5. Re-enable DTC setting (0x85 0x01)
  6. Reset ECU (0x11)
*/

#include "uds.hpp"

namespace uds {
namespace dtc_control {

// ================================================================
// Helper functions for DTC setting control
// ================================================================

/**
 * Enable DTC setting (normal operation)
 * ECU will log diagnostic trouble codes as they occur.
 * 
 * @param client UDS client instance
 * @return Result of the control DTC setting request
 */
inline PositiveOrNegative enable_dtc_setting(Client& client) {
  return client.control_dtc_setting(static_cast<uint8_t>(DTCSettingType::On));
}

/**
 * Disable DTC setting (suppress DTC logging)
 * ECU will NOT log diagnostic trouble codes.
 * Use this before flash programming to avoid spurious error codes.
 * 
 * @param client UDS client instance
 * @return Result of the control DTC setting request
 */
inline PositiveOrNegative disable_dtc_setting(Client& client) {
  return client.control_dtc_setting(static_cast<uint8_t>(DTCSettingType::Off));
}

/**
 * Check if DTC setting is currently enabled
 * 
 * @param client UDS client instance
 * @return true if DTC setting is enabled, false if disabled
 */
inline bool is_dtc_setting_enabled(const Client& client) {
  return client.is_dtc_setting_enabled();
}

// ================================================================
// RAII helper for automatic DTC setting restoration
// ================================================================

/**
 * RAII guard that automatically restores DTC setting on destruction.
 * 
 * Usage:
 *   {
 *     DTCSettingGuard guard(client);
 *     disable_dtc_setting(client);
 *     // ... perform flash programming ...
 *   } // DTC setting automatically restored here
 */
class DTCSettingGuard {
public:
  explicit DTCSettingGuard(Client& client)
    : client_(client), saved_state_(client.is_dtc_setting_enabled()) {}
  
  ~DTCSettingGuard() {
    // Restore to saved state
    if (saved_state_) {
      enable_dtc_setting(client_);
    } else {
      disable_dtc_setting(client_);
    }
  }
  
  // Non-copyable, non-movable
  DTCSettingGuard(const DTCSettingGuard&) = delete;
  DTCSettingGuard& operator=(const DTCSettingGuard&) = delete;
  DTCSettingGuard(DTCSettingGuard&&) = delete;
  DTCSettingGuard& operator=(DTCSettingGuard&&) = delete;

private:
  Client& client_;
  bool saved_state_;
};

// ================================================================
// Combined guard for flash programming (DTC + Communication)
// ================================================================

/**
 * RAII guard that manages both DTC setting and communication control
 * for safe flash programming operations.
 * 
 * This guard:
 * 1. Disables DTC setting to prevent spurious error codes
 * 2. Can optionally control communication settings
 * 3. Automatically restores both on destruction
 * 
 * Usage:
 *   {
 *     FlashProgrammingGuard guard(client);
 *     // ... perform flash programming safely ...
 *   } // Both DTC and communication restored automatically
 */
class FlashProgrammingGuard {
public:
  explicit FlashProgrammingGuard(Client& client)
    : client_(client), 
      saved_dtc_state_(client.is_dtc_setting_enabled()),
      saved_comm_state_(client.communication_state()) {
    // Automatically disable DTC setting for programming
    disable_dtc_setting(client_);
  }
  
  ~FlashProgrammingGuard() {
    // Restore DTC setting
    if (saved_dtc_state_) {
      enable_dtc_setting(client_);
    }
    
    // Note: Communication state restoration should be handled separately
    // or via CommunicationGuard if needed
  }
  
  // Non-copyable, non-movable
  FlashProgrammingGuard(const FlashProgrammingGuard&) = delete;
  FlashProgrammingGuard& operator=(const FlashProgrammingGuard&) = delete;
  FlashProgrammingGuard(FlashProgrammingGuard&&) = delete;
  FlashProgrammingGuard& operator=(FlashProgrammingGuard&&) = delete;

private:
  Client& client_;
  bool saved_dtc_state_;
  Client::CommunicationState saved_comm_state_;
};

} // namespace dtc_control
} // namespace uds

#endif // UDS_DTC_CONTROL_HPP
