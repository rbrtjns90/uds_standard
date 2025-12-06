#!/usr/bin/env python3
"""
ELM327/SLCAN UDS Demo Script

Demonstrates UDS operations over SLCAN connection:
1. Connect via SLCAN (ELM327 or compatible adapter)
2. Enter extended diagnostic session
3. Read VIN and ECU identification DIDs
4. Read live PIDs (engine data)
5. List and clear DTCs

Usage:
    python elm327_slcan_demo.py /dev/ttyUSB0
    python elm327_slcan_demo.py COM3 --baudrate 115200
    python elm327_slcan_demo.py /dev/tty.usbserial-1234 --tx-id 0x7E0 --rx-id 0x7E8

Requirements:
    pip install pyserial

Author: UDS_standard project
"""

import argparse
import sys
import time
from typing import Optional, List, Tuple

# Add parent directory to path for imports
sys.path.insert(0, '..')

try:
    import uds_py as uds
except ImportError:
    print("Error: uds_py module not found. Run from the examples directory.")
    sys.exit(1)


# =============================================================================
# Standard DIDs (Data Identifiers)
# =============================================================================

STANDARD_DIDS = {
    # Vehicle Identification
    0xF190: "VIN (Vehicle Identification Number)",
    0xF187: "Vehicle Manufacturer Spare Part Number",
    0xF18A: "System Supplier Identifier",
    0xF18B: "ECU Manufacturing Date",
    0xF18C: "ECU Serial Number",
    0xF191: "Vehicle Manufacturer ECU Hardware Number",
    0xF193: "System Supplier ECU Hardware Version Number",
    0xF195: "System Supplier ECU Software Version Number",
    0xF197: "System Name or Engine Type",
    0xF19E: "ASAM ODX File Identifier",
    
    # Session/State
    0xF186: "Active Diagnostic Session",
}

# Live PIDs (OBD-II style, but via UDS ReadDataByIdentifier)
LIVE_PIDS = {
    0xF40C: ("Engine RPM", lambda x: int.from_bytes(x, 'big') * 0.25, "rpm"),
    0xF405: ("Engine Coolant Temp", lambda x: x[0] - 40, "°C"),
    0xF40D: ("Vehicle Speed", lambda x: x[0], "km/h"),
    0xF411: ("Throttle Position", lambda x: x[0] * 100 / 255, "%"),
    0xF404: ("Calculated Engine Load", lambda x: x[0] * 100 / 255, "%"),
    0xF406: ("Short Term Fuel Trim", lambda x: (x[0] - 128) * 100 / 128, "%"),
}


# =============================================================================
# Helper Functions
# =============================================================================

def format_hex(data: bytes) -> str:
    """Format bytes as hex string."""
    return ' '.join(f'{b:02X}' for b in data)


def format_ascii(data: bytes) -> str:
    """Format bytes as ASCII, replacing non-printable chars."""
    return ''.join(chr(b) if 32 <= b < 127 else '.' for b in data)


def print_header(title: str) -> None:
    """Print a section header."""
    print()
    print("=" * 60)
    print(f"  {title}")
    print("=" * 60)


def print_result(name: str, value: str, raw: Optional[bytes] = None) -> None:
    """Print a result line."""
    if raw:
        print(f"  {name}: {value}")
        print(f"    Raw: {format_hex(raw)}")
    else:
        print(f"  {name}: {value}")


# =============================================================================
# Demo Functions
# =============================================================================

def connect_slcan(port: str, baudrate: int, can_speed: int) -> uds.SLCANTransport:
    """Connect to SLCAN adapter.
    
    Args:
        port: Serial port (e.g., /dev/ttyUSB0, COM3)
        baudrate: Serial baudrate (typically 115200 or 500000)
        can_speed: CAN bus speed in kbps (typically 500)
    
    Returns:
        Connected transport
    """
    print(f"Connecting to SLCAN adapter on {port}...")
    print(f"  Serial baudrate: {baudrate}")
    print(f"  CAN speed: {can_speed} kbps")
    
    transport = uds.SLCANTransport(
        port=port,
        baudrate=baudrate,
        can_speed=can_speed
    )
    
    if not transport.open():
        raise ConnectionError(f"Failed to open SLCAN connection on {port}")
    
    print("  ✓ Connected successfully")
    return transport


def enter_extended_session(client: uds.Client) -> bool:
    """Enter extended diagnostic session.
    
    Args:
        client: UDS client
    
    Returns:
        True if successful
    """
    print_header("Entering Extended Diagnostic Session")
    
    try:
        response = client.diagnostic_session_control(uds.DiagnosticSession.EXTENDED)
        print(f"  ✓ Session changed to Extended (0x03)")
        
        # Parse timing parameters from response
        if len(response) >= 6:
            p2_server = (response[2] << 8) | response[3]
            p2_star = ((response[4] << 8) | response[5]) * 10
            print(f"  P2 Server: {p2_server} ms")
            print(f"  P2* Server: {p2_star} ms")
        
        return True
        
    except uds.NegativeResponseError as e:
        print(f"  ✗ Failed: {e}")
        return False


def read_identification(client: uds.Client) -> None:
    """Read vehicle and ECU identification DIDs.
    
    Args:
        client: UDS client
    """
    print_header("Reading Vehicle/ECU Identification")
    
    for did, name in STANDARD_DIDS.items():
        try:
            data = client.read_data_by_identifier(did)
            
            # Try to decode as ASCII if it looks like text
            if all(32 <= b < 127 or b == 0 for b in data):
                value = data.decode('ascii', errors='replace').rstrip('\x00')
            else:
                value = format_hex(data)
            
            print_result(f"0x{did:04X} {name}", value)
            
        except uds.NegativeResponseError as e:
            if e.nrc == uds.NRC.REQUEST_OUT_OF_RANGE:
                print(f"  0x{did:04X} {name}: (not supported)")
            else:
                print(f"  0x{did:04X} {name}: Error - {e}")
        except Exception as e:
            print(f"  0x{did:04X} {name}: Error - {e}")


def read_live_pids(client: uds.Client) -> None:
    """Read live engine/vehicle PIDs.
    
    Args:
        client: UDS client
    """
    print_header("Reading Live PIDs")
    
    for did, (name, decoder, unit) in LIVE_PIDS.items():
        try:
            data = client.read_data_by_identifier(did)
            
            try:
                value = decoder(data)
                if isinstance(value, float):
                    print_result(f"0x{did:04X} {name}", f"{value:.1f} {unit}", data)
                else:
                    print_result(f"0x{did:04X} {name}", f"{value} {unit}", data)
            except Exception:
                print_result(f"0x{did:04X} {name}", format_hex(data))
            
        except uds.NegativeResponseError as e:
            if e.nrc == uds.NRC.REQUEST_OUT_OF_RANGE:
                print(f"  0x{did:04X} {name}: (not supported)")
            else:
                print(f"  0x{did:04X} {name}: Error - {e}")
        except Exception as e:
            print(f"  0x{did:04X} {name}: Error - {e}")


def read_and_clear_dtcs(client: uds.Client, clear: bool = False) -> None:
    """Read and optionally clear DTCs.
    
    Args:
        client: UDS client
        clear: Whether to clear DTCs after reading
    """
    print_header("Reading DTCs (Diagnostic Trouble Codes)")
    
    try:
        # Read number of DTCs
        count_response = client.read_dtc_information(
            uds.DTCReportType.NUMBER_BY_STATUS_MASK, 
            status_mask=0xFF
        )
        
        if hasattr(count_response, 'count'):
            dtc_count = count_response.count
        elif isinstance(count_response, (list, tuple)):
            dtc_count = len(count_response)
        else:
            dtc_count = 0
        
        print(f"  Total DTCs: {dtc_count}")
        
        # Read DTC list
        dtcs = client.read_dtc_information(
            uds.DTCReportType.BY_STATUS_MASK,
            status_mask=0xFF
        )
        
        if dtcs:
            print()
            print("  DTC List:")
            print("  " + "-" * 50)
            
            for dtc in dtcs:
                if hasattr(dtc, 'code') and hasattr(dtc, 'status'):
                    status_str = format_dtc_status(dtc.status)
                    print(f"    {dtc.code:06X} - Status: {dtc.status:02X} ({status_str})")
                elif isinstance(dtc, tuple) and len(dtc) >= 2:
                    code, status = dtc[0], dtc[1]
                    status_str = format_dtc_status(status)
                    print(f"    {code:06X} - Status: {status:02X} ({status_str})")
                else:
                    print(f"    {dtc}")
        else:
            print("  No DTCs stored")
        
        # Clear DTCs if requested
        if clear and dtc_count > 0:
            print()
            print("  Clearing DTCs...")
            try:
                client.clear_dtc_information(0xFFFFFF)  # Clear all
                print("  ✓ DTCs cleared successfully")
                
                # Verify
                time.sleep(0.5)
                verify = client.read_dtc_information(
                    uds.DTCReportType.NUMBER_BY_STATUS_MASK,
                    status_mask=0xFF
                )
                if hasattr(verify, 'count'):
                    print(f"  Remaining DTCs: {verify.count}")
                    
            except uds.NegativeResponseError as e:
                print(f"  ✗ Failed to clear: {e}")
        
    except uds.NegativeResponseError as e:
        print(f"  ✗ Failed to read DTCs: {e}")
    except Exception as e:
        print(f"  ✗ Error: {e}")


def format_dtc_status(status: int) -> str:
    """Format DTC status byte as human-readable string."""
    flags = []
    if status & 0x01:
        flags.append("testFailed")
    if status & 0x02:
        flags.append("testFailedThisOp")
    if status & 0x04:
        flags.append("pending")
    if status & 0x08:
        flags.append("confirmed")
    if status & 0x10:
        flags.append("testNotCompletedSinceClear")
    if status & 0x20:
        flags.append("testFailedSinceClear")
    if status & 0x40:
        flags.append("testNotCompletedThisOp")
    if status & 0x80:
        flags.append("warningIndicator")
    
    return ", ".join(flags) if flags else "none"


def tester_present_loop(client: uds.Client, duration: float = 5.0) -> None:
    """Send TesterPresent to keep session alive.
    
    Args:
        client: UDS client
        duration: How long to run (seconds)
    """
    print_header("Sending TesterPresent (keeping session alive)")
    
    start = time.time()
    count = 0
    
    while (time.time() - start) < duration:
        try:
            client.tester_present()
            count += 1
            print(f"  TesterPresent #{count} sent", end='\r')
            time.sleep(2.0)  # Send every 2 seconds
        except Exception as e:
            print(f"  TesterPresent failed: {e}")
            break
    
    print(f"\n  Sent {count} TesterPresent messages")


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="ELM327/SLCAN UDS Demo - Read vehicle data via UDS",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s /dev/ttyUSB0
  %(prog)s COM3 --baudrate 115200
  %(prog)s /dev/tty.usbserial-1234 --tx-id 0x7E0 --rx-id 0x7E8
  %(prog)s /dev/ttyUSB0 --clear-dtcs
        """
    )
    
    parser.add_argument(
        'port',
        help="Serial port (e.g., /dev/ttyUSB0, COM3)"
    )
    parser.add_argument(
        '--baudrate', '-b',
        type=int,
        default=115200,
        help="Serial baudrate (default: 115200)"
    )
    parser.add_argument(
        '--can-speed', '-s',
        type=int,
        default=500,
        help="CAN bus speed in kbps (default: 500)"
    )
    parser.add_argument(
        '--tx-id',
        type=lambda x: int(x, 0),
        default=0x7E0,
        help="Transmit CAN ID (default: 0x7E0)"
    )
    parser.add_argument(
        '--rx-id',
        type=lambda x: int(x, 0),
        default=0x7E8,
        help="Receive CAN ID (default: 0x7E8)"
    )
    parser.add_argument(
        '--clear-dtcs',
        action='store_true',
        help="Clear DTCs after reading"
    )
    parser.add_argument(
        '--skip-pids',
        action='store_true',
        help="Skip reading live PIDs"
    )
    parser.add_argument(
        '--keep-alive',
        type=float,
        default=0,
        help="Send TesterPresent for N seconds before exiting"
    )
    
    args = parser.parse_args()
    
    print()
    print("╔══════════════════════════════════════════════════════════╗")
    print("║          ELM327/SLCAN UDS Demo Script                    ║")
    print("║          UDS_standard Python Implementation              ║")
    print("╚══════════════════════════════════════════════════════════╝")
    print()
    print(f"Configuration:")
    print(f"  Port: {args.port}")
    print(f"  TX ID: 0x{args.tx_id:03X}")
    print(f"  RX ID: 0x{args.rx_id:03X}")
    
    transport = None
    
    try:
        # Connect
        transport = connect_slcan(args.port, args.baudrate, args.can_speed)
        
        # Create client
        client = uds.Client(
            transport=transport,
            tx_id=args.tx_id,
            rx_id=args.rx_id
        )
        
        # Enter extended session
        if not enter_extended_session(client):
            print("\nWarning: Could not enter extended session, continuing in default session")
        
        # Read identification
        read_identification(client)
        
        # Read live PIDs
        if not args.skip_pids:
            read_live_pids(client)
        
        # Read/clear DTCs
        read_and_clear_dtcs(client, clear=args.clear_dtcs)
        
        # Keep alive if requested
        if args.keep_alive > 0:
            tester_present_loop(client, args.keep_alive)
        
        print_header("Demo Complete")
        print("  All operations completed successfully!")
        
    except KeyboardInterrupt:
        print("\n\nInterrupted by user")
    except ConnectionError as e:
        print(f"\nConnection error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
    finally:
        if transport:
            print("\nClosing connection...")
            transport.close()
            print("Done.")


if __name__ == "__main__":
    main()
