"""
uds-py: Python UDS (ISO 14229) Library

A complete Python implementation of Unified Diagnostic Services,
wrapping the C++ UDS_standard library.

Example:
    >>> import uds_py as uds
    >>> transport = uds.SLCANTransport("/dev/ttyUSB0", baudrate=500000)
    >>> client = uds.Client(transport, tx_id=0x7E0, rx_id=0x7E8)
    >>> vin = client.read_data_by_identifier(0xF190)
"""

__version__ = "1.0.0"
__author__ = "Your Name"

# Core classes
from uds_py.client import Client
from uds_py.transport import Transport, SLCANTransport, SocketCANTransport
from uds_py.timing import TimingManager, TimingParameters

# Optional transports (import individually to avoid dependency errors)
# from uds_py.ble_transport import BLETransport, WiFiTransport
# from uds_py.macchina_transport import MacchinaTransport

# Enums and constants
from uds_py.enums import (
    DiagnosticSession,
    ResetType,
    SecurityLevel,
    DTCReportType,
    DTCSetting,
    CommunicationControlType,
    CommunicationType,
    RoutineControlType,
    LinkControlType,
    FixedBaudrate,
    NRC,
)

# Data structures
from uds_py.types import (
    DTC,
    DTCStatus,
    NegativeResponse,
    RoutineResult,
    TransferResult,
)

# Programming
from uds_py.programming import (
    ECUProgrammer,
    ProgrammingConfig,
    ProgrammingResult,
    ProgrammingState,
)

# Guards (RAII)
from uds_py.guards import (
    DTCSettingGuard,
    CommunicationGuard,
    ProgrammingGuard,
    BaudrateGuard,
)

# Algorithms
from uds_py import algorithms

# Exceptions
from uds_py.exceptions import (
    UDSError,
    NegativeResponseError,
    TimeoutError,
    SecurityAccessDenied,
    ServiceNotSupported,
    TransferError,
)

# Advanced features - now fully implemented
from uds_py.auth import (
    AuthenticationManager,
    AuthenticationConfig,
    Certificate,
    CertificateFormat,
    AlgorithmIndicator,
    AuthenticationState,
    SecureSession,
)
from uds_py.cache import (
    CacheManager,
    CachedClient,
    CacheConfig,
    CachePolicy,
    DIDCache,
    DTCCache,
)
from uds_py.events import (
    ResponseOnEventManager,
    EventHandler,
    EventType,
    EventResponse,
    CallbackEventHandler,
    QueueEventHandler,
    DTCStatusChangeHandler,
    DIDChangeHandler,
)
from uds_py.scaling import (
    ScalingFormula,
    UnitIdentifier,
    ScalingByteExtension,
    ScalingData,
    ScalingDatabase,
    ScalingClient,
    UnitConverter,
)
from uds_py.async_client import AsyncClient, AsyncTransport
from uds_py.memory import (
    MemoryReader,
    MemoryWriter,
    MemoryLocation,
    MemoryBlock,
    FileTransfer,
    FileInfo,
)
from uds_py.isotp import ISOTPHandler, ISOTPConfig, PCIType, FlowStatus

__all__ = [
    # Version
    "__version__",
    # Core
    "Client",
    "Transport",
    "SLCANTransport",
    "SocketCANTransport",
    "TimingManager",
    "TimingParameters",
    # Enums
    "DiagnosticSession",
    "ResetType",
    "SecurityLevel",
    "DTCReportType",
    "DTCSetting",
    "CommunicationControlType",
    "CommunicationType",
    "RoutineControlType",
    "LinkControlType",
    "FixedBaudrate",
    "NRC",
    # Types
    "DTC",
    "DTCStatus",
    "NegativeResponse",
    "RoutineResult",
    "TransferResult",
    # Programming
    "ECUProgrammer",
    "ProgrammingConfig",
    "ProgrammingResult",
    "ProgrammingState",
    # Guards
    "DTCSettingGuard",
    "CommunicationGuard",
    "ProgrammingGuard",
    "BaudrateGuard",
    # Algorithms
    "algorithms",
    # Exceptions
    "UDSError",
    "NegativeResponseError",
    "TimeoutError",
    "SecurityAccessDenied",
    "ServiceNotSupported",
    "TransferError",
    # Authentication (Service 0x29)
    "AuthenticationManager",
    "AuthenticationConfig",
    "Certificate",
    "CertificateFormat",
    "AlgorithmIndicator",
    "AuthenticationState",
    "SecureSession",
    # Caching
    "CacheManager",
    "CachedClient",
    "CacheConfig",
    "CachePolicy",
    "DIDCache",
    "DTCCache",
    # Events (Service 0x86)
    "ResponseOnEventManager",
    "EventHandler",
    "EventType",
    "EventResponse",
    "CallbackEventHandler",
    "QueueEventHandler",
    "DTCStatusChangeHandler",
    "DIDChangeHandler",
    # Scaling (Service 0x24)
    "ScalingFormula",
    "UnitIdentifier",
    "ScalingByteExtension",
    "ScalingData",
    "ScalingDatabase",
    "ScalingClient",
    "UnitConverter",
    # Async
    "AsyncClient",
    "AsyncTransport",
    # Memory (Services 0x23, 0x3D, 0x38)
    "MemoryReader",
    "MemoryWriter",
    "MemoryLocation",
    "MemoryBlock",
    "FileTransfer",
    "FileInfo",
    # ISO-TP
    "ISOTPHandler",
    "ISOTPConfig",
    "PCIType",
    "FlowStatus",
]
