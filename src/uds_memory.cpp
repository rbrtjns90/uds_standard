#include "uds_memory.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

namespace uds {
namespace memory {

// ============================================================================
// CRC32 Implementation (IEEE 802.3 polynomial)
// ============================================================================

static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706B3,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

uint32_t crc32(const std::vector<uint8_t>& data) {
    return crc32(data, 0xFFFFFFFF) ^ 0xFFFFFFFF;
}

uint32_t crc32(const std::vector<uint8_t>& data, uint32_t initial) {
    uint32_t crc = initial;
    for (uint8_t byte : data) {
        crc = crc32_table[(crc ^ byte) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

// ============================================================================
// MemoryManager Implementation
// ============================================================================

MemoryManager::MemoryManager(Client& client)
    : client_(client), default_format_(4, 4) {
}

void MemoryManager::define_area(const MemoryArea& area) {
    areas_[area.area_id] = area;
}

std::optional<MemoryArea> MemoryManager::get_area(uint16_t area_id) const {
    auto it = areas_.find(area_id);
    if (it != areas_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<MemoryArea> MemoryManager::get_area_by_name(const std::string& name) const {
    for (const auto& [id, area] : areas_) {
        if (area.name == name) {
            return area;
        }
    }
    return std::nullopt;
}

std::optional<MemoryArea> MemoryManager::find_area_for_address(uint64_t address) const {
    for (const auto& [id, area] : areas_) {
        if (address >= area.start_address && address < area.end_address()) {
            return area;
        }
    }
    return std::nullopt;
}

std::vector<MemoryArea> MemoryManager::get_all_areas() const {
    std::vector<MemoryArea> result;
    result.reserve(areas_.size());
    for (const auto& [id, area] : areas_) {
        result.push_back(area);
    }
    return result;
}

void MemoryManager::clear_areas() {
    areas_.clear();
}

std::vector<uint8_t> MemoryManager::encode_address(uint64_t address, uint8_t bytes) const {
    std::vector<uint8_t> result(bytes);
    for (int i = bytes - 1; i >= 0; --i) {
        result[i] = static_cast<uint8_t>(address & 0xFF);
        address >>= 8;
    }
    return result;
}

std::vector<uint8_t> MemoryManager::encode_size(uint64_t size, uint8_t bytes) const {
    std::vector<uint8_t> result(bytes);
    for (int i = bytes - 1; i >= 0; --i) {
        result[i] = static_cast<uint8_t>(size & 0xFF);
        size >>= 8;
    }
    return result;
}

MemoryReadResult MemoryManager::read(uint64_t address, uint32_t size, AddressFormat format) {
    MemoryReadResult result;
    result.address = address;
    
    // Build payload: [addressAndLengthFormatIdentifier][memoryAddress][memorySize]
    std::vector<uint8_t> payload;
    payload.push_back(format.to_format_byte());
    
    auto addr_bytes = encode_address(address, format.address_bytes);
    payload.insert(payload.end(), addr_bytes.begin(), addr_bytes.end());
    
    auto size_bytes = encode_size(size, format.size_bytes);
    payload.insert(payload.end(), size_bytes.begin(), size_bytes.end());
    
    // Send request
    auto response = client_.exchange(SID::ReadMemoryByAddress, payload);
    
    if (!response.ok) {
        result.ok = false;
        result.nrc = response.nrc.code;
        
        switch (response.nrc.code) {
            case NegativeResponseCode::RequestOutOfRange:
                result.error_message = "Address or size out of range";
                break;
            case NegativeResponseCode::SecurityAccessDenied:
                result.error_message = "Security access required";
                break;
            case NegativeResponseCode::ConditionsNotCorrect:
                result.error_message = "Conditions not correct";
                break;
            default:
                result.error_message = "Read failed with NRC 0x" + 
                    std::to_string(static_cast<int>(response.nrc.code));
        }
        return result;
    }
    
    result.ok = true;
    result.data = response.payload;
    return result;
}

MemoryWriteResult MemoryManager::write(uint64_t address, const std::vector<uint8_t>& data,
                                        AddressFormat format) {
    MemoryWriteResult result;
    result.address = address;
    
    // Build payload: [addressAndLengthFormatIdentifier][memoryAddress][memorySize][dataRecord]
    std::vector<uint8_t> payload;
    payload.push_back(format.to_format_byte());
    
    auto addr_bytes = encode_address(address, format.address_bytes);
    payload.insert(payload.end(), addr_bytes.begin(), addr_bytes.end());
    
    auto size_bytes = encode_size(data.size(), format.size_bytes);
    payload.insert(payload.end(), size_bytes.begin(), size_bytes.end());
    
    payload.insert(payload.end(), data.begin(), data.end());
    
    // Send request
    auto response = client_.exchange(SID::WriteMemoryByAddress, payload);
    
    if (!response.ok) {
        result.ok = false;
        result.nrc = response.nrc.code;
        
        switch (response.nrc.code) {
            case NegativeResponseCode::RequestOutOfRange:
                result.error_message = "Address or size out of range";
                break;
            case NegativeResponseCode::SecurityAccessDenied:
                result.error_message = "Security access required";
                break;
            case NegativeResponseCode::ConditionsNotCorrect:
                result.error_message = "Conditions not correct";
                break;
            case NegativeResponseCode::GeneralProgrammingFailure:
                result.error_message = "Programming failure";
                break;
            default:
                result.error_message = "Write failed with NRC 0x" + 
                    std::to_string(static_cast<int>(response.nrc.code));
        }
        return result;
    }
    
    result.ok = true;
    result.bytes_written = data.size();
    return result;
}

MemoryReadResult MemoryManager::read_area(uint16_t area_id, uint64_t offset, uint32_t size) {
    auto area = get_area(area_id);
    if (!area) {
        MemoryReadResult result;
        result.ok = false;
        result.error_message = "Memory area not defined";
        return result;
    }
    
    if (!area->is_readable) {
        MemoryReadResult result;
        result.ok = false;
        result.error_message = "Memory area is not readable";
        return result;
    }
    
    if (offset + size > area->size) {
        MemoryReadResult result;
        result.ok = false;
        result.error_message = "Read extends beyond area boundary";
        return result;
    }
    
    return read(area->start_address + offset, size, default_format_);
}

MemoryWriteResult MemoryManager::write_area(uint16_t area_id, uint64_t offset,
                                             const std::vector<uint8_t>& data) {
    auto area = get_area(area_id);
    if (!area) {
        MemoryWriteResult result;
        result.ok = false;
        result.error_message = "Memory area not defined";
        return result;
    }
    
    if (!area->is_writable) {
        MemoryWriteResult result;
        result.ok = false;
        result.error_message = "Memory area is not writable";
        return result;
    }
    
    if (offset + data.size() > area->size) {
        MemoryWriteResult result;
        result.ok = false;
        result.error_message = "Write extends beyond area boundary";
        return result;
    }
    
    return write(area->start_address + offset, data, default_format_);
}

MemoryReadResult MemoryManager::read_blocks(uint64_t address, uint64_t size,
                                             uint32_t block_size,
                                             ProgressCallback progress) {
    MemoryReadResult result;
    result.address = address;
    result.data.reserve(size);
    
    uint64_t remaining = size;
    uint64_t current_addr = address;
    
    while (remaining > 0) {
        uint32_t chunk = static_cast<uint32_t>(std::min(static_cast<uint64_t>(block_size), remaining));
        
        auto block_result = read(current_addr, chunk, default_format_);
        if (!block_result.ok) {
            result.ok = false;
            result.nrc = block_result.nrc;
            result.error_message = block_result.error_message;
            return result;
        }
        
        result.data.insert(result.data.end(), 
                          block_result.data.begin(), 
                          block_result.data.end());
        
        current_addr += chunk;
        remaining -= chunk;
        
        if (progress) {
            progress(size - remaining, size);
        }
    }
    
    result.ok = true;
    return result;
}

MemoryWriteResult MemoryManager::write_blocks(uint64_t address, const std::vector<uint8_t>& data,
                                               uint32_t block_size,
                                               ProgressCallback progress) {
    MemoryWriteResult result;
    result.address = address;
    
    uint64_t remaining = data.size();
    uint64_t offset = 0;
    
    while (remaining > 0) {
        uint32_t chunk = static_cast<uint32_t>(std::min(static_cast<uint64_t>(block_size), remaining));
        
        std::vector<uint8_t> block_data(data.begin() + offset, 
                                        data.begin() + offset + chunk);
        
        auto block_result = write(address + offset, block_data, default_format_);
        if (!block_result.ok) {
            result.ok = false;
            result.nrc = block_result.nrc;
            result.error_message = block_result.error_message;
            result.bytes_written = offset;
            return result;
        }
        
        offset += chunk;
        remaining -= chunk;
        
        if (progress) {
            progress(offset, data.size());
        }
    }
    
    result.ok = true;
    result.bytes_written = data.size();
    return result;
}

bool MemoryManager::verify(uint64_t address, const std::vector<uint8_t>& expected) {
    auto result = read(address, static_cast<uint32_t>(expected.size()), default_format_);
    if (!result.ok) {
        return false;
    }
    return result.data == expected;
}

std::optional<uint32_t> MemoryManager::calculate_crc32(uint64_t address, uint64_t size) {
    auto result = read_blocks(address, size, max_block_size_);
    if (!result.ok) {
        return std::nullopt;
    }
    return crc32(result.data);
}

bool MemoryManager::compare(uint64_t addr1, uint64_t addr2, uint64_t size) {
    auto result1 = read_blocks(addr1, size, max_block_size_);
    if (!result1.ok) return false;
    
    auto result2 = read_blocks(addr2, size, max_block_size_);
    if (!result2.ok) return false;
    
    return result1.data == result2.data;
}

// ============================================================================
// Common Memory Maps
// ============================================================================

namespace common_maps {

std::vector<MemoryArea> create_automotive_ecu_map() {
    std::vector<MemoryArea> areas;
    
    // Boot loader
    MemoryArea boot;
    boot.area_id = 0x0001;
    boot.name = "Bootloader";
    boot.start_address = 0x00000000;
    boot.size = 0x8000;  // 32KB
    boot.type = MemoryType::Flash;
    boot.access = MemoryAccessLevel::Programming;
    boot.is_readable = true;
    boot.is_writable = false;
    boot.is_erasable = false;
    areas.push_back(boot);
    
    // Application code
    MemoryArea app;
    app.area_id = 0x0002;
    app.name = "Application";
    app.start_address = 0x00008000;
    app.size = 0x78000;  // 480KB
    app.type = MemoryType::Flash;
    app.access = MemoryAccessLevel::Programming;
    app.is_readable = true;
    app.is_writable = true;
    app.is_erasable = true;
    app.erase_block_size = 4096;
    app.write_block_size = 256;
    areas.push_back(app);
    
    // Calibration data
    MemoryArea cal;
    cal.area_id = 0x0003;
    cal.name = "Calibration";
    cal.start_address = 0x00080000;
    cal.size = 0x10000;  // 64KB
    cal.type = MemoryType::Calibration;
    cal.access = MemoryAccessLevel::Extended;
    cal.is_readable = true;
    cal.is_writable = true;
    cal.is_erasable = true;
    cal.erase_block_size = 4096;
    cal.write_block_size = 64;
    areas.push_back(cal);
    
    // EEPROM emulation
    MemoryArea eeprom;
    eeprom.area_id = 0x0004;
    eeprom.name = "EEPROM";
    eeprom.start_address = 0x00090000;
    eeprom.size = 0x2000;  // 8KB
    eeprom.type = MemoryType::EEPROM;
    eeprom.access = MemoryAccessLevel::Extended;
    eeprom.is_readable = true;
    eeprom.is_writable = true;
    eeprom.is_erasable = false;
    eeprom.write_block_size = 4;
    areas.push_back(eeprom);
    
    // RAM
    MemoryArea ram;
    ram.area_id = 0x0010;
    ram.name = "RAM";
    ram.start_address = 0x20000000;
    ram.size = 0x20000;  // 128KB
    ram.type = MemoryType::RAM;
    ram.access = MemoryAccessLevel::Extended;
    ram.is_readable = true;
    ram.is_writable = true;
    ram.is_erasable = false;
    areas.push_back(ram);
    
    return areas;
}

std::vector<MemoryArea> create_bcm_map() {
    auto areas = create_automotive_ecu_map();
    
    // Add BCM-specific areas
    MemoryArea io_config;
    io_config.area_id = 0x0020;
    io_config.name = "IO_Config";
    io_config.start_address = 0x00092000;
    io_config.size = 0x1000;  // 4KB
    io_config.type = MemoryType::NVM;
    io_config.access = MemoryAccessLevel::Extended;
    io_config.is_readable = true;
    io_config.is_writable = true;
    io_config.is_erasable = false;
    areas.push_back(io_config);
    
    return areas;
}

std::vector<MemoryArea> create_ecm_map() {
    auto areas = create_automotive_ecu_map();
    
    // Add ECM-specific areas
    MemoryArea fuel_map;
    fuel_map.area_id = 0x0030;
    fuel_map.name = "Fuel_Maps";
    fuel_map.start_address = 0x00084000;
    fuel_map.size = 0x4000;  // 16KB
    fuel_map.type = MemoryType::Calibration;
    fuel_map.access = MemoryAccessLevel::Programming;
    fuel_map.is_readable = true;
    fuel_map.is_writable = true;
    fuel_map.is_erasable = true;
    areas.push_back(fuel_map);
    
    MemoryArea timing_map;
    timing_map.area_id = 0x0031;
    timing_map.name = "Timing_Maps";
    timing_map.start_address = 0x00088000;
    timing_map.size = 0x4000;  // 16KB
    timing_map.type = MemoryType::Calibration;
    timing_map.access = MemoryAccessLevel::Programming;
    timing_map.is_readable = true;
    timing_map.is_writable = true;
    timing_map.is_erasable = true;
    areas.push_back(timing_map);
    
    return areas;
}

} // namespace common_maps

// ============================================================================
// Utility Functions
// ============================================================================

std::string format_address(uint64_t address, uint8_t width) {
    std::ostringstream ss;
    ss << "0x" << std::hex << std::uppercase << std::setw(width) 
       << std::setfill('0') << address;
    return ss.str();
}

std::string format_size(uint64_t size) {
    std::ostringstream ss;
    
    if (size >= 1024 * 1024 * 1024) {
        ss << std::fixed << std::setprecision(2) << (size / (1024.0 * 1024.0 * 1024.0)) << " GB";
    } else if (size >= 1024 * 1024) {
        ss << std::fixed << std::setprecision(2) << (size / (1024.0 * 1024.0)) << " MB";
    } else if (size >= 1024) {
        ss << std::fixed << std::setprecision(2) << (size / 1024.0) << " KB";
    } else {
        ss << size << " bytes";
    }
    
    return ss.str();
}

std::string hex_dump(const std::vector<uint8_t>& data, uint64_t base_address,
                     uint32_t bytes_per_line) {
    std::ostringstream ss;
    
    for (size_t i = 0; i < data.size(); i += bytes_per_line) {
        // Address
        ss << format_address(base_address + i, 8) << "  ";
        
        // Hex bytes
        for (size_t j = 0; j < bytes_per_line; ++j) {
            if (i + j < data.size()) {
                ss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                   << static_cast<int>(data[i + j]) << " ";
            } else {
                ss << "   ";
            }
            
            if (j == bytes_per_line / 2 - 1) {
                ss << " ";
            }
        }
        
        ss << " |";
        
        // ASCII
        for (size_t j = 0; j < bytes_per_line && i + j < data.size(); ++j) {
            char c = static_cast<char>(data[i + j]);
            ss << (std::isprint(static_cast<unsigned char>(c)) ? c : '.');
        }
        
        ss << "|\n";
    }
    
    return ss.str();
}

} // namespace memory
} // namespace uds
