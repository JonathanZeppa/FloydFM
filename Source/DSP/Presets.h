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
//  Ear-audit CLEARED 2026-07-26 -- "presets sound like their intended
//  instrument". The values below are therefore load-bearing; change them
//  only with a fresh ear check.
//
//  `vel` (added 2026-08-01) is the ONE column the mockup has no source
//  for: Floyd's FACTORY array predates the velocity work, and the DX7
//  model needs a per-operator sensitivity per patch. Authored here from
//  the instrument each preset is imitating, under three rules:
//    - Modulators run hot (0.8-1.0) wherever "hit it harder" should mean
//      "brighter": pianos, brass, plucks, percussion. This is Floyd's
//      actual request and the expressive core of FM.
//    - Carriers run moderate (0.4-0.7) on struck/plucked instruments and
//      low (0.25-0.35) on sustained ones, so a pad played softly gets
//      gentler rather than disappearing.
//    - ORGAN is 0.00 across the board. A drawbar organ has no velocity
//      response at all; faking one would be wrong, and it doubles as the
//      regression case proving op.velSens = 0 restores v0.1.0 behaviour.
// =====================================================================

namespace floyd
{

inline constexpr int kNumPresets = 10;

struct PresetOp
{
    float attack, decay, sustain, release, amp, ratio, vel;
};

struct Preset
{
    const char* name;
    int         algorithmIndex;     // 0-based; display is +1
    std::array<PresetOp, kNumOps> op;
};

// The trailing comment on each row is that operator's ROLE under the
// preset's own algorithm, so the vel column can be read against it
// without cross-referencing Algorithm.h. Role is per-algorithm; vel is
// per-operator and does not follow it if the user changes algorithm.
inline constexpr std::array<Preset, kNumPresets> kPresets { {
    //     attack  decay   sus     rel     amp     ratio   vel
    { "GLASS BELL", 4, { {                                          // ALG 5 TWIN PAIR
        { 0.00f, 0.90f, 0.00f, 0.90f, 0.80f,  3.50f, 0.95f },       // mod     -- strike brightness
        { 0.00f, 1.80f, 0.00f, 1.80f, 1.00f,  1.00f, 0.55f },       // carrier
        { 0.00f, 0.60f, 0.00f, 0.60f, 0.65f,  7.00f, 0.95f },       // mod     -- strike brightness
        { 0.00f, 2.20f, 0.00f, 2.20f, 0.85f,  1.00f, 0.55f } } } }, // carrier

    { "E PIANO", 4, { {                                             // ALG 5 TWIN PAIR
        { 0.00f, 0.45f, 0.00f, 0.35f, 0.72f, 14.00f, 1.00f },       // mod     -- 14.00 -> snapped. Floyd's own example
        { 0.00f, 2.40f, 0.18f, 0.80f, 1.00f,  1.00f, 0.65f },       // carrier
        { 0.00f, 0.30f, 0.00f, 0.30f, 0.40f,  1.00f, 1.00f },       // mod
        { 0.00f, 2.80f, 0.22f, 0.90f, 0.70f,  1.00f, 0.65f } } } }, // carrier

    { "BRASS", 0, { {                                               // ALG 1 STACK
        { 0.18f, 0.60f, 0.55f, 0.30f, 0.55f,  1.00f, 0.90f },       // mod
        { 0.12f, 0.50f, 0.62f, 0.30f, 0.62f,  1.00f, 0.90f },       // mod
        { 0.10f, 0.45f, 0.70f, 0.28f, 0.70f,  1.00f, 0.85f },       // mod
        { 0.06f, 0.35f, 0.82f, 0.35f, 1.00f,  1.00f, 0.45f } } } }, // carrier -- blow harder, mostly brighter

    { "WOOD FLUTE", 6, { {                                          // ALG 7 PAIR+SINE
        { 0.14f, 0.50f, 0.35f, 0.30f, 0.28f,  2.00f, 0.70f },       // mod     -- breath noise, not a hard edge
        { 0.10f, 0.40f, 0.85f, 0.30f, 0.90f,  1.00f, 0.35f },       // carrier
        { 0.16f, 0.60f, 0.80f, 0.35f, 0.30f,  2.00f, 0.30f },       // carrier
        { 0.20f, 0.70f, 0.75f, 0.40f, 0.20f,  4.00f, 0.30f } } } }, // carrier

    { "METAL HIT", 1, { {                                           // ALG 2 TWIN FEED
        { 0.00f, 0.22f, 0.00f, 0.22f, 0.95f,  5.25f, 1.00f },       // mod     -- 5.25 -> snapped
        { 0.00f, 0.18f, 0.00f, 0.18f, 0.85f,  8.75f, 1.00f },       // mod     -- 8.75 -> snapped
        { 0.00f, 0.30f, 0.00f, 0.30f, 0.90f,  3.00f, 0.90f },       // mod
        { 0.00f, 0.55f, 0.00f, 0.55f, 1.00f,  1.00f, 0.70f } } } }, // carrier -- percussion: loud AND bright

    { "SUB BASS", 6, { {                                            // ALG 7 PAIR+SINE
        { 0.00f, 0.30f, 0.20f, 0.20f, 0.35f,  1.00f, 0.85f },       // mod     -- dig in for growl
        { 0.00f, 0.80f, 0.70f, 0.25f, 1.00f,  0.50f, 0.35f },       // carrier -- bass stays solid when played soft
        { 0.00f, 0.60f, 0.55f, 0.25f, 0.45f,  0.50f, 0.35f },       // carrier
        { 0.00f, 0.40f, 0.30f, 0.20f, 0.20f,  1.00f, 0.30f } } } }, // carrier

    { "ORGAN", 7, { {                                               // ALG 8 ADDITIVE
        { 0.02f, 0.20f, 1.00f, 0.15f, 0.85f,  1.00f, 0.00f },       // carrier -- a drawbar organ has NO velocity
        { 0.02f, 0.20f, 1.00f, 0.15f, 0.55f,  2.00f, 0.00f },       // carrier    response. All four deliberately 0.
        { 0.02f, 0.20f, 1.00f, 0.15f, 0.35f,  4.00f, 0.00f },       // carrier
        { 0.02f, 0.20f, 1.00f, 0.15f, 0.22f,  8.00f, 0.00f } } } }, // carrier

    { "PLUCK", 0, { {                                               // ALG 1 STACK
        { 0.00f, 0.28f, 0.00f, 0.20f, 0.75f,  2.00f, 0.95f },       // mod
        { 0.00f, 0.35f, 0.00f, 0.25f, 0.70f,  1.00f, 0.90f },       // mod
        { 0.00f, 0.45f, 0.00f, 0.30f, 0.65f,  1.00f, 0.85f },       // mod
        { 0.00f, 0.70f, 0.00f, 0.45f, 1.00f,  1.00f, 0.60f } } } }, // carrier

    { "GROWL", 2, { {                                               // ALG 3 SPLIT MOD
        { 0.05f, 0.90f, 0.45f, 0.40f, 0.80f,  2.00f, 0.90f },       // mod
        { 0.00f, 0.40f, 0.30f, 0.30f, 0.70f,  3.00f, 0.85f },       // mod
        { 0.08f, 1.10f, 0.50f, 0.45f, 0.60f,  1.00f, 0.80f },       // mod
        { 0.02f, 0.60f, 0.70f, 0.50f, 1.00f,  1.00f, 0.50f } } } }, // carrier

    { "AIR PAD", 5, { {                                             // ALG 6 FAN
        { 1.20f, 2.00f, 0.55f, 1.80f, 0.30f,  2.00f, 0.60f },       // mod     -- pad: audible but never abrupt
        { 0.90f, 1.60f, 0.80f, 2.20f, 0.85f,  1.00f, 0.25f },       // carrier
        { 1.40f, 2.40f, 0.70f, 2.40f, 0.55f,  1.00f, 0.25f },       // carrier
        { 1.10f, 2.20f, 0.65f, 2.00f, 0.45f,  4.00f, 0.25f } } } }, // carrier
} };

inline const Preset& presetAt (int index)
{
    return kPresets[(std::size_t) (index < 0 ? 0 : (index >= kNumPresets ? kNumPresets - 1 : index))];
}

} // namespace floyd
