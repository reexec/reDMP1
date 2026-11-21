#ifndef REDMP1_APP_AUTOPLAY_H
#define REDMP1_APP_AUTOPLAY_H

#include <array>
#include <atomic>
#include <cstdint>

namespace repsp::runtime
{
class GuestMemory;
}

namespace redmp1::app
{

// Game-specific memory layout constants for DJMAX Portable (ULKS-46050 v1.02).
// Basis: docs/design/20261121-i059-autoplay.md section 2 (generated code + guest memory dump).
namespace autoplay_layout
{
inline constexpr uint32_t kAddrCurrentScenePtr = 0x0891D74Cu;       // §2.2: global current scene pointer
inline constexpr uint32_t kGameplaySceneVtable = 0x088ED7A0u;       // §2.2: gameplay scene vtable at *(scene + 0)
inline constexpr uint32_t kAddrLayoutByte = 0x0891E4DBu;            // §2.1: button layout (0=4K, 1=6K, 2=8K)
inline constexpr uint32_t kAddrLayoutFlagByte = 0x0891E58Fu;        // §2.1: alternate L/R flag (0 or 1)

inline constexpr uint32_t kSceneJudgeOffset = 8u;                   // §2.2: judgement object G = scene + 8
inline constexpr uint32_t kJudgeLaneCountOffset = 276u;             // §2.2: lane count at G + 276
inline constexpr uint32_t kJudgeScorePtrOffset = 5540u;             // §2.2: score object S = *(G + 5540)
inline constexpr uint32_t kJudgeLaneChannelBaseOffset = 5668u;      // §2.2: lane i channel at *(G + 5668 + 4*i)

inline constexpr uint32_t kScoreChartPtrOffset = 448u;              // §2.2: chart C = *(S + 448)
inline constexpr uint32_t kScoreNowTimeOffset = 8884u;              // §2.2: current chart time at *(S + 8884)

inline constexpr uint32_t kChartChannelArrayBaseOffset = 8812u;     // §2.2: note array at C + 8812 + channel*12
inline constexpr uint32_t kChartChannelEntryStride = 12u;           // §2.2: {begin(+0), end(+4), ...}
inline constexpr uint32_t kNoteArrayElemSize = 8u;                  // §2.2: {note_addr(+0), index(+4)}

inline constexpr uint32_t kNoteTimeOffset = 0u;                     // §2.2: note time (u32, unaligned)
inline constexpr uint32_t kNoteTypeOffset = 4u;                     // §2.2: note type (u8)
inline constexpr uint32_t kNoteDurationOffset = 14u;                // 0x08866370: duration (u16, unaligned); >= 7 is long note
inline constexpr uint16_t kLongNoteMinDuration = 7u;                // 0x08866384: sltiu $v0, $v0, 7
inline constexpr uint8_t kNoteTypeNormal = 1u;                      // §2.2: normal/long note
inline constexpr uint8_t kNoteTypeHeader = 4u;                      // §2.2: channel header (time 0)

inline constexpr uint32_t kMaxLanes = 8u;
// Filter calls with the chart time unchanged before autoplay treats the game
// as paused and hands the buttons over (about 3 frames).
inline constexpr uint32_t kStalledCallsForPause = 3u;
} // namespace autoplay_layout

// Input-stage autoplay controller for DJMAX Portable.
// Reads the live chart state from guest memory inside sceCtrlReadBuffer* and
// synthesizes single-frame PSP lane button presses when notes reach the judgement line.
class Autoplay
{
public:
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    bool IsInGameplay() const;

    // Button filter callback invoked on the guest thread from sceCtrlReadBuffer*.
    uint32_t FilterButtons(uint32_t buttons, const repsp::runtime::GuestMemory& mem);

    // Resets per-song lane cursors and tracking state.
    void ResetState();

private:
    static uint32_t PrimaryLaneButton(uint8_t layout, uint32_t lane);
    static uint32_t AllLaneButtonsMask(uint8_t layout, uint8_t flag, uint32_t lane_count);

    std::atomic<bool> enabled_{false};
    std::atomic<bool> in_gameplay_{false};

    uint32_t last_score_addr_ = 0;
    uint32_t last_chart_addr_ = 0;
    uint32_t last_now_ = 0;
    bool has_last_now_ = false;
    // Consecutive filter calls that saw the chart time unchanged. The chart
    // clock stops while the game is paused (and before the song starts); the
    // pause menu is driven by the same lane buttons, so autoplay steps aside.
    uint32_t stalled_calls_ = 0;
    bool logged_unsupported_type_ = false;

    std::array<uint32_t, autoplay_layout::kMaxLanes> lane_cursor_{};
    std::array<bool, autoplay_layout::kMaxLanes> lane_pressed_prev_{};
    std::array<bool, autoplay_layout::kMaxLanes> lane_holding_{};
    std::array<uint64_t, autoplay_layout::kMaxLanes> lane_hold_until_{};
};

} // namespace redmp1::app

#endif // REDMP1_APP_AUTOPLAY_H
