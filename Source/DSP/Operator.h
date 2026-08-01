#pragma once

#include <cmath>

#include "EnvelopeShape.h"

// =====================================================================
//  Operator.h -- one FM operator. Phase-accumulator sine ONLY.
//  No wavetables, no alternative waveforms (handoff, "DSP Notes").
//
//  CG-FM-1 (CRITICAL). Modulator output is summed DIRECTLY into the
//  downstream phase accumulator:
//
//      phase += baseFreq + modInput;         // RIGHT
//      phase += baseFreq + modInput / twoPi; // WRONG -- caps index at 1.0 rad
//
//  Phase here is normalised 0..1 (cycles), so a modulator output of 1.0
//  is one full cycle == 2*pi radians of modulation index per stage --
//  which is the DX7-lineage depth. Dividing by 2*pi caps each stage at
//  1 radian, roughly 16% of that, and makes cascades sound thin.
//  Verified against GlacierFM, which removed exactly that scaling on
//  2026-04-25 after A/B-ing against Dexed.
// =====================================================================

namespace floyd
{

inline constexpr float kTwoPi = 6.28318530717958647692f;

class Operator
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = (sampleRate > 0.0) ? sampleRate : 44100.0;
        env.prepare (sr);
        reset();
    }

    void reset() noexcept
    {
        phase      = 0.0f;
        modInput   = 0.0f;
        lastOutput = 0.0f;
        env.reset();
    }

    void noteOn (float frequencyHz, const EnvParams& p, float amplitude) noexcept
    {
        setFrequency (frequencyHz);
        amp        = amplitude;
        phase      = 0.0f;
        modInput   = 0.0f;
        lastOutput = 0.0f;
        env.noteOn (p);
    }

    void noteOff() noexcept { env.noteOff(); }

    void setFrequency (float frequencyHz) noexcept
    {
        increment = frequencyHz / (float) sr;
    }

    // Amplitude is pushed per block: it is op{n}_amp, already scaled by
    // the voice through this operator's own velocity sensitivity.
    void setAmplitude (float a) noexcept { amp = a; }

    // OP1 self-feedback, one-sample delayed (handoff, "Feedback").
    void setFeedback (float amount01) noexcept { feedback = amount01; }

    void addModInput (float m) noexcept { modInput += m; }

    float tick() noexcept
    {
        phase += increment;
        while (phase >= 1.0f) phase -= 1.0f;

        // CG-FM-1: raw, undivided.
        float modPhase = phase + modInput;

        if (feedback > 0.0f)
        {
            // Feedback is expressed in RADIANS and clamped, so it -- and
            // only it -- converts to cycles on the way in. The clamp was
            // re-verified against the undivided modulation scaling above,
            // as the handoff requires.
            constexpr float kFeedbackMax = 7.0f;
            constexpr float kClamp       = 3.14159265358979323846f;

            float fb = lastOutput * feedback * kFeedbackMax;
            if (fb >  kClamp) fb =  kClamp;
            if (fb < -kClamp) fb = -kClamp;

            modPhase += fb * (1.0f / kTwoPi);
        }

        const float wrapped = modPhase - std::floor (modPhase);
        const float out     = std::sin (kTwoPi * wrapped) * amp * env.tick();

        lastOutput = out;
        modInput   = 0.0f;
        return out;
    }

    bool isFinished() const noexcept { return env.isFinished(); }
    float lastValue() const noexcept { return lastOutput; }

private:
    double sr = 44100.0;

    float phase      = 0.0f;
    float increment  = 0.0f;
    float amp        = 0.0f;
    float feedback   = 0.0f;
    float modInput   = 0.0f;
    float lastOutput = 0.0f;

    Envelope env;
};

} // namespace floyd
