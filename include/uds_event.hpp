#ifndef UDS_EVENT_HPP
#define UDS_EVENT_HPP

/*
  ResponseOnEvent (Service 0x86) - Event-driven Diagnostic Responses
  
  ISO 14229-1:2013 Section 9.10 (p. 75)
  
  This header provides types and functions for the ResponseOnEvent service
  defined in ISO 14229-1. This service allows the tester to configure the ECU
  to send diagnostic responses when specific events occur.
  
  Message Format:
  Request:  [0x86] [eventType] [eventWindowTime] [eventTypeRecord] [serviceToRespondToRecord]
  Response: [0xC6] [numberOfIdentifiedEvents] [eventWindowTime] [eventTypeRecord]
  
  Supported event types:
  - 0x00: StopResponseOnEvent - Stop all event-triggered responses
  - 0x01: OnDTCStatusChange - Respond when DTC status changes
  - 0x02: OnTimerInterrupt - Respond at timed intervals
  - 0x03: OnChangeOfDataIdentifier - Respond when a DID value changes
  - 0x04: ReportActivatedEvents - Report currently active events
  - 0x05: StartResponseOnEvent - Start event monitoring
  - 0x06: ClearResponseOnEvent - Clear stored events
  - 0x07: OnComparisonOfValues - Respond when values meet comparison criteria
  - 0x08-0x1F: Reserved
  - 0x20-0x3F: Vehicle manufacturer specific
  - 0x40-0x5F: System supplier specific
  
  Event Window Time (Table 55, p. 161):
  - 0x02: Infinite time to response (store event until cleared)
  - 0x03-0x7F: Time in seconds (3-127 seconds)
  - 0x80-0xFE: Vehicle manufacturer specific
  
  Usage Example (Section 8.8.3, p. 166):
    uds::event::EventConfig config;
    config.event_type = uds::event::EventType::OnDTCStatusChange;
    config.event_window_time = 0x02; // Store event
    config.service_to_respond = 0x19; // ReadDTCInformation
    config.service_record = {0x02, 0xFF}; // reportDTCByStatusMask, all DTCs
    
    auto result = uds::event::configure(client, config);
    if (result.ok) {
      uds::event::start(client);
    }
*/

#include "uds.hpp"
#include <chrono>
#include <optional>
#include <functional>

namespace uds {
namespace event {

// ============================================================================
// Event Types (Sub-functions for ResponseOnEvent 0x86)
// ============================================================================

enum class EventType : uint8_t {
  StopResponseOnEvent           = 0x00,  // Stop all event-triggered responses
  OnDTCStatusChange             = 0x01,  // Respond when DTC status changes
  OnTimerInterrupt              = 0x02,  // Respond at timed intervals
  OnChangeOfDataIdentifier      = 0x03,  // Respond when DID value changes
  ReportActivatedEvents         = 0x04,  // Report currently active events
  StartResponseOnEvent          = 0x05,  // Start event monitoring
  ClearResponseOnEvent          = 0x06,  // Clear stored events
  OnComparisonOfValues          = 0x07   // Respond when values meet criteria
  // 0x08-0x1F: Reserved
  // 0x20-0x3F: Vehicle manufacturer specific
  // 0x40-0x5F: System supplier specific
};

// ============================================================================
// Event Window Time Values
// ============================================================================

enum class EventWindowTime : uint8_t {
  InfiniteTimeToResponse = 0x02,  // Event stored until cleared
  // 0x03-0x7F: Time in seconds (3-127 seconds)
  // 0x80-0xFE: OEM-specific
};

// ============================================================================
// Event Configuration
// ============================================================================

struct EventConfig {
  EventType event_type{EventType::StopResponseOnEvent};
  
  // Event window time: how long the event setup remains active
  // 0x02 = infinite (store until cleared)
  // 0x03-0x7F = time in seconds
  uint8_t event_window_time{0x02};
  
  // Service to respond with when event triggers
  // e.g., 0x19 for ReadDTCInformation, 0x22 for ReadDataByIdentifier
  uint8_t service_to_respond{0x00};
  
  // Service-specific record (sub-function + parameters)
  // For OnDTCStatusChange: [DTCStatusMask]
  // For OnChangeOfDataIdentifier: [DID_High][DID_Low]
  // For OnTimerInterrupt: [TimerRate]
  std::vector<uint8_t> service_record;
  
  // For OnComparisonOfValues: comparison parameters
  std::optional<uint8_t> comparison_logic;
  std::optional<std::vector<uint8_t>> comparison_value;
};

// ============================================================================
// Event Response Structures
// ============================================================================

struct EventResponse {
  EventType event_type{};
  uint8_t number_of_identified_events{0};
  uint8_t event_window_time{0};
  std::vector<uint8_t> event_type_record;  // Event-specific data
};

// Event notification received from ECU (when event triggers)
struct EventNotification {
  EventType event_type{};
  uint8_t number_of_events{0};
  uint8_t service_id{0};           // The service that was triggered (e.g., 0x19)
  std::vector<uint8_t> payload;    // Service response payload
};

// Active event information (from ReportActivatedEvents)
struct ActiveEvent {
  EventType event_type{};
  uint8_t event_window_time{0};
  uint8_t service_to_respond{0};
  std::vector<uint8_t> service_record;
};

struct ActiveEventsReport {
  uint8_t number_of_activated_events{0};
  std::vector<ActiveEvent> events;
};

// ============================================================================
// Result Type for Event Operations
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

// Specialization for void
template<>
struct Result<void> {
  bool ok{false};
  NegativeResponse nrc{};
  
  static Result success() {
    Result r; r.ok = true; return r;
  }
  
  static Result error(const NegativeResponse& n) {
    Result r; r.ok = false; r.nrc = n; return r;
  }
  
  static Result error() {
    Result r; r.ok = false; return r;
  }
};

// ============================================================================
// Event Service API
// ============================================================================

/**
 * Configure an event on the ECU
 * 
 * @param client UDS client instance
 * @param config Event configuration
 * @return Result with EventResponse on success
 */
Result<EventResponse> configure(Client& client, const EventConfig& config);

/**
 * Start event monitoring (activates configured events)
 * 
 * @param client UDS client instance
 * @param store_event If true, events are stored for later retrieval
 * @return Result indicating success/failure
 */
Result<void> start(Client& client, bool store_event = true);

/**
 * Stop all event-triggered responses
 * 
 * @param client UDS client instance
 * @return Result indicating success/failure
 */
Result<void> stop(Client& client);

/**
 * Clear all stored events
 * 
 * @param client UDS client instance
 * @return Result indicating success/failure
 */
Result<void> clear(Client& client);

/**
 * Report currently activated events
 * 
 * @param client UDS client instance
 * @return Result with ActiveEventsReport on success
 */
Result<ActiveEventsReport> report_activated_events(Client& client);

/**
 * Configure OnDTCStatusChange event (convenience function)
 * 
 * @param client UDS client instance
 * @param dtc_status_mask DTC status mask to monitor
 * @return Result with EventResponse on success
 */
Result<EventResponse> configure_dtc_status_change(Client& client, uint8_t dtc_status_mask);

/**
 * Configure OnChangeOfDataIdentifier event (convenience function)
 * 
 * @param client UDS client instance
 * @param did Data identifier to monitor
 * @return Result with EventResponse on success
 */
Result<EventResponse> configure_did_change(Client& client, DID did);

/**
 * Configure OnTimerInterrupt event (convenience function)
 * 
 * @param client UDS client instance
 * @param timer_rate Timer rate (OEM-specific interpretation)
 * @param service_id Service to respond with
 * @param service_record Service parameters
 * @return Result with EventResponse on success
 */
Result<EventResponse> configure_timer_interrupt(
    Client& client,
    uint8_t timer_rate,
    uint8_t service_id,
    const std::vector<uint8_t>& service_record);

/**
 * Try to receive an event notification (non-blocking with timeout)
 * 
 * @param client UDS client instance
 * @param notification Output: received notification
 * @param timeout Maximum time to wait
 * @return true if notification received, false on timeout
 */
bool try_receive_event(Client& client, 
                       EventNotification& notification,
                       std::chrono::milliseconds timeout);

// ============================================================================
// RAII Guard for Event Subscriptions
// ============================================================================

/**
 * RAII guard that automatically stops event monitoring on destruction.
 * 
 * Usage:
 *   {
 *     EventGuard guard(client);
 *     configure_dtc_status_change(client, 0xFF);
 *     start(client);
 *     // ... monitor events ...
 *   } // Events automatically stopped here
 */
class EventGuard {
public:
  explicit EventGuard(Client& client) : client_(client) {}
  
  ~EventGuard() {
    // Stop all event monitoring
    stop(client_);
  }
  
  // Non-copyable, non-movable
  EventGuard(const EventGuard&) = delete;
  EventGuard& operator=(const EventGuard&) = delete;
  EventGuard(EventGuard&&) = delete;
  EventGuard& operator=(EventGuard&&) = delete;

private:
  Client& client_;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Get human-readable name for event type
 */
const char* event_type_name(EventType type);

/**
 * Check if event type requires service record
 */
bool event_type_requires_service_record(EventType type);

} // namespace event
} // namespace uds

#endif // UDS_EVENT_HPP
