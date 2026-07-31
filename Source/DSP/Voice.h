#pragma once

#include <array>
#include <cmath>

#include "Algorithm.h"
#include "EnvelopeShape.h"
#include "Operator.h"
#include "RatioTable.h"

// =====================================================================
//  Voice.h -- one polyphonic voice: 4 sine operators routed by the
//  current algorithm.
//
//  Handoff, "DSP Notes":
//    Frequency  = noteHz * ratio * detuneFactor, detuneFactor = 2^(cents/1200)
//    Velocity   = scales MODULATOR amp only, never carrier amp:
//                 effectiveAmp = amp * (1 - velAmt + velAmt * vel)
//                 "Play harder -> brighter, not just louder."
//    Note-on-only: envelope times and initial ratio resolve once at
//                  note-on, not per block.
// =====================================================================

namespace floyd
{

struct OpParams
{
    EnvParams env;
    float     amp         = 0.0f;
    int       ratioIndex  = kDefaultRatioIndex;
    float     detuneCents = 0.0f;
};

struct PatchParams
{
    std::array<OpParams, kNumOps> op {};
    int   algorithmIndex = 0;
    float feedback       = 0.0f;
    float velocityAmount = 0.7f;
};

class Voice
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = (sampleRate > 0.0) ? sampleRate : 44100.0;

        for (auto& o : ops)
            o.prepare (sr);

        // ~1.5 ms onset ramp. Several factory presets have attack 0.00,
        // which steps from silence to full level in one sample.
        onsetRampLength = (int) (sr * 0.0015);
        if (onsetRampLength < 8) onsetRampLength = 8;

        reset();
    }

    void reset() noexcept
    {
        for (auto& o : ops)
            o.reset();

        active         = false;
        noteNumber     = -1;
        onsetCountdown = 0;
    }

    void noteOn (int midiNote, float velocity01, const PatchParams& patch, std::uint64_t stamp) noexcept
    {
        noteNumber   = midiNote;
        velocity     = velocity01;
        noteHz       = 440.0f * std::pow (2.0f, ((float) midiNote - 69.0f) / 12.0f);
        active       = true;
        released     = false;
        noteOnStamp  = stamp;
        releaseStamp = 0;

        algorithm   = &algorithmAt (patch.algorithmIndex);
        renderOrder = computeRenderOrder (*algorithm);

        // Carrier-count gain compensation. Carriers sum coherently, so
        // ALG 8 (four carriers) arrives ~2.7x hotter than ALG 1 (one) at
        // identical operator amounts. Left uncompensated this pushed the
        // master limiter into 6.8 dB of CONTINUOUS gain reduction, heard
        // as distortion while the host meter stayed clean -- the limiter
        // asymptotes at unity, so it can never light a clip indicator.
        //
        // sqrt(N), not N: the carriers run at different ratios and only
        // partially correlate, so power-summing level-matches the eight
        // algorithms far better than amplitude-summing, which would
        // over-attenuate the additive ones into near-silence.
        //
        // This is the systematic, per-voice component. Polyphony is the
        // other half and is genuinely unpredictable -- that half is what
        // the limiter is for.
        int carriers = 0;
        for (int i = 0; i < kNumOps; ++i)
            if (isCarrier (*algorithm, i))
                ++carriers;

        carrierScale = (carriers > 0) ? 1.0f / std::sqrt ((float) carriers) : 1.0f;

        for (int i = 0; i < kNumOps; ++i)
        {
            // Note-on-only: envelope times and the ratio snapshot.
            heldRatio[(std::size_t) i] = ratioAt (patch.op[(std::size_t) i].ratioIndex);

            ops[(std::size_t) i].noteOn (frequencyFor (i, patch.op[(std::size_t) i].detuneCents),
                                         patch.op[(std::size_t) i].env,
                                         effectiveAmp (i, patch));
        }

        ops[0].setFeedback (patch.feedback);
        onsetCountdown = onsetRampLength;
    }

    void noteOff() noexcept
    {
        if (! active || released)
            return;

        released = true;
        for (auto& o : ops)
            o.noteOff();
    }

    // Per-block push. Amp, feedback and detune track live edits; ratio
    // and envelope times deliberately do not (handoff).
    void applyContinuous (const PatchParams& patch) noexcept
    {
        if (! active)
            return;

        for (int i = 0; i < kNumOps; ++i)
        {
            ops[(std::size_t) i].setAmplitude (effectiveAmp (i, patch));
            ops[(std::size_t) i].setFrequency (frequencyFor (i, patch.op[(std::size_t) i].detuneCents));
        }

        ops[0].setFeedback (patch.feedback);
    }

    float renderSample() noexcept
    {
        if (! active)
            return 0.0f;

        std::array<float, kNumOps> outs {};

        // Deepest modulators first, so every destination's modulation
        // input is complete before it ticks -- zero-latency modulation.
        for (int slot = 0; slot < kNumOps; ++slot)
        {
            const int op = renderOrder.op[(std::size_t) slot];
            outs[(std::size_t) op] = ops[(std::size_t) op].tick();

            for (std::uint8_t e = 0; e < algorithm->edgeCount; ++e)
            {
                const auto& edge = algorithm->edges[e];
                if (edge.src == (std::uint8_t) op && edge.src != edge.dst)
                    ops[edge.dst].addModInput (outs[(std::size_t) op]);
            }
        }

        float out = 0.0f;
        for (int i = 0; i < kNumOps; ++i)
            if (isCarrier (*algorithm, i))
                out += outs[(std::size_t) i];

        out *= carrierScale;

        if (onsetCountdown > 0)
        {
            const float g = 1.0f - ((float) onsetCountdown / (float) onsetRampLength);
            out *= g;
            --onsetCountdown;
        }

        if (allCarriersFinished())
            active = false;

        return out;
    }

    bool isActive() const noexcept   { return active; }
    bool isReleased() const noexcept { return released; }
    int  getNote() const noexcept    { return noteNumber; }
    std::uint64_t getNoteOnStamp() const noexcept  { return noteOnStamp; }
    std::uint64_t getReleaseStamp() const noexcept { return releaseStamp; }
    void setReleaseStamp (std::uint64_t s) noexcept { releaseStamp = s; }

private:
    float frequencyFor (int op, float detuneCents) const noexcept
    {
        const float detuneFactor = std::pow (2.0f, detuneCents / 1200.0f);
        return noteHz * heldRatio[(std::size_t) op] * detuneFactor;
    }

    // Velocity scales MODULATOR amp only -- never carrier amp.
    // Uses the voice's OWN algorithm (snapshotted at note-on), not the
    // live patch index, so a mid-note algorithm change cannot make the
    // amp role and the routing disagree for one block.
    float effectiveAmp (int op, const PatchParams& patch) const noexcept
    {
        const float amp = patch.op[(std::size_t) op].amp;

        if (isCarrier (*algorithm, op))
            return amp;

        const float v = patch.velocityAmount;
        return amp * (1.0f - v + v * velocity);
    }

    bool allCarriersFinished() const noexcept
    {
        for (int i = 0; i < kNumOps; ++i)
            if (isCarrier (*algorithm, i) && ! ops[(std::size_t) i].isFinished())
                return false;

        return true;
    }

    double sr = 44100.0;

    std::array<Operator, kNumOps> ops;
    std::array<float, kNumOps>    heldRatio { { 1.0f, 1.0f, 1.0f, 1.0f } };

    const Algorithm* algorithm = &kAlgorithms[0];
    RenderOrder      renderOrder = computeRenderOrder (kAlgorithms[0]);

    bool  active   = false;
    bool  released = false;
    int   noteNumber = -1;
    float noteHz     = 440.0f;
    float velocity   = 1.0f;

    std::uint64_t noteOnStamp  = 0;
    std::uint64_t releaseStamp = 0;

    float carrierScale = 1.0f;   // 1/sqrt(carrier count), set at note-on

    int onsetRampLength = 64;
    int onsetCountdown  = 0;
};

} // namespace floyd
