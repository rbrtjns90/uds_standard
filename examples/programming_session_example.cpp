/*
  Example: Using ProgrammingSession for step-by-step ECU programming
  
  This example demonstrates the high-level ProgrammingSession API for
  procedural, step-by-step control over ECU programming operations.
  
  Compare with:
  - ECUProgrammer (ecu_programming.hpp) - Fully automated programming
  - FirmwareProgrammer (firmware_programmer.hpp) - Lower-level manual control
*/

#include "uds.hpp"
#include "uds_programming.hpp"
#include "isotp.hpp"
#include "slcan_serial.hpp"
#include <iostream>
#include <fstream>
#include <vector>

// Example OEM-specific key calculation
// In real code, this would implement the actual security algorithm
std::vector<uint8_t> calculate_key_from_seed(const std::vector<uint8_t>& seed) {
    std::vector<uint8_t> key;
    key.reserve(seed.size());
    
    // PLACEHOLDER: Replace with actual OEM algorithm
    // This is just XOR with 0xAA for demonstration
    for (auto byte : seed) {
        key.push_back(byte ^ 0xAA);
    }
    
    return key;
}

// Helper: load binary file
std::vector<uint8_t> load_firmware(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        return {};
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    
    return data;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <serial_device> <firmware.bin>\n";
        std::cerr << "Example: " << argv[0] << " /dev/ttyUSB0 ecu_firmware.bin\n";
        return 1;
    }
    
    std::string device = argv[1];
    std::string firmware_file = argv[2];
    
    std::cout << "=== UDS Programming Session Example ===\n";
    std::cout << "Device: " << device << "\n";
    std::cout << "Firmware: " << firmware_file << "\n\n";
    
    // Load firmware
    auto firmware = load_firmware(firmware_file);
    if (firmware.empty()) {
        std::cerr << "Failed to load firmware file\n";
        return 1;
    }
    std::cout << "Loaded firmware: " << firmware.size() << " bytes\n\n";
    
    // Setup CAN/ISO-TP stack
    slcan::SerialDriver driver;
    if (!driver.open(device, CANProtocol::CAN_BITRATE_500K)) {
        std::cerr << "Failed to open SLCAN device\n";
        return 1;
    }
    
    isotp::Transport transport(driver);
    
    // Configure addressing
    uds::Address addr;
    addr.tx_can_id = 0x7E0;  // Tester -> ECU
    addr.rx_can_id = 0x7E8;  // ECU -> Tester
    transport.set_address(addr);
    
    // Create UDS client
    uds::Client client(transport);
    
    // Create programming session helper
    uds::ProgrammingSession prog(client);
    
    // ========================================================================
    // STEP-BY-STEP PROGRAMMING WORKFLOW
    // ========================================================================
    
    // Step 1: Enter programming session
    std::cout << "[1/9] Entering programming session...\n";
    auto status = prog.enter_programming_session();
    if (!status.ok) {
        std::cerr << "ERROR: " << status.message << "\n";
        return 1;
    }
    std::cout << "  ✓ " << status.message << "\n\n";
    
    // Step 2: Unlock security
    std::cout << "[2/9] Unlocking security access...\n";
    status = prog.unlock(0x01, calculate_key_from_seed);
    if (!status.ok) {
        std::cerr << "ERROR: " << status.message << "\n";
        return 1;
    }
    std::cout << "  ✓ " << status.message << "\n\n";
    
    // Step 3: Disable DTC setting
    std::cout << "[3/9] Disabling DTC setting...\n";
    status = prog.disable_dtcs();
    if (!status.ok) {
        std::cerr << "ERROR: " << status.message << "\n";
        return 1;
    }
    std::cout << "  ✓ " << status.message << "\n\n";
    
    // Step 4: Disable communications
    std::cout << "[4/9] Disabling non-diagnostic communications...\n";
    status = prog.disable_comms();
    if (!status.ok) {
        std::cerr << "ERROR: " << status.message << "\n";
        return 1;
    }
    std::cout << "  ✓ " << status.message << "\n\n";
    
    // Step 5: Erase memory
    std::cout << "[5/9] Erasing ECU memory...\n";
    
    // Build erase record (OEM-specific format)
    // Example: 4-byte address + 4-byte size
    std::vector<uint8_t> erase_record;
    uint32_t erase_addr = 0x00020000;
    uint32_t erase_size = 0x00100000; // 1MB
    
    erase_record.push_back((erase_addr >> 24) & 0xFF);
    erase_record.push_back((erase_addr >> 16) & 0xFF);
    erase_record.push_back((erase_addr >> 8) & 0xFF);
    erase_record.push_back(erase_addr & 0xFF);
    
    erase_record.push_back((erase_size >> 24) & 0xFF);
    erase_record.push_back((erase_size >> 16) & 0xFF);
    erase_record.push_back((erase_size >> 8) & 0xFF);
    erase_record.push_back(erase_size & 0xFF);
    
    status = prog.erase_memory(0xFF00, erase_record);
    if (!status.ok) {
        std::cerr << "ERROR: " << status.message << "\n";
        return 1;
    }
    std::cout << "  ✓ " << status.message << "\n\n";
    
    // Step 6: Request download
    std::cout << "[6/9] Requesting download...\n";
    
    // Build address and size vectors
    std::vector<uint8_t> dl_addr;
    std::vector<uint8_t> dl_size;
    
    uint32_t download_addr = 0x00020000;
    uint32_t download_size = firmware.size();
    
    // 4-byte big-endian address
    dl_addr.push_back((download_addr >> 24) & 0xFF);
    dl_addr.push_back((download_addr >> 16) & 0xFF);
    dl_addr.push_back((download_addr >> 8) & 0xFF);
    dl_addr.push_back(download_addr & 0xFF);
    
    // 4-byte big-endian size
    dl_size.push_back((download_size >> 24) & 0xFF);
    dl_size.push_back((download_size >> 16) & 0xFF);
    dl_size.push_back((download_size >> 8) & 0xFF);
    dl_size.push_back(download_size & 0xFF);
    
    status = prog.request_download(0x00, dl_addr, dl_size);
    if (!status.ok) {
        std::cerr << "ERROR: " << status.message << "\n";
        return 1;
    }
    std::cout << "  ✓ " << status.message << "\n";
    std::cout << "  Max block size: " << prog.max_block_size() << " bytes\n\n";
    
    // Step 7: Transfer firmware
    std::cout << "[7/9] Transferring firmware...\n";
    std::cout << "  Total size: " << firmware.size() << " bytes\n";
    std::cout << "  Blocks: " << (firmware.size() + prog.max_block_size() - 1) / prog.max_block_size() << "\n";
    
    status = prog.transfer_image(firmware);
    if (!status.ok) {
        std::cerr << "ERROR: " << status.message << "\n";
        return 1;
    }
    std::cout << "  ✓ " << status.message << "\n\n";
    
    // Step 8: Exit transfer
    std::cout << "[8/9] Exiting transfer...\n";
    status = prog.request_transfer_exit();
    if (!status.ok) {
        std::cerr << "ERROR: " << status.message << "\n";
        return 1;
    }
    std::cout << "  ✓ " << status.message << "\n\n";
    
    // Step 9: Finalize and reset
    std::cout << "[9/9] Finalizing (re-enable services and reset ECU)...\n";
    status = prog.finalize(uds::EcuResetType::HardReset);
    if (!status.ok) {
        std::cerr << "ERROR: " << status.message << "\n";
        return 1;
    }
    std::cout << "  ✓ " << status.message << "\n\n";
    
    std::cout << "===========================================\n";
    std::cout << "✓ Programming completed successfully!\n";
    std::cout << "===========================================\n";
    
    driver.close();
    return 0;
}
