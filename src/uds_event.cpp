#include "uds_event.hpp"

namespace uds {
namespace event {

// ============================================================================
// Helper: Build ResponseOnEvent request payload
// ============================================================================

static std::vector<uint8_t> build_roe_request(const EventConfig& config) {
  std::vector<uint8_t> payload;
  
  // Sub-function (event type)
  payload.push_back(static_cast<uint8_t>(config.event_type));
  
  // Event window time (for setup commands)
  if (config.event_type != EventType::StopResponseOnEvent &&
      config.event_type != EventType::ReportActivatedEvents &&
      config.event_type != EventType::StartResponseOnEvent &&
      config.event_type != EventType::ClearResponseOnEvent) {
    payload.push_back(config.event_window_time);
  }
  
  // Event type record (service to respond + service record)
  if (config.event_type == EventType::OnDTCStatusChange ||
      config.event_type == EventType::OnTimerInterrupt ||
      config.event_type == EventType::OnChangeOfDataIdentifier ||
      config.event_type == EventType::OnComparisonOfValues) {
    
    // Service to respond with
    payload.push_back(config.service_to_respond);
    
    // Service record (sub-function + parameters)
    payload.insert(payload.end(), 
                   config.service_record.begin(), 
                   config.service_record.end());
  }
  
  return payload;
}

// ============================================================================
// Helper: Parse EventResponse from positive response
// ============================================================================

static EventResponse parse_event_response(const std::vector<uint8_t>& payload) {
  EventResponse resp;
  
  if (payload.empty()) {
    return resp;
  }
  
  // Byte 0: eventType (sub-function echo)
  resp.event_type = static_cast<EventType>(payload[0] & 0x7F);
  
  // Byte 1: numberOfIdentifiedEvents (for some event types)
  if (payload.size() > 1) {
    resp.number_of_identified_events = payload[1];
  }
  
  // Byte 2: eventWindowTime (for setup responses)
  if (payload.size() > 2) {
    resp.event_window_time = payload[2];
  }
  
  // Remaining bytes: eventTypeRecord
  if (payload.size() > 3) {
    resp.event_type_record.assign(payload.begin() + 3, payload.end());
  }
  
  return resp;
}

// ============================================================================
// Core API Implementation
// ============================================================================

Result<EventResponse> configure(Client& client, const EventConfig& config) {
  auto payload = build_roe_request(config);
  
  auto result = client.exchange(SID::ResponseOnEvent, payload);
  
  if (!result.ok) {
    return Result<EventResponse>::error(result.nrc);
  }
  
  return Result<EventResponse>::success(parse_event_response(result.payload));
}

Result<void> start(Client& client, bool store_event) {
  EventConfig config;
  config.event_type = EventType::StartResponseOnEvent;
  
  std::vector<uint8_t> payload;
  payload.push_back(static_cast<uint8_t>(EventType::StartResponseOnEvent));
  
  // Event window time for start: 0x02 = store event
  if (store_event) {
    payload.push_back(0x02);
  }
  
  auto result = client.exchange(SID::ResponseOnEvent, payload);
  
  if (!result.ok) {
    return Result<void>::error(result.nrc);
  }
  
  return Result<void>::success();
}

Result<void> stop(Client& client) {
  std::vector<uint8_t> payload;
  payload.push_back(static_cast<uint8_t>(EventType::StopResponseOnEvent));
  
  auto result = client.exchange(SID::ResponseOnEvent, payload);
  
  if (!result.ok) {
    return Result<void>::error(result.nrc);
  }
  
  return Result<void>::success();
}

Result<void> clear(Client& client) {
  std::vector<uint8_t> payload;
  payload.push_back(static_cast<uint8_t>(EventType::ClearResponseOnEvent));
  
  auto result = client.exchange(SID::ResponseOnEvent, payload);
  
  if (!result.ok) {
    return Result<void>::error(result.nrc);
  }
  
  return Result<void>::success();
}

Result<ActiveEventsReport> report_activated_events(Client& client) {
  std::vector<uint8_t> payload;
  payload.push_back(static_cast<uint8_t>(EventType::ReportActivatedEvents));
  
  auto result = client.exchange(SID::ResponseOnEvent, payload);
  
  if (!result.ok) {
    return Result<ActiveEventsReport>::error(result.nrc);
  }
  
  ActiveEventsReport report;
  
  // Parse response: [subFunction][numberOfActivatedEvents][eventTypeRecord...]
  if (result.payload.size() >= 2) {
    report.number_of_activated_events = result.payload[1];
    
    // Parse individual event records (format varies by event type)
    // This is a simplified parser - full implementation would need
    // to handle variable-length records based on event type
    size_t offset = 2;
    while (offset < result.payload.size() && 
           report.events.size() < report.number_of_activated_events) {
      ActiveEvent event;
      
      if (offset < result.payload.size()) {
        event.event_type = static_cast<EventType>(result.payload[offset++]);
      }
      
      if (offset < result.payload.size()) {
        event.event_window_time = result.payload[offset++];
      }
      
      if (offset < result.payload.size()) {
        event.service_to_respond = result.payload[offset++];
      }
      
      // Remaining bytes for this event are service record
      // Length depends on event type - simplified: read until next event marker
      // In practice, you'd need to know the expected length per event type
      
      report.events.push_back(event);
    }
  }
  
  return Result<ActiveEventsReport>::success(report);
}

// ============================================================================
// Convenience Functions
// ============================================================================

Result<EventResponse> configure_dtc_status_change(Client& client, uint8_t dtc_status_mask) {
  EventConfig config;
  config.event_type = EventType::OnDTCStatusChange;
  config.event_window_time = 0x02;  // Store event
  config.service_to_respond = 0x19; // ReadDTCInformation
  
  // Service record: [subFunction=0x02 (reportDTCByStatusMask)][DTCStatusMask]
  config.service_record = {0x02, dtc_status_mask};
  
  return configure(client, config);
}

Result<EventResponse> configure_did_change(Client& client, DID did) {
  EventConfig config;
  config.event_type = EventType::OnChangeOfDataIdentifier;
  config.event_window_time = 0x02;  // Store event
  config.service_to_respond = 0x22; // ReadDataByIdentifier
  
  // Service record: [DID_High][DID_Low]
  config.service_record = {
    static_cast<uint8_t>(did >> 8),
    static_cast<uint8_t>(did & 0xFF)
  };
  
  return configure(client, config);
}

Result<EventResponse> configure_timer_interrupt(
    Client& client,
    uint8_t timer_rate,
    uint8_t service_id,
    const std::vector<uint8_t>& service_record) {
  
  EventConfig config;
  config.event_type = EventType::OnTimerInterrupt;
  config.event_window_time = 0x02;  // Store event
  config.service_to_respond = service_id;
  
  // Service record: [timerRate][serviceRecord...]
  config.service_record.push_back(timer_rate);
  config.service_record.insert(config.service_record.end(),
                               service_record.begin(),
                               service_record.end());
  
  return configure(client, config);
}

// ============================================================================
// Event Reception
// ============================================================================

bool try_receive_event(Client& client, 
                       EventNotification& notification,
                       std::chrono::milliseconds timeout) {
  // ResponseOnEvent notifications come as positive responses with SID 0xC6 (0x86 + 0x40)
  // Format: [0xC6][eventType][numberOfIdentifiedEvents][eventTypeRecord...]
  // where eventTypeRecord contains [serviceId][serviceResponse...]
  
  // We need to receive without sending - use the transport's recv_unsolicited
  // This requires access to the underlying transport, which we don't have directly
  // from the Client API. For now, we'll use a workaround.
  
  // Note: In a full implementation, you'd add a method to Client for receiving
  // unsolicited messages, or the event module would have direct transport access.
  
  // Placeholder implementation - returns false (no event received)
  // Real implementation would poll the transport for incoming messages
  (void)client;
  (void)notification;
  (void)timeout;
  
  return false;
}

// ============================================================================
// Helper Functions
// ============================================================================

const char* event_type_name(EventType type) {
  switch (type) {
    case EventType::StopResponseOnEvent:      return "StopResponseOnEvent";
    case EventType::OnDTCStatusChange:        return "OnDTCStatusChange";
    case EventType::OnTimerInterrupt:         return "OnTimerInterrupt";
    case EventType::OnChangeOfDataIdentifier: return "OnChangeOfDataIdentifier";
    case EventType::ReportActivatedEvents:    return "ReportActivatedEvents";
    case EventType::StartResponseOnEvent:     return "StartResponseOnEvent";
    case EventType::ClearResponseOnEvent:     return "ClearResponseOnEvent";
    case EventType::OnComparisonOfValues:     return "OnComparisonOfValues";
    default:                                  return "Unknown";
  }
}

bool event_type_requires_service_record(EventType type) {
  switch (type) {
    case EventType::OnDTCStatusChange:
    case EventType::OnTimerInterrupt:
    case EventType::OnChangeOfDataIdentifier:
    case EventType::OnComparisonOfValues:
      return true;
    default:
      return false;
  }
}

} // namespace event
} // namespace uds
