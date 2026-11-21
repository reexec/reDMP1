#include "autoplay.h"

#include "repsp/platform/input.h"
#include "repsp/runtime/guest_memory.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define PROBE_ASSERT(cond)                                                              \
    do                                                                                  \
    {                                                                                   \
        if (!(cond))                                                                    \
        {                                                                               \
            std::fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__);      \
            std::fflush(nullptr);                                                       \
            std::_Exit(1);                                                              \
        }                                                                               \
    } while (false)

namespace
{

using namespace redmp1::app::autoplay_layout;
using namespace repsp::platform;

struct SynthNote
{
    uint32_t time = 0;
    uint8_t type = kNoteTypeNormal;
    uint8_t channel = 0;
    uint16_t duration = 6u;
};

struct SynthChartFixture
{
    static constexpr uint32_t kSceneAddr = 0x08A00000u;
    static constexpr uint32_t kJudgeAddr = kSceneAddr + kSceneJudgeOffset;
    static constexpr uint32_t kScoreAddr = 0x08A10000u;
    static constexpr uint32_t kChartAddr = 0x08A20000u;
    static constexpr uint32_t kArraysBase = 0x08A30000u;
    static constexpr uint32_t kNotesBase = 0x08A40000u;

    uint32_t next_array_addr = kArraysBase;
    uint32_t next_note_addr = kNotesBase;

    void WriteScene(repsp::runtime::GuestMemory& mem, uint32_t vtable, uint32_t lane_count,
                    uint8_t layout, uint8_t flag)
    {
        mem.WriteU32(kAddrCurrentScenePtr, kSceneAddr);
        mem.WriteU32(kSceneAddr, vtable);
        mem.WriteU32(kJudgeAddr + kJudgeLaneCountOffset, lane_count);
        mem.WriteU32(kJudgeAddr + kJudgeScorePtrOffset, kScoreAddr);
        mem.WriteU32(kScoreAddr + kScoreChartPtrOffset, kChartAddr);
        mem.WriteU32(kScoreAddr + kScoreNowTimeOffset, 0u);
        mem.WriteU8(kAddrLayoutByte, layout);
        mem.WriteU8(kAddrLayoutFlagByte, flag);
    }

    void SetLaneNotes(repsp::runtime::GuestMemory& mem, uint32_t lane, uint32_t channel,
                      const std::vector<SynthNote>& notes)
    {
        mem.WriteU32(kJudgeAddr + kJudgeLaneChannelBaseOffset + lane * 4u, channel);
        const uint32_t arr_begin = next_array_addr;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const uint32_t note_addr = next_note_addr;
            next_note_addr += 17u; // intentionally odd stride (>=16) to test unaligned reads
            uint8_t raw[16] = {0};
            raw[0] = static_cast<uint8_t>(notes[i].time & 0xFFu);
            raw[1] = static_cast<uint8_t>((notes[i].time >> 8) & 0xFFu);
            raw[2] = static_cast<uint8_t>((notes[i].time >> 16) & 0xFFu);
            raw[3] = static_cast<uint8_t>((notes[i].time >> 24) & 0xFFu);
            raw[4] = notes[i].type;
            raw[5] = notes[i].channel;
            raw[14] = static_cast<uint8_t>(notes[i].duration & 0xFFu);
            raw[15] = static_cast<uint8_t>((notes[i].duration >> 8) & 0xFFu);
            mem.WriteBlock(note_addr, raw, sizeof(raw));

            mem.WriteU32(next_array_addr + 0u, note_addr);
            mem.WriteU32(next_array_addr + 4u, static_cast<uint32_t>(i));
            next_array_addr += kNoteArrayElemSize;
        }
        const uint32_t arr_end = next_array_addr;
        const uint32_t ch_entry = kChartAddr + kChartChannelArrayBaseOffset + channel * kChartChannelEntryStride;
        mem.WriteU32(ch_entry + 0u, arr_begin);
        mem.WriteU32(ch_entry + 4u, arr_end);
    }

    void SetNow(repsp::runtime::GuestMemory& mem, uint32_t now)
    {
        mem.WriteU32(kScoreAddr + kScoreNowTimeOffset, now);
    }
};

} // namespace

int main()
{
    auto& mem = repsp::runtime::GetGuestMemory();
    PROBE_ASSERT(mem.Initialize());

    redmp1::app::Autoplay ap;

    // 1. Disabled or non-gameplay scene leaves buttons untouched.
    {
        SynthChartFixture fx;
        fx.WriteScene(mem, kGameplaySceneVtable, 4u, 0u, 0u);
        ap.SetEnabled(false);
        PROBE_ASSERT(ap.FilterButtons(kPspCtrlLeft | kPspCtrlStart, mem) == (kPspCtrlLeft | kPspCtrlStart));
        PROBE_ASSERT(!ap.IsInGameplay());

        ap.SetEnabled(true);
        mem.WriteU32(SynthChartFixture::kSceneAddr, 0x08800000u); // non-gameplay vtable
        PROBE_ASSERT(ap.FilterButtons(kPspCtrlLeft | kPspCtrlStart, mem) == (kPspCtrlLeft | kPspCtrlStart));
        PROBE_ASSERT(!ap.IsInGameplay());
    }

    // 2. Layout 0 (4K): each note presses once, released on next call, back-to-back notes each pressed,
    //    human lane buttons cleared while START is preserved.
    {
        SynthChartFixture fx;
        fx.WriteScene(mem, kGameplaySceneVtable, 4u, 0u, 1u);
        // Lane 0 (LEFT): header + two back-to-back notes at t=100, t=116
        fx.SetLaneNotes(mem, 0u, 2u, {
            {0u, kNoteTypeHeader, 2u},
            {100u, kNoteTypeNormal, 2u},
            {116u, kNoteTypeNormal, 2u},
        });
        // Lane 1 (UP): unsupported type 2 at t=50 (skipped) + normal note at t=200
        fx.SetLaneNotes(mem, 1u, 3u, {
            {50u, 2u, 3u},
            {200u, kNoteTypeNormal, 3u},
        });
        // Lane 2 (TRIANGLE): note at t=300
        fx.SetLaneNotes(mem, 2u, 4u, {
            {300u, kNoteTypeNormal, 4u},
        });
        // Lane 3 (CIRCLE): note at t=300 (chord with lane 2)
        fx.SetLaneNotes(mem, 3u, 5u, {
            {300u, kNoteTypeNormal, 5u},
        });

        ap.ResetState();
        ap.SetEnabled(true);

        int lane0_presses = 0;
        int lane1_presses = 0;
        int lane2_presses = 0;
        int lane3_presses = 0;
        bool prev_lane0 = false;

        // Advance now in 16 ms steps from 0 to 360.
        for (uint32_t t = 0; t <= 360u; t += 16u)
        {
            fx.SetNow(mem, t);
            // Pass human lane buttons + flag buttons + START; lane/flag buttons must be cleared, START kept.
            const uint32_t in = kPspCtrlStart | kPspCtrlLTrigger | kPspCtrlRTrigger | kPspCtrlUp;
            const uint32_t out = ap.FilterButtons(in, mem);
            PROBE_ASSERT(ap.IsInGameplay());
            PROBE_ASSERT((out & kPspCtrlStart) != 0u);
            PROBE_ASSERT((out & (kPspCtrlLTrigger | kPspCtrlRTrigger)) == 0u);

            const bool l0 = (out & kPspCtrlLeft) != 0u;
            const bool l1 = (out & kPspCtrlUp) != 0u;
            const bool l2 = (out & kPspCtrlTriangle) != 0u;
            const bool l3 = (out & kPspCtrlCircle) != 0u;

            // Call immediately after a press must always be released.
            if (prev_lane0)
            {
                PROBE_ASSERT(!l0);
            }
            prev_lane0 = l0;

            if (l0) ++lane0_presses;
            if (l1) ++lane1_presses;
            if (l2) ++lane2_presses;
            if (l3) ++lane3_presses;
        }

        PROBE_ASSERT(lane0_presses == 2); // back-to-back notes at t=100 and t=116 both pressed
        PROBE_ASSERT(lane1_presses == 1); // type-2 skipped, t=200 pressed once
        PROBE_ASSERT(lane2_presses == 1); // t=300 pressed once
        PROBE_ASSERT(lane3_presses == 1); // t=300 pressed once
    }

    // 3. Layout 1 (6K) and Layout 2 (8K) button mapping verification.
    {
        // Layout 1 (6K): LEFT, UP, RIGHT, SQUARE, TRIANGLE, CIRCLE
        const uint32_t expected6[6] = {
            kPspCtrlLeft, kPspCtrlUp, kPspCtrlRight, kPspCtrlSquare, kPspCtrlTriangle, kPspCtrlCircle
        };
        SynthChartFixture fx6;
        fx6.WriteScene(mem, kGameplaySceneVtable, 6u, 1u, 0u);
        for (uint32_t i = 0; i < 6u; ++i)
        {
            fx6.SetLaneNotes(mem, i, i, {{100u + i * 50u, kNoteTypeNormal, static_cast<uint8_t>(i)}});
        }
        ap.ResetState();
        for (uint32_t i = 0; i < 6u; ++i)
        {
            fx6.SetNow(mem, 100u + i * 50u);
            const uint32_t out = ap.FilterButtons(0u, mem);
            PROBE_ASSERT(out == expected6[i]);
            // Next call releases
            fx6.SetNow(mem, 100u + i * 50u + 16u);
            PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
        }

        // Layout 2 (8K): L, LEFT, UP, RIGHT, SQUARE, TRIANGLE, CIRCLE, R
        const uint32_t expected8[8] = {
            kPspCtrlLTrigger, kPspCtrlLeft, kPspCtrlUp, kPspCtrlRight,
            kPspCtrlSquare, kPspCtrlTriangle, kPspCtrlCircle, kPspCtrlRTrigger
        };
        SynthChartFixture fx8;
        fx8.WriteScene(mem, kGameplaySceneVtable, 8u, 2u, 0u);
        for (uint32_t i = 0; i < 8u; ++i)
        {
            fx8.SetLaneNotes(mem, i, i, {{100u + i * 50u, kNoteTypeNormal, static_cast<uint8_t>(i)}});
        }
        ap.ResetState();
        for (uint32_t i = 0; i < 8u; ++i)
        {
            fx8.SetNow(mem, 100u + i * 50u);
            const uint32_t out = ap.FilterButtons(0u, mem);
            PROBE_ASSERT(out == expected8[i]);
            fx8.SetNow(mem, 100u + i * 50u + 16u);
            PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
        }
    }

    // 4. Long notes (duration >= 7 at +14): held from start until start + duration, then released.
    {
        SynthChartFixture fx;
        fx.WriteScene(mem, kGameplaySceneVtable, 4u, 0u, 0u);
        fx.SetLaneNotes(mem, 0u, 2u, {});
        // Long note at t=100, duration=64 (end=164), followed by tap note at t=200.
        fx.SetLaneNotes(mem, 1u, 4u, {
            {100u, kNoteTypeNormal, 4u, 64u},
            {200u, kNoteTypeNormal, 4u, 6u},
        });
        fx.SetLaneNotes(mem, 2u, 5u, {});
        fx.SetLaneNotes(mem, 3u, 6u, {});
        ap.ResetState();
        ap.SetEnabled(true);

        // t=80: not yet pressed
        fx.SetNow(mem, 80u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
        // t=100: long note starts -> UP pressed
        fx.SetNow(mem, 100u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == kPspCtrlUp);
        // t=116, 132, 148, 160: still held
        for (uint32_t t : {116u, 132u, 148u, 160u})
        {
            fx.SetNow(mem, t);
            PROBE_ASSERT(ap.FilterButtons(0u, mem) == kPspCtrlUp);
        }
        // t=164 (>= 100+64): released
        fx.SetNow(mem, 164u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
        // t=180: still released
        fx.SetNow(mem, 180u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
        // t=200: following tap note pressed
        fx.SetNow(mem, 200u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == kPspCtrlUp);
        // t=216: tap note released
        fx.SetNow(mem, 216u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
    }

    // Pause: the chart time stands still, so autoplay hands the buttons over
    // (the pause menu uses lane buttons), then resumes and presses the note.
    {
        SynthChartFixture fx;
        fx.WriteScene(mem, kGameplaySceneVtable, 4u, 0u, 0u);
        fx.SetLaneNotes(mem, 0u, 2u, {{200u, kNoteTypeNormal, 2u}});
        // Lanes 1-3 get empty channels, so nothing left by earlier checks is pressed.
        fx.SetLaneNotes(mem, 1u, 20u, {});
        fx.SetLaneNotes(mem, 2u, 21u, {});
        fx.SetLaneNotes(mem, 3u, 22u, {});
        ap.ResetState();
        ap.SetEnabled(true);

        const uint32_t human = kPspCtrlUp | kPspCtrlCircle | kPspCtrlStart;
        fx.SetNow(mem, 100u);
        PROBE_ASSERT((ap.FilterButtons(human, mem) & (kPspCtrlUp | kPspCtrlCircle)) == 0u); // running: lane buttons cleared
        uint32_t out = 0;
        for (uint32_t i = 0; i < kStalledCallsForPause; ++i)
        {
            out = ap.FilterButtons(human, mem); // now stays at 100
        }
        PROBE_ASSERT(out == human); // paused: passed through unchanged
        PROBE_ASSERT(ap.FilterButtons(human, mem) == human);

        bool pressed = false;
        for (uint32_t t = 116u; t <= 260u; t += 16u)
        {
            fx.SetNow(mem, t);
            out = ap.FilterButtons(human, mem);
            PROBE_ASSERT((out & (kPspCtrlUp | kPspCtrlCircle)) == 0u); // running again: cleared
            pressed = pressed || ((out & kPspCtrlLeft) != 0u);
        }
        PROBE_ASSERT(pressed); // the note at t=200 is still pressed after the pause
    }

    mem.Shutdown();
    std::printf("[PASS] redmp1_autoplay_probe: all checks passed\n");
    return 0;
}
