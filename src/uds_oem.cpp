/**
 * @file uds_oem.cpp
 * @brief OEM-specific UDS service extensions implementation
 */

#include "../include/uds_oem.hpp"
#include "../include/uds.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace uds {
namespace oem {

// ============================================================================
// OEMExtensions Implementation
// ============================================================================

OEMExtensions::OEMExtensions(Manufacturer manufacturer)
    : manufacturer_(manufacturer) {
    load_manufacturer_presets();
}

void OEMExtensions::set_manufacturer(Manufacturer manufacturer) {
    manufacturer_ = manufacturer;
    load_manufacturer_presets();
}

bool OEMExtensions::register_service(uint8_t service_id, OEMServiceHandler handler) {
    if (!is_oem_service(service_id)) {
        return false; // Not in OEM range
    }
    
    service_handlers_[service_id] = handler;
    return true;
}

bool OEMExtensions::unregister_service(uint8_t service_id) {
    auto it = service_handlers_.find(service_id);
    if (it != service_handlers_.end()) {
        service_handlers_.erase(it);
        return true;
    }
    return false;
}

bool OEMExtensions::is_service_registered(uint8_t service_id) const {
    return service_handlers_.find(service_id) != service_handlers_.end();
}

OEMServiceResponse OEMExtensions::execute_service([[maybe_unused]] Client& client, 
                                                   const OEMServiceRequest& request) {
    // Check if handler is registered
    auto it = service_handlers_.find(request.service_id);
    if (it == service_handlers_.end()) {
        return {false, request.service_id, {}, 0x11}; // ServiceNotSupported
    }
    
    // Execute the custom handler
    return it->second(request);
}

bool OEMExtensions::register_key_calculator(uint8_t level, OEMKeyCalculator calculator) {
    if (!is_oem_security_level(level)) {
        return false;
    }
    
    key_calculators_[level] = calculator;
    return true;
}

std::optional<OEMKeyCalculator> OEMExtensions::get_key_calculator(uint8_t level) const {
    auto it = key_calculators_.find(level);
    if (it != key_calculators_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<std::vector<uint8_t>> OEMExtensions::calculate_key(
    uint8_t level, const std::vector<uint8_t>& seed) {
    
    auto calculator = get_key_calculator(level);
    if (calculator) {
        return (*calculator)(seed);
    }
    return std::nullopt;
}

bool OEMExtensions::enter_oem_session(Client& client, OEMSession session) {
    uint8_t session_type = static_cast<uint8_t>(session);
    
    if (!is_oem_session(session_type)) {
        return false;
    }
    
    // Use standard diagnostic session control with OEM session type
    auto result = client.diagnostic_session_control(static_cast<Session>(session_type));
    return result.ok;
}

bool OEMExtensions::is_oem_session(uint8_t session_type) {
    return (session_type >= 0x40 && session_type <= 0x5F);
}

void OEMExtensions::load_manufacturer_presets() {
    // Clear existing presets
    service_handlers_.clear();
    key_calculators_.clear();
    did_descriptions_.clear();
    
    // Load manufacturer-specific presets
    switch (manufacturer_) {
        case Manufacturer::Volkswagen:
        case Manufacturer::Audi:
            load_volkswagen_presets();
            break;
            
        case Manufacturer::Ford:
            load_ford_presets();
            break;
            
        case Manufacturer::Toyota:
        case Manufacturer::Honda:
            load_toyota_presets();
            break;
            
        default:
            // Generic/no presets
            break;
    }
}

std::string OEMExtensions::get_manufacturer_name() const {
    switch (manufacturer_) {
        case Manufacturer::Generic: return "Generic";
        case Manufacturer::Volkswagen: return "Volkswagen";
        case Manufacturer::Audi: return "Audi";
        case Manufacturer::BMW: return "BMW";
        case Manufacturer::Mercedes: return "Mercedes-Benz";
        case Manufacturer::Porsche: return "Porsche";
        case Manufacturer::Volvo: return "Volvo";
        case Manufacturer::Ford: return "Ford";
        case Manufacturer::GeneralMotors: return "General Motors";
        case Manufacturer::Chrysler: return "Chrysler";
        case Manufacturer::Tesla: return "Tesla";
        case Manufacturer::Toyota: return "Toyota";
        case Manufacturer::Honda: return "Honda";
        case Manufacturer::Nissan: return "Nissan";
        case Manufacturer::Mazda: return "Mazda";
        case Manufacturer::Hyundai: return "Hyundai";
        case Manufacturer::Kia: return "Kia";
        case Manufacturer::Custom: return "Custom";
        default: return "Unknown";
    }
}

void OEMExtensions::register_did_description(uint16_t did, const std::string& description) {
    did_descriptions_[did] = description;
}

std::optional<std::string> OEMExtensions::get_did_description(uint16_t did) const {
    auto it = did_descriptions_.find(did);
    if (it != did_descriptions_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool OEMExtensions::is_oem_did(uint16_t did) {
    return (did >= DIDRanges::OEM_SPECIFIC_START && did <= DIDRanges::OEM_SPECIFIC_END) ||
           (did >= DIDRanges::VEHICLE_MANUFACTURER_START && did <= DIDRanges::VEHICLE_MANUFACTURER_END) ||
           (did >= DIDRanges::SUPPLIER_SPECIFIC_START && did <= DIDRanges::SUPPLIER_SPECIFIC_END);
}

bool OEMExtensions::is_oem_service(uint8_t service_id) {
    return (service_id >= ServiceID::OEM_START_1 && service_id <= ServiceID::OEM_END_1) ||
           (service_id >= ServiceID::OEM_START_2 && service_id <= ServiceID::OEM_END_2);
}

bool OEMExtensions::is_oem_security_level(uint8_t level) {
    return (level >= 0x41 && level <= 0x5E);
}

// ============================================================================
// Manufacturer-Specific Presets
// ============================================================================

void OEMExtensions::load_volkswagen_presets() {
    // VW/Audi common DIDs
    register_did_description(0xF187, "VW Spare Part Number");
    register_did_description(0xF189, "VW Application Software Version");
    register_did_description(0xF18A, "System Supplier Identifier");
    register_did_description(0xF18B, "ECU Manufacturing Date");
    register_did_description(0xF18C, "ECU Serial Number");
    register_did_description(0xF190, "VIN - Vehicle Identification Number");
    register_did_description(0xF191, "Vehicle Manufacturer ECU Software Number");
    register_did_description(0xF19E, "ASAM/ODX File Identifier");
    
    // VW-specific security key algorithm (example - not real algorithm)
    register_key_calculator(0x43, [](const std::vector<uint8_t>& seed) {
        return KeyAlgorithms::complex_algorithm(seed);
    });
}

void OEMExtensions::load_ford_presets() {
    // Ford common DIDs
    register_did_description(0xF190, "VIN - Vehicle Identification Number");
    register_did_description(0xF191, "Vehicle Manufacturer ECU Software Number");
    register_did_description(0xF18C, "ECU Serial Number");
    register_did_description(0xDE00, "Ford Module Configuration");
    register_did_description(0xDE01, "Ford Module Serial Number");
    
    // Ford-specific security key algorithm (example)
    register_key_calculator(0x41, [](const std::vector<uint8_t>& seed) {
        return KeyAlgorithms::add_constant(seed, 0x12345678);
    });
}

void OEMExtensions::load_toyota_presets() {
    // Toyota common DIDs
    register_did_description(0xF190, "VIN - Vehicle Identification Number");
    register_did_description(0xF191, "Vehicle Manufacturer ECU Software Number");
    register_did_description(0xF18C, "ECU Serial Number");
    
    // Toyota-specific security key algorithm (example)
    register_key_calculator(0x45, [](const std::vector<uint8_t>& seed) {
        return KeyAlgorithms::rotate_bits(seed, 8);
    });
}

// ============================================================================
// Common Key Algorithms
// ============================================================================

namespace KeyAlgorithms {

std::vector<uint8_t> simple_xor(const std::vector<uint8_t>& seed, uint8_t xor_value) {
    std::vector<uint8_t> key = seed;
    for (auto& byte : key) {
        byte ^= xor_value;
    }
    return key;
}

std::vector<uint8_t> add_constant(const std::vector<uint8_t>& seed, uint32_t constant) {
    if (seed.size() < 4) {
        return seed; // Invalid seed size
    }
    
    // Convert seed to uint32_t
    uint32_t seed_value = (static_cast<uint32_t>(seed[0]) << 24) |
                          (static_cast<uint32_t>(seed[1]) << 16) |
                          (static_cast<uint32_t>(seed[2]) << 8) |
                          static_cast<uint32_t>(seed[3]);
    
    // Add constant
    uint32_t key_value = seed_value + constant;
    
    // Convert back to bytes
    std::vector<uint8_t> key(4);
    key[0] = (key_value >> 24) & 0xFF;
    key[1] = (key_value >> 16) & 0xFF;
    key[2] = (key_value >> 8) & 0xFF;
    key[3] = key_value & 0xFF;
    
    return key;
}

std::vector<uint8_t> rotate_bits(const std::vector<uint8_t>& seed, int positions) {
    if (seed.size() < 4) {
        return seed;
    }
    
    uint32_t seed_value = (static_cast<uint32_t>(seed[0]) << 24) |
                          (static_cast<uint32_t>(seed[1]) << 16) |
                          (static_cast<uint32_t>(seed[2]) << 8) |
                          static_cast<uint32_t>(seed[3]);
    
    // Rotate left
    positions = positions % 32; // Normalize rotation
    uint32_t rotated = (seed_value << positions) | (seed_value >> (32 - positions));
    
    std::vector<uint8_t> key(4);
    key[0] = (rotated >> 24) & 0xFF;
    key[1] = (rotated >> 16) & 0xFF;
    key[2] = (rotated >> 8) & 0xFF;
    key[3] = rotated & 0xFF;
    
    return key;
}

std::vector<uint8_t> complex_algorithm(const std::vector<uint8_t>& seed) {
    if (seed.size() < 4) {
        return seed;
    }
    
    // Example complex multi-step algorithm
    // Step 1: XOR with pattern
    std::vector<uint8_t> key = simple_xor(seed, 0xAA);
    
    // Step 2: Add constant
    key = add_constant(key, 0x12345678);
    
    // Step 3: Rotate
    key = rotate_bits(key, 13);
    
    // Step 4: Final XOR
    for (auto& byte : key) {
        byte ^= 0x55;
    }
    
    return key;
}

} // namespace KeyAlgorithms

} // namespace oem
} // namespace uds

