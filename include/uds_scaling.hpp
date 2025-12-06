#ifndef UDS_SCALING_HPP
#define UDS_SCALING_HPP

/*
  ReadScalingDataByIdentifier (Service 0x24) - Data Scaling Information
  
  This header provides types and functions for interpreting scaling data
  returned by the ReadScalingDataByIdentifier service (0x24) defined in
  ISO 14229-1.
  
  Scaling data describes how to convert raw DID values to engineering units.
  Common scaling formats include:
  - Linear formula: physical = (raw * multiplier) + offset
  - Text/ASCII: raw bytes represent ASCII characters
  - Bit-mapped: individual bits have specific meanings
  - State-encoded: discrete values map to states
  
  Usage:
    // Read scaling info for a DID
    auto result = uds::scaling::read_scaling_info(client, 0xF190);
    if (result.ok) {
      // Apply scaling to raw data
      auto raw_data = client.read_data_by_identifier(0xF190);
      if (raw_data.ok) {
        double physical = uds::scaling::apply_linear_scaling(
          raw_data.payload, result.value);
      }
    }
*/

#include "uds.hpp"
#include <optional>
#include <variant>
#include <cmath>

namespace uds {
namespace scaling {

// ============================================================================
// Scaling Data Format Identifiers (ISO 14229-1 Table C.7)
// ============================================================================

enum class ScalingFormat : uint8_t {
  // Unscaled formats
  UnscaledUnsigned      = 0x00,  // Unsigned integer, no scaling
  UnscaledSigned        = 0x01,  // Signed integer, no scaling
  
  // Linear scaling formats
  LinearUnsigned        = 0x10,  // Linear formula, unsigned
  LinearSigned          = 0x11,  // Linear formula, signed
  
  // Bit-mapped formats
  BitMappedReported     = 0x20,  // Bit-mapped, reported as-is
  
  // Text formats
  ASCII                 = 0x30,  // ASCII text
  
  // Formula-based formats
  FormulaUnsigned       = 0x40,  // Formula-based, unsigned
  FormulaSigned         = 0x41,  // Formula-based, signed
  
  // Unit/format specifier
  UnitFormat            = 0x50,  // Unit and format specification
  
  // State-encoded formats
  StateEncoded          = 0x60,  // Discrete state values
  
  // OEM-specific
  OEMSpecific           = 0x80   // 0x80-0xFF: OEM-specific
};

// ============================================================================
// Unit Identifiers (ISO 14229-1 Table C.8 - Common units)
// ============================================================================

enum class Unit : uint8_t {
  NoUnit                = 0x00,
  Percent               = 0x01,
  PerMille              = 0x02,
  DegreeCelsius         = 0x03,
  Kelvin                = 0x04,
  KiloPascal            = 0x05,
  Bar                   = 0x06,
  Volt                  = 0x07,
  Ampere                = 0x08,
  Ohm                   = 0x09,
  Kilogram              = 0x0A,
  Gram                  = 0x0B,
  Milligram             = 0x0C,
  Meter                 = 0x0D,
  Centimeter            = 0x0E,
  Millimeter            = 0x0F,
  KilometersPerHour     = 0x10,
  MetersPerSecond       = 0x11,
  RevolutionsPerMinute  = 0x12,
  Hertz                 = 0x13,
  Kilohertz             = 0x14,
  Second                = 0x15,
  Millisecond           = 0x16,
  Microsecond           = 0x17,
  Degree                = 0x18,  // Angle
  Radian                = 0x19,
  LiterPerHour          = 0x1A,
  GramPerSecond         = 0x1B,
  Newton                = 0x1C,
  NewtonMeter           = 0x1D,
  Watt                  = 0x1E,
  Kilowatt              = 0x1F
  // 0x20-0x7F: Reserved
  // 0x80-0xFF: OEM-specific
};

// ============================================================================
// Scaling Information Structures
// ============================================================================

// Linear scaling parameters: physical = (raw * coefficient) + offset
struct LinearScaling {
  double coefficient{1.0};
  double offset{0.0};
  uint8_t num_decimals{0};  // Number of decimal places
};

// Formula-based scaling (more complex than linear)
struct FormulaScaling {
  std::vector<double> coefficients;  // Polynomial coefficients
  uint8_t formula_type{0};
};

// Bit-mapped scaling
struct BitMappedScaling {
  struct BitDefinition {
    uint8_t bit_position;
    std::string description;
    bool active_high{true};
  };
  std::vector<BitDefinition> bits;
};

// State-encoded scaling
struct StateEncodedScaling {
  struct StateDefinition {
    uint8_t value;
    std::string description;
  };
  std::vector<StateDefinition> states;
};

// Complete scaling information for a DID
struct ScalingInfo {
  DID did{0};
  ScalingFormat format{ScalingFormat::UnscaledUnsigned};
  
  // Data type information
  uint8_t data_length{0};      // Length of raw data in bytes
  bool is_signed{false};
  
  // Unit information
  std::optional<Unit> unit;
  std::optional<std::string> unit_text;
  
  // Scaling parameters (depends on format)
  std::optional<LinearScaling> linear;
  std::optional<FormulaScaling> formula;
  std::optional<BitMappedScaling> bit_mapped;
  std::optional<StateEncodedScaling> state_encoded;
  
  // Raw scaling bytes (for formats we don't fully parse)
  std::vector<uint8_t> raw_scaling_bytes;
  
  // Text description (for ASCII format)
  std::optional<std::string> text_value;
};

// ============================================================================
// Result Type
// ============================================================================

template<typename T>
struct Result {
  bool ok{false};
  T value{};
  NegativeResponse nrc{};
  
  static Result success(const T& v) {
    Result r; r.ok = true; r.value = v; return r;
  }
  
  static Result error(const NegativeResponse& n) {
    Result r; r.ok = false; r.nrc = n; return r;
  }
  
  static Result error() {
    Result r; r.ok = false; return r;
  }
};

// ============================================================================
// Scaling API
// ============================================================================

/**
 * Read scaling information for a DID
 * 
 * @param client UDS client instance
 * @param did Data identifier to get scaling for
 * @return Result with ScalingInfo on success
 */
Result<ScalingInfo> read_scaling_info(Client& client, DID did);

/**
 * Read scaling information for multiple DIDs
 * 
 * @param client UDS client instance
 * @param dids List of data identifiers
 * @return Result with vector of ScalingInfo on success
 */
Result<std::vector<ScalingInfo>> read_scaling_info(Client& client, 
                                                    const std::vector<DID>& dids);

/**
 * Parse scaling information from raw response bytes
 * 
 * @param did The DID this scaling applies to
 * @param payload Raw scaling data bytes (after DID echo)
 * @return Parsed ScalingInfo
 */
ScalingInfo parse_scaling_info(DID did, const std::vector<uint8_t>& payload);

// ============================================================================
// Scaling Application Functions
// ============================================================================

/**
 * Apply linear scaling to raw data
 * 
 * @param raw_data Raw bytes from ReadDataByIdentifier
 * @param scaling Scaling information
 * @return Physical value after scaling
 */
double apply_linear_scaling(const std::vector<uint8_t>& raw_data,
                            const ScalingInfo& scaling);

/**
 * Apply linear scaling with explicit parameters
 */
double apply_linear_scaling(const std::vector<uint8_t>& raw_data,
                            double coefficient,
                            double offset,
                            bool is_signed = false);

/**
 * Convert raw bytes to integer (big-endian)
 */
int64_t bytes_to_int(const std::vector<uint8_t>& bytes, bool is_signed = false);

/**
 * Convert raw bytes to unsigned integer (big-endian)
 */
uint64_t bytes_to_uint(const std::vector<uint8_t>& bytes);

/**
 * Interpret raw data as ASCII text
 */
std::string bytes_to_ascii(const std::vector<uint8_t>& bytes);

/**
 * Apply bit-mapped interpretation
 * 
 * @param raw_data Raw bytes
 * @param scaling Scaling info with bit definitions
 * @return Map of bit names to their states
 */
std::vector<std::pair<std::string, bool>> apply_bit_mapped_scaling(
    const std::vector<uint8_t>& raw_data,
    const ScalingInfo& scaling);

/**
 * Apply state-encoded interpretation
 * 
 * @param raw_value Raw value
 * @param scaling Scaling info with state definitions
 * @return State description or empty string if not found
 */
std::string apply_state_encoded_scaling(uint8_t raw_value,
                                        const ScalingInfo& scaling);

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Get human-readable name for scaling format
 */
const char* scaling_format_name(ScalingFormat format);

/**
 * Get human-readable name for unit
 */
const char* unit_name(Unit unit);

/**
 * Get unit symbol (e.g., "°C", "kPa", "V")
 */
const char* unit_symbol(Unit unit);

/**
 * Check if scaling format is linear
 */
bool is_linear_format(ScalingFormat format);

/**
 * Check if scaling format is text-based
 */
bool is_text_format(ScalingFormat format);

/**
 * Format a physical value with unit
 * 
 * @param value Physical value
 * @param scaling Scaling info with unit
 * @param precision Number of decimal places (-1 for auto)
 * @return Formatted string (e.g., "25.5 °C")
 */
std::string format_with_unit(double value, 
                             const ScalingInfo& scaling,
                             int precision = -1);

} // namespace scaling
} // namespace uds

#endif // UDS_SCALING_HPP
