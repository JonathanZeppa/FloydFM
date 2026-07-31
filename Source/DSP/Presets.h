#pragma once

#include <array>

#include "Algorithm.h"
#include "RatioTable.h"

// =====================================================================
//  Presets.h -- 10 factory presets, hardcoded. No user save (free tier).
//
//  Transcribed VERBATIM from the FACTORY array in
//  zedlab-fm-brightness-v11.jsx, which the handoff names as the source:
//  "Parameter values are in the FACTORY array ... Transcribe them
//  exactly." Ratios are stored as the mockup's literal floats and mapped
//  through nearestRatioIndex() at load, so the three values that are not
//  in the 19-entry snapped table (E PIANO op1 = 14.00, METAL HIT op1 =
//  5.25, op2 = 8.75) are resolved by one documented rule rather than by
//  three silent hand-edits. See DISC-3 in floydfm-logfile.md.
//
//  ⚠️ These presets have never been heard. The handoff is explicit that
//  an ear-audit is a hard gate before any public build.
// =====================================================================

namespace floyd
{

inline constexpr int kNumPresets = 10;

struct PresetOp
{
    float attack, decay, sustain, release, amp, ratio;
};

struct Preset
{
    const char* name;
    int         algorithmIndex;     // 0-based; display is +1
    std::array<PresetOp, kNumOps> op;
};

inline constexpr std::array<Preset, kNumPresets> kPresets { {
    { "GLASS BELL", 4, { {
        { 0.00f, 0.90f, 0.00f, 0.90f, 0.80f,  3.50f },
        { 0.00f, 1.80f, 0.00f, 1.80f, 1.00f,  1.00f },
        { 0.00f, 0.60f, 0.00f, 0.60f, 0.65f,  7.00f },
        { 0.00f, 2.20f, 0.00f, 2.20f, 0.85f,  1.00f } } } },

    { "E PIANO", 4, { {
        { 0.00f, 0.45f, 0.00f, 0.35f, 0.72f, 14.00f },   // 14.00 -> snapped
        { 0.00f, 2.40f, 0.18f, 0.80f, 1.00f,  1.00f },
        { 0.00f, 0.30f, 0.00f, 0.30f, 0.40f,  1.00f },
        { 0.00f, 2.80f, 0.22f, 0.90f, 0.70f,  1.00f } } } },

    { "BRASS", 0, { {
        { 0.18f, 0.60f, 0.55f, 0.30f, 0.55f,  1.00f },
        { 0.12f, 0.50f, 0.62f, 0.30f, 0.62f,  1.00f },
        { 0.10f, 0.45f, 0.70f, 0.28f, 0.70f,  1.00f },
        { 0.06f, 0.35f, 0.82f, 0.35f, 1.00f,  1.00f } } } },

    { "WOOD FLUTE", 6, { {
        { 0.14f, 0.50f, 0.35f, 0.30f, 0.28f,  2.00f },
        { 0.10f, 0.40f, 0.85f, 0.30f, 0.90f,  1.00f },
        { 0.16f, 0.60f, 0.80f, 0.35f, 0.30f,  2.00f },
        { 0.20f, 0.70f, 0.75f, 0.40f, 0.20f,  4.00f } } } },

    { "METAL HIT", 1, { {
        { 0.00f, 0.22f, 0.00f, 0.22f, 0.95f,  5.25f },   // 5.25 -> snapped
        { 0.00f, 0.18f, 0.00f, 0.18f, 0.85f,  8.75f },   // 8.75 -> snapped
        { 0.00f, 0.30f, 0.00f, 0.30f, 0.90f,  3.00f },
        { 0.00f, 0.55f, 0.00f, 0.55f, 1.00f,  1.00f } } } },

    { "SUB BASS", 6, { {
        { 0.00f, 0.30f, 0.20f, 0.20f, 0.35f,  1.00f },
        { 0.00f, 0.80f, 0.70f, 0.25f, 1.00f,  0.50f },
        { 0.00f, 0.60f, 0.55f, 0.25f, 0.45f,  0.50f },
        { 0.00f, 0.40f, 0.30f, 0.20f, 0.20f,  1.00f } } } },

    { "ORGAN", 7, { {
        { 0.02f, 0.20f, 1.00f, 0.15f, 0.85f,  1.00f },
        { 0.02f, 0.20f, 1.00f, 0.15f, 0.55f,  2.00f },
        { 0.02f, 0.20f, 1.00f, 0.15f, 0.35f,  4.00f },
        { 0.02f, 0.20f, 1.00f, 0.15f, 0.22f,  8.00f } } } },

    { "PLUCK", 0, { {
        { 0.00f, 0.28f, 0.00f, 0.20f, 0.75f,  2.00f },
        { 0.00f, 0.35f, 0.00f, 0.25f, 0.70f,  1.00f },
        { 0.00f, 0.45f, 0.00f, 0.30f, 0.65f,  1.00f },
        { 0.00f, 0.70f, 0.00f, 0.45f, 1.00f,  1.00f } } } },

    { "GROWL", 2, { {
        { 0.05f, 0.90f, 0.45f, 0.40f, 0.80f,  2.00f },
        { 0.00f, 0.40f, 0.30f, 0.30f, 0.70f,  3.00f },
        { 0.08f, 1.10f, 0.50f, 0.45f, 0.60f,  1.00f },
        { 0.02f, 0.60f, 0.70f, 0.50f, 1.00f,  1.00f } } } },

    { "AIR PAD", 5, { {
        { 1.20f, 2.00f, 0.55f, 1.80f, 0.30f,  2.00f },
        { 0.90f, 1.60f, 0.80f, 2.20f, 0.85f,  1.00f },
        { 1.40f, 2.40f, 0.70f, 2.40f, 0.55f,  1.00f },
        { 1.10f, 2.20f, 0.65f, 2.00f, 0.45f,  4.00f } } } },
} };

inline const Preset& presetAt (int index)
{
    return kPresets[(std::size_t) (index < 0 ? 0 : (index >= kNumPresets ? kNumPresets - 1 : index))];
}

} // namespace floyd
