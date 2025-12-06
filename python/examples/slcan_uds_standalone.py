#!/usr/bin/env python3
"""
Standalone SLCAN UDS Demo

A self-contained script that demonstrates UDS over SLCAN without
requiring the full uds_py library. Only requires pyserial.

Operations:
1. Connect via SLCAN (ELM327, CANable, or compatible adapter)
2. Enter extended diagnostic session
3. Read VIN and ECU identification
4. Read live PIDs
5. List and clear DTCs

Usage:
    python slcan_uds_standalone.py /dev/ttyUSB0
    python slcan_uds_standalone.py COM3 --baudrate 115200

Requirements:
    pip install pyserial
"""

import argparse
import serial
import struct
import time
import sys
from typing import Optional, List, Tuple


# =============================================================================
# SLCAN Protocol
# =============================================================================

class SLCANAdapter:
    """Simple SLCAN adapter interface."""
    
    # SLCAN commands
    CMD_OPEN = b'O\r'       # Open CAN channel
    CMD_CLOSE = b'C\r'      # Close CAN channel
    CMD_SPEED_500K = b'S6\r'  # Set 500 kbps
    CMD_SPEED_250K = b'S5\r'  # Set 250 kbps
    
    def __init__(self, port: str, baudrate: int = 115200):
        self.port = port
        self.baudrate = baudrate
        self.serial: Optional[serial.Serial] = None
    
    def open(self, can_speed: int = 500) -> bool:
        """Open SLCAN connection."""
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=0.1,
                write_timeout=1.0
            )
            
            # Clear any pending data
            self.serial.reset_input_buffer()
            self.serial.reset_output_buffer()
            
            # Close any existing connection
            self.serial.write(self.CMD_CLOSE)
            time.sleep(0.1)
            self.serial.reset_input_buffer()
            
            # Set CAN speed
            if can_speed == 500:
                self.serial.write(self.CMD_SPEED_500K)
            elif can_speed == 250:
                self.serial.write(self.CMD_SPEED_250K)
            else:
                self.serial.write(f'S{can_speed // 100}\r'.encode())
            time.sleep(0.05)
            
            # Open channel
            self.serial.write(self.CMD_OPEN)
            time.sleep(0.05)
            
            # Check for errors
            response = self.serial.read(100)
            if b'\x07' in response:  # Bell = error
                return False
            
            return True
            
        except Exception as e:
            print(f"Error opening SLCAN: {e}")
            return False
    
    def close(self) -> None:
        """Close SLCAN connection."""
        if self.serial:
            try:
                self.serial.write(self.CMD_CLOSE)
                time.sleep(0.05)
                self.serial.close()
            except Exception:
                pass
            self.serial = None
    
    def send_frame(self, can_id: int, data: bytes) -> bool:
        """Send a CAN frame.
        
        SLCAN format: tIIILDD...DD\r
        t = standard frame, T = extended frame
        III = CAN ID (3 hex digits for standard)
        L = data length
        DD = data bytes
        """
        if not self.serial:
            return False
        
        # Build SLCAN frame
        frame = f't{can_id:03X}{len(data):01X}'
        frame += ''.join(f'{b:02X}' for b in data)
        frame += '\r'
        
        try:
            self.serial.write(frame.encode())
            return True
        except Exception:
            return False
    
    def receive_frame(self, timeout_ms: int = 1000) -> Optional[Tuple[int, bytes]]:
        """Receive a CAN frame.
        
        Returns:
            Tuple of (can_id, data) or None
        """
        if not self.serial:
            return None
        
        start = time.time()
        timeout_sec = timeout_ms / 1000.0
        buffer = b''
        
        while (time.time() - start) < timeout_sec:
            chunk = self.serial.read(100)
            if chunk:
                buffer += chunk
                
                # Look for complete frame
                while b'\r' in buffer:
                    idx = buffer.index(b'\r')
                    frame = buffer[:idx]
                    buffer = buffer[idx+1:]
                    
                    result = self._parse_frame(frame)
                    if result:
                        return result
        
        return None
    
    def _parse_frame(self, frame: bytes) -> Optional[Tuple[int, bytes]]:
        """Parse SLCAN frame."""
        if len(frame) < 5:
            return None
        
        try:
            frame_str = frame.decode('ascii')
            
            if frame_str[0] == 't':  # Standard frame
                can_id = int(frame_str[1:4], 16)
                length = int(frame_str[4], 16)
                data = bytes.fromhex(frame_str[5:5 + length * 2])
                return (can_id, data)
            
            elif frame_str[0] == 'T':  # Extended frame
                can_id = int(frame_str[1:9], 16)
                length = int(frame_str[9], 16)
                data = bytes.fromhex(frame_str[10:10 + length * 2])
                return (can_id, data)
                
        except Exception:
            pass
        
        return None


# =============================================================================
# ISO-TP (ISO 15765-2) - Simplified
# =============================================================================

class SimpleISOTP:
    """Simplified ISO-TP for single-frame and basic multi-frame."""
    
    def __init__(self, adapter: SLCANAdapter, tx_id: int, rx_id: int):
        self.adapter = adapter
        self.tx_id = tx_id
        self.rx_id = rx_id
    
    def send_receive(self, data: bytes, timeout_ms: int = 1000) -> Optional[bytes]:
        """Send request and receive response."""
        
        # Send request
        if len(data) <= 7:
            # Single frame
            frame_data = bytes([len(data)]) + data
            frame_data = frame_data.ljust(8, b'\xCC')  # Padding
            
            if not self.adapter.send_frame(self.tx_id, frame_data):
                return None
        else:
            # Multi-frame (First Frame + Consecutive Frames)
            # First Frame
            ff = bytes([0x10 | ((len(data) >> 8) & 0x0F), len(data) & 0xFF])
            ff += data[:6]
            
            if not self.adapter.send_frame(self.tx_id, ff):
                return None
            
            # Wait for Flow Control
            fc = self._wait_for_flow_control(timeout_ms)
            if fc is None:
                return None
            
            # Send Consecutive Frames
            remaining = data[6:]
            seq = 1
            
            while remaining:
                cf = bytes([0x20 | (seq & 0x0F)]) + remaining[:7]
                cf = cf.ljust(8, b'\xCC')
                
                if not self.adapter.send_frame(self.tx_id, cf):
                    return None
                
                remaining = remaining[7:]
                seq = (seq + 1) & 0x0F
                
                if fc['st_min'] > 0:
                    time.sleep(fc['st_min'] / 1000.0)
        
        # Receive response
        return self._receive_response(timeout_ms)
    
    def _wait_for_flow_control(self, timeout_ms: int) -> Optional[dict]:
        """Wait for Flow Control frame."""
        start = time.time()
        
        while (time.time() - start) < (timeout_ms / 1000.0):
            result = self.adapter.receive_frame(100)
            if result:
                can_id, data = result
                if can_id == self.rx_id and len(data) >= 3:
                    if (data[0] & 0xF0) == 0x30:  # Flow Control
                        return {
                            'status': data[0] & 0x0F,
                            'block_size': data[1],
                            'st_min': data[2]
                        }
        
        return None
    
    def _receive_response(self, timeout_ms: int) -> Optional[bytes]:
        """Receive ISO-TP response."""
        start = time.time()
        buffer = b''
        expected_length = 0
        seq = 1
        
        while (time.time() - start) < (timeout_ms / 1000.0):
            result = self.adapter.receive_frame(100)
            if result is None:
                continue
            
            can_id, data = result
            if can_id != self.rx_id:
                continue
            
            pci_type = data[0] & 0xF0
            
            if pci_type == 0x00:  # Single Frame
                length = data[0] & 0x0F
                return data[1:1 + length]
            
            elif pci_type == 0x10:  # First Frame
                expected_length = ((data[0] & 0x0F) << 8) | data[1]
                buffer = data[2:]
                seq = 1
                
                # Send Flow Control
                fc = bytes([0x30, 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC])
                self.adapter.send_frame(self.tx_id, fc)
            
            elif pci_type == 0x20:  # Consecutive Frame
                frame_seq = data[0] & 0x0F
                if frame_seq == seq:
                    buffer += data[1:]
                    seq = (seq + 1) & 0x0F
                    
                    if len(buffer) >= expected_length:
                        return buffer[:expected_length]
        
        return buffer if buffer else None


# =============================================================================
# UDS Services
# =============================================================================

class SimpleUDSClient:
    """Simple UDS client."""
    
    # Service IDs
    SID_DIAGNOSTIC_SESSION_CONTROL = 0x10
    SID_ECU_RESET = 0x11
    SID_CLEAR_DTC = 0x14
    SID_READ_DTC = 0x19
    SID_READ_DATA_BY_ID = 0x22
    SID_TESTER_PRESENT = 0x3E
    
    # NRC
    NRC_RESPONSE = 0x7F
    
    def __init__(self, isotp: SimpleISOTP):
        self.isotp = isotp
    
    def send_request(self, data: bytes, timeout_ms: int = 1000) -> bytes:
        """Send UDS request and get response."""
        response = self.isotp.send_receive(data, timeout_ms)
        
        if response is None:
            raise TimeoutError("No response received")
        
        # Check for negative response
        if len(response) >= 3 and response[0] == self.NRC_RESPONSE:
            nrc = response[2]
            nrc_name = self._get_nrc_name(nrc)
            raise Exception(f"Negative response: {nrc_name} (0x{nrc:02X})")
        
        return response
    
    def diagnostic_session_control(self, session: int) -> bytes:
        """Change diagnostic session."""
        return self.send_request(bytes([self.SID_DIAGNOSTIC_SESSION_CONTROL, session]))
    
    def read_data_by_identifier(self, did: int) -> bytes:
        """Read data by identifier."""
        request = bytes([self.SID_READ_DATA_BY_ID, (did >> 8) & 0xFF, did & 0xFF])
        response = self.send_request(request)
        
        # Response: [0x62] [DID_high] [DID_low] [data...]
        if len(response) > 3:
            return response[3:]
        return b''
    
    def read_dtc_information(self, sub_function: int, status_mask: int = 0xFF) -> bytes:
        """Read DTC information."""
        request = bytes([self.SID_READ_DTC, sub_function, status_mask])
        return self.send_request(request, timeout_ms=2000)
    
    def clear_dtc_information(self, group: int = 0xFFFFFF) -> bytes:
        """Clear DTCs."""
        request = bytes([
            self.SID_CLEAR_DTC,
            (group >> 16) & 0xFF,
            (group >> 8) & 0xFF,
            group & 0xFF
        ])
        return self.send_request(request)
    
    def tester_present(self) -> bytes:
        """Send TesterPresent."""
        return self.send_request(bytes([self.SID_TESTER_PRESENT, 0x00]))
    
    def _get_nrc_name(self, nrc: int) -> str:
        """Get NRC name."""
        nrc_names = {
            0x10: "generalReject",
            0x11: "serviceNotSupported",
            0x12: "subFunctionNotSupported",
            0x13: "incorrectMessageLength",
            0x14: "responseTooLong",
            0x21: "busyRepeatRequest",
            0x22: "conditionsNotCorrect",
            0x24: "requestSequenceError",
            0x25: "noResponseFromSubnet",
            0x26: "failurePreventsExecution",
            0x31: "requestOutOfRange",
            0x33: "securityAccessDenied",
            0x35: "invalidKey",
            0x36: "exceededNumberOfAttempts",
            0x37: "requiredTimeDelayNotExpired",
            0x70: "uploadDownloadNotAccepted",
            0x71: "transferDataSuspended",
            0x72: "generalProgrammingFailure",
            0x73: "wrongBlockSequenceCounter",
            0x78: "requestCorrectlyReceivedResponsePending",
            0x7E: "subFunctionNotSupportedInActiveSession",
            0x7F: "serviceNotSupportedInActiveSession",
        }
        return nrc_names.get(nrc, "unknown")


# =============================================================================
# Demo Functions
# =============================================================================

def format_hex(data: bytes) -> str:
    return ' '.join(f'{b:02X}' for b in data)


def print_header(title: str) -> None:
    print()
    print("=" * 60)
    print(f"  {title}")
    print("=" * 60)


def run_demo(client: SimpleUDSClient, clear_dtcs: bool = False) -> None:
    """Run the demo."""
    
    # 1. Enter Extended Session
    print_header("Entering Extended Diagnostic Session")
    try:
        response = client.diagnostic_session_control(0x03)  # Extended
        print(f"  ✓ Session changed to Extended (0x03)")
        print(f"  Response: {format_hex(response)}")
    except Exception as e:
        print(f"  ✗ Failed: {e}")
        print("  Continuing in default session...")
    
    # 2. Read Identification DIDs
    print_header("Reading Vehicle/ECU Identification")
    
    dids = [
        (0xF190, "VIN"),
        (0xF18C, "ECU Serial Number"),
        (0xF191, "Hardware Version"),
        (0xF195, "Software Version"),
        (0xF18B, "Manufacturing Date"),
        (0xF197, "System Name"),
    ]
    
    for did, name in dids:
        try:
            data = client.read_data_by_identifier(did)
            # Try ASCII decode
            try:
                value = data.decode('ascii').rstrip('\x00')
            except Exception:
                value = format_hex(data)
            print(f"  0x{did:04X} {name}: {value}")
        except Exception as e:
            print(f"  0x{did:04X} {name}: {e}")
    
    # 3. Read Live PIDs
    print_header("Reading Live PIDs")
    
    pids = [
        (0xF40C, "Engine RPM", lambda x: int.from_bytes(x[:2], 'big') * 0.25 if len(x) >= 2 else 0, "rpm"),
        (0xF405, "Coolant Temp", lambda x: x[0] - 40 if x else 0, "°C"),
        (0xF40D, "Vehicle Speed", lambda x: x[0] if x else 0, "km/h"),
        (0xF411, "Throttle", lambda x: x[0] * 100 / 255 if x else 0, "%"),
    ]
    
    for did, name, decoder, unit in pids:
        try:
            data = client.read_data_by_identifier(did)
            value = decoder(data)
            print(f"  0x{did:04X} {name}: {value:.1f} {unit}")
        except Exception as e:
            print(f"  0x{did:04X} {name}: {e}")
    
    # 4. Read DTCs
    print_header("Reading DTCs")
    
    try:
        # Report DTCs by status mask
        response = client.read_dtc_information(0x02, 0xFF)
        
        # Parse response: [0x59] [subFunc] [statusMask] [DTC1_high] [DTC1_mid] [DTC1_low] [status1] ...
        if len(response) > 3:
            dtc_data = response[3:]
            dtc_count = len(dtc_data) // 4
            
            print(f"  Found {dtc_count} DTCs:")
            
            for i in range(dtc_count):
                offset = i * 4
                dtc_code = (dtc_data[offset] << 16) | (dtc_data[offset+1] << 8) | dtc_data[offset+2]
                status = dtc_data[offset+3]
                print(f"    DTC: {dtc_code:06X}  Status: {status:02X}")
        else:
            print("  No DTCs found")
        
        # Clear if requested
        if clear_dtcs:
            print()
            print("  Clearing DTCs...")
            try:
                client.clear_dtc_information()
                print("  ✓ DTCs cleared")
            except Exception as e:
                print(f"  ✗ Failed to clear: {e}")
                
    except Exception as e:
        print(f"  Error reading DTCs: {e}")
    
    print_header("Demo Complete")


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Standalone SLCAN UDS Demo (requires only pyserial)"
    )
    parser.add_argument('port', help="Serial port (e.g., /dev/ttyUSB0, COM3)")
    parser.add_argument('--baudrate', '-b', type=int, default=115200, help="Serial baudrate")
    parser.add_argument('--can-speed', '-s', type=int, default=500, help="CAN speed in kbps")
    parser.add_argument('--tx-id', type=lambda x: int(x, 0), default=0x7E0, help="TX CAN ID")
    parser.add_argument('--rx-id', type=lambda x: int(x, 0), default=0x7E8, help="RX CAN ID")
    parser.add_argument('--clear-dtcs', action='store_true', help="Clear DTCs after reading")
    
    args = parser.parse_args()
    
    print()
    print("╔══════════════════════════════════════════════════════════╗")
    print("║       Standalone SLCAN UDS Demo                          ║")
    print("║       (No dependencies except pyserial)                  ║")
    print("╚══════════════════════════════════════════════════════════╝")
    print()
    print(f"Port: {args.port}")
    print(f"TX ID: 0x{args.tx_id:03X}, RX ID: 0x{args.rx_id:03X}")
    
    adapter = SLCANAdapter(args.port, args.baudrate)
    
    try:
        print()
        print("Connecting to SLCAN adapter...")
        
        if not adapter.open(args.can_speed):
            print("Failed to open SLCAN connection")
            sys.exit(1)
        
        print("✓ Connected")
        
        isotp = SimpleISOTP(adapter, args.tx_id, args.rx_id)
        client = SimpleUDSClient(isotp)
        
        run_demo(client, args.clear_dtcs)
        
    except KeyboardInterrupt:
        print("\n\nInterrupted")
    except Exception as e:
        print(f"\nError: {e}")
        import traceback
        traceback.print_exc()
    finally:
        print("\nClosing connection...")
        adapter.close()
        print("Done.")


if __name__ == "__main__":
    main()
