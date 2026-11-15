#!/usr/bin/env python3
"""Measure short silence gaps (10-50 ms) between sounding stretches in a PCM WAV file."""

import argparse
import math
import os
import struct
import sys
import wave


def analyze_wav(path: str, threshold: float, window_ms: int = 10) -> dict:
    with wave.open(path, "rb") as wf:
        channels = wf.getnchannels()
        sampwidth = wf.getsampwidth()
        rate = wf.getframerate()
        nframes = wf.getnframes()
        if sampwidth != 2:
            raise ValueError(f"Only 16-bit PCM WAV is supported (got sampwidth={sampwidth})")
        raw = wf.readframes(nframes)

    total_samples = nframes * channels
    samples = struct.unpack(f"<{total_samples}h", raw)

    win_frames = max(1, (rate * window_ms) // 1000)
    win_samples = win_frames * channels
    num_windows = nframes // win_frames

    rms_list = []
    for w in range(num_windows):
        start = w * win_samples
        end = start + win_samples
        sq = 0
        for s in samples[start:end]:
            sq += s * s
        rms = math.sqrt(sq / win_samples)
        rms_list.append(rms)

    sounding = [r >= threshold for r in rms_list]

    # Find silent runs that have sounding windows on both sides.
    short_gaps = []  # (start_win, len_win)
    long_silences = []  # (start_win, len_win)
    i = 0
    seen_sound = False
    while i < num_windows:
        if sounding[i]:
            seen_sound = True
            i += 1
            continue
        j = i
        while j < num_windows and not sounding[j]:
            j += 1
        # Check if there is sound after this silent run.
        has_sound_after = j < num_windows
        if seen_sound and has_sound_after:
            run_len = j - i
            if 1 <= run_len <= 5:
                short_gaps.append((i, run_len))
            elif run_len > 5:
                long_silences.append((i, run_len))
        i = j

    first_sound = next((idx for idx, s in enumerate(sounding) if s), None)
    last_sound = next((idx for idx in range(num_windows - 1, -1, -1) if sounding[idx]), None)
    active_windows = (last_sound - first_sound + 1) if (first_sound is not None and last_sound is not None) else 0

    short_gap_windows = sum(length for _, length in short_gaps)
    long_silence_windows = sum(length for _, length in long_silences)
    hist = {k: 0 for k in range(1, 6)}
    for _, length in short_gaps:
        hist[length] += 1

    # 5-second bucket distribution
    bucket_sec = 5
    buckets = {}
    for start_w, length in short_gaps:
        t_sec = (start_w * window_ms) / 1000.0
        b = int(t_sec // bucket_sec)
        buckets[b] = buckets.get(b, 0) + 1

    return {
        "path": path,
        "rate": rate,
        "channels": channels,
        "frames": nframes,
        "duration_s": nframes / float(rate) if rate > 0 else 0.0,
        "window_ms": window_ms,
        "threshold": threshold,
        "num_windows": num_windows,
        "sounding_windows": sum(1 for s in sounding if s),
        "active_windows": active_windows,
        "short_gaps": short_gaps,
        "short_gap_count": len(short_gaps),
        "short_gap_ms": short_gap_windows * window_ms,
        "short_gap_share_pct": (100.0 * short_gap_windows / active_windows) if active_windows > 0 else 0.0,
        "hist": hist,
        "long_silence_count": len(long_silences),
        "long_silence_ms": long_silence_windows * window_ms,
        "buckets": buckets,
        "bucket_sec": bucket_sec,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Measure short silence gaps in a WAV file.")
    parser.add_argument("wav", help="Path to 16-bit PCM WAV file")
    parser.add_argument("--threshold", type=float, default=32.0, help="10 ms window RMS silence threshold (default: 32.0)")
    args = parser.parse_args()

    if not os.path.isfile(args.wav):
        print(f"Error: file not found: {args.wav}", file=sys.stderr)
        return 1

    res = analyze_wav(args.wav, args.threshold)
    print(f"File: {res['path']}")
    print(
        f"Format: {res['rate']} Hz, {res['channels']} ch, "
        f"{res['frames']} frames ({res['duration_s']:.2f} s), "
        f"windows={res['num_windows']} ({res['window_ms']} ms), "
        f"sounding={res['sounding_windows']}, active_span={res['active_windows']} windows"
    )
    print(
        f"Short gaps (10-50 ms, sound on both sides): count={res['short_gap_count']}, "
        f"total={res['short_gap_ms']} ms ({res['short_gap_share_pct']:.2f}% of active span)"
    )
    print(
        "Gap duration histogram: "
        + ", ".join(f"{k * 10}ms={res['hist'][k]}" for k in range(1, 6))
    )
    print(
        f"Long silences (>50 ms, sound on both sides): count={res['long_silence_count']}, "
        f"total={res['long_silence_ms']} ms"
    )
    if res["buckets"]:
        bsec = res["bucket_sec"]
        parts = [f"{b * bsec}-{(b + 1) * bsec}s:{cnt}" for b, cnt in sorted(res["buckets"].items())]
        print("Time distribution: " + ", ".join(parts))
        sample_times = [f"{(w * res['window_ms']) / 1000.0:.2f}s({l * 10}ms)" for w, l in res["short_gaps"][:10]]
        print("First gaps: " + ", ".join(sample_times))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
