#pragma once

// =====================================================================
//  EnvelopeShape.h -- the one definition of envelope shape.
//
//  FLOYD-FM-HANDOFF.md > "DSP Notes" > Envelopes:
//  "juce::ADSR is fine for audio, but it has no 'value at time t' query,
//  so the display needs its own evaluation function. THESE TWO MUST NOT
//  DRIFT. Put the envelope shape math in one shared header consumed by
//  both the voice and the editor."
//
//  Hence: no juce::ADSR anywhere in this plugin. The voice ticks this
//  same math, and the editor draws straight lines between the same
//  breakpoints. Five points, straight segments, hard corners -- no
//  curvature (handoff, "Envelope geometry").
//
//  Everything here is amplitude-NORMALISED (0..1). The caller multiplies
//  by op{n}_amp: "Amplitude scales the whole envelope: peak height is
//  amp, not 1.0."
// =====================================================================

namespace floyd
{

struct EnvParams
{
    float attack  = 0.0f;   // seconds, 0..5
    float decay   = 0.0f;   // seconds, 0..5
    float sustain = 0.0f;   // 0..1
    float release = 0.0f;   // seconds, 0..4
};

// Breakpoints in (time, value) space. Time for `end` is measured from
// the START OF RELEASE, not from note-on -- the display's release band
// is its own window.
struct Breakpoints
{
    float peakT,  peakV;    // (attack,          1)
    float susInT, susInV;   // (attack + decay,  sustain)
    float endT,   endV;     // (release,         0)
};

inline constexpr Breakpoints breakpointsOf (const EnvParams& p)
{
    return { p.attack,             1.0f,
             p.attack + p.decay,   p.sustain,
             p.release,            0.0f };
}

// Value while the note is held, t seconds after note-on.
inline constexpr float envHeldAt (const EnvParams& p, float t)
{
    if (t <= 0.0f)
        return 0.0f;

    if (t < p.attack)
        return p.attack <= 0.0f ? 1.0f : t / p.attack;

    if (t < p.attack + p.decay)
        return p.decay <= 0.0f ? p.sustain
                               : 1.0f - (1.0f - p.sustain) * ((t - p.attack) / p.decay);

    return p.sustain;
}

// Value during release, tRel seconds after note-off, decaying linearly
// from whatever level the envelope had reached.
inline constexpr float envReleaseAt (const EnvParams& p, float tRel, float levelAtRelease)
{
    if (p.release <= 0.0f || tRel >= p.release)
        return 0.0f;

    const float v = levelAtRelease * (1.0f - tRel / p.release);
    return v < 0.0f ? 0.0f : v;
}

// -----------------------------------------------------------------------
//  Sample-rate ticker. Holds only a time cursor, so it evaluates exactly
//  the functions above -- it cannot drift from the display by
//  construction.
// -----------------------------------------------------------------------
class Envelope
{
public:
    void prepare (double sampleRate) noexcept
    {
        secondsPerSample = (sampleRate > 0.0) ? (float) (1.0 / sampleRate) : 0.0f;
    }

    void noteOn (const EnvParams& p) noexcept
    {
        params          = p;
        t               = 0.0f;
        releasing       = false;
        levelAtRelease  = 0.0f;
        value           = 0.0f;
        finished        = false;
    }

    void noteOff() noexcept
    {
        if (releasing || finished)
            return;

        levelAtRelease = value;
        releasing      = true;
        t              = 0.0f;

        if (params.release <= 0.0f)
        {
            value    = 0.0f;
            finished = true;
        }
    }

    // Advance one sample and return the new value.
    float tick() noexcept
    {
        if (finished)
            return 0.0f;

        value = releasing ? envReleaseAt (params, t, levelAtRelease)
                          : envHeldAt   (params, t);

        t += secondsPerSample;

        if (releasing && t >= params.release)
            finished = true;

        return value;
    }

    bool isFinished() const noexcept { return finished; }
    bool isReleasing() const noexcept { return releasing; }
    float currentValue() const noexcept { return value; }

    void reset() noexcept
    {
        t = 0.0f; value = 0.0f; levelAtRelease = 0.0f;
        releasing = false; finished = true;
    }

private:
    EnvParams params;
    float secondsPerSample = 0.0f;
    float t                = 0.0f;
    float value            = 0.0f;
    float levelAtRelease   = 0.0f;
    bool  releasing        = false;
    bool  finished         = true;
};

} // namespace floyd
