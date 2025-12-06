/*
  Example: DynamicallyDefineDataIdentifier (DDDI) Service 0x2C
  
  This demonstrates how to use the DDDI service to create custom data identifiers
  for efficient multi-parameter logging and sensor streaming.
*/

#include "uds.hpp"
#include <iostream>
#include <iomanip>

// Mock transport for example purposes
class MockTransport : public uds::Transport {
public:
  void set_address(const uds::Address& addr) override { addr_ = addr; }
  const uds::Address& address() const override { return addr_; }
  
  bool request_response(const std::vector<uint8_t>& tx,
                       std::vector<uint8_t>& rx,
                       [[maybe_unused]] std::chrono::milliseconds timeout) override {
    // Simulate positive response
    if (!tx.empty()) {
      uint8_t sid = tx[0];
      rx.push_back(sid + 0x40); // Positive response
      
      if (sid == 0x2C) { // DDDI
        // Echo back the sub-function and dynamic DID
        if (tx.size() >= 3) {
          rx.push_back(tx[1]); // sub-function
          rx.push_back(tx[2]); // dynamic DID high byte
          rx.push_back(tx[3]); // dynamic DID low byte
        }
      }
    }
    return true;
  }
  
private:
  uds::Address addr_;
};

void print_hex(const std::vector<uint8_t>& data) {
  for (uint8_t b : data) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') 
              << static_cast<int>(b) << " ";
  }
  std::cout << std::dec << std::endl;
}

int main() {
  MockTransport transport;
  uds::Client client(transport);
  
  std::cout << "=== DynamicallyDefineDataIdentifier (0x2C) Examples ===" << std::endl << std::endl;
  
  // Example 1: Define DDDI by combining multiple DIDs
  // Create a custom DID 0xF200 that combines:
  // - Engine RPM (DID 0x010C) - bytes 1-2
  // - Vehicle Speed (DID 0x010D) - byte 1
  // - Coolant Temp (DID 0x0105) - byte 1
  {
    std::cout << "Example 1: Define DDDI 0xF200 by combining multiple source DIDs" << std::endl;
    
    std::vector<uds::DDDI_SourceByDID> sources;
    
    // Engine RPM: read 2 bytes starting at position 1
    sources.push_back({0x010C, 1, 2});
    
    // Vehicle Speed: read 1 byte starting at position 1
    sources.push_back({0x010D, 1, 1});
    
    // Coolant Temperature: read 1 byte starting at position 1
    sources.push_back({0x0105, 1, 1});
    
    auto result = client.dynamically_define_data_identifier_by_did(0xF200, sources);
    
    if (result.ok) {
      std::cout << "  ✓ Successfully defined DDDI 0xF200" << std::endl;
      std::cout << "  Response payload: ";
      print_hex(result.payload);
    } else {
      std::cout << "  ✗ Failed to define DDDI" << std::endl;
    }
    std::cout << std::endl;
  }
  
  // Example 2: Define DDDI by memory address
  // Create a custom DID 0xF201 that reads from specific memory locations
  {
    std::cout << "Example 2: Define DDDI 0xF201 by memory address" << std::endl;
    
    std::vector<uds::DDDI_SourceByMemory> sources;
    
    // Read 4 bytes from address 0x12345678
    uds::DDDI_SourceByMemory mem_source1;
    mem_source1.address_and_length_format_id = 0x44; // 4-byte address, 4-byte size
    mem_source1.memory_address = {0x12, 0x34, 0x56, 0x78};
    mem_source1.memory_size = {0x00, 0x00, 0x00, 0x04};
    sources.push_back(mem_source1);
    
    // Read 2 bytes from address 0x87654321
    uds::DDDI_SourceByMemory mem_source2;
    mem_source2.address_and_length_format_id = 0x44;
    mem_source2.memory_address = {0x87, 0x65, 0x43, 0x21};
    mem_source2.memory_size = {0x00, 0x00, 0x00, 0x02};
    sources.push_back(mem_source2);
    
    auto result = client.dynamically_define_data_identifier_by_memory(0xF201, sources);
    
    if (result.ok) {
      std::cout << "  ✓ Successfully defined DDDI 0xF201" << std::endl;
      std::cout << "  Response payload: ";
      print_hex(result.payload);
    } else {
      std::cout << "  ✗ Failed to define DDDI" << std::endl;
    }
    std::cout << std::endl;
  }
  
  // Example 3: Read the dynamically defined DID
  {
    std::cout << "Example 3: Read dynamically defined DID 0xF200" << std::endl;
    
    // After defining, you can read it like any other DID
    auto result = client.read_data_by_identifier(0xF200);
    
    if (result.ok) {
      std::cout << "  ✓ Successfully read DDDI 0xF200" << std::endl;
      std::cout << "  Data: ";
      print_hex(result.payload);
    } else {
      std::cout << "  ✗ Failed to read DDDI" << std::endl;
    }
    std::cout << std::endl;
  }
  
  // Example 4: Clear a dynamically defined DID
  {
    std::cout << "Example 4: Clear DDDI 0xF200" << std::endl;
    
    auto result = client.clear_dynamically_defined_data_identifier(0xF200);
    
    if (result.ok) {
      std::cout << "  ✓ Successfully cleared DDDI 0xF200" << std::endl;
      std::cout << "  Response payload: ";
      print_hex(result.payload);
    } else {
      std::cout << "  ✗ Failed to clear DDDI" << std::endl;
    }
    std::cout << std::endl;
  }
  
  std::cout << "=== Use Cases ===" << std::endl;
  std::cout << "• Logging: Create a single DID with all parameters you want to log" << std::endl;
  std::cout << "• Multi-PID streaming: Combine multiple sensor values for efficient polling" << std::endl;
  std::cout << "• Complex sensor combinations: Build custom data packets for analysis" << std::endl;
  std::cout << "• Memory dumps: Define DIDs pointing to specific memory regions" << std::endl;
  
  return 0;
}
