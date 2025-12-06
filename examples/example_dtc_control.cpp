/*
  Example: Control DTC Setting (Service 0x85) Usage
  
  This example demonstrates how to use the ControlDTCSetting service
  to prevent ECU from logging diagnostic trouble codes during flash
  programming and other critical operations.
*/

#include "uds.hpp"
#include "uds_dtc_control.hpp"
#include "uds_comm_control.hpp"
#include <iostream>
#include <iomanip>

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

// Example 1: Basic DTC setting control
void example_basic_dtc_control(uds::Client& client) {
  std::cout << "\n=== Example 1: Basic DTC Setting Control ===\n";
  
  // Disable DTC setting
  auto resp = uds::dtc_control::disable_dtc_setting(client);
  print_response(resp, "Disable DTC setting");
  
  if (resp.ok) {
    std::cout << "  DTC setting enabled: " 
              << (client.is_dtc_setting_enabled() ? "Yes" : "No") << "\n";
  }
  
  // ... perform operations that might trigger DTCs ...
  
  // Re-enable DTC setting
  resp = uds::dtc_control::enable_dtc_setting(client);
  print_response(resp, "Enable DTC setting");
  
  if (resp.ok) {
    std::cout << "  DTC setting enabled: " 
              << (client.is_dtc_setting_enabled() ? "Yes" : "No") << "\n";
  }
}

// Example 2: Using RAII guard
void example_dtc_guard(uds::Client& client) {
  std::cout << "\n=== Example 2: DTC Setting RAII Guard ===\n";
  
  {
    // Create guard - will restore DTC setting on scope exit
    uds::dtc_control::DTCSettingGuard guard(client);
    
    // Disable DTC setting
    auto resp = uds::dtc_control::disable_dtc_setting(client);
    print_response(resp, "Disable DTC setting (with guard)");
    
    if (resp.ok) {
      std::cout << "  Performing operations that might trigger DTCs...\n";
      // ... critical operations ...
    }
    
    std::cout << "  Guard going out of scope - DTC setting will be restored\n";
  } // DTC setting automatically restored here
  
  std::cout << "✓ DTC setting restored automatically by guard\n";
}

// Example 3: Flash programming workflow with DTC control
void example_flash_programming_with_dtc(uds::Client& client) {
  std::cout << "\n=== Example 3: Flash Programming with DTC Control ===\n";
  
  // 1. Enter programming session
  auto resp = client.diagnostic_session_control(uds::Session::ProgrammingSession);
  print_response(resp, "Enter programming session");
  if (!resp.ok) return;
  
  // 2. Disable DTC setting (CRITICAL before programming!)
  {
    uds::dtc_control::DTCSettingGuard dtc_guard(client);
    
    resp = uds::dtc_control::disable_dtc_setting(client);
    print_response(resp, "Disable DTC setting");
    if (!resp.ok) return;
    
    // 3. Request security access
    resp = client.security_access_request_seed(1);
    print_response(resp, "Request security seed");
    if (!resp.ok) return;
    
    // (In real code, compute key from seed)
    std::vector<uint8_t> key = {0x12, 0x34, 0x56, 0x78}; // Placeholder
    resp = client.security_access_send_key(1, key);
    print_response(resp, "Send security key");
    if (!resp.ok) return;
    
    // 4. Request download
    std::vector<uint8_t> address = {0x00, 0x10, 0x00, 0x00};
    std::vector<uint8_t> size = {0x00, 0x01, 0x00, 0x00};
    resp = client.request_download(0x00, address, size);
    print_response(resp, "Request download");
    if (!resp.ok) return;
    
    // 5. Transfer data
    std::vector<uint8_t> data(256, 0xFF);
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
    
  } // DTC setting automatically restored
  
  std::cout << "✓ Flash programming completed with DTC protection\n";
}

// Example 4: Combined flash programming guard
void example_combined_flash_guard(uds::Client& client) {
  std::cout << "\n=== Example 4: Combined Flash Programming Guard ===\n";
  
  // Enter programming session
  auto resp = client.diagnostic_session_control(uds::Session::ProgrammingSession);
  print_response(resp, "Enter programming session");
  if (!resp.ok) return;
  
  {
    // Use combined guard that handles DTC setting automatically
    uds::dtc_control::FlashProgrammingGuard flash_guard(client);
    
    std::cout << "  DTC setting disabled automatically by guard\n";
    std::cout << "  DTC setting enabled: " 
              << (client.is_dtc_setting_enabled() ? "Yes" : "No") << "\n";
    
    // Perform programming operations safely
    // The guard ensures DTC setting is disabled throughout
    
    std::cout << "  Performing flash operations...\n";
    // ... flash programming steps ...
    
  } // DTC setting restored automatically
  
  std::cout << "✓ Flash programming guard cleaned up\n";
  std::cout << "  DTC setting enabled: " 
            << (client.is_dtc_setting_enabled() ? "Yes" : "No") << "\n";
}

// Example 5: Manual control with raw service
void example_manual_dtc_control(uds::Client& client) {
  std::cout << "\n=== Example 5: Manual DTC Control (Low-level) ===\n";
  
  // Disable DTC setting using raw service call
  auto resp = client.control_dtc_setting(
    static_cast<uint8_t>(uds::DTCSettingType::Off)
  );
  print_response(resp, "Disable DTC setting (raw)");
  
  // Enable DTC setting using raw service call
  resp = client.control_dtc_setting(
    static_cast<uint8_t>(uds::DTCSettingType::On)
  );
  print_response(resp, "Enable DTC setting (raw)");
}

// Example 6: Complete OEM-compliant programming sequence
void example_oem_compliant_programming(uds::Client& client) {
  std::cout << "\n=== Example 6: OEM-Compliant Programming Sequence ===\n";
  
  // This demonstrates the full sequence used by professional OEM tools
  
  // Step 1: Enter programming session
  auto resp = client.diagnostic_session_control(uds::Session::ProgrammingSession);
  print_response(resp, "1. Enter programming session");
  if (!resp.ok) return;
  
  // Step 2: Disable DTC setting (MUST be before any programming)
  resp = uds::dtc_control::disable_dtc_setting(client);
  print_response(resp, "2. Disable DTC setting");
  if (!resp.ok) {
    std::cout << "  ERROR: Cannot proceed without disabling DTC setting!\n";
    return;
  }
  
  // Step 3: Optionally disable communication
  // (Some ECUs require this, some don't)
  // resp = uds::comm_control::disable_normal_communication(client, nullptr);
  // print_response(resp, "3. Disable normal communication");
  
  // Step 4: Security access
  resp = client.security_access_request_seed(1);
  print_response(resp, "3. Request security seed");
  if (!resp.ok) return;
  
  std::vector<uint8_t> key = {0x12, 0x34, 0x56, 0x78}; // Placeholder
  resp = client.security_access_send_key(1, key);
  print_response(resp, "4. Send security key");
  if (!resp.ok) return;
  
  // Step 5: Perform programming
  std::cout << "5. Performing flash programming...\n";
  // ... RequestDownload, TransferData, RequestTransferExit ...
  
  // Step 6: Verify programming
  std::cout << "6. Verifying flash integrity...\n";
  // ... Read back and verify ...
  
  // Step 7: Re-enable DTC setting
  resp = uds::dtc_control::enable_dtc_setting(client);
  print_response(resp, "7. Re-enable DTC setting");
  
  // Step 8: Re-enable communication if disabled
  // resp = uds::comm_control::enable_normal_communication(client, nullptr);
  // print_response(resp, "8. Re-enable communication");
  
  // Step 9: Reset ECU to apply changes
  resp = client.ecu_reset(uds::EcuResetType::HardReset);
  print_response(resp, "8. Reset ECU");
  
  std::cout << "✓ OEM-compliant programming sequence completed\n";
}

int main() {
  std::cout << "=== UDS Control DTC Setting (0x85) Examples ===\n";
  std::cout << "This demonstrates ISO 14229-1 ControlDTCSetting service\n";
  
  // NOTE: You need to provide your own Transport implementation
  // This is just a skeleton to show the API usage
  
  /*
  YourCanDriver can_driver;
  isotp::Transport transport(can_driver);
  
  // Configure addressing
  uds::Address addr;
  addr.type = uds::AddressType::Physical;
  addr.tx_can_id = 0x7E0;
  addr.rx_can_id = 0x7E8;
  transport.set_address(addr);
  
  // Create UDS client
  uds::Client client(transport);
  
  // Run examples
  example_basic_dtc_control(client);
  example_dtc_guard(client);
  example_flash_programming_with_dtc(client);
  example_combined_flash_guard(client);
  example_manual_dtc_control(client);
  example_oem_compliant_programming(client);
  */
  
  std::cout << "\nNote: This is a code example. To run, implement Transport\n";
  std::cout << "and uncomment the main() implementation above.\n";
  
  std::cout << "\n⚠️  WARNING: Always disable DTC setting before flash programming!\n";
  std::cout << "Failure to do so may cause permanent error codes or ECU damage.\n";
  
  return 0;
}
