#include "slcan_serial.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>

// Example demonstrating enhanced SLCAN features:
// - TX queue with back-pressure
// - Error frame parsing
// - FC(WT) and FC(CTS) classification
// - Event callback mechanism
// - Timestamp capture
// - Statistics tracking

std::atomic<bool> running{true};

// Flow Control type to string
const char* fc_type_to_string(slcan::FlowControlType type) {
    switch (type) {
        case slcan::FlowControlType::CTS: return "CTS";
        case slcan::FlowControlType::WT: return "WT";
        case slcan::FlowControlType::OVFL: return "OVFL";
        default: return "Unknown";
    }
}

// Frame event to string
const char* event_to_string(slcan::FrameEvent event) {
    switch (event) {
        case slcan::FrameEvent::Received: return "Received";
        case slcan::FrameEvent::Transmitted: return "Transmitted";
        case slcan::FrameEvent::Error: return "Error";
        case slcan::FrameEvent::FlowControl: return "FlowControl";
        case slcan::FrameEvent::Timeout: return "Timeout";
        case slcan::FrameEvent::QueueFull: return "QueueFull";
        default: return "Unknown";
    }
}

// RX callback - called for every received frame
void on_frame_received(const slcan::CanFrame& frame) {
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - frame.timestamp
    );
    
    std::cout << "[RX] ID: 0x" << std::hex << std::setw(3) << std::setfill('0') 
              << frame.id << std::dec
              << " DLC: " << (int)frame.dlc
              << " Latency: " << elapsed.count() << " μs";
    
    if (frame.fc_type != slcan::FlowControlType::Unknown) {
        std::cout << " [FC:" << fc_type_to_string(frame.fc_type) << "]";
    }
    
    if (frame.flags & CANProtocol::CAN_ERR_FLAG) {
        std::cout << " [ERROR]";
    }
    
    std::cout << " Data:";
    for (int i = 0; i < frame.dlc; i++) {
        std::cout << " " << std::hex << std::setw(2) << std::setfill('0') 
                  << (int)frame.data[i];
    }
    std::cout << std::dec << std::endl;
}

// Event callback - called for all events
void on_event(slcan::FrameEvent event, const slcan::CanFrame& frame) {
    std::cout << "[EVENT] " << event_to_string(event);
    
    switch (event) {
        case slcan::FrameEvent::FlowControl:
            std::cout << " - " << fc_type_to_string(frame.fc_type);
            break;
        case slcan::FrameEvent::QueueFull:
            std::cout << " - TX queue overflow!";
            break;
        case slcan::FrameEvent::Error:
            std::cout << " - Error frame detected";
            break;
        default:
            break;
    }
    
    std::cout << std::endl;
}

// Statistics printing thread
void print_statistics(const slcan::SerialDriver& driver) {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        const auto& stats = driver.stats();
        std::cout << "\n=== Statistics ===\n";
        std::cout << "Frames sent:     " << stats.frames_sent << "\n";
        std::cout << "Frames received: " << stats.frames_received << "\n";
        std::cout << "Error frames:    " << stats.error_frames << "\n";
        std::cout << "FC(CTS):         " << stats.fc_cts_count << "\n";
        std::cout << "FC(WT):          " << stats.fc_wt_count << "\n";
        std::cout << "FC(OVFL):        " << stats.fc_ovfl_count << "\n";
        std::cout << "TX overflows:    " << stats.tx_queue_overflows << "\n";
        std::cout << "Parse errors:    " << stats.parse_errors << "\n";
        std::cout << "TX queue size:   " << driver.tx_queue_size() 
                  << "/" << driver.tx_queue_max_size() << "\n";
        std::cout << "==================\n" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <serial_device>" << std::endl;
        std::cerr << "Example: " << argv[0] << " /dev/ttyUSB0" << std::endl;
        return 1;
    }
    
    std::string device = argv[1];
    
    std::cout << "=== Enhanced SLCAN Driver Example ===" << std::endl;
    std::cout << "Device: " << device << std::endl;
    
    // Create driver
    slcan::SerialDriver driver;
    
    // Configure before opening
    driver.enable_timestamps(true);
    driver.set_tx_queue_max_size(50); // Smaller queue to demonstrate back-pressure
    
    // Set callbacks
    driver.set_rx_callback(on_frame_received);
    driver.set_event_callback(on_event);
    
    // Open with 500 kbps CAN bus
    if (!driver.open(device, 500000)) {
        std::cerr << "Failed to open SLCAN device" << std::endl;
        return 1;
    }
    
    std::cout << "SLCAN opened successfully\n";
    std::cout << "Timestamps: " << (driver.timestamps_enabled() ? "enabled" : "disabled") << "\n";
    std::cout << "TX queue max size: " << driver.tx_queue_max_size() << "\n";
    std::cout << std::endl;
    
    // Start statistics thread
    std::thread stats_thread(print_statistics, std::ref(driver));
    
    // Example 1: Send standard diagnostic request
    std::cout << "=== Example 1: Standard UDS Request ===" << std::endl;
    {
        slcan::CanFrame frame;
        frame.id = 0x7E0;
        frame.dlc = 2;
        frame.data[0] = 0x10;  // DiagnosticSessionControl
        frame.data[1] = 0x01;  // Default session
        frame.flags = 0;  // Standard frame, no RTR
        
        if (driver.send_can_frame(frame)) {
            std::cout << "Sent DiagnosticSessionControl request" << std::endl;
        }
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Example 2: Receive with enhanced frame info
    std::cout << "\n=== Example 2: Enhanced Frame Reception ===" << std::endl;
    {
        slcan::CanFrame frame;
        if (driver.receive_frame(frame)) {
            std::cout << "Received enhanced frame with metadata" << std::endl;
        }
    }
    
    // Example 3: Stress test TX queue (demonstrate back-pressure)
    std::cout << "\n=== Example 3: TX Queue Back-Pressure Test ===" << std::endl;
    {
        int sent = 0;
        int failed = 0;
        
        for (int i = 0; i < 100; i++) {
            slcan::CanFrame frame;
            frame.id = 0x7E0;
            frame.dlc = 8;
            for (int j = 0; j < 8; j++) {
                frame.data[j] = (i + j) & 0xFF;
            }
            
            if (driver.send_can_frame(frame)) {
                sent++;
            } else {
                failed++;
                std::cout << "TX queue full at frame " << i << std::endl;
                break;
            }
        }
        
        std::cout << "Sent: " << sent << ", Failed: " << failed << std::endl;
    }
    
    // Example 4: Monitor for Flow Control frames
    std::cout << "\n=== Example 4: Flow Control Monitoring ===" << std::endl;
    std::cout << "Monitoring for Flow Control frames for 5 seconds..." << std::endl;
    
    auto monitor_start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - monitor_start < std::chrono::seconds(5)) {
        slcan::CanFrame frame;
        if (driver.receive_frame(frame)) {
            if (frame.fc_type != slcan::FlowControlType::Unknown) {
                std::cout << "Flow Control detected: " 
                          << fc_type_to_string(frame.fc_type) << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Example 5: Final statistics
    std::cout << "\n=== Final Statistics ===" << std::endl;
    const auto& stats = driver.stats();
    std::cout << "Total frames sent:     " << stats.frames_sent << std::endl;
    std::cout << "Total frames received: " << stats.frames_received << std::endl;
    std::cout << "Total error frames:    " << stats.error_frames << std::endl;
    std::cout << "Total FC(CTS):         " << stats.fc_cts_count << std::endl;
    std::cout << "Total FC(WT):          " << stats.fc_wt_count << std::endl;
    std::cout << "Total FC(OVFL):        " << stats.fc_ovfl_count << std::endl;
    std::cout << "Total TX overflows:    " << stats.tx_queue_overflows << std::endl;
    std::cout << "Total parse errors:    " << stats.parse_errors << std::endl;
    
    // Cleanup
    running = false;
    stats_thread.join();
    driver.close();
    
    std::cout << "\nClosed successfully" << std::endl;
    return 0;
}
