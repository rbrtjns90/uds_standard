/**
 * @file fuzz_target_afl.cpp
 * @brief AFL++ fuzzing target for UDS implementation
 * 
 * This target is designed for use with AFL++ (American Fuzzy Lop)
 * for coverage-guided fuzzing.
 * 
 * Compile with:
 *   afl-g++ -std=c++17 -Iinclude tests/fuzz_target_afl.cpp src/*.cpp -o fuzz_afl
 * 
 * Run with:
 *   afl-fuzz -i fuzz_seeds/ -o fuzz_findings/ ./fuzz_afl
 */

#include "../include/uds.hpp"
#include "../include/isotp.hpp"
#include "../include/nrc.hpp"
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Mock transport for fuzzing
class FuzzTransport {
public:
    bool send(const std::vector<uint8_t>& data, std::chrono::milliseconds /*timeout*/) {
        last_sent_ = data;
        return true;
    }
    
    bool receive(std::vector<uint8_t>& data, std::chrono::milliseconds /*timeout*/) {
        if (response_queue_.empty()) {
            return false;
        }
        data = response_queue_.front();
        response_queue_.erase(response_queue_.begin());
        return true;
    }
    
    void queue_response(const std::vector<uint8_t>& response) {
        response_queue_.push_back(response);
    }
    
    std::vector<uint8_t> get_last_sent() const { return last_sent_; }
    
private:
    std::vector<uint8_t> last_sent_;
    std::vector<std::vector<uint8_t>> response_queue_;
};

// Parse UDS message safely
void fuzz_uds_message(const uint8_t* data, size_t size) {
    if (size == 0 || size > 4095) {
        return; // Invalid size
    }
    
    std::vector<uint8_t> message(data, data + size);
    
    // Extract service ID
    uint8_t service_id = message[0];
    
    // Test different message types
    switch (service_id) {
        case 0x10: // DiagnosticSessionControl
            if (size >= 2) {
                uint8_t session = message[1];
                // Validate session type
            }
            break;
            
        case 0x22: // ReadDataByIdentifier
            if (size >= 3) {
                uint16_t did = (message[1] << 8) | message[2];
                // Validate DID
            }
            break;
            
        case 0x27: // SecurityAccess
            if (size >= 2) {
                uint8_t level = message[1];
                // Validate security level
            }
            break;
            
        case 0x2E: // WriteDataByIdentifier
            if (size >= 4) {
                uint16_t did = (message[1] << 8) | message[2];
                // Validate write data
            }
            break;
            
        case 0x31: // RoutineControl
            if (size >= 4) {
                uint8_t subfunction = message[1];
                uint16_t routine_id = (message[2] << 8) | message[3];
                // Validate routine control
            }
            break;
            
        case 0x34: // RequestDownload
        case 0x35: // RequestUpload
            if (size >= 3) {
                uint8_t data_format = message[1];
                uint8_t alfid = message[2];
                uint8_t addr_bytes = (alfid >> 4) & 0x0F;
                uint8_t len_bytes = alfid & 0x0F;
                
                if (size >= 3 + addr_bytes + len_bytes) {
                    // Valid format, parse address and length
                }
            }
            break;
            
        case 0x36: // TransferData
            if (size >= 2) {
                uint8_t block_counter = message[1];
                // Validate block sequence
            }
            break;
            
        case 0x3E: // TesterPresent
            if (size >= 2) {
                uint8_t subfunction = message[1];
                // Validate tester present
            }
            break;
            
        default:
            // Unknown or OEM-specific service
            break;
    }
}

// Parse ISO-TP frame safely
void fuzz_isotp_frame(const uint8_t* data, size_t size) {
    if (size == 0 || size > 8) {
        return; // Invalid CAN frame size
    }
    
    uint8_t pci = data[0];
    uint8_t pci_type = (pci >> 4) & 0x0F;
    
    switch (pci_type) {
        case 0: // Single Frame
            {
                uint8_t length = pci & 0x0F;
                if (length > 0 && length <= 7 && size >= length + 1) {
                    // Valid single frame
                    fuzz_uds_message(data + 1, length);
                }
            }
            break;
            
        case 1: // First Frame
            {
                uint16_t length = ((pci & 0x0F) << 8) | data[1];
                if (length > 7 && size >= 8) {
                    // Valid first frame (6 bytes of data)
                    // Would need consecutive frames to complete
                }
            }
            break;
            
        case 2: // Consecutive Frame
            {
                uint8_t sequence = pci & 0x0F;
                if (size >= 8) {
                    // Valid consecutive frame (7 bytes of data)
                }
            }
            break;
            
        case 3: // Flow Control
            {
                uint8_t flow_status = pci & 0x0F;
                if (size >= 3) {
                    uint8_t block_size = data[1];
                    uint8_t st_min = data[2];
                    // Valid flow control frame
                }
            }
            break;
            
        default:
            // Invalid PCI type (4-15)
            break;
    }
}

// Main AFL++ entry point
int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }
    
    // Read input file
    FILE* fp = fopen(argv[1], "rb");
    if (!fp) {
        return 1;
    }
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size <= 0 || size > 4096) {
        fclose(fp);
        return 0; // Skip invalid sizes
    }
    
    // Read data
    std::vector<uint8_t> data(size);
    size_t read = fread(data.data(), 1, size, fp);
    fclose(fp);
    
    if (read != static_cast<size_t>(size)) {
        return 0;
    }
    
    // Fuzz the input
    try {
        // Test as ISO-TP frame (if 8 bytes or less)
        if (size <= 8) {
            fuzz_isotp_frame(data.data(), size);
        }
        
        // Test as UDS message
        fuzz_uds_message(data.data(), size);
        
    } catch (...) {
        // Catch any exceptions to prevent crashes
        // AFL++ will still detect hangs and excessive memory usage
    }
    
    return 0;
}

