#pragma once
/**
 * @file uds_memory.hpp
 * @brief Memory Services - ISO 14229-1:2013 Sections 10.3, 10.8
 * 
 * ISO 14229-1:2013 Memory Operations:
 * - ReadMemoryByAddress (0x23) - Section 10.3 (p. 113)
 * - WriteMemoryByAddress (0x3D) - Section 10.8 (p. 167)
 * 
 * This module provides extended functionality for memory operations including:
 * - Memory area definitions and management
 * - Block-oriented transfers with CRC verification
 * - Memory protection and access control
 * - Resume capability for interrupted transfers
 * 
 * Message Formats:
 * 
 * ReadMemoryByAddress:
 *   Request:  [0x23] [addressAndLengthFormatIdentifier] [memoryAddress] [memorySize]
 *   Response: [0x63] [dataRecord (memory contents)]
 * 
 * WriteMemoryByAddress:
 *   Request:  [0x3D] [addressAndLengthFormatIdentifier] [memoryAddress] [memorySize] [dataRecord]
 *   Response: [0x7D] [addressAndLengthFormatIdentifier] [memoryAddress] [memorySize]
 * 
 * Address Format (Annex H):
 *   addressAndLengthFormatIdentifier format: [High nibble: address bytes][Low nibble: size bytes]
 *   Example: 0x44 = 4 bytes address, 4 bytes size
 * 
 * Builds on top of the core ReadMemoryByAddress (0x23) and WriteMemoryByAddress (0x3D)
 * services defined in uds.hpp.
 */

#include "uds.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <optional>
#include <functional>

namespace uds {
namespace memory {

// ============================================================================
// Memory Address Format (ISO 14229-1 Annex G)
// ============================================================================

/**
 * @brief Address and length format identifier
 * 
 * Encodes the size of memory address and memory size fields.
 * High nibble = address bytes (1-5), Low nibble = size bytes (1-4)
 */
struct AddressFormat {
    uint8_t address_bytes;  ///< Number of bytes for address (1-5)
    uint8_t size_bytes;     ///< Number of bytes for size (1-4)
    
    AddressFormat() : address_bytes(4), size_bytes(4) {}
    AddressFormat(uint8_t addr, uint8_t sz) : address_bytes(addr), size_bytes(sz) {}
    
    /**
     * @brief Get the format byte for UDS request
     */
    uint8_t to_format_byte() const {
        return ((address_bytes & 0x0F) << 4) | (size_bytes & 0x0F);
    }
    
    /**
     * @brief Parse format byte from UDS response
     */
    static AddressFormat from_format_byte(uint8_t fmt) {
        return AddressFormat((fmt >> 4) & 0x0F, fmt & 0x0F);
    }
};

// ============================================================================
// Memory Area Definition
// ============================================================================

/**
 * @brief Access level for memory areas
 */
enum class MemoryAccessLevel : uint8_t {
    Public = 0x00,          ///< No security required
    Level1 = 0x01,          ///< Security level 1 required
    Level2 = 0x02,          ///< Security level 2 required  
    Level3 = 0x03,          ///< Security level 3 required
    Programming = 0x10,     ///< Programming session required
    Extended = 0x20,        ///< Extended session required
    OEM = 0x40              ///< OEM-specific access
};

/**
 * @brief Memory area type
 */
enum class MemoryType : uint8_t {
    RAM = 0x00,             ///< Volatile RAM
    ROM = 0x01,             ///< Read-only memory
    EEPROM = 0x02,          ///< Electrically erasable ROM
    Flash = 0x03,           ///< Flash memory
    NVM = 0x04,             ///< Non-volatile memory (generic)
    Calibration = 0x10,     ///< Calibration data area
    Code = 0x20,            ///< Executable code area
    Data = 0x30,            ///< Data storage area
    Reserved = 0xFF         ///< Reserved/unknown
};

/**
 * @brief Memory area definition
 */
struct MemoryArea {
    uint16_t area_id;           ///< Unique identifier for this area
    std::string name;           ///< Human-readable name
    uint64_t start_address;     ///< Start address
    uint64_t size;              ///< Size in bytes
    MemoryType type;            ///< Memory type
    MemoryAccessLevel access;   ///< Required access level
    bool is_readable;           ///< Can be read
    bool is_writable;           ///< Can be written
    bool is_erasable;           ///< Can be erased (flash)
    uint32_t erase_block_size;  ///< Erase block size (for flash)
    uint32_t write_block_size;  ///< Write block size
    
    MemoryArea()
        : area_id(0), start_address(0), size(0), type(MemoryType::RAM),
          access(MemoryAccessLevel::Public), is_readable(true), is_writable(true),
          is_erasable(false), erase_block_size(0), write_block_size(1) {}
    
    /**
     * @brief Check if an address range is within this area
     */
    bool contains(uint64_t addr, uint64_t len) const {
        return addr >= start_address && (addr + len) <= (start_address + size);
    }
    
    /**
     * @brief Get end address (exclusive)
     */
    uint64_t end_address() const { return start_address + size; }
};

// ============================================================================
// Memory Operation Results
// ============================================================================

/**
 * @brief Result of a memory read operation
 */
struct MemoryReadResult {
    bool ok = false;
    std::vector<uint8_t> data;
    uint64_t address = 0;
    NegativeResponseCode nrc = static_cast<NegativeResponseCode>(0x00);
    std::string error_message;
};

/**
 * @brief Result of a memory write operation
 */
struct MemoryWriteResult {
    bool ok = false;
    uint64_t address = 0;
    uint64_t bytes_written = 0;
    NegativeResponseCode nrc = static_cast<NegativeResponseCode>(0x00);
    std::string error_message;
};

/**
 * @brief Progress callback for long operations
 */
using ProgressCallback = std::function<void(uint64_t current, uint64_t total)>;

// ============================================================================
// Memory Manager Class
// ============================================================================

/**
 * @brief Manages memory areas and provides enhanced memory operations
 */
class MemoryManager {
public:
    explicit MemoryManager(Client& client);
    
    // ========================================================================
    // Memory Area Management
    // ========================================================================
    
    /**
     * @brief Define a memory area
     * @param area Memory area definition
     */
    void define_area(const MemoryArea& area);
    
    /**
     * @brief Get a defined memory area by ID
     */
    std::optional<MemoryArea> get_area(uint16_t area_id) const;
    
    /**
     * @brief Get a defined memory area by name
     */
    std::optional<MemoryArea> get_area_by_name(const std::string& name) const;
    
    /**
     * @brief Find area containing an address
     */
    std::optional<MemoryArea> find_area_for_address(uint64_t address) const;
    
    /**
     * @brief Get all defined areas
     */
    std::vector<MemoryArea> get_all_areas() const;
    
    /**
     * @brief Clear all defined areas
     */
    void clear_areas();
    
    // ========================================================================
    // Basic Memory Operations
    // ========================================================================
    
    /**
     * @brief Read memory by address
     * @param address Start address
     * @param size Number of bytes to read
     * @param format Address format (default: 4-byte address, 4-byte size)
     * @return Read result with data
     */
    MemoryReadResult read(uint64_t address, uint32_t size,
                          AddressFormat format = AddressFormat());
    
    /**
     * @brief Write memory by address
     * @param address Start address
     * @param data Data to write
     * @param format Address format
     * @return Write result
     */
    MemoryWriteResult write(uint64_t address, const std::vector<uint8_t>& data,
                            AddressFormat format = AddressFormat());
    
    /**
     * @brief Read from a defined memory area
     * @param area_id Memory area ID
     * @param offset Offset within the area
     * @param size Number of bytes to read
     * @return Read result
     */
    MemoryReadResult read_area(uint16_t area_id, uint64_t offset, uint32_t size);
    
    /**
     * @brief Write to a defined memory area
     * @param area_id Memory area ID
     * @param offset Offset within the area
     * @param data Data to write
     * @return Write result
     */
    MemoryWriteResult write_area(uint16_t area_id, uint64_t offset,
                                  const std::vector<uint8_t>& data);
    
    // ========================================================================
    // Block Operations
    // ========================================================================
    
    /**
     * @brief Read memory in blocks with progress callback
     * @param address Start address
     * @param size Total size to read
     * @param block_size Size of each block
     * @param progress Progress callback
     * @return Read result with all data
     */
    MemoryReadResult read_blocks(uint64_t address, uint64_t size,
                                  uint32_t block_size = 256,
                                  ProgressCallback progress = nullptr);
    
    /**
     * @brief Write memory in blocks with progress callback
     * @param address Start address
     * @param data Data to write
     * @param block_size Size of each block
     * @param progress Progress callback
     * @return Write result
     */
    MemoryWriteResult write_blocks(uint64_t address, const std::vector<uint8_t>& data,
                                    uint32_t block_size = 256,
                                    ProgressCallback progress = nullptr);
    
    // ========================================================================
    // Verification
    // ========================================================================
    
    /**
     * @brief Verify memory contents match expected data
     * @param address Start address
     * @param expected Expected data
     * @return True if memory matches
     */
    bool verify(uint64_t address, const std::vector<uint8_t>& expected);
    
    /**
     * @brief Calculate CRC32 of memory region
     * @param address Start address
     * @param size Size of region
     * @return CRC32 value, or nullopt on error
     */
    std::optional<uint32_t> calculate_crc32(uint64_t address, uint64_t size);
    
    /**
     * @brief Compare two memory regions
     * @param addr1 First address
     * @param addr2 Second address
     * @param size Size to compare
     * @return True if regions are identical
     */
    bool compare(uint64_t addr1, uint64_t addr2, uint64_t size);
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    /**
     * @brief Set default address format
     */
    void set_default_format(AddressFormat format) { default_format_ = format; }
    
    /**
     * @brief Get default address format
     */
    AddressFormat default_format() const { return default_format_; }
    
    /**
     * @brief Set maximum block size for transfers
     */
    void set_max_block_size(uint32_t size) { max_block_size_ = size; }
    
    /**
     * @brief Get maximum block size
     */
    uint32_t max_block_size() const { return max_block_size_; }

private:
    Client& client_;
    std::map<uint16_t, MemoryArea> areas_;
    AddressFormat default_format_;
    uint32_t max_block_size_ = 4096;
    
    // Helper to encode address
    std::vector<uint8_t> encode_address(uint64_t address, uint8_t bytes) const;
    
    // Helper to encode size
    std::vector<uint8_t> encode_size(uint64_t size, uint8_t bytes) const;
};

// ============================================================================
// Common Memory Maps
// ============================================================================

namespace common_maps {

/**
 * @brief Create a typical automotive ECU memory map
 * @return Vector of common memory areas
 */
std::vector<MemoryArea> create_automotive_ecu_map();

/**
 * @brief Create a typical body control module memory map
 */
std::vector<MemoryArea> create_bcm_map();

/**
 * @brief Create a typical engine control module memory map
 */
std::vector<MemoryArea> create_ecm_map();

} // namespace common_maps

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Format address for display
 */
std::string format_address(uint64_t address, uint8_t width = 8);

/**
 * @brief Format memory size for display (with units)
 */
std::string format_size(uint64_t size);

/**
 * @brief Hex dump of memory data
 */
std::string hex_dump(const std::vector<uint8_t>& data, uint64_t base_address = 0,
                     uint32_t bytes_per_line = 16);

/**
 * @brief Calculate CRC32 of data
 */
uint32_t crc32(const std::vector<uint8_t>& data);

/**
 * @brief Calculate CRC32 of data with initial value
 */
uint32_t crc32(const std::vector<uint8_t>& data, uint32_t initial);

} // namespace memory
} // namespace uds
