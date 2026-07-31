#pragma once

#include <array>
#include <cmath>
#include <cstdint>

#include <juce_graphics/juce_graphics.h>

#include "../DSP/Algorithm.h"
#include "../DSP/EnvelopeShape.h"
#include "FloydLayout.h"

// =====================================================================
//  Brightness.h -- Floyd's brightness gradient. THE metric.
//
//  Deferred through v1 because nobody knew how the value was derived.
//  Floyd supplied it directly (2026-07-30):
//
//     "The gradient shows the brightness of the sound (or the overall
//      modulation strength). It's quite simple to do: add up all the
//      MODULATORS values on the Y-axis and divide by the number of
//      modulators, then map the result to a color index that goes from
//      color 1 to color 2. Then use the result for coloring the
//      CARRIERS graph. You will now get a graph that shows exactly
//      where the sound is dark and where it's bright."
//
//  So: brightness(target, x) = mean over the operators modulating
//  `target` of (envelope value at x) * amp -- amplitude is part of the
//  y-value, so a modulator turned down is a dark carrier. The carrier's
//  stroke is then drawn with that value mapped along the time axis.
//
//  Everything here is DISPLAY-space. It reads the same envelope math the
//  voice ticks (EnvelopeShape.h) and the same routing table the voice
//  renders through (Algorithm.h), so the picture cannot drift from the
//  sound. No parallel tables, no second copy of the envelope shape.
// =====================================================================

namespace floyd
{

// One operator as the editor sees it. Lives here rather than in
// EnvelopeCanvas because both the canvas and the brightness metric need
// it, and the metric is the lower layer.
struct OpDisplay
{
    float attack = 0.0f, decay = 0.0f, sustain = 0.0f, release = 0.0f, amp = 0.0f;

    constexpr EnvParams env() const { return { attack, decay, sustain, release }; }
};

// --- display-space envelope ------------------------------------------
// Normalised (0..1, amp NOT applied) envelope value at display position
// x, where x is the fraction of the plot width: 0 = left edge of the
// attack band, 1 = right edge of the release band. Delegates to the
// shared envelope math for every branch -- the three bands here are the
// display's time windows, not a second envelope shape.
inline float envDisplayAt (const OpDisplay& o, float x)
{
    namespace L = Layout;

    if (x <= L::AD_FRAC)
        return envHeldAt (o.env(), (x / L::AD_FRAC) * L::AD_WINDOW);

    if (x <= L::AD_FRAC + L::SUS_FRAC)
        return o.sustain;

    const float tRel = ((x - L::AD_FRAC - L::SUS_FRAC) / L::REL_FRAC) * L::REL_WINDOW;

    // The drawn release segment falls from the sustain level, so that is
    // the level the display releases from.
    return envReleaseAt (o.env(), tRel, o.sustain);
}

// --- who modulates whom ----------------------------------------------
// Floyd says "all the modulators". In a 4-op patch that has two honest
// readings, because a modulator can feed a carrier through another
// modulator: the operators feeding the carrier DIRECTLY, or every
// operator upstream of it. Upstream is used, because on a stack
// (1>2>3>4) the direct reading would colour OP4 from OP3 alone and show
// a flat line while OP1 and OP2 were plainly changing the timbre. On
// algorithms with independent chains (TWIN PAIR) both readings agree
// that each carrier gets its own brightness, which is the information
// the picture exists to carry.
//
// Flip this to false for the strict direct-feeder reading; it is the one
// place that decides.
inline constexpr bool kBrightnessUsesUpstreamChain = true;

struct Feeders
{
    std::array<std::uint8_t, kNumOps> op {};
    int count = 0;
};

inline Feeders feedersOf (const Algorithm& a, int target)
{
    bool upstream[kNumOps] {};

    for (std::uint8_t e = 0; e < a.edgeCount; ++e)
        if ((int) a.edges[e].dst == target)
            upstream[a.edges[e].src] = true;

    if (kBrightnessUsesUpstreamChain)
        for (int pass = 0; pass < kNumOps; ++pass)      // DAG is at most kNumOps deep
            for (std::uint8_t e = 0; e < a.edgeCount; ++e)
                if (upstream[a.edges[e].dst])
                    upstream[a.edges[e].src] = true;

    Feeders f;
    for (int i = 0; i < kNumOps; ++i)
        if (upstream[i] && i != target)
            f.op[(std::size_t) f.count++] = (std::uint8_t) i;

    return f;
}

// --- the metric -------------------------------------------------------
// Returns 0..1 brightness for operator `target` at display position x,
// or NEGATIVE if `target` has no modulators at all -- ADDITIVE, and the
// bare sines in PAIR+SINE. Those carriers have no brightness information
// to show, so they keep their identity colour rather than being painted
// the "dark" end of a scale they are not on.
inline float brightnessAt (int target, const Algorithm& alg,
                           const std::array<OpDisplay, kNumOps>& ops, float x)
{
    const auto feeders = feedersOf (alg, target);

    if (feeders.count == 0)
        return -1.0f;

    float sum = 0.0f;

    for (int i = 0; i < feeders.count; ++i)
    {
        const auto& m = ops[feeders.op[(std::size_t) i]];
        sum += envDisplayAt (m, x) * m.amp;             // amp is baked into the y-value
    }

    return juce::jlimit (0.0f, 1.0f, sum / (float) feeders.count);
}

inline bool isModulated (const Algorithm& alg, int target)
{
    return feedersOf (alg, target).count > 0;
}

// --- normalisation ----------------------------------------------------
// Floyd: "The result is NORMALIZED and mapped to a color scale from pure
// blue to pure red." Without this the raw mean rarely gets near 1.0 --
// average three modulators sitting at 0.6 and the answer is 0.6, so the
// hot end of the scale is never reached and nothing on the panel ever
// glows red. Normalising against the patch's own peak is what makes the
// scale span blue to red on every sound, which is the whole point of a
// picture that says "here is where this patch is brightest".
//
// One scale for the whole picture, not one per carrier: on TWIN PAIR the
// two chains are independent, and per-carrier scaling would paint a weak
// chain as hot as a strong one and hide the difference between them.
//
// The reference peak is the floor on the divisor, and it is what keeps
// the display honest. A patch whose modulators barely move has a low
// peak; dividing by that peak alone would blow it up to full red and
// claim a bright sound that is not there. Dividing by at least
// kBrightnessRefPeak means a genuinely dark patch STAYS dark, while any
// normally-driven patch uses the full scale.
inline constexpr float kBrightnessRefPeak      = 0.5f;
inline constexpr int   kBrightnessScaleSamples = 64;

// --- perceptual mapping ----------------------------------------------
// Floyd's "values on the Y-axis" were TX81Z-family operator LEVELS,
// which run ~0.75 dB per step -- a log-amplitude axis. This synth's amp
// is linear, and the voice sums modulator output straight into phase
// (CG-FM-1), so a modulator at half amplitude still carries a modulation
// index of ~pi: plainly bright to the ear, yet a linear map paints it
// mid-scale and slams to blue while the sound is still shimmering.
// Applying Floyd's linear colour-index map to LINEAR amps therefore
// understates brightness everywhere below the peak -- his own screen
// was averaging perceptual-domain values.
//
// The gamma restores that: the normalised value is raised to this power
// before the 12-shade quantise. 1.0 is the literal linear map (the
// 2026-07-30 first ship); 0.5 approximates the log domain over the
// range the modulation index actually spans. It slightly softens the
// kBrightnessRefPeak honesty floor (a floored patch maps a bit hotter
// than its raw value), which is accepted: the floor still prevents a
// barely-modulated patch from reaching full red.
inline constexpr float kBrightnessGamma = 0.5f;

// The one place the normalised metric becomes a shade position: clamp,
// then the perceptual curve. Callers feed the result to
// Colours::brightnessColour, which stays Floyd's pure ramp.
inline float brightnessShade (float raw, float scale)
{
    return std::pow (juce::jlimit (0.0f, 1.0f, raw * scale), kBrightnessGamma);
}

inline float brightnessScale (const Algorithm& alg, const std::array<OpDisplay, kNumOps>& ops)
{
    float peak = 0.0f;

    for (int c = 0; c < kNumOps; ++c)
        if (isCarrier (alg, c) && isModulated (alg, c))
            for (int i = 0; i <= kBrightnessScaleSamples; ++i)
                peak = juce::jmax (peak, brightnessAt (c, alg, ops,
                                                       (float) i / (float) kBrightnessScaleSamples));

    return 1.0f / juce::jmax (kBrightnessRefPeak, peak);
}

} // namespace floyd
