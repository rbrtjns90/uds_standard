/*
  Example: ReadDataByPeriodicIdentifier (0x2A) - ECU-Initiated Streaming
  
  This demonstrates how to use the periodic data service for:
  - Live telemetry streaming
  - Real-time graphing
  - High-frequency sensor monitoring
  
  Unlike polling (0x22), this service has the ECU send data automatically
  at configured rates without needing repeated requests.
*/

#include "uds.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>

// Mock transport with periodic data simulation
class MockPeriodicTransport : public uds::Transport {
public:
  void set_address(const uds::Address& addr) override { addr_ = addr; }
  const uds::Address& address() const override { return addr_; }
  
  bool request_response(const std::vector<uint8_t>& tx,
                       std::vector<uint8_t>& rx,
                       std::chrono::milliseconds timeout) override {
    (void)timeout;
    
    if (tx.empty()) return false;
    
    uint8_t sid = tx[0];
    
    if (sid == 0x2A) { // ReadDataByPeriodicIdentifier
      // Positive response
      rx.push_back(0x6A); // 0x2A + 0x40
      
      if (tx.size() >= 2) {
        uint8_t mode = tx[1];
        
        if (mode == 0x04) { // StopSending
          is_streaming_ = false;
          std::cout << "  [Transport] Periodic streaming stopped" << std::endl;
        } else {
          is_streaming_ = true;
          transmission_mode_ = mode;
          
          // Store the periodic DIDs
          periodic_dids_.clear();
          for (size_t i = 2; i < tx.size(); ++i) {
            periodic_dids_.push_back(tx[i]);
          }
          
          std::cout << "  [Transport] Periodic streaming started with mode " 
                    << static_cast<int>(mode) << std::endl;
        }
      }
      
      return true;
    }
    
    return false;
  }
  
  // Receive unsolicited periodic messages
  bool recv_unsolicited(std::vector<uint8_t>& rx,
                       std::chrono::milliseconds timeout) override {
    if (!is_streaming_ || periodic_dids_.empty()) {
      std::this_thread::sleep_for(timeout);
      return false;
    }
    
    // Calculate delay based on transmission mode
    std::chrono::milliseconds delay;
    switch (transmission_mode_) {
      case 0x01: delay = std::chrono::milliseconds(2000); break; // Slow: 0.5 Hz
      case 0x02: delay = std::chrono::milliseconds(500);  break; // Medium: 2 Hz
      case 0x03: delay = std::chrono::milliseconds(100);  break; // Fast: 10 Hz
      default:   delay = std::chrono::milliseconds(1000); break;
    }
    
    // Simulate waiting for next periodic message
    std::this_thread::sleep_for(std::min(delay, timeout));
    
    if (!is_streaming_) return false;
    
    // Generate simulated periodic data for each DID
    static uint16_t counter = 0;
    counter++;
    
    for (auto did : periodic_dids_) {
      rx.clear();
      rx.push_back(0x6A); // Periodic data response SID
      rx.push_back(did);  // Periodic DID
      
      // Generate simulated data based on DID
      switch (did) {
        case 0x01: // Engine RPM
          rx.push_back((counter >> 8) & 0xFF);
          rx.push_back(counter & 0xFF);
          break;
        case 0x02: // Vehicle Speed
          rx.push_back(static_cast<uint8_t>(50 + (counter % 50)));
          break;
        case 0x03: // Coolant Temperature
          rx.push_back(static_cast<uint8_t>(80 + (counter % 20)));
          break;
        case 0x04: // Throttle Position
          rx.push_back(static_cast<uint8_t>((counter * 2) % 100));
          break;
        default:
          rx.push_back(0xAA);
          rx.push_back(0xBB);
          break;
      }
      
      return true; // Return first DID's data
    }
    
    return false;
  }
  
private:
  uds::Address addr_;
  std::atomic<bool> is_streaming_{false};
  uint8_t transmission_mode_{0};
  std::vector<uint8_t> periodic_dids_;
};

void print_hex(const std::vector<uint8_t>& data) {
  for (uint8_t b : data) {
    std::cout << std::hex << std::setw(2) << std::setfill('0') 
              << static_cast<int>(b) << " ";
  }
  std::cout << std::dec;
}

int main() {
  MockPeriodicTransport transport;
  uds::Client client(transport);
  
  std::cout << "=== ReadDataByPeriodicIdentifier (0x2A) Examples ===" << std::endl << std::endl;
  
  // Example 1: Start periodic transmission at slow rate
  {
    std::cout << "Example 1: Start periodic transmission (Slow Rate)" << std::endl;
    
    std::vector<uds::PeriodicDID> dids = {0x01, 0x02}; // RPM and Speed
    
    auto result = client.start_periodic_transmission(
        uds::PeriodicTransmissionMode::SendAtSlowRate, dids);
    
    if (result.ok) {
      std::cout << "  ✓ Periodic transmission started" << std::endl;
      std::cout << "  Response: "; print_hex(result.payload); std::cout << std::endl;
    } else {
      std::cout << "  ✗ Failed to start periodic transmission" << std::endl;
    }
    std::cout << std::endl;
  }
  
  // Example 2: Receive periodic data
  {
    std::cout << "Example 2: Receiving periodic data (slow rate, ~0.5 Hz)" << std::endl;
    std::cout << "Collecting 5 samples..." << std::endl;
    
    for (int i = 0; i < 5; ++i) {
      uds::PeriodicDataMessage msg;
      
      if (client.receive_periodic_data(msg, std::chrono::milliseconds(5000))) {
        std::cout << "  Sample " << (i+1) << " - DID 0x" 
                  << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(msg.identifier) << std::dec
                  << " Data: ";
        print_hex(msg.data);
        
        // Decode based on DID
        if (msg.identifier == 0x01 && msg.data.size() >= 2) {
          uint16_t rpm = (msg.data[0] << 8) | msg.data[1];
          std::cout << " (RPM: " << rpm << ")";
        } else if (msg.identifier == 0x02 && msg.data.size() >= 1) {
          std::cout << " (Speed: " << static_cast<int>(msg.data[0]) << " km/h)";
        }
        
        std::cout << std::endl;
      } else {
        std::cout << "  Timeout waiting for periodic data" << std::endl;
      }
    }
    std::cout << std::endl;
  }
  
  // Example 3: Change to fast rate
  {
    std::cout << "Example 3: Change to fast rate transmission (~10 Hz)" << std::endl;
    
    std::vector<uds::PeriodicDID> dids = {0x03, 0x04}; // Coolant temp and throttle
    
    auto result = client.read_data_by_periodic_identifier(
        uds::PeriodicTransmissionMode::SendAtFastRate, dids);
    
    if (result.ok) {
      std::cout << "  ✓ Changed to fast rate transmission" << std::endl;
      
      // Receive a few fast samples
      std::cout << "  Collecting 10 fast samples..." << std::endl;
      
      auto start = std::chrono::steady_clock::now();
      int samples = 0;
      
      for (int i = 0; i < 10; ++i) {
        uds::PeriodicDataMessage msg;
        
        if (client.receive_periodic_data(msg, std::chrono::milliseconds(500))) {
          samples++;
        }
      }
      
      auto end = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
      
      std::cout << "  Received " << samples << " samples in " 
                << duration.count() << " ms" << std::endl;
      std::cout << "  Effective rate: " 
                << (samples * 1000.0 / duration.count()) << " Hz" << std::endl;
    }
    std::cout << std::endl;
  }
  
  // Example 4: Stop periodic transmission
  {
    std::cout << "Example 4: Stop periodic transmission" << std::endl;
    
    std::vector<uds::PeriodicDID> dids = {0x03, 0x04};
    
    auto result = client.stop_periodic_transmission(dids);
    
    if (result.ok) {
      std::cout << "  ✓ Periodic transmission stopped" << std::endl;
    } else {
      std::cout << "  ✗ Failed to stop periodic transmission" << std::endl;
    }
    
    // Verify no more data is received
    uds::PeriodicDataMessage msg;
    if (!client.receive_periodic_data(msg, std::chrono::milliseconds(1000))) {
      std::cout << "  ✓ Confirmed: No more periodic data received" << std::endl;
    }
    std::cout << std::endl;
  }
  
  // Example 5: Multiple periodic identifiers at medium rate
  {
    std::cout << "Example 5: Stream multiple parameters (Medium Rate)" << std::endl;
    
    std::vector<uds::PeriodicDID> dids = {0x01, 0x02, 0x03, 0x04};
    
    auto result = client.start_periodic_transmission(
        uds::PeriodicTransmissionMode::SendAtMediumRate, dids);
    
    if (result.ok) {
      std::cout << "  ✓ Streaming 4 parameters at medium rate (~2 Hz)" << std::endl;
      std::cout << "  Monitoring for 3 seconds..." << std::endl;
      
      auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(3);
      int count = 0;
      
      while (std::chrono::steady_clock::now() < end_time) {
        uds::PeriodicDataMessage msg;
        
        if (client.receive_periodic_data(msg, std::chrono::milliseconds(1000))) {
          count++;
          std::cout << "    [" << count << "] DID 0x" 
                    << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(msg.identifier) << std::dec 
                    << " = ";
          print_hex(msg.data);
          std::cout << std::endl;
        }
      }
      
      std::cout << "  Total messages received: " << count << std::endl;
      
      // Stop streaming
      client.stop_periodic_transmission(dids);
    }
    std::cout << std::endl;
  }
  
  std::cout << "=== Use Cases ===" << std::endl;
  std::cout << "• Live Telemetry: Stream sensor data to dashboards without polling" << std::endl;
  std::cout << "• Real-time Graphing: Continuous data feed for oscilloscope-style displays" << std::endl;
  std::cout << "• High-frequency Monitoring: Capture fast-changing parameters efficiently" << std::endl;
  std::cout << "• Data Logging: Record multiple parameters with precise timing" << std::endl;
  std::cout << "• Performance Tuning: Monitor engine parameters during dyno runs" << std::endl;
  std::cout << std::endl;
  
  std::cout << "=== Key Differences from Polling (0x22) ===" << std::endl;
  std::cout << "• ECU-initiated: ECU sends data automatically, no repeated requests" << std::endl;
  std::cout << "• Lower latency: No request-response round trip for each sample" << std::endl;
  std::cout << "• Reduced bus load: Fewer messages on the CAN bus" << std::endl;
  std::cout << "• Precise timing: ECU controls update rate, eliminating jitter" << std::endl;
  std::cout << "• Efficient for high-rate streaming: Ideal for 10+ Hz data capture" << std::endl;
  
  return 0;
}
