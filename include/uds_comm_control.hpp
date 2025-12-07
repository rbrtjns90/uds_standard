#ifndef UDS_COMM_CONTROL_HPP
#define UDS_COMM_CONTROL_HPP

/*
  Communication Control (Service 0x28) Helper Utilities
  
  ISO 14229-1:2013 Section 9.5 (p. 53)
  Tables: 53-55 (request/response definitions)
  
  This header provides high-level helper functions for using the
  CommunicationControl service (0x28) in ISO 14229-1 UDS.
  
  The CommunicationControl service controls which vehicle messages
  the ECU transmits or receives during diagnostics. This is critical for:
  - Flash programming (disable all comms except diagnostics)
  - DoIP gateway management
  - Disabling chatter during critical routines
  - Preventing bus overload
  
  Message Format:
  Request:  [0x28] [controlType] [communicationType]
  Response: [0x68] [controlType]
  
  Control Types (Table 54, p. 54):
  - 0x00: EnableRxAndTx (resume normal communication)
  - 0x01: EnableRxDisableTx (listen-only mode)
  - 0x02: DisableRxEnableTx (transmit-only mode)
  - 0x03: DisableRxAndTx (silence ECU except diagnostics)
  
  Communication Types (Table 55, p. 55):
  - 0x01: Normal communication messages
  - 0x02: Network management communication messages
  - 0x03: Network management + normal messages
*/

#include "uds.hpp"
#include "isotp.hpp"

namespace uds {
namespace comm_control {

// ================================================================
// Helper functions for common CommunicationControl scenarios
// ================================================================

/**
 * Disable all normal communication (except diagnostics)
 * This is typically used before flash programming to ensure the ECU
 * only responds to diagnostic messages.
 * 
 * @param client UDS client instance
 * @param transport ISO-TP transport (to sync state)
 * @return Result of the communication control request
 */
inline PositiveOrNegative disable_normal_communication(Client& client, 
                                                       isotp::Transport* transport = nullptr) {
  // Disable Rx and Tx for normal communication messages (0x03 + 0x01)
  auto result = client.communication_control(
    static_cast<uint8_t>(CommunicationControlType::DisableRxAndTx),
    static_cast<uint8_t>(CommunicationType::NormalCommunicationMessages)
  );
  
  // Sync transport layer state if provided
  if (result.ok && transport) {
    const auto& state = client.communication_state();
    transport->enable_rx(state.rx_enabled);
    transport->enable_tx(state.tx_enabled);
  }
  
  return result;
}

/**
 * Enable all normal communication
 * This restores normal ECU communication after programming or special operations.
 * 
 * @param client UDS client instance
 * @param transport ISO-TP transport (to sync state)
 * @return Result of the communication control request
 */
inline PositiveOrNegative enable_normal_communication(Client& client,
                                                      isotp::Transport* transport = nullptr) {
  // Enable Rx and Tx for normal communication messages (0x00 + 0x01)
  auto result = client.communication_control(
    static_cast<uint8_t>(CommunicationControlType::EnableRxAndTx),
    static_cast<uint8_t>(CommunicationType::NormalCommunicationMessages)
  );
  
  // Sync transport layer state if provided
  if (result.ok && transport) {
    const auto& state = client.communication_state();
    transport->enable_rx(state.rx_enabled);
    transport->enable_tx(state.tx_enabled);
  }
  
  return result;
}

/**
 * Disable all ECU communication (including network management)
 * This is the most restrictive mode - only diagnostic messages work.
 * 
 * @param client UDS client instance
 * @param transport ISO-TP transport (to sync state)
 * @return Result of the communication control request
 */
inline PositiveOrNegative disable_all_communication(Client& client,
                                                    isotp::Transport* transport = nullptr) {
  // Disable Rx and Tx for normal + network management (0x03 + 0x03)
  auto result = client.communication_control(
    static_cast<uint8_t>(CommunicationControlType::DisableRxAndTx),
    static_cast<uint8_t>(CommunicationType::NetworkDownloadUpload)
  );
  
  // Sync transport layer state if provided
  if (result.ok && transport) {
    const auto& state = client.communication_state();
    transport->enable_rx(state.rx_enabled);
    transport->enable_tx(state.tx_enabled);
  }
  
  return result;
}

/**
 * Enable ECU receive, disable transmit (listen-only mode)
 * Useful for monitoring ECU behavior without affecting bus traffic.
 * 
 * @param client UDS client instance
 * @param transport ISO-TP transport (to sync state)
 * @param comm_type Communication type to control (default: normal messages)
 * @return Result of the communication control request
 */
inline PositiveOrNegative enable_listen_only(Client& client,
                                             isotp::Transport* transport = nullptr,
                                             uint8_t comm_type = 0x01) {
  auto result = client.communication_control(
    static_cast<uint8_t>(CommunicationControlType::EnableRxDisableTx),
    comm_type
  );
  
  // Sync transport layer state if provided
  if (result.ok && transport) {
    const auto& state = client.communication_state();
    transport->enable_rx(state.rx_enabled);
    transport->enable_tx(state.tx_enabled);
  }
  
  return result;
}

/**
 * Restore all communication to default state
 * Convenience function to ensure everything is enabled.
 * 
 * @param client UDS client instance
 * @param transport ISO-TP transport (to sync state)
 * @return Result of the communication control request
 */
inline PositiveOrNegative restore_communication(Client& client,
                                                isotp::Transport* transport = nullptr) {
  // Enable everything
  auto result = client.communication_control(
    static_cast<uint8_t>(CommunicationControlType::EnableRxAndTx),
    static_cast<uint8_t>(CommunicationType::NetworkDownloadUpload)
  );
  
  // Sync transport layer state if provided
  if (result.ok && transport) {
    const auto& state = client.communication_state();
    transport->enable_rx(state.rx_enabled);
    transport->enable_tx(state.tx_enabled);
  }
  
  return result;
}

// ================================================================
// RAII helper for automatic communication restoration
// ================================================================

/**
 * RAII guard that automatically restores communication on destruction.
 * 
 * Usage:
 *   {
 *     CommunicationGuard guard(client, transport);
 *     disable_normal_communication(client, transport);
 *     // ... do programming operations ...
 *   } // communication automatically restored here
 */
class CommunicationGuard {
public:
  CommunicationGuard(Client& client, isotp::Transport* transport = nullptr)
    : client_(client), transport_(transport), 
      saved_state_(client.communication_state()) {}
  
  ~CommunicationGuard() {
    // Restore to saved state
    restore_communication(client_, transport_);
  }
  
  // Non-copyable, non-movable
  CommunicationGuard(const CommunicationGuard&) = delete;
  CommunicationGuard& operator=(const CommunicationGuard&) = delete;
  CommunicationGuard(CommunicationGuard&&) = delete;
  CommunicationGuard& operator=(CommunicationGuard&&) = delete;

private:
  Client& client_;
  isotp::Transport* transport_;
  Client::CommunicationState saved_state_;
};

} // namespace comm_control
} // namespace uds

#endif // UDS_COMM_CONTROL_HPP
