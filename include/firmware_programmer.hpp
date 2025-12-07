#pragma once

#include "uds.hpp"
#include <vector>
#include <cstdint>
#include <optional>

namespace uds {

// ============================================================================
// Simplified Firmware Programming API
// ============================================================================
// This provides individual step-by-step control over the programming process.
// For full automated programming, see ecu_programming.hpp (ECUProgrammer class)
// ============================================================================

/// Result structure for programming operations
struct Result {
    bool ok;
    std::vector<uint8_t> data;
    std::optional<uint8_t> nrc;  // Negative Response Code if !ok
};

class FirmwareProgrammer {
public:
    explicit FirmwareProgrammer(Client& client);
    
    // ========================================================================
    // Step 1: Enter Programming Session
    // ========================================================================
    
    /// Enter programming session (0x10 0x02)
    Result enter_programming_session();
    
    // ========================================================================
    // Step 2: Security Access
    // ========================================================================
    
    /// Unlock security with pre-calculated key
    /// @param level Security level (typically 0x01)
    /// @param key Pre-calculated key from seed
    Result unlock_security(uint8_t level, const std::vector<uint8_t>& key);
    
    // ========================================================================
    // Step 3-4: Disable DTCs and Communications
    // ========================================================================
    
    /// Disable DTC setting (0x85 0x02)
    Result disable_dtcs();
    
    /// Disable communications (0x28 0x03 0xFF)
    Result disable_comms();
    
    // ========================================================================
    // Step 5: Erase Memory
    // ========================================================================
    
    /// Erase memory region (RoutineControl 0x31, routine 0xFF00)
    /// @param address Memory address to erase
    /// @param size Size of region to erase
    Result erase_memory(uint32_t address, uint32_t size);
    
    // ========================================================================
    // Step 6: Request Download
    // ========================================================================
    
    /// Request download operation (0x34)
    /// @param address Target memory address
    /// @param size Total size of data to download
    /// @param fmt Data format identifier (0x00 = uncompressed/unencrypted)
    /// @return Result with max_block_size in data field
    Result request_download(uint32_t address, uint32_t size, uint8_t fmt = 0x00);
    
    // ========================================================================
    // Step 7: Transfer Data
    // ========================================================================
    
    /// Transfer single data block (0x36)
    /// @param block Data block to transfer
    /// @param blockCounter Block sequence counter (starts at 1)
    Result transfer_data(const std::vector<uint8_t>& block, uint8_t blockCounter);
    
    // ========================================================================
    // Step 8: Request Transfer Exit
    // ========================================================================
    
    /// Exit transfer mode (0x37)
    Result request_transfer_exit();
    
    // ========================================================================
    // Step 9-10: Finalize
    // ========================================================================
    
    /// Re-enable DTCs/comms and optionally reset ECU
    /// This combines steps 9 and 10 for convenience
    Result finalize();
    
    // ========================================================================
    // Helpers
    // ========================================================================
    
    /// Get maximum block size from last request_download
    uint32_t get_max_block_size() const { return max_block_size_; }
    
    /// Set maximum block size manually (if known)
    void set_max_block_size(uint32_t size) { max_block_size_ = size; }

private:
    Client& client_;
    uint32_t max_block_size_ = 0;
    
    /// Parse max block length from RequestDownload response
    uint32_t parse_max_block_length(const std::vector<uint8_t>& response);
};

} // namespace uds
