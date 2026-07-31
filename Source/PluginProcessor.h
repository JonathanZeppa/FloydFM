#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "DSP/Algorithm.h"
#include "DSP/Presets.h"
#include "DSP/VoiceManager.h"

// =====================================================================
//  FloydFM -- 4-operator FM synthesizer.
//  Spec: FLOYD-FM-HANDOFF.md. Canonical mockup:
//  zedlab-fm-brightness-v11.jsx.
//
//  Signal flow (handoff, "DSP Notes"):
//    MIDI note -> Voice (x8) -> 4 sine operators routed by algorithm
//              -> carrier sum -> master level -> soft-clip limiter -> out
// =====================================================================

class FloydFMAudioProcessor : public juce::AudioProcessor
{
public:
    FloydFMAudioProcessor();
    ~FloydFMAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }   // max release

    // Factory presets only -- no user save (free tier).
    int getNumPrograms() override { return floyd::kNumPresets; }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- UI-facing -----------------------------------------------------
    juce::AudioProcessorValueTreeState apvts;

    std::atomic<bool> needsReset { false };

    // Playhead, driven by the audio thread and read by a 30 Hz UI timer.
    // CG17: lock-free atomics only -- the audio thread never calls back
    // into the editor.
    enum class PlayheadState { off = 0, held = 1, released = 2 };
    std::atomic<int>   playheadState   { (int) PlayheadState::off };
    std::atomic<float> playheadElapsed { 0.0f };   // seconds in the current state

    // HOLD button: previews envelope timing by driving the playhead only.
    std::atomic<bool> holdPreview { false };

    // Editor zoom, persisted with the session state.
    std::atomic<float> uiScale { 1.0f };

    static juce::String opParamId (int op, const char* suffix);

    // Diagnostics: peak level reaching the master limiter's input. A
    // value well above kSoftKneeThreshold means the limiter is
    // compressing continuously -- audible as distortion while the host
    // meter stays clean, because the limiter asymptotes at unity.
    float getPreLimiterPeak() const noexcept { return voiceManager.getPreLimiterPeak(); }
    void  resetPreLimiterPeak() noexcept     { voiceManager.resetPreLimiterPeak(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void cacheParameterPointers();
    void pushParameters();
    void applyPreset (int index);
    void updatePlayhead (int numSamples);

    floyd::VoiceManager voiceManager;
    floyd::PatchParams  patch;

    // Cached raw pointers -- resolved once in the constructor. Looking
    // parameters up by name per block (and building juce::Strings to do
    // it) is an allocation risk on the audio thread.
    struct OpPointers
    {
        std::atomic<float>* attack  = nullptr;
        std::atomic<float>* decay   = nullptr;
        std::atomic<float>* sustain = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* amp     = nullptr;
        std::atomic<float>* ratio   = nullptr;
        std::atomic<float>* detune  = nullptr;
    };

    std::array<OpPointers, floyd::kNumOps> opPtr;
    std::atomic<float>* algorithmPtr   = nullptr;
    std::atomic<float>* feedbackPtr    = nullptr;
    std::atomic<float>* velocityAmtPtr = nullptr;
    std::atomic<float>* masterLevelPtr = nullptr;

    std::atomic<int> currentProgram { 0 };
    int  lastAlgorithm  = -1;
    double currentSampleRate = 44100.0;

    // Playhead bookkeeping (audio thread only).
    int    playheadNoteCount = 0;
    float  playheadTime      = 0.0f;
    bool   lastHoldPreview   = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FloydFMAudioProcessor)
};
