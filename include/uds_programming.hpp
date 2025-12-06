#ifndef UDS_PROGRAMMING_HPP
#define UDS_PROGRAMMING_HPP

/**
 * @file uds_programming.hpp
 * @brief Programming Services - ISO 14229-1:2013 Section 14
 * 
 * ISO 14229-1:2013 Flash Programming Services:
 * - RequestDownload (0x34) - Section 14.2 (p. 270)
 * - RequestUpload (0x35) - Section 14.3 (p. 275)
 * - TransferData (0x36) - Section 14.4 (p. 280)
 * - RequestTransferExit (0x37) - Section 14.5 (p. 285)
 * 
 * Programming Workflow:
 * 1. Enter programming session (0x10 0x02)
 * 2. Unlock security access (0x27)
 * 3. Disable DTC setting (0x85 0x02)
 * 4. Request download (0x34) ← Define memory region
 * 5. Transfer data (0x36) ← Send firmware blocks
 * 6. Request transfer exit (0x37) ← Finalize transfer
 * 7. Optionally verify with checksum (0x31)
 * 8. Reset ECU (0x11)
 * 
 * Message Formats:
 * 
 * RequestDownload:
 *   Request:  [0x34] [dataFormatIdentifier] [addressAndLengthFormatIdentifier] 
 *             [memoryAddress] [memorySize]
 *   Response: [0x74] [lengthFormatIdentifier] [maxNumberOfBlockLength]
 * 
 * TransferData:
 *   Request:  [0x36] [blockSequenceCounter] [transferRequestParameterRecord]
 *   Response: [0x76] [blockSequenceCounter] [transferResponseParameterRecord]
 * 
 * RequestTransferExit:
 *   Request:  [0x37] [transferRequestParameterRecord (optional)]
 *   Response: [0x77] [transferResponseParameterRecord (optional)]
 */

#include <cstdint>
#include <vector>
#include <string>
#include "uds.hpp"

namespace uds {

// Simple status wrapper for high-level operations
struct ProgStatus {
    bool ok;
    std::string message;

    static ProgStatus success(const std::string& msg = {}) {
        return { true, msg };
    }

    static ProgStatus failure(const std::string& msg) {
        return { false, msg };
    }
};

/**
 * ProgrammingSession - High-level wrapper for UDS programming workflows
 * 
 * This class provides a simplified, step-by-step API for common programming
 * operations. It wraps the low-level UDS Client methods into a more procedural
 * workflow suitable for applications that need explicit control over each step.
 * 
 * Example usage:
 *   ProgrammingSession prog(client);
 *   prog.enter_programming_session();
 *   prog.unlock(level, key_calculator);
 *   prog.disable_dtcs();
 *   prog.request_download(dfi, addr, size);
 *   prog.transfer_image(firmware);
 *   prog.request_transfer_exit();
 *   prog.finalize();
 * 
 * For fully automated programming, see ecu_programming.hpp (ECUProgrammer class).
 */
class ProgrammingSession {
public:
    explicit ProgrammingSession(Client& client);

    // 1) Enter programming session and update timings
    ProgStatus enter_programming_session(Session s = Session::ProgrammingSession);

    // 2) Do security access seed/key at given level
    //    'calc_key' is a user-supplied function that turns seed -> key.
    ProgStatus unlock(uint8_t level,
                      std::vector<uint8_t> (*calc_key)(const std::vector<uint8_t>& seed));

    // 3) Turn DTC logging off before flashing
    ProgStatus disable_dtcs();

    // 4) Disable normal comms to quiet the bus
    ProgStatus disable_comms();

    // 5) Erase memory via RoutineControl
    ProgStatus erase_memory(uint16_t routine_id,
                            const std::vector<uint8_t>& erase_record);

    // 6) RequestDownload + internal parse of max block length
    ProgStatus request_download(uint8_t dfi,
                                const std::vector<uint8_t>& addr,
                                const std::vector<uint8_t>& size);

    // 7) Transfer a full image (split into blocks automatically)
    ProgStatus transfer_image(const std::vector<uint8_t>& image);

    // 8) RequestTransferExit
    ProgStatus request_transfer_exit();

    // 9) Re-enable comms and DTCs, then ECU reset
    ProgStatus finalize(EcuResetType reset_type = EcuResetType::HardReset);

    // Optional: expose negotiated block size
    uint32_t max_block_size() const { return max_block_size_; }

private:
    Client& client_;
    uint32_t max_block_size_ = 0;   // from RequestDownload positive response

    ProgStatus check_pn(const PositiveOrNegative& r,
                        const std::string& context);
};

} // namespace uds

#endif // UDS_PROGRAMMING_HPP
