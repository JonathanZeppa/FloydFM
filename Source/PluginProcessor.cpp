#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "DSP/RatioTable.h"

using namespace floyd;

//======================================================================
//  Parameters
//======================================================================

juce::String FloydFMAudioProcessor::opParamId (int op, const char* suffix)
{
    return "op" + juce::String (op + 1) + "_" + suffix;
}

juce::AudioProcessorValueTreeState::ParameterLayout FloydFMAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // 19 snapped musical ratios, displayed to 2 decimals.
    juce::StringArray ratioChoices;
    for (int i = 0; i < kNumRatios; ++i)
        ratioChoices.add (juce::String (ratioAt (i), 2));

    juce::StringArray algorithmChoices;
    for (const auto& a : kAlgorithms)
        algorithmChoices.add ("ALG " + juce::String (a.display) + " " + juce::String (a.name));

    // Handoff defaults for the envelope/amp block are "see presets"; the
    // constructor cold-start-applies preset 001, so these mirror preset
    // 001 (GLASS BELL) to keep a raw-defaults instance and a freshly
    // loaded one consistent. Ratio/detune use the handoff's explicit
    // defaults (3 = 1.00, and 0.0 cents).
    const auto& init = presetAt (0);

    for (int n = 0; n < kNumOps; ++n)
    {
        const auto& o   = init.op[(std::size_t) n];
        const auto  tag = "OP" + juce::String (n + 1) + " ";

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { opParamId (n, "attack"), 1 }, tag + "Attack",
            juce::NormalisableRange<float> (0.0f, 5.0f, 0.001f), o.attack));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { opParamId (n, "decay"), 1 }, tag + "Decay",
            juce::NormalisableRange<float> (0.0f, 5.0f, 0.001f), o.decay));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { opParamId (n, "sustain"), 1 }, tag + "Sustain",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), o.sustain));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { opParamId (n, "release"), 1 }, tag + "Release",
            juce::NormalisableRange<float> (0.0f, 4.0f, 0.001f), o.release));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { opParamId (n, "amp"), 1 }, tag + "Amount",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), o.amp));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { opParamId (n, "ratio"), 1 }, tag + "Ratio",
            ratioChoices, kDefaultRatioIndex));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { opParamId (n, "detune"), 1 }, tag + "Detune",
            juce::NormalisableRange<float> (-50.0f, 50.0f, 0.1f), 0.0f));
    }

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "algorithm", 1 }, "Algorithm", algorithmChoices, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "feedback", 1 }, "Feedback",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "velocity_amt", 1 }, "Velocity Amount",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.7f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "master_level", 1 }, "Master",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));

    return layout;
}

//======================================================================
//  Construction
//======================================================================

FloydFMAudioProcessor::FloydFMAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FloydFM", createParameterLayout())
{
    cacheParameterPointers();

    // Cold-start apply (Phase 4 gate): without it a fresh instance READS
    // preset 001 but SOUNDS like the raw APVTS defaults.
    applyPreset (0);
}

void FloydFMAudioProcessor::cacheParameterPointers()
{
    for (int n = 0; n < kNumOps; ++n)
    {
        auto& p = opPtr[(std::size_t) n];
        p.attack  = apvts.getRawParameterValue (opParamId (n, "attack"));
        p.decay   = apvts.getRawParameterValue (opParamId (n, "decay"));
        p.sustain = apvts.getRawParameterValue (opParamId (n, "sustain"));
        p.release = apvts.getRawParameterValue (opParamId (n, "release"));
        p.amp     = apvts.getRawParameterValue (opParamId (n, "amp"));
        p.ratio   = apvts.getRawParameterValue (opParamId (n, "ratio"));
        p.detune  = apvts.getRawParameterValue (opParamId (n, "detune"));
    }

    algorithmPtr   = apvts.getRawParameterValue ("algorithm");
    feedbackPtr    = apvts.getRawParameterValue ("feedback");
    velocityAmtPtr = apvts.getRawParameterValue ("velocity_amt");
    masterLevelPtr = apvts.getRawParameterValue ("master_level");
}

//======================================================================
//  Presets
//======================================================================

void FloydFMAudioProcessor::applyPreset (int index)
{
    const auto& preset = presetAt (index);

    auto set = [this] (const juce::String& id, float plainValue)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
    };

    for (int n = 0; n < kNumOps; ++n)
    {
        const auto& o = preset.op[(std::size_t) n];
        set (opParamId (n, "attack"),  o.attack);
        set (opParamId (n, "decay"),   o.decay);
        set (opParamId (n, "sustain"), o.sustain);
        set (opParamId (n, "release"), o.release);
        set (opParamId (n, "amp"),     o.amp);
        set (opParamId (n, "detune"),  0.0f);

        // Snapped choice -- the mockup's literal ratio maps through one
        // documented rule (DISC-3).
        set (opParamId (n, "ratio"), (float) nearestRatioIndex (o.ratio));
    }

    set ("algorithm", (float) preset.algorithmIndex);

    needsReset.store (true);
}

void FloydFMAudioProcessor::setCurrentProgram (int index)
{
    index = juce::jlimit (0, kNumPresets - 1, index);

    // Single write path for the current-index atomic (Phase 4 gate).
    currentProgram.store (index);
    applyPreset (index);
    updateHostDisplay();
}

const juce::String FloydFMAudioProcessor::getProgramName (int index)
{
    return presetAt (index).name;
}

//======================================================================
//  Audio
//======================================================================

void FloydFMAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    voiceManager.prepare (sampleRate);
    needsReset.store (false);

    playheadNoteCount = 0;
    playheadTime      = 0.0f;
    playheadState.store ((int) PlayheadState::off);
    playheadElapsed.store (0.0f);
}

void FloydFMAudioProcessor::releaseResources()
{
    voiceManager.allNotesOff();
}

bool FloydFMAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet().isDisabled();
}

void FloydFMAudioProcessor::pushParameters()
{
    for (int n = 0; n < kNumOps; ++n)
    {
        const auto& src = opPtr[(std::size_t) n];
        auto&       dst = patch.op[(std::size_t) n];

        dst.env.attack  = src.attack->load();
        dst.env.decay   = src.decay->load();
        dst.env.sustain = src.sustain->load();
        dst.env.release = src.release->load();
        dst.amp         = src.amp->load();
        dst.ratioIndex  = (int) src.ratio->load();
        dst.detuneCents = src.detune->load();
    }

    patch.algorithmIndex = (int) algorithmPtr->load();
    patch.feedback       = feedbackPtr->load();
    patch.velocityAmount = velocityAmtPtr->load();

    // Algorithm changes are a routing change -- Pattern 3 says set
    // needsReset on those, never on continuous knob movement.
    if (patch.algorithmIndex != lastAlgorithm)
    {
        lastAlgorithm = patch.algorithmIndex;
        needsReset.store (true);
    }
}

void FloydFMAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();
    pushParameters();

    if (needsReset.exchange (false))
    {
        voiceManager.allNotesOff();
        playheadNoteCount = 0;
        playheadState.store ((int) PlayheadState::off);
    }

    voiceManager.applyContinuous (patch);

    const float masterLevel = masterLevelPtr->load();
    const int   numSamples  = buffer.getNumSamples();

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : left;

    int lastEventSample = 0;

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        const int  position = juce::jlimit (0, numSamples, metadata.samplePosition);
        const int  segment  = position - lastEventSample;

        if (segment > 0)
        {
            voiceManager.render (left + lastEventSample, right + lastEventSample,
                                 segment, masterLevel);
            updatePlayhead (segment);
            lastEventSample = position;
        }

        if (message.isNoteOn())
        {
            voiceManager.noteOn (message.getNoteNumber(), message.getFloatVelocity(), patch);

            // Playhead tracks the MOST RECENT note only (handoff) -- a
            // new note-on restarts the sweep.
            ++playheadNoteCount;
            playheadTime = 0.0f;
            playheadState.store ((int) PlayheadState::held);
        }
        else if (message.isNoteOff())
        {
            voiceManager.noteOff (message.getNoteNumber());

            if (playheadNoteCount > 0)
                --playheadNoteCount;

            if (playheadNoteCount == 0 && playheadState.load() == (int) PlayheadState::held)
            {
                playheadTime = 0.0f;
                playheadState.store ((int) PlayheadState::released);
            }
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            voiceManager.allNotesOff();
            playheadNoteCount = 0;
            playheadState.store ((int) PlayheadState::off);
        }
    }

    if (numSamples > lastEventSample)
    {
        voiceManager.render (left + lastEventSample, right + lastEventSample,
                             numSamples - lastEventSample, masterLevel);
        updatePlayhead (numSamples - lastEventSample);
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

// Playhead is driven by note state, not by parameter changes (handoff).
void FloydFMAudioProcessor::updatePlayhead (int numSamples)
{
    // HOLD button previews envelope timing without producing audio.
    const bool hold = holdPreview.load();

    if (hold != lastHoldPreview)
    {
        lastHoldPreview = hold;

        if (hold)
        {
            playheadTime = 0.0f;
            playheadState.store ((int) PlayheadState::held);
        }
        else if (playheadState.load() == (int) PlayheadState::held && playheadNoteCount == 0)
        {
            playheadTime = 0.0f;
            playheadState.store ((int) PlayheadState::released);
        }
    }

    const auto state = (PlayheadState) playheadState.load();
    if (state == PlayheadState::off)
        return;

    playheadTime += (float) (numSamples / currentSampleRate);

    if (state == PlayheadState::released)
    {
        float maxRelease = 0.0f;
        for (const auto& o : patch.op)
            maxRelease = juce::jmax (maxRelease, o.env.release);

        if (playheadTime >= maxRelease)
        {
            playheadState.store ((int) PlayheadState::off);
            playheadElapsed.store (0.0f);
            return;
        }
    }

    playheadElapsed.store (playheadTime);
}

//======================================================================
//  State
//======================================================================

juce::AudioProcessorEditor* FloydFMAudioProcessor::createEditor()
{
    return new FloydFMAudioProcessorEditor (*this);
}

void FloydFMAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("currentProgram", currentProgram.load(), nullptr);
    state.setProperty ("uiScale", uiScale.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void FloydFMAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);

    // Restore the preset INDEX but do NOT re-apply the preset -- the
    // restored APVTS state is the session's sound, edits included
    // (Phase 4 gate).
    if (state.hasProperty ("currentProgram"))
        currentProgram.store (juce::jlimit (0, kNumPresets - 1, (int) state["currentProgram"]));

    if (state.hasProperty ("uiScale"))
        uiScale.store (juce::jlimit (0.75f, 2.0f, (float) (double) state["uiScale"]));

    apvts.replaceState (state);

    // CG1: no AudioParameterBool params in this plugin, and
    // AudioParameterChoice auto-snaps in getValue(), so there is nothing
    // to snap. Kept as the documented hook for any bool added later.
    [[maybe_unused]] auto snapBool = [this] (const juce::String& id)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->getValue() > 0.5f ? 1.0f : 0.0f);
    };

    needsReset.store (true);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FloydFMAudioProcessor();
}
