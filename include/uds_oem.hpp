/**
 * @file uds_oem.hpp
 * @brief OEM-specific UDS service extensions
 * 
 * This module provides support for manufacturer-specific UDS services,
 * DIDs, security algorithms, and session types.
 */

#ifndef UDS_OEM_HPP
#define UDS_OEM_HPP

#include "uds.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <optional>

namespace uds {
namespace oem {

// ============================================================================
// Manufacturer Identifiers
// ============================================================================

enum class Manufacturer {
    Generic,
    Volkswagen,
    Audi,
    BMW,
    Mercedes,
    Porsche,
    Volvo,
    Ford,
    GeneralMotors,
    Chrysler,
    Tesla,
    Toyota,
    Honda,
    Nissan,
    Mazda,
    Hyundai,
    Kia,
    Custom
};

// ============================================================================
// OEM Session Types (0x40-0x5F per ISO 14229-1)
// ============================================================================

enum class OEMSession : uint8_t {
    OEM_Session_40 = 0x40,
    OEM_Session_41 = 0x41,
    OEM_Session_42 = 0x42,
    // ... extend as needed
    OEM_Session_5F = 0x5F
};

// ============================================================================
// OEM DID Ranges
// ============================================================================

namespace DIDRanges {
    constexpr uint16_t OEM_SPECIFIC_START = 0xF000;
    constexpr uint16_t OEM_SPECIFIC_END = 0xF0FF;
    constexpr uint16_t VEHICLE_MANUFACTURER_START = 0xF100;
    constexpr uint16_t VEHICLE_MANUFACTURER_END = 0xF1FF;
    constexpr uint16_t SUPPLIER_SPECIFIC_START = 0xFD00;
    constexpr uint16_t SUPPLIER_SPECIFIC_END = 0xFEFF;
}

// ============================================================================
// OEM Service ID Ranges
// ============================================================================

namespace ServiceID {
    constexpr uint8_t OEM_START_1 = 0xA0;
    constexpr uint8_t OEM_END_1 = 0xBF;
    constexpr uint8_t OEM_START_2 = 0xC0;
    constexpr uint8_t OEM_END_2 = 0xFE;
}

// ============================================================================
// OEM Service Request/Response
// ============================================================================

struct OEMServiceRequest {
    uint8_t service_id;
    std::vector<uint8_t> data;
};

struct OEMServiceResponse {
    bool success;
    uint8_t service_id;
    std::vector<uint8_t> data;
    uint8_t nrc;  // Negative response code if !success
};

// ============================================================================
// Type Aliases
// ============================================================================

using OEMServiceHandler = std::function<OEMServiceResponse(const OEMServiceRequest&)>;
using OEMKeyCalculator = std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>;

// ============================================================================
// OEM Extensions Class
// ============================================================================

class OEMExtensions {
public:
    explicit OEMExtensions(Manufacturer manufacturer = Manufacturer::Generic);
    
    // Manufacturer management
    void set_manufacturer(Manufacturer manufacturer);
    Manufacturer manufacturer() const { return manufacturer_; }
    std::string get_manufacturer_name() const;
    
    // Service registration
    bool register_service(uint8_t service_id, OEMServiceHandler handler);
    bool unregister_service(uint8_t service_id);
    bool is_service_registered(uint8_t service_id) const;
    OEMServiceResponse execute_service(Client& client, const OEMServiceRequest& request);
    
    // Security key calculators
    bool register_key_calculator(uint8_t level, OEMKeyCalculator calculator);
    std::optional<OEMKeyCalculator> get_key_calculator(uint8_t level) const;
    std::optional<std::vector<uint8_t>> calculate_key(uint8_t level, 
                                                       const std::vector<uint8_t>& seed);
    
    // OEM session control
    bool enter_oem_session(Client& client, OEMSession session);
    
    // DID descriptions
    void register_did_description(uint16_t did, const std::string& description);
    std::optional<std::string> get_did_description(uint16_t did) const;
    
    // Static helpers
    static bool is_oem_session(uint8_t session_type);
    static bool is_oem_did(uint16_t did);
    static bool is_oem_service(uint8_t service_id);
    static bool is_oem_security_level(uint8_t level);

private:
    Manufacturer manufacturer_;
    std::map<uint8_t, OEMServiceHandler> service_handlers_;
    std::map<uint8_t, OEMKeyCalculator> key_calculators_;
    std::map<uint16_t, std::string> did_descriptions_;
    
    void load_manufacturer_presets();
    void load_volkswagen_presets();
    void load_ford_presets();
    void load_toyota_presets();
};

// ============================================================================
// Common Key Algorithms
// ============================================================================

namespace KeyAlgorithms {

/**
 * @brief Simple XOR algorithm
 */
std::vector<uint8_t> simple_xor(const std::vector<uint8_t>& seed, uint8_t xor_value);

/**
 * @brief Add constant to seed (32-bit)
 */
std::vector<uint8_t> add_constant(const std::vector<uint8_t>& seed, uint32_t constant);

/**
 * @brief Rotate bits left
 */
std::vector<uint8_t> rotate_bits(const std::vector<uint8_t>& seed, int positions);

/**
 * @brief Complex multi-step algorithm
 */
std::vector<uint8_t> complex_algorithm(const std::vector<uint8_t>& seed);

} // namespace KeyAlgorithms

} // namespace oem
} // namespace uds

#endif // UDS_OEM_HPP
