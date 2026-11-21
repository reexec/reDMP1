#include "autoplay.h"

#include "repsp/platform/input.h"
#include "repsp/runtime/guest_memory.h"

#include <cstdio>

namespace redmp1::app
{

namespace
{

bool ReadU8Checked(const repsp::runtime::GuestMemory& mem, uint32_t addr, uint8_t& out)
{
    if (!mem.IsValidAddress(addr, 1u))
    {
        return false;
    }
    mem.ReadBlock(addr, &out, 1u);
    return true;
}

bool ReadU16Checked(const repsp::runtime::GuestMemory& mem, uint32_t addr, uint16_t& out)
{
    if (!mem.IsValidAddress(addr, 2u))
    {
        return false;
    }
    mem.ReadBlock(addr, &out, 2u);
    return true;
}

bool ReadU32Checked(const repsp::runtime::GuestMemory& mem, uint32_t addr, uint32_t& out)
{
    if (!mem.IsValidAddress(addr, 4u))
    {
        return false;
    }
    mem.ReadBlock(addr, &out, 4u);
    return true;
}

} // namespace

void Autoplay::SetEnabled(bool enabled)
{
    enabled_.store(enabled, std::memory_order_release);
}

bool Autoplay::IsEnabled() const
{
    return enabled_.load(std::memory_order_acquire);
}

bool Autoplay::IsInGameplay() const
{
    return in_gameplay_.load(std::memory_order_acquire);
}

void Autoplay::ResetState()
{
    last_score_addr_ = 0;
    last_chart_addr_ = 0;
    last_now_ = 0;
    has_last_now_ = false;
    stalled_calls_ = 0;
    logged_unsupported_type_ = false;
    lane_cursor_.fill(0u);
    lane_pressed_prev_.fill(false);
    lane_holding_.fill(false);
    lane_hold_until_.fill(0u);
}

uint32_t Autoplay::PrimaryLaneButton(uint8_t layout, uint32_t lane)
{
    using namespace repsp::platform;
    // Layout table from docs/design/20261121-i059-autoplay.md section 2.1.
    static constexpr uint32_t kLayout0[4] = {
        kPspCtrlLeft, kPspCtrlUp, kPspCtrlTriangle, kPspCtrlCircle
    };
    static constexpr uint32_t kLayout1[6] = {
        kPspCtrlLeft, kPspCtrlUp, kPspCtrlRight, kPspCtrlSquare, kPspCtrlTriangle, kPspCtrlCircle
    };
    static constexpr uint32_t kLayout2[8] = {
        kPspCtrlLTrigger, kPspCtrlLeft, kPspCtrlUp, kPspCtrlRight,
        kPspCtrlSquare, kPspCtrlTriangle, kPspCtrlCircle, kPspCtrlRTrigger
    };

    switch (layout)
    {
    case 0:
        return (lane < 4u) ? kLayout0[lane] : 0u;
    case 1:
        return (lane < 6u) ? kLayout1[lane] : 0u;
    case 2:
        return (lane < 8u) ? kLayout2[lane] : 0u;
    default:
        return 0u;
    }
}

uint32_t Autoplay::AllLaneButtonsMask(uint8_t layout, uint8_t flag, uint32_t lane_count)
{
    using namespace repsp::platform;
    uint32_t mask = 0u;
    for (uint32_t i = 0; i < lane_count; ++i)
    {
        mask |= PrimaryLaneButton(layout, i);
    }
    if (flag == 1u && (layout == 0u || layout == 1u))
    {
        mask |= kPspCtrlLTrigger | kPspCtrlRTrigger;
    }
    return mask;
}

uint32_t Autoplay::FilterButtons(uint32_t buttons, const repsp::runtime::GuestMemory& mem)
{
    using namespace autoplay_layout;

    if (!IsEnabled())
    {
        in_gameplay_.store(false, std::memory_order_release);
        ResetState();
        return buttons;
    }

    uint32_t scene_addr = 0;
    uint32_t scene_vtable = 0;
    if (!ReadU32Checked(mem, kAddrCurrentScenePtr, scene_addr) || scene_addr == 0u ||
        !ReadU32Checked(mem, scene_addr, scene_vtable) || scene_vtable != kGameplaySceneVtable)
    {
        in_gameplay_.store(false, std::memory_order_release);
        ResetState();
        return buttons;
    }

    in_gameplay_.store(true, std::memory_order_release);

    const uint32_t g_addr = scene_addr + kSceneJudgeOffset;
    uint32_t lane_count = 0;
    uint32_t score_addr = 0;
    uint32_t chart_addr = 0;
    uint32_t now = 0;
    uint8_t layout = 0;
    uint8_t flag = 0;

    if (!ReadU32Checked(mem, g_addr + kJudgeLaneCountOffset, lane_count) ||
        lane_count == 0u || lane_count > kMaxLanes ||
        !ReadU32Checked(mem, g_addr + kJudgeScorePtrOffset, score_addr) || score_addr == 0u ||
        !ReadU32Checked(mem, score_addr + kScoreChartPtrOffset, chart_addr) || chart_addr == 0u ||
        !ReadU32Checked(mem, score_addr + kScoreNowTimeOffset, now) ||
        !ReadU8Checked(mem, kAddrLayoutByte, layout) || layout > 2u ||
        !ReadU8Checked(mem, kAddrLayoutFlagByte, flag))
    {
        return buttons;
    }

    if (score_addr != last_score_addr_ || chart_addr != last_chart_addr_ ||
        (has_last_now_ && now < last_now_))
    {
        ResetState();
    }

    // Paused (or not started): the chart time stands still. Hand the buttons
    // over unchanged so the pause menu works, and drop any hold in progress.
    if (has_last_now_ && now == last_now_)
    {
        if (stalled_calls_ < kStalledCallsForPause)
        {
            ++stalled_calls_;
        }
    }
    else
    {
        stalled_calls_ = 0;
    }
    if (stalled_calls_ >= kStalledCallsForPause)
    {
        lane_pressed_prev_.fill(false);
        lane_holding_.fill(false);
        last_score_addr_ = score_addr;
        last_chart_addr_ = chart_addr;
        return buttons;
    }

    uint32_t delta = has_last_now_ ? (now - last_now_) : 0u;
    if (delta > 200u)
    {
        delta = 0u;
    }
    const uint64_t threshold = static_cast<uint64_t>(now) + static_cast<uint64_t>(delta / 2u);

    last_score_addr_ = score_addr;
    last_chart_addr_ = chart_addr;
    last_now_ = now;
    has_last_now_ = true;

    // Clear human lane buttons while preserving non-lane buttons (START, SELECT, etc.).
    uint32_t out = buttons & ~AllLaneButtonsMask(layout, flag, lane_count);

    for (uint32_t lane = 0; lane < lane_count; ++lane)
    {
        // Hold active long notes until their end time, then release for at least one frame.
        if (lane_holding_[lane])
        {
            if (static_cast<uint64_t>(now) < lane_hold_until_[lane])
            {
                out |= PrimaryLaneButton(layout, lane);
            }
            else
            {
                lane_holding_[lane] = false;
            }
            continue;
        }

        // Always release on the frame following a tap press so back-to-back notes trigger a fresh edge.
        if (lane_pressed_prev_[lane])
        {
            lane_pressed_prev_[lane] = false;
            continue;
        }

        uint32_t channel = 0;
        if (!ReadU32Checked(mem, g_addr + kJudgeLaneChannelBaseOffset + lane * 4u, channel))
        {
            continue;
        }

        const uint32_t arr_addr = chart_addr + kChartChannelArrayBaseOffset + channel * kChartChannelEntryStride;
        uint32_t arr_begin = 0;
        uint32_t arr_end = 0;
        if (!ReadU32Checked(mem, arr_addr + 0u, arr_begin) ||
            !ReadU32Checked(mem, arr_addr + 4u, arr_end) ||
            arr_begin == 0u || arr_end < arr_begin)
        {
            continue;
        }

        const uint32_t byte_len = arr_end - arr_begin;
        if ((byte_len % kNoteArrayElemSize) != 0u || !mem.IsValidAddress(arr_begin, byte_len))
        {
            continue;
        }
        const uint32_t note_count = byte_len / kNoteArrayElemSize;

        while (lane_cursor_[lane] < note_count)
        {
            const uint32_t elem_addr = arr_begin + lane_cursor_[lane] * kNoteArrayElemSize;
            uint32_t note_addr = 0;
            if (!ReadU32Checked(mem, elem_addr, note_addr) || note_addr == 0u ||
                !mem.IsValidAddress(note_addr, 16u))
            {
                break;
            }

            uint8_t note_type = 0;
            if (!ReadU8Checked(mem, note_addr + kNoteTypeOffset, note_type))
            {
                break;
            }

            if (note_type == kNoteTypeHeader)
            {
                ++lane_cursor_[lane];
                continue;
            }

            if (note_type != kNoteTypeNormal)
            {
                if (!logged_unsupported_type_)
                {
                    uint32_t note_time = 0;
                    (void)ReadU32Checked(mem, note_addr + kNoteTimeOffset, note_time);
                    std::fprintf(stderr,
                                 "[autoplay] skipping unsupported note type %u (time=%u, lane=%u, ch=%u)\n",
                                 static_cast<unsigned>(note_type),
                                 note_time,
                                 lane,
                                 channel);
                    logged_unsupported_type_ = true;
                }
                ++lane_cursor_[lane];
                continue;
            }

            uint32_t note_time = 0;
            uint16_t duration = 0;
            if (!ReadU32Checked(mem, note_addr + kNoteTimeOffset, note_time) ||
                !ReadU16Checked(mem, note_addr + kNoteDurationOffset, duration))
            {
                break;
            }

            if (static_cast<uint64_t>(note_time) <= threshold)
            {
                const uint32_t btn = PrimaryLaneButton(layout, lane);
                if (btn != 0u)
                {
                    out |= btn;
                    if (duration >= kLongNoteMinDuration)
                    {
                        lane_holding_[lane] = true;
                        lane_hold_until_[lane] = static_cast<uint64_t>(note_time) + static_cast<uint64_t>(duration);
                    }
                    else
                    {
                        lane_pressed_prev_[lane] = true;
                    }
                }
                ++lane_cursor_[lane];
            }
            break;
        }
    }

    return out;
}

} // namespace redmp1::app
