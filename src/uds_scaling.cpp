#include "uds_scaling.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace uds {
namespace scaling {

// ============================================================================
// Byte Conversion Helpers
// ============================================================================

uint64_t bytes_to_uint(const std::vector<uint8_t>& bytes) {
  uint64_t result = 0;
  for (size_t i = 0; i < bytes.size() && i < 8; ++i) {
    result = (result << 8) | bytes[i];
  }
  return result;
}

int64_t bytes_to_int(const std::vector<uint8_t>& bytes, bool is_signed) {
  if (bytes.empty()) {
    return 0;
  }
  
  uint64_t unsigned_val = bytes_to_uint(bytes);
  
  if (!is_signed) {
    return static_cast<int64_t>(unsigned_val);
  }
  
  // Sign extension for signed values
  size_t bit_count = bytes.size() * 8;
  uint64_t sign_bit = 1ULL << (bit_count - 1);
  
  if (unsigned_val & sign_bit) {
    // Negative value - sign extend
    uint64_t mask = ~((1ULL << bit_count) - 1);
    return static_cast<int64_t>(unsigned_val | mask);
  }
  
  return static_cast<int64_t>(unsigned_val);
}

std::string bytes_to_ascii(const std::vector<uint8_t>& bytes) {
  std::string result;
  result.reserve(bytes.size());
  
  for (uint8_t b : bytes) {
    if (b == 0) break;  // Null terminator
    if (std::isprint(b)) {
      result.push_back(static_cast<char>(b));
    }
  }
  
  // Trim trailing spaces
  while (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }
  
  return result;
}

// ============================================================================
// Scaling Info Parsing
// ============================================================================

ScalingInfo parse_scaling_info(DID did, const std::vector<uint8_t>& payload) {
  ScalingInfo info;
  info.did = did;
  info.raw_scaling_bytes = payload;
  
  if (payload.empty()) {
    return info;
  }
  
  // First byte is scaling data format identifier
  info.format = static_cast<ScalingFormat>(payload[0]);
  
  size_t offset = 1;
  
  switch (info.format) {
    case ScalingFormat::UnscaledUnsigned:
    case ScalingFormat::UnscaledSigned:
      info.is_signed = (info.format == ScalingFormat::UnscaledSigned);
      if (payload.size() > 1) {
        info.data_length = payload[1];
      }
      break;
      
    case ScalingFormat::LinearUnsigned:
    case ScalingFormat::LinearSigned: {
      info.is_signed = (info.format == ScalingFormat::LinearSigned);
      
      // Parse linear scaling parameters
      // Format: [scalingFormat][numDecimals][coefficient(4)][offset(4)]
      LinearScaling linear;
      
      if (payload.size() > offset) {
        linear.num_decimals = payload[offset++];
      }
      
      // Coefficient (4 bytes, IEEE 754 float or fixed-point)
      if (payload.size() >= offset + 4) {
        // Interpret as fixed-point: coefficient = bytes / 10^decimals
        int32_t coef_raw = static_cast<int32_t>(
          (payload[offset] << 24) | (payload[offset+1] << 16) |
          (payload[offset+2] << 8) | payload[offset+3]);
        linear.coefficient = static_cast<double>(coef_raw) / 
                            std::pow(10.0, linear.num_decimals);
        offset += 4;
      }
      
      // Offset (4 bytes)
      if (payload.size() >= offset + 4) {
        int32_t off_raw = static_cast<int32_t>(
          (payload[offset] << 24) | (payload[offset+1] << 16) |
          (payload[offset+2] << 8) | payload[offset+3]);
        linear.offset = static_cast<double>(off_raw) /
                       std::pow(10.0, linear.num_decimals);
        offset += 4;
      }
      
      info.linear = linear;
      break;
    }
    
    case ScalingFormat::ASCII:
      // ASCII format - remaining bytes are the text
      if (payload.size() > 1) {
        info.text_value = bytes_to_ascii(
          std::vector<uint8_t>(payload.begin() + 1, payload.end()));
      }
      break;
      
    case ScalingFormat::UnitFormat:
      // Unit format: [scalingFormat][unit][dataLength]
      if (payload.size() > 1) {
        info.unit = static_cast<Unit>(payload[1]);
      }
      if (payload.size() > 2) {
        info.data_length = payload[2];
      }
      break;
      
    case ScalingFormat::BitMappedReported:
      // Bit-mapped: raw bytes represent bit flags
      // Interpretation is OEM-specific
      break;
      
    case ScalingFormat::StateEncoded:
      // State-encoded: discrete values
      // Interpretation is OEM-specific
      break;
      
    default:
      // Unknown or OEM-specific format
      // Store raw bytes for caller to interpret
      break;
  }
  
  return info;
}

// ============================================================================
// Core API Implementation
// ============================================================================

Result<ScalingInfo> read_scaling_info(Client& client, DID did) {
  auto result = client.read_scaling_data_by_identifier(did);
  
  if (!result.ok) {
    return Result<ScalingInfo>::error(result.nrc);
  }
  
  // Response format: [DID_High][DID_Low][scalingDataRecord...]
  if (result.payload.size() < 2) {
    return Result<ScalingInfo>::error();
  }
  
  // Verify DID echo
  DID echoed_did = (static_cast<DID>(result.payload[0]) << 8) | result.payload[1];
  if (echoed_did != did) {
    // DID mismatch - unexpected response
    return Result<ScalingInfo>::error();
  }
  
  // Parse scaling data (everything after DID)
  std::vector<uint8_t> scaling_bytes(result.payload.begin() + 2, result.payload.end());
  
  return Result<ScalingInfo>::success(parse_scaling_info(did, scaling_bytes));
}

Result<std::vector<ScalingInfo>> read_scaling_info(Client& client,
                                                    const std::vector<DID>& dids) {
  std::vector<ScalingInfo> results;
  results.reserve(dids.size());
  
  for (DID did : dids) {
    auto result = read_scaling_info(client, did);
    if (result.ok) {
      results.push_back(result.value);
    } else {
      // Create empty entry for failed DIDs
      ScalingInfo empty;
      empty.did = did;
      results.push_back(empty);
    }
  }
  
  return Result<std::vector<ScalingInfo>>::success(results);
}

// ============================================================================
// Scaling Application
// ============================================================================

double apply_linear_scaling(const std::vector<uint8_t>& raw_data,
                            const ScalingInfo& scaling) {
  if (!scaling.linear.has_value()) {
    // No linear scaling - return raw value
    return static_cast<double>(bytes_to_int(raw_data, scaling.is_signed));
  }
  
  return apply_linear_scaling(raw_data,
                              scaling.linear->coefficient,
                              scaling.linear->offset,
                              scaling.is_signed);
}

double apply_linear_scaling(const std::vector<uint8_t>& raw_data,
                            double coefficient,
                            double offset,
                            bool is_signed) {
  int64_t raw_value = bytes_to_int(raw_data, is_signed);
  return (static_cast<double>(raw_value) * coefficient) + offset;
}

std::vector<std::pair<std::string, bool>> apply_bit_mapped_scaling(
    const std::vector<uint8_t>& raw_data,
    const ScalingInfo& scaling) {
  
  std::vector<std::pair<std::string, bool>> results;
  
  if (!scaling.bit_mapped.has_value()) {
    return results;
  }
  
  uint64_t raw_value = bytes_to_uint(raw_data);
  
  for (const auto& bit_def : scaling.bit_mapped->bits) {
    bool bit_set = (raw_value >> bit_def.bit_position) & 1;
    bool active = bit_def.active_high ? bit_set : !bit_set;
    results.emplace_back(bit_def.description, active);
  }
  
  return results;
}

std::string apply_state_encoded_scaling(uint8_t raw_value,
                                        const ScalingInfo& scaling) {
  if (!scaling.state_encoded.has_value()) {
    return "";
  }
  
  for (const auto& state : scaling.state_encoded->states) {
    if (state.value == raw_value) {
      return state.description;
    }
  }
  
  return "";
}

// ============================================================================
// Helper Functions
// ============================================================================

const char* scaling_format_name(ScalingFormat format) {
  switch (format) {
    case ScalingFormat::UnscaledUnsigned:  return "Unscaled Unsigned";
    case ScalingFormat::UnscaledSigned:    return "Unscaled Signed";
    case ScalingFormat::LinearUnsigned:    return "Linear Unsigned";
    case ScalingFormat::LinearSigned:      return "Linear Signed";
    case ScalingFormat::BitMappedReported: return "Bit-Mapped";
    case ScalingFormat::ASCII:             return "ASCII";
    case ScalingFormat::FormulaUnsigned:   return "Formula Unsigned";
    case ScalingFormat::FormulaSigned:     return "Formula Signed";
    case ScalingFormat::UnitFormat:        return "Unit Format";
    case ScalingFormat::StateEncoded:      return "State Encoded";
    case ScalingFormat::OEMSpecific:       return "OEM Specific";
    default:                               return "Unknown";
  }
}

const char* unit_name(Unit unit) {
  switch (unit) {
    case Unit::NoUnit:               return "No Unit";
    case Unit::Percent:              return "Percent";
    case Unit::PerMille:             return "Per Mille";
    case Unit::DegreeCelsius:        return "Degree Celsius";
    case Unit::Kelvin:               return "Kelvin";
    case Unit::KiloPascal:           return "Kilopascal";
    case Unit::Bar:                  return "Bar";
    case Unit::Volt:                 return "Volt";
    case Unit::Ampere:               return "Ampere";
    case Unit::Ohm:                  return "Ohm";
    case Unit::Kilogram:             return "Kilogram";
    case Unit::Gram:                 return "Gram";
    case Unit::Milligram:            return "Milligram";
    case Unit::Meter:                return "Meter";
    case Unit::Centimeter:           return "Centimeter";
    case Unit::Millimeter:           return "Millimeter";
    case Unit::KilometersPerHour:    return "Kilometers per Hour";
    case Unit::MetersPerSecond:      return "Meters per Second";
    case Unit::RevolutionsPerMinute: return "Revolutions per Minute";
    case Unit::Hertz:                return "Hertz";
    case Unit::Kilohertz:            return "Kilohertz";
    case Unit::Second:               return "Second";
    case Unit::Millisecond:          return "Millisecond";
    case Unit::Microsecond:          return "Microsecond";
    case Unit::Degree:               return "Degree";
    case Unit::Radian:               return "Radian";
    case Unit::LiterPerHour:         return "Liter per Hour";
    case Unit::GramPerSecond:        return "Gram per Second";
    case Unit::Newton:               return "Newton";
    case Unit::NewtonMeter:          return "Newton Meter";
    case Unit::Watt:                 return "Watt";
    case Unit::Kilowatt:             return "Kilowatt";
    default:                         return "Unknown";
  }
}

const char* unit_symbol(Unit unit) {
  switch (unit) {
    case Unit::NoUnit:               return "";
    case Unit::Percent:              return "%";
    case Unit::PerMille:             return "‰";
    case Unit::DegreeCelsius:        return "°C";
    case Unit::Kelvin:               return "K";
    case Unit::KiloPascal:           return "kPa";
    case Unit::Bar:                  return "bar";
    case Unit::Volt:                 return "V";
    case Unit::Ampere:               return "A";
    case Unit::Ohm:                  return "Ω";
    case Unit::Kilogram:             return "kg";
    case Unit::Gram:                 return "g";
    case Unit::Milligram:            return "mg";
    case Unit::Meter:                return "m";
    case Unit::Centimeter:           return "cm";
    case Unit::Millimeter:           return "mm";
    case Unit::KilometersPerHour:    return "km/h";
    case Unit::MetersPerSecond:      return "m/s";
    case Unit::RevolutionsPerMinute: return "rpm";
    case Unit::Hertz:                return "Hz";
    case Unit::Kilohertz:            return "kHz";
    case Unit::Second:               return "s";
    case Unit::Millisecond:          return "ms";
    case Unit::Microsecond:          return "µs";
    case Unit::Degree:               return "°";
    case Unit::Radian:               return "rad";
    case Unit::LiterPerHour:         return "L/h";
    case Unit::GramPerSecond:        return "g/s";
    case Unit::Newton:               return "N";
    case Unit::NewtonMeter:          return "Nm";
    case Unit::Watt:                 return "W";
    case Unit::Kilowatt:             return "kW";
    default:                         return "";
  }
}

bool is_linear_format(ScalingFormat format) {
  return format == ScalingFormat::LinearUnsigned ||
         format == ScalingFormat::LinearSigned;
}

bool is_text_format(ScalingFormat format) {
  return format == ScalingFormat::ASCII;
}

std::string format_with_unit(double value,
                             const ScalingInfo& scaling,
                             int precision) {
  std::ostringstream oss;
  
  // Determine precision
  if (precision < 0) {
    if (scaling.linear.has_value()) {
      precision = scaling.linear->num_decimals;
    } else {
      precision = 2;  // Default
    }
  }
  
  oss << std::fixed << std::setprecision(precision) << value;
  
  // Add unit symbol
  if (scaling.unit.has_value()) {
    const char* symbol = unit_symbol(scaling.unit.value());
    if (symbol[0] != '\0') {
      oss << " " << symbol;
    }
  } else if (scaling.unit_text.has_value()) {
    oss << " " << scaling.unit_text.value();
  }
  
  return oss.str();
}

} // namespace scaling
} // namespace uds
