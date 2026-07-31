#pragma once

#include <array>
#include <cmath>

#include "Voice.h"

// =====================================================================
//  VoiceManager.h -- 8 voices (handoff, "Plugin Overview": Polyphony 8).
//
//  Also owns the master limiter. Handoff, "DSP Notes":
//    "Limiter (REQUIRED, not optional). Algorithms 5-8 are multi-carrier
//     and sum coherently at chord onset -- roughly +9.5 dB above the
//     monophonic case. Per-algorithm gain trimming cannot fix this
//     because it depends on what the player plays."
// =====================================================================

namespace floyd
{

inline constexpr int kNumVoices = 8;

// Soft-knee master limiter, post-master-level and pre-output.
//
// The handoff specifies "linear at |x| <= 0.8, tanh(x * 2.5) above".
// Taken literally that is DISCONTINUOUS -- tanh(0.8 * 2.5) = 0.964, so
// the transfer curve would jump 0.8 -> 0.964 at the knee and buzz on
// every crossing. Anchored so it is continuous at the knee and
// asymptotes to unity. Logged as DISC-6 in floydfm-logfile.md.
//
// THRESHOLD HISTORY
//   0.80  handoff literal. Caused audible saturation with NO clipping
//         shown in the host meter -- the asymptote is unity, so output
//         can never exceed 0 dBFS and the meter never lights, while
//         everything above 0.80 is being tanh-compressed. Six of the ten
//         factory presets peaked between 0.88 and 0.99, i.e. they were
//         living inside the compressor.
//   0.97  current (~-0.3 dBFS). The industry norm for a synth master
//         safety limiter: catches emergency peaks without colouring the
//         usable range. This is GlacierFM's landed fix for the identical
//         symptom (its recon named the 0.8 knee as the dominant suspect
//         for "unexpected saturation on all-sine presets", 2026-05-19).
//         Fallbacks if peaks feel uncontrolled: 0.94, then 0.92.
//
// TRAP: the headroom coefficient MUST be derived as (1 - threshold), not
// hardcoded. The old form's literal 0.2f is exactly (1 - 0.8); changing
// only the threshold and leaving 0.2f behind moves the asymptote to 1.17
// and the plugin then genuinely clips the host.
inline constexpr float kSoftKneeThreshold = 0.97f;

// Fixed output trim, applied after master_level and before the limiter.
//
// WHY THIS EXISTS. master_level defaults to 0.8 per the handoff, which
// put a SINGLE held note at 0.80 -- i.e. the instrument reached full
// scale on one note and had zero headroom left for polyphony. A plain
// four-note chord then arrived at ~3.05 and the limiter clamped it back
// to unity: 9 to 12 dB of continuous gain reduction on ordinary playing,
// heard as distortion, invisible on the host meter.
//
// The handoff predicted the +9.5 dB chord-onset sum and concluded the
// limiter should absorb it. Measurement says otherwise -- absorbing 10 dB
// with tanh IS the distortion. The limiter's job is emergency peaks, so
// the gain structure has to leave room for chords BEFORE it.
//
// 0.25 puts one note near -12 dBFS (normal for a polyphonic synth) and a
// four-note chord just under the knee, leaving the limiter idle during
// ordinary playing. Deliberately NOT voice-count normalisation, which
// would pump audibly as notes are added and released.
inline constexpr float kOutputTrim = 0.25f;

inline float softClip (float x) noexcept
{
    const float a = std::abs (x);
    if (a <= kSoftKneeThreshold)
        return x;                       // linear range -- no coloration

    const float sign     = (x < 0.0f) ? -1.0f : 1.0f;
    const float over     = a - kSoftKneeThreshold;
    const float headroom = 1.0f - kSoftKneeThreshold;   // -> unity asymptote
    return sign * (kSoftKneeThreshold + headroom * std::tanh (over * 2.5f));
}

class VoiceManager
{
public:
    void prepare (double sampleRate) noexcept
    {
        for (auto& v : voices)
            v.prepare (sampleRate);

        stamp = 0;
    }

    void reset() noexcept
    {
        for (auto& v : voices)
            v.reset();
    }

    void noteOn (int midiNote, float velocity01, const PatchParams& patch) noexcept
    {
        ++stamp;

        if (auto* v = findFreeVoice())
        {
            v->noteOn (midiNote, velocity01, patch, stamp);
            return;
        }

        if (auto* v = stealVoice())
        {
            v->reset();
            v->noteOn (midiNote, velocity01, patch, stamp);
        }
    }

    void noteOff (int midiNote) noexcept
    {
        ++stamp;

        for (auto& v : voices)
            if (v.isActive() && ! v.isReleased() && v.getNote() == midiNote)
            {
                v.noteOff();
                v.setReleaseStamp (stamp);
            }
    }

    void allNotesOff() noexcept
    {
        for (auto& v : voices)
            v.reset();
    }

    void applyContinuous (const PatchParams& patch) noexcept
    {
        for (auto& v : voices)
            v.applyContinuous (patch);
    }

    // Renders mono into L/R. masterLevel is applied before the limiter.
    void render (float* left, float* right, int numSamples, float masterLevel) noexcept
    {
        for (int s = 0; s < numSamples; ++s)
        {
            float sum = 0.0f;

            for (auto& v : voices)
                if (v.isActive())
                    sum += v.renderSample();

            const float pre = sum * masterLevel * kOutputTrim;
            const float out = softClip (pre);

            // Metering only -- how hard the limiter is working. A large
            // gap between pre and post means the dirt is compression,
            // not clipping, and the host meter will not show it.
            const float absPre = std::abs (pre);
            if (absPre > preLimiterPeak)
                preLimiterPeak = absPre;

            left[s]  += out;
            right[s] += out;
        }
    }

    float getPreLimiterPeak() const noexcept { return preLimiterPeak; }
    void  resetPreLimiterPeak() noexcept     { preLimiterPeak = 0.0f; }

    bool anyVoiceActive() const noexcept
    {
        for (const auto& v : voices)
            if (v.isActive())
                return true;

        return false;
    }

private:
    Voice* findFreeVoice() noexcept
    {
        for (auto& v : voices)
            if (! v.isActive())
                return &v;

        return nullptr;
    }

    // Prefer the oldest RELEASED voice; fall back to the oldest active.
    Voice* stealVoice() noexcept
    {
        Voice* best = nullptr;
        std::uint64_t bestStamp = ~0ull;

        for (auto& v : voices)
            if (v.isReleased() && v.getReleaseStamp() < bestStamp)
            {
                bestStamp = v.getReleaseStamp();
                best      = &v;
            }

        if (best != nullptr)
            return best;

        bestStamp = ~0ull;
        for (auto& v : voices)
            if (v.getNoteOnStamp() < bestStamp)
            {
                bestStamp = v.getNoteOnStamp();
                best      = &v;
            }

        return best;
    }

    std::array<Voice, kNumVoices> voices;
    std::uint64_t stamp = 0;
    float preLimiterPeak = 0.0f;
};

} // namespace floyd
