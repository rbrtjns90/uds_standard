/*
  Example: Communication Control (Service 0x28) Usage
  
  This example demonstrates how to use the CommunicationControl service
  to manage ECU communication during diagnostic operations, especially
  for flash programming scenarios.
*/

#include "uds.hpp"
#include "isotp.hpp"
#include "uds_comm_control.hpp"
#include <iostream>
#include <iomanip>

// Forward declaration - you would provide your own ICanDriver implementation
class YourCanDriver; 

void print_response(const uds::PositiveOrNegative& resp, const std::string& operation) {
  if (resp.ok) {
    std::cout << "✓ " << operation << " succeeded\n";
    if (!resp.payload.empty()) {
      std::cout << "  Response payload: ";
      for (auto b : resp.payload) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<int>(b) << " ";
      }
      std::cout << std::dec << "\n";
    }
  } else {
    std::cout << "✗ " << operation << " failed\n";
    std::cout << "  NRC: 0x" << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(resp.nrc.code) << std::dec << "\n";
  }
}

// Example 1: Basic usage with manual control
void example_basic_usage(uds::Client& client, isotp::Transport& transport) {
  std::cout << "\n=== Example 1: Basic Communication Control ===\n";
  
  // Disable normal communication before flash programming
  auto resp = client.communication_control(
    static_cast<uint8_t>(uds::CommunicationControlType::DisableRxAndTx),
    static_cast<uint8_t>(uds::CommunicationType::NormalCommunicationMessages)
  );
  print_response(resp, "Disable normal communication");
  
  if (resp.ok) {
    // Sync the transport layer
    const auto& state = client.communication_state();
    transport.enable_rx(state.rx_enabled);
    transport.enable_tx(state.tx_enabled);
    
    std::cout << "  Rx enabled: " << (state.rx_enabled ? "Yes" : "No") << "\n";
    std::cout << "  Tx enabled: " << (state.tx_enabled ? "Yes" : "No") << "\n";
  }
  
  // ... perform flash programming operations here ...
  
  // Re-enable communication after programming
  resp = client.communication_control(
    static_cast<uint8_t>(uds::CommunicationControlType::EnableRxAndTx),
    static_cast<uint8_t>(uds::CommunicationType::NormalCommunicationMessages)
  );
  print_response(resp, "Enable normal communication");
}

// Example 2: Using helper functions
void example_helper_functions(uds::Client& client, isotp::Transport& transport) {
  std::cout << "\n=== Example 2: Using Helper Functions ===\n";
  
  // Disable normal communication using helper
  auto resp = uds::comm_control::disable_normal_communication(client, &transport);
  print_response(resp, "Disable normal communication (helper)");
  
  // ... perform operations ...
  
  // Restore communication
  resp = uds::comm_control::restore_communication(client, &transport);
  print_response(resp, "Restore communication (helper)");
}

// Example 3: RAII guard for automatic restoration
void example_raii_guard(uds::Client& client, isotp::Transport& transport) {
  std::cout << "\n=== Example 3: RAII Communication Guard ===\n";
  
  {
    // Create guard - will restore communication on scope exit
    uds::comm_control::CommunicationGuard guard(client, &transport);
    
    // Disable communication
    auto resp = uds::comm_control::disable_normal_communication(client, &transport);
    print_response(resp, "Disable communication (with guard)");
    
    if (resp.ok) {
      std::cout << "  Performing critical operations...\n";
      // ... flash programming or other operations ...
    }
    
    std::cout << "  Guard going out of scope - communication will be restored\n";
  } // Communication automatically restored here
  
  std::cout << "✓ Communication restored automatically by guard\n";
}

// Example 4: Flash programming workflow
void example_flash_programming_workflow(uds::Client& client, isotp::Transport& transport) {
  std::cout << "\n=== Example 4: Flash Programming Workflow ===\n";
  
  // 1. Enter programming session
  auto resp = client.diagnostic_session_control(uds::Session::ProgrammingSession);
  print_response(resp, "Enter programming session");
  if (!resp.ok) return;
  
  // 2. Disable all communication except diagnostics
  {
    uds::comm_control::CommunicationGuard guard(client, &transport);
    
    resp = uds::comm_control::disable_all_communication(client, &transport);
    print_response(resp, "Disable all communication");
    if (!resp.ok) return;
    
    // 3. Request security access (example - level 1)
    resp = client.security_access_request_seed(1);
    print_response(resp, "Request security seed");
    if (!resp.ok) return;
    
    // (In real code, compute key from seed using manufacturer's algorithm)
    std::vector<uint8_t> key = {0x12, 0x34, 0x56, 0x78}; // Placeholder
    resp = client.security_access_send_key(1, key);
    print_response(resp, "Send security key");
    if (!resp.ok) return;
    
    // 4. Request download
    std::vector<uint8_t> address = {0x00, 0x10, 0x00, 0x00}; // Example address
    std::vector<uint8_t> size = {0x00, 0x01, 0x00, 0x00};    // Example size
    resp = client.request_download(0x00, address, size);
    print_response(resp, "Request download");
    if (!resp.ok) return;
    
    // 5. Transfer data (simplified - normally multiple blocks)
    std::vector<uint8_t> data(256, 0xFF); // Example data block
    resp = client.transfer_data(1, data);
    print_response(resp, "Transfer data block 1");
    if (!resp.ok) return;
    
    // 6. Request transfer exit
    resp = client.request_transfer_exit();
    print_response(resp, "Request transfer exit");
    if (!resp.ok) return;
    
    // 7. Reset ECU
    resp = client.ecu_reset(uds::EcuResetType::HardReset);
    print_response(resp, "ECU reset");
    
  } // Communication restored automatically
  
  std::cout << "✓ Flash programming workflow completed\n";
}

// Example 5: Listen-only mode for monitoring
void example_listen_only(uds::Client& client, isotp::Transport& transport) {
  std::cout << "\n=== Example 5: Listen-Only Mode ===\n";
  
  // Enable receive but disable transmit - monitor ECU without affecting bus
  auto resp = uds::comm_control::enable_listen_only(client, &transport);
  print_response(resp, "Enable listen-only mode");
  
  if (resp.ok) {
    const auto& state = client.communication_state();
    std::cout << "  Rx enabled: " << (state.rx_enabled ? "Yes" : "No") << "\n";
    std::cout << "  Tx enabled: " << (state.tx_enabled ? "Yes" : "No") << "\n";
    
    // ... monitor ECU behavior ...
  }
  
  // Restore normal operation
  resp = uds::comm_control::restore_communication(client, &transport);
  print_response(resp, "Restore normal communication");
}

// Example 6: Suppress positive response (bit 7 of subfunction)
void example_suppress_positive_response(uds::Client& client, [[maybe_unused]] isotp::Transport& transport) {
  std::cout << "\n=== Example 6: Suppress Positive Response ===\n";
  
  // For high-frequency operations, suppress positive response to reduce bus load
  uint8_t subfunction = static_cast<uint8_t>(uds::CommunicationControlType::DisableRxAndTx);
  subfunction |= 0x80; // Set bit 7 to suppress positive response
  
  auto resp = client.communication_control(
    subfunction,
    static_cast<uint8_t>(uds::CommunicationType::NormalCommunicationMessages)
  );
  
  // Note: When suppress is set, ECU won't send positive response (ok will be false due to timeout)
  // This is expected behavior
  std::cout << "  Sent communication control with suppress bit set\n";
  std::cout << "  No positive response expected (ok=" << (resp.ok ? "true" : "false") << ")\n";
}

int main() {
  std::cout << "=== UDS Communication Control (0x28) Examples ===\n";
  std::cout << "This demonstrates ISO 14229-1 CommunicationControl service\n";
  
  // NOTE: You need to provide your own ICanDriver implementation
  // This is just a skeleton to show the API usage
  
  /*
  YourCanDriver can_driver;
  isotp::Transport transport(can_driver);
  
  // Configure addressing
  uds::Address addr;
  addr.type = uds::AddressType::Physical;
  addr.tx_can_id = 0x7E0; // Example: tester -> ECU
  addr.rx_can_id = 0x7E8; // Example: ECU -> tester
  transport.set_address(addr);
  
  // Create UDS client
  uds::Client client(transport);
  
  // Run examples
  example_basic_usage(client, transport);
  example_helper_functions(client, transport);
  example_raii_guard(client, transport);
  example_flash_programming_workflow(client, transport);
  example_listen_only(client, transport);
  example_suppress_positive_response(client, transport);
  */
  
  std::cout << "\nNote: This is a code example. To run, implement ICanDriver\n";
  std::cout << "and uncomment the main() implementation above.\n";
  
  return 0;
}
