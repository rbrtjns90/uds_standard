# ISO 14229-1 UDS Stack Implementation

[![CI](https://github.com/rbrtjns90/UDS_standard/actions/workflows/ci.yml/badge.svg)](https://github.com/rbrtjns90/UDS_standard/actions/workflows/ci.yml)
[![License](https://img.shields.io/badge/license-AGPL--3.0-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)

A complete, C++17 implementation of ISO 14229-1 Unified Diagnostic Services (UDS) for automotive diagnostics and ECU flash programming.

## Features

### Implemented Services (Full ISO 14229-1 Coverage)

| SID | Service | Module |
|-----|---------|--------|
| **0x10** | Diagnostic Session Control | `uds.cpp` |
| **0x11** | ECU Reset | `uds.cpp` |
| **0x14** | Clear Diagnostic Information | `uds_dtc.cpp` |
| **0x19** | Read DTC Information | `uds_dtc.cpp` |
| **0x22** | Read Data By Identifier | `uds.cpp` |
| **0x23** | Read Memory By Address | `uds_memory.cpp` |
| **0x24** | Read Scaling Data By Identifier | `uds_scaling.cpp` |
| **0x27** | Security Access | `uds.cpp`, `uds_security.cpp` |
| **0x28** | Communication Control | `uds_comm_control.hpp` |
| **0x29** | Authentication | `uds_auth.cpp` |
| **0x2A** | Read Data By Periodic Identifier | `uds.cpp` |
| **0x2C** | Dynamically Define Data Identifier | `uds.cpp` |
| **0x2E** | Write Data By Identifier | `uds.cpp` |
| **0x2F** | Input Output Control By Identifier | `uds_io.cpp` |
| **0x31** | Routine Control | `uds.cpp` |
| **0x34** | Request Download | `uds.cpp`, `uds_block.cpp` |
| **0x35** | Request Upload | `uds.cpp`, `uds_block.cpp` |
| **0x36** | Transfer Data | `uds.cpp`, `uds_block.cpp` |
| **0x37** | Request Transfer Exit | `uds.cpp` |
| **0x3D** | Write Memory By Address | `uds_memory.cpp` |
| **0x3E** | Tester Present | `uds.cpp` |
| **0x84** | Secured Data Transmission | `uds_security.cpp` |
| **0x85** | Control DTC Setting | `uds_dtc_control.hpp` |
| **0x86** | Response On Event | `uds_event.cpp` |
| **0x87** | Link Control | `uds_link.cpp` |

### Transport Layer
- **ISO-TP** (ISO 15765-2) - Complete implementation with flow control, multi-frame, WT handling
- **SLCAN** - Serial Line CAN with error frames, timestamps, TX queue with back-pressure

### Advanced Features

#### Core Features
- Automatic NRC 0x78 (ResponsePending) handling
- NRC 0x21 (BusyRepeatRequest) retry logic
- NRC 0x73 (WrongBlockSequence) recovery
- State tracking for communication control and DTC setting
- RAII guards for automatic resource cleanup

#### ECU Programming (`ecu_programming.hpp`)
- Complete 10-step OEM programming sequence
- Block counter management with wrap-around
- Progress callbacks for UI integration
- Safe abort with cleanup

#### Block Transfers (`uds_block.hpp`)
- Resume capability for interrupted transfers
- CRC32 verification
- Configurable retry policies
- Progress tracking with cancellation support

#### DID Caching (`uds_cache.hpp`)
- LRU eviction policy
- Time-based expiration (TTL, TTI, Sliding)
- Per-DID configuration
- Thread-safe operations

#### Async Operations (`uds_async.hpp`)
- Task queue with priority levels
- Worker thread pool
- Future-based and callback-based APIs
- Periodic DID monitoring
- Batch execution

#### Security (`uds_security.hpp`, `uds_auth.hpp`)
- Seed/key authentication
- Role-based access control
- Encrypted data transmission
- Audit logging

#### OEM Extensions (`uds_oem.hpp`)
- Custom service registration
- Manufacturer-specific handlers

## Project Structure

```
.
├── include/                    # Header files (23 files)
│   ├── uds.hpp                 # Core UDS protocol definitions
│   ├── isotp.hpp               # ISO-TP transport layer (ISO 15765-2)
│   ├── can_slcan.hpp           # CAN/SLCAN protocol definitions
│   ├── slcan_serial.hpp        # SLCAN serial driver
│   ├── nrc.hpp                 # Negative Response Code handling
│   ├── timings.hpp             # UDS timing parameters (P2, P2*, S3)
│   ├── ecu_programming.hpp     # ECU flash programming sequences
│   ├── uds_async.hpp           # Async operations & task queues
│   ├── uds_auth.hpp            # Authentication (0x29)
│   ├── uds_block.hpp           # Block transfers with CRC32
│   ├── uds_cache.hpp           # DID caching with LRU/TTL
│   ├── uds_comm_control.hpp    # Communication Control (0x28)
│   ├── uds_dtc.hpp             # DTC management (0x14, 0x19)
│   ├── uds_dtc_control.hpp     # Control DTC Setting (0x85)
│   ├── uds_event.hpp           # Response On Event (0x86)
│   ├── uds_io.hpp              # I/O Control (0x2F)
│   ├── uds_link.hpp            # Link Control (0x87)
│   ├── uds_memory.hpp          # Memory operations (0x23, 0x3D)
│   ├── uds_oem.hpp             # OEM extensions
│   ├── uds_scaling.hpp         # Scaling data (0x24)
│   └── uds_security.hpp        # Security services (0x27)
│
├── src/                        # Implementation files (20 files)
│
├── examples/                   # Example programs (7 files)
│   ├── dddi_example.cpp        # Dynamic DID example
│   ├── example_comm_control.cpp
│   ├── example_dtc_control.cpp
│   ├── isotp_config_example.cpp
│   ├── periodic_data_example.cpp
│   ├── programming_session_example.cpp
│   └── slcan_enhanced_example.cpp
│
├── tests/                      # Test suite
│   ├── test_framework.hpp      # Custom test framework with mocks
│   ├── test_*.cpp              # Legacy tests (18 files)
│   └── gtest/                  # Google Test suite (22 files)
│       ├── *_test.cpp          # Unit tests
│       └── iso_spec_*.cpp      # ISO 14229-1 spec-anchored tests
│
├── .github/                    # GitHub CI/CD
│   ├── workflows/
│   │   ├── ci.yml              # CI pipeline (Ubuntu + macOS)
│   │   └── release.yml         # Release automation
│   ├── ISSUE_TEMPLATE/         # Bug report & feature request forms
│   └── dependabot.yml          # Dependency updates
│
├── CONTRIBUTING.md             # Contribution guide
├── LICENSE                     # MIT License
├── README.md                   # This file
└── Makefile                    # Build system
```

## Quick Start

### Prerequisites
- C++17 compatible compiler (GCC 7+, Clang 5+)
- POSIX-compliant system (Linux, macOS)
- CAN adapter (ELM327 or compatible)

### Building

```bash
# Build the library
make lib

# Build examples
make examples

# Build everything
make all

# Clean
make clean
```

### Basic Usage

```cpp
#include "uds.hpp"
#include "isotp.hpp"
#include "slcan_serial.hpp"

// Create SLCAN driver
slcan::SerialDriver can_driver;
can_driver.open("/dev/cu.usbserial-XXX", 500000);

// Create ISO-TP transport
isotp::Transport transport(can_driver);

// Configure addressing
uds::Address addr;
addr.type = uds::AddressType::Physical;
addr.tx_can_id = 0x7E0;  // Tester -> ECU
addr.rx_can_id = 0x7E8;  // ECU -> Tester
transport.set_address(addr);

// Create UDS client
uds::Client client(transport);

// Enter programming session
auto resp = client.diagnostic_session_control(uds::Session::ProgrammingSession);
if (resp.ok) {
    std::cout << "Programming session active\n";
}
```

## Flash Programming Example

```cpp
#include "uds_comm_control.hpp"
#include "uds_dtc_control.hpp"

// Safe flash programming with automatic cleanup
{
    // Guards ensure proper restoration
    uds::dtc_control::FlashProgrammingGuard dtc_guard(client);
    uds::comm_control::CommunicationGuard comm_guard(client, &transport);
    
    // 1. Enter programming session
    client.diagnostic_session_control(uds::Session::ProgrammingSession);
    
    // 2. Disable DTC setting (CRITICAL!)
    uds::dtc_control::disable_dtc_setting(client);
    
    // 3. Disable communication
    uds::comm_control::disable_normal_communication(client, &transport);
    
    // 4. Security access
    auto seed_resp = client.security_access_request_seed(1);
    // ... compute key ...
    client.security_access_send_key(1, key);
    
    // 5. Flash programming
    client.request_download(0x00, address, size);
    for (auto& block : data_blocks) {
        client.transfer_data(block.seq, block.data);
    }
    client.request_transfer_exit();
    
    // 6. Reset ECU
    client.ecu_reset(uds::EcuResetType::HardReset);
    
} // Guards automatically restore DTC and communication settings
```

## API Reference

### Core Services

```cpp
// Session management
client.diagnostic_session_control(Session::ProgrammingSession);
client.tester_present(suppress_response = true);

// Security
client.security_access_request_seed(level);
client.security_access_send_key(level, key);

// Data services
client.read_data_by_identifier(did);
client.write_data_by_identifier(did, data);

// Programming services
client.request_download(dfi, address, size);
client.transfer_data(block_counter, data);
client.request_transfer_exit();

// Communication control (0x28)
client.communication_control(subfunction, comm_type);

// DTC control (0x85)
client.control_dtc_setting(setting_type);

// ECU control
client.ecu_reset(EcuResetType::HardReset);
client.routine_control(action, routine_id, record);
```

### Helper Utilities

```cpp
// Communication Control helpers
uds::comm_control::disable_normal_communication(client, &transport);
uds::comm_control::enable_normal_communication(client, &transport);
uds::comm_control::disable_all_communication(client, &transport);
uds::comm_control::restore_communication(client, &transport);

// DTC Setting helpers
uds::dtc_control::disable_dtc_setting(client);
uds::dtc_control::enable_dtc_setting(client);

// RAII Guards
uds::comm_control::CommunicationGuard comm_guard(client, &transport);
uds::dtc_control::DTCSettingGuard dtc_guard(client);
uds::dtc_control::FlashProgrammingGuard flash_guard(client);
```

## Testing

Compile and run the examples:

```bash
# Build examples
make examples

# Note: Examples require a CAN interface to be connected
./examples/example_comm_control
./examples/example_dtc_control
```

## Safety & Compliance

### Important Safety Notes

- **Always disable DTC setting before flash programming** (Service 0x85)
- **Test on development/non-critical ECUs first**
- **Follow OEM programming procedures exactly**
- **Never leave DTC setting permanently disabled**
- **Use RAII guards to prevent resource leaks**

### Standards Compliance

- ISO 14229-1:2020 (UDS) - Unified Diagnostic Services
- ISO 15765-2:2016 (ISO-TP) - Diagnostic communication over CAN
- Clean-room implementation (no proprietary code)
- Production-grade error handling

## Supported Hardware

### CAN Adapters
- ELM327 (via SLCAN)
- STN1110/2120/2220
- Any SLCAN-compatible adapter

### ECUs
- Any ISO 14229-1 compliant ECU
- Tested with automotive ECUs requiring flash programming

## License

AGPL-3.0 License - see [LICENSE](LICENSE) for details.

## Contributing

Contributions welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

## Support

For issues or questions:
- Open a GitHub issue
- Review the examples in `examples/`
- Consult ISO 14229-1 standard
