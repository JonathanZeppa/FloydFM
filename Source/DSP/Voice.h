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
//    Note-on-only: envelope times and initial ratio resolve once at
//                  note-on, not per block.
//
//  VELOCITY -- SUPERSEDES THE HANDOFF (2026-08-01).
//  The handoff (and this plugin through v0.1.0) scaled MODULATOR amp
//  only, never carrier amp, from one global `velocity_amt`. Floyd asked
//  for velocity on the carriers as well -- "it makes the pianos and pads
//  come alive" -- and Jonathan chose the DX7/TX81Z model to deliver it:
//  velocity sensitivity is a PER-OPERATOR property of the patch, not a
//  property of the operator's current role in the algorithm.
//
//      s             = velocityAmount * op.velSens      // 0..1
//      effectiveAmp  = amp * (1 - s + s * velocity)
//
//  Same formula shape as before, in one place, now applied to every
//  operator. `velocity_amt` survives as the global master depth, so
//  op.velSens = 1 with the default 0.7 reproduces the old modulator
//  response exactly, and op.velSens = 0 reproduces the old carrier
//  response exactly (ORGAN sets all four to 0 for precisely that).
//
//  Role-independence is the point of the DX7 model: an operator that is
//  a carrier in ADDITIVE and a modulator in STACK keeps ONE sensitivity,
//  so changing algorithm can no longer silently change how hard the
//  patch responds to the keyboard.
//
//  PITCH BEND. The handoff never mentions the wheel; added 2026-08-01.
//  The processor resolves wheel position * bend_range into a frequency
//  MULTIPLIER once per block (one pow, not one per operator per voice)
//  and the voice folds it into frequencyFor alongside ratio and detune.
// =====================================================================

namespace floyd
{

struct OpParams
{
    EnvParams env;
    float     amp         = 0.0f;
    int       ratioIndex  = kDefaultRatioIndex;
    float     detuneCents = 0.0f;
    float     velSens     = 1.0f;   // per-operator velocity sensitivity, 0..1
};

struct PatchParams
{
    std::array<OpParams, kNumOps> op {};
    int   algorithmIndex = 0;
    float feedback       = 0.0f;
    float velocityAmount = 0.7f;    // global master depth over op.velSens
    float pitchBendFactor = 1.0f;   // frequency multiplier, 2^(semitones/12)
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
        bendFactor  = patch.pitchBendFactor;   // before any frequencyFor call

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

    // Per-block push. Amp, feedback, detune and pitch bend track live
    // edits; ratio and envelope times deliberately do not (handoff).
    // Also re-called mid-block on a pitch-wheel message, so the wheel
    // resolves at the event rather than at the next block boundary.
    void applyContinuous (const PatchParams& patch) noexcept
    {
        if (! active)
            return;

        bendFactor = patch.pitchBendFactor;

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
        return noteHz * heldRatio[(std::size_t) op] * detuneFactor * bendFactor;
    }

    // Per-operator velocity sensitivity, DX7-style: an operator's
    // response to the keyboard is a property of the PATCH, not of its
    // current role in the algorithm. One formula, every operator.
    //
    // The role test that used to live here is gone deliberately -- see
    // the header comment. It made an operator's velocity response change
    // whenever the algorithm changed, which is exactly what a
    // per-operator sensitivity exists to prevent.
    float effectiveAmp (int op, const PatchParams& patch) const noexcept
    {
        const auto& o = patch.op[(std::size_t) op];
        const float s = patch.velocityAmount * o.velSens;

        return o.amp * (1.0f - s + s * velocity);
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
    float bendFactor   = 1.0f;   // pitch-wheel frequency multiplier

    int onsetRampLength = 64;
    int onsetCountdown  = 0;
};

} // namespace floyd
