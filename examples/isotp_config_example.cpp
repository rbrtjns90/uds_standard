#include "isotp.hpp"
#include "slcan_serial.hpp"
#include <iostream>

// Example demonstrating the simplified IsoTpConfig API

int main() {
    // Create SLCAN driver (assumes /dev/ttyUSB0 or similar)
    slcan::SerialDriver driver;
    
    if (!driver.open("/dev/ttyUSB0", CANProtocol::CAN_BITRATE_500K)) {
        std::cerr << "Failed to open SLCAN device" << std::endl;
        return 1;
    }
    
    // Create ISO-TP transport
    isotp::Transport transport(driver);
    
    // Set address for diagnostic communication
    uds::Address addr;
    addr.tx_can_id = 0x7E0;  // Tester -> ECU
    addr.rx_can_id = 0x7E8;  // ECU -> Tester
    transport.set_address(addr);
    
    // METHOD 1: Using simplified IsoTpConfig structure
    std::cout << "=== Method 1: Simplified IsoTpConfig ===" << std::endl;
    
    isotp::IsoTpConfig config;
    config.blockSize = 16;                                      // 16 frames per block
    config.stMin = 10;                                          // 10ms between frames
    config.n_ar = std::chrono::milliseconds(100);               // 100ms N_Ar timeout
    config.n_bs = std::chrono::milliseconds(1000);              // 1s N_Bs timeout
    config.n_cr = std::chrono::milliseconds(1000);              // 1s N_Cr timeout
    config.functional = false;                                  // Physical addressing
    
    transport.set_config(config);
    
    std::cout << "ISO-TP configured with simplified API" << std::endl;
    std::cout << "  Block Size: " << (int)config.blockSize << std::endl;
    std::cout << "  STmin: " << (int)config.stMin << " ms" << std::endl;
    std::cout << "  N_Bs timeout: " << config.n_bs.count() << " ms" << std::endl;
    
    // Read back configuration
    auto current_config = transport.config();
    std::cout << "Current config verified: BS=" << (int)current_config.blockSize << std::endl;
    
    // METHOD 2: Using legacy detailed API (still supported)
    std::cout << "\n=== Method 2: Legacy Detailed API ===" << std::endl;
    
    isotp::ISOTPTimings timings;
    timings.N_Ar = std::chrono::milliseconds(200);
    timings.N_Bs = std::chrono::milliseconds(1500);
    timings.N_Cr = std::chrono::milliseconds(1500);
    timings.max_wft = 15;  // Allow up to 15 FC(WT) frames
    
    transport.set_timings(timings);
    transport.set_block_size(8);
    transport.set_stmin(5);
    transport.set_functional_addressing(false);
    
    std::cout << "ISO-TP configured with legacy API" << std::endl;
    std::cout << "  Max WFT: " << (int)transport.timings().max_wft << std::endl;
    
    // METHOD 3: Functional addressing example
    std::cout << "\n=== Method 3: Functional Addressing ===" << std::endl;
    
    isotp::IsoTpConfig func_config;
    func_config.functional = true;  // Enable broadcast mode
    func_config.blockSize = 0;      // Unlimited (typical for functional)
    func_config.stMin = 0;          // No delay
    
    transport.set_config(func_config);
    
    std::cout << "Functional addressing configured" << std::endl;
    
    // Example: Send a UDS request
    std::cout << "\n=== Sending UDS Request ===" << std::endl;
    
    // Reset to physical addressing for actual communication
    config.functional = false;
    transport.set_config(config);
    
    std::vector<uint8_t> request = {0x10, 0x01};  // DiagnosticSessionControl
    std::vector<uint8_t> response;
    
    bool success = transport.request_response(
        request, 
        response, 
        std::chrono::milliseconds(5000)
    );
    
    if (success) {
        std::cout << "Response received: ";
        for (auto byte : response) {
            printf("%02X ", byte);
        }
        std::cout << std::endl;
    } else {
        std::cout << "Request failed or timed out" << std::endl;
    }
    
    driver.close();
    return 0;
}
