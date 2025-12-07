#include "uds_programming.hpp"
#include <sstream>
#include <iomanip>

namespace uds {

ProgrammingSession::ProgrammingSession(Client& client)
    : client_(client)
{
}

ProgStatus ProgrammingSession::check_pn(const PositiveOrNegative& r,
                                        const std::string& context)
{
    if (r.ok) {
        return ProgStatus::success();
    }

    std::ostringstream oss;
    oss << context << " failed";

    // Add NRC code to error message
    oss << " (NRC 0x"
        << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(r.nrc.code)
        << ")";

    return ProgStatus::failure(oss.str());
}

ProgStatus ProgrammingSession::enter_programming_session(Session s)
{
    auto res = client_.diagnostic_session_control(s);
    auto st = check_pn(res, "DiagnosticSessionControl");

    if (!st.ok) return st;

    // diagnostic_session_control already updated timings_ in Client,
    // based on the positive response payload. Nothing more to do here.
    return ProgStatus::success("Programming session entered");
}

ProgStatus ProgrammingSession::unlock(
    uint8_t level,
    std::vector<uint8_t> (*calc_key)(const std::vector<uint8_t>& seed))
{
    // 1) request seed
    auto seed_res = client_.security_access_request_seed(level);
    auto st = check_pn(seed_res, "SecurityAccess (request seed)");
    if (!st.ok) return st;

    // Expect payload to be [seed...]
    std::vector<uint8_t> seed = seed_res.payload;
    if (seed.empty()) {
        return ProgStatus::failure("SecurityAccess seed response empty");
    }

    // 2) compute key (user-supplied crypto / OEM algo)
    std::vector<uint8_t> key = calc_key(seed);
    if (key.empty()) {
        return ProgStatus::failure("SecurityAccess calc_key produced empty key");
    }

    // 3) send key
    auto key_res = client_.security_access_send_key(level, key);
    st = check_pn(key_res, "SecurityAccess (send key)");
    if (!st.ok) return st;

    return ProgStatus::success("Security unlocked");
}

ProgStatus ProgrammingSession::disable_dtcs()
{
    // 0x02 = OFF (suppress DTC setting)
    auto res = client_.control_dtc_setting(0x02);
    auto st = check_pn(res, "ControlDTCSetting(OFF)");
    if (!st.ok) return st;
    return ProgStatus::success("DTC setting disabled");
}

ProgStatus ProgrammingSession::disable_comms()
{
    // Common pattern: subFunction=0x03 (disable Rx+Tx),
    // commType=0xFF (all communication types)
    auto res = client_.communication_control(0x03, 0xFF);
    auto st = check_pn(res, "CommunicationControl(DISABLE)");
    if (!st.ok) return st;
    return ProgStatus::success("Communications disabled");
}

ProgStatus ProgrammingSession::erase_memory(uint16_t routine_id,
                                            const std::vector<uint8_t>& erase_record)
{
    // Many OEMs use routine IDs like 0xFF00 / 0xFF01 for erase.
    auto res = client_.routine_control(RoutineAction::Start,
                                       routine_id,
                                       erase_record);
    auto st = check_pn(res, "RoutineControl(erase)");
    if (!st.ok) return st;

    return ProgStatus::success("Erase routine started");
}

ProgStatus ProgrammingSession::request_download(uint8_t dfi,
                                                const std::vector<uint8_t>& addr,
                                                const std::vector<uint8_t>& size)
{
    auto res = client_.request_download(dfi, addr, size);
    auto st = check_pn(res, "RequestDownload");
    if (!st.ok) return st;

    // Positive response payload layout (0x74):
    // res.payload[0] = lengthFormatIdentifier (FL)
    // res.payload[1..] = maxNumberOfBlockLength (FL's lower nibble is length)
    //
    // We decode "maxNumberOfBlockLength" as a big-endian integer.
    if (res.payload.empty()) {
        return ProgStatus::failure("RequestDownload response payload empty");
    }

    const uint8_t fl = res.payload[0];
    const uint8_t len = static_cast<uint8_t>(fl & 0x0F);

    if (len == 0 || res.payload.size() < 1 + len) {
        return ProgStatus::failure("RequestDownload response has invalid lengthFormatIdentifier");
    }

    uint32_t max_len = 0;
    for (uint8_t i = 0; i < len; ++i) {
        max_len = (max_len << 8) | res.payload[1 + i];
    }

    if (max_len == 0) {
        return ProgStatus::failure("RequestDownload returned maxNumberOfBlockLength = 0");
    }

    max_block_size_ = max_len;

    std::ostringstream oss;
    oss << "RequestDownload OK; max_block_size=" << max_block_size_;
    return ProgStatus::success(oss.str());
}

ProgStatus ProgrammingSession::transfer_image(const std::vector<uint8_t>& image)
{
    if (max_block_size_ == 0) {
        return ProgStatus::failure("transfer_image called before request_download");
    }

    if (image.empty()) {
        return ProgStatus::failure("transfer_image called with empty image");
    }

    uint8_t block_counter = 1;
    std::size_t offset = 0;

    while (offset < image.size()) {
        const std::size_t remaining = image.size() - offset;
        const std::size_t chunk_size = remaining > max_block_size_
                                     ? max_block_size_
                                     : remaining;

        std::vector<uint8_t> chunk;
        chunk.reserve(chunk_size);
        chunk.insert(chunk.end(),
                     image.begin() + static_cast<std::ptrdiff_t>(offset),
                     image.begin() + static_cast<std::ptrdiff_t>(offset + chunk_size));

        auto res = client_.transfer_data(block_counter, chunk);
        auto st = check_pn(res, "TransferData");
        if (!st.ok) {
            std::ostringstream oss;
            oss << st.message << " at block " << (int)block_counter
                << ", offset " << offset;
            return ProgStatus::failure(oss.str());
        }

        offset       += chunk_size;
        block_counter = static_cast<uint8_t>(block_counter + 1);
        if (block_counter == 0) {
            // UDS block counter wraps from 0xFF to 0x00, but some OEMs
            // prefer 1..0xFF only; adjust as needed.
            block_counter = 1;
        }
    }

    std::ostringstream oss;
    oss << "TransferData complete (" << image.size() << " bytes)";
    return ProgStatus::success(oss.str());
}

ProgStatus ProgrammingSession::request_transfer_exit()
{
    auto res = client_.request_transfer_exit({});
    auto st = check_pn(res, "RequestTransferExit");
    if (!st.ok) return st;

    return ProgStatus::success("RequestTransferExit OK");
}

ProgStatus ProgrammingSession::finalize(EcuResetType reset_type)
{
    // 1) Re-enable DTC setting
    {
        auto res = client_.control_dtc_setting(0x01); // ON
        auto st = check_pn(res, "ControlDTCSetting(ON)");
        if (!st.ok) return st;
    }

    // 2) Re-enable communications
    {
        auto res = client_.communication_control(0x00, 0xFF);
        auto st = check_pn(res, "CommunicationControl(ENABLE)");
        if (!st.ok) return st;
    }

    // 3) ECU Reset
    {
        auto res = client_.ecu_reset(reset_type);
        auto st = check_pn(res, "ECUReset");
        if (!st.ok) return st;
    }

    return ProgStatus::success("Programming finalized and ECU reset");
}

} // namespace uds
