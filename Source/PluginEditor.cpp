#include "PluginEditor.h"

#include "DSP/Presets.h"
#include "DSP/RatioTable.h"
#include "UI/FloydColours.h"
#include "UI/FloydFonts.h"
#include "UI/FloydLayout.h"
#include "UI/TextHelpers.h"

namespace L = floyd::Layout;
namespace C = floyd::Colours;
namespace F = floyd::Fonts;
namespace T = floyd::Text;

using floyd::kNumOps;

FloydFMAudioProcessorEditor::FloydFMAudioProcessorEditor (FloydFMAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processorRef (p)
{
    addAndMakeVisible (rootContainer);

    for (int i = 0; i < kNumOps; ++i)
    {
        auto tab = std::make_unique<floyd::OperatorTab> (i);
        tab->setSelected (i == selectedOperator);
        tab->onSelect = [this] (int op) { selectOperator (op); };
        rootContainer.addAndMakeVisible (*tab);
        operatorTabs[(std::size_t) i] = std::move (tab);
    }

    rootContainer.addAndMakeVisible (envelopeCanvas);
    rootContainer.addAndMakeVisible (routingDiagram);
    rootContainer.addAndMakeVisible (amountSlider);
    rootContainer.addAndMakeVisible (ratioSlider);
    rootContainer.addAndMakeVisible (holdButton);
    rootContainer.addAndMakeVisible (algPrev);
    rootContainer.addAndMakeVisible (algNext);
    rootContainer.addAndMakeVisible (presetBar);

    // --- envelope drag handles -> parameters --------------------------
    envelopeCanvas.onPeakDrag = [this] (float attack, float decay, float amp)
    {
        const int n = selectedOperator;
        setParam (FloydFMAudioProcessor::opParamId (n, "attack"), attack);
        setParam (FloydFMAudioProcessor::opParamId (n, "decay"),  decay);
        setParam (FloydFMAudioProcessor::opParamId (n, "amp"),    amp);
        refreshFromParameters();
    };

    envelopeCanvas.onSusInDrag = [this] (float decay, float sustain)
    {
        const int n = selectedOperator;
        setParam (FloydFMAudioProcessor::opParamId (n, "decay"),   decay);
        setParam (FloydFMAudioProcessor::opParamId (n, "sustain"), sustain);
        refreshFromParameters();
    };

    envelopeCanvas.onEndDrag = [this] (float release)
    {
        setParam (FloydFMAudioProcessor::opParamId (selectedOperator, "release"), release);
        refreshFromParameters();
    };

    // --- sliders ------------------------------------------------------
    // The Amount slider and the peak handle's Y are two views of the
    // same parameter -- moving either updates the other (handoff).
    amountSlider.onValueChange = [this] (float normalised)
    {
        setParam (FloydFMAudioProcessor::opParamId (selectedOperator, "amp"), normalised);
        refreshFromParameters();
    };

    ratioSlider.onValueChange = [this] (float normalised)
    {
        const int index = juce::jlimit (0, floyd::kNumRatios - 1,
                                        juce::roundToInt (normalised * (float) (floyd::kNumRatios - 1)));
        setParam (FloydFMAudioProcessor::opParamId (selectedOperator, "ratio"), (float) index);
        refreshFromParameters();
    };

    // --- buttons ------------------------------------------------------
    algPrev.onClick = [this] { nudgeAlgorithm (-1); };
    algNext.onClick = [this] { nudgeAlgorithm (1); };

    presetBar.onPrev = [this] { nudgePreset (-1); };
    presetBar.onNext = [this] { nudgePreset (1); };

    holdButton.onHoldChanged = [this] (bool held)
    {
        processorRef.holdPreview.store (held);
        holdButton.setActive (held);
    };

    presetBar.setCreditText (juce::String (JucePlugin_Name).toUpperCase()
                             + " -- interface concept: Floyd Steinberg");

    // Seed the preset display here rather than leaving it to the first
    // timer tick -- otherwise an editor opened on preset 10 shows preset
    // 1's name for the first 33 ms, and a headless snapshot shows it
    // forever.
    lastProgram = processorRef.getCurrentProgram();
    presetBar.setPreset (processorRef.getProgramName (lastProgram),
                         lastProgram, floyd::kNumPresets);

    // Same reason: an editor opened while a note is already sounding
    // would show no playhead until the first timer tick, and a headless
    // snapshot would show none at all.
    envelopeCanvas.setPlayhead (processorRef.playheadState.load(),
                                processorRef.playheadElapsed.load());

    // Drag-to-zoom: corner drag, aspect locked, 0.75x to 2.0x, scale
    // persisted in the processor so it survives a DAW session reload.
    constrainer.setFixedAspectRatio ((double) L::PANEL_W / (double) L::PANEL_H);
    constrainer.setSizeLimits ((int) (L::PANEL_W * kMinScale), (int) (L::PANEL_H * kMinScale),
                               (int) (L::PANEL_W * kMaxScale), (int) (L::PANEL_H * kMaxScale));
    setConstrainer (&constrainer);
    setResizable (true, true);

    const float scale = juce::jlimit (kMinScale, kMaxScale, processorRef.uiScale.load());
    setSize ((int) (L::PANEL_W * scale), (int) (L::PANEL_H * scale));

    // Arrow-key navigation. Only the four arrows are consumed; every
    // other key returns false so the DAW keeps its transport shortcuts.
    setWantsKeyboardFocus (true);

    refreshFromParameters();
    startTimerHz (30);
}

// Left/right steps presets, up/down steps algorithms. Both wrap in both
// directions -- CIRCULAR is the fleet requirement, and it matches the
// on-screen nudge buttons, which already wrap.
bool FloydFMAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::leftKey)  { nudgePreset (-1);   return true; }
    if (key == juce::KeyPress::rightKey) { nudgePreset (1);    return true; }
    if (key == juce::KeyPress::upKey)    { nudgeAlgorithm (1); return true; }
    if (key == juce::KeyPress::downKey)  { nudgeAlgorithm (-1);return true; }

    return false;
}

FloydFMAudioProcessorEditor::~FloydFMAudioProcessorEditor()
{
    stopTimer();

    // CG17: clear every callback lambda that captured `this` before the
    // editor dies.
    for (auto& tab : operatorTabs)
        if (tab != nullptr)
            tab->onSelect = nullptr;

    envelopeCanvas.onPeakDrag  = nullptr;
    envelopeCanvas.onSusInDrag = nullptr;
    envelopeCanvas.onEndDrag   = nullptr;
    amountSlider.onValueChange = nullptr;
    ratioSlider.onValueChange  = nullptr;
    algPrev.onClick            = nullptr;
    algNext.onClick            = nullptr;
    presetBar.onPrev           = nullptr;
    presetBar.onNext           = nullptr;
    holdButton.onHoldChanged   = nullptr;

    processorRef.holdPreview.store (false);
}

//======================================================================
//  Parameter access
//======================================================================

void FloydFMAudioProcessorEditor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = processorRef.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

float FloydFMAudioProcessorEditor::getParam (const juce::String& id) const
{
    if (auto* v = processorRef.apvts.getRawParameterValue (id))
        return v->load();

    return 0.0f;
}

bool FloydFMAudioProcessorEditor::selectedOperatorIsCarrier() const
{
    const int algIndex = (int) getParam ("algorithm");
    return floyd::isCarrier (floyd::algorithmAt (algIndex), selectedOperator);
}

//======================================================================
//  Sync
//======================================================================

void FloydFMAudioProcessorEditor::selectOperator (int op)
{
    if (op == selectedOperator)
        return;

    selectedOperator = op;

    for (int i = 0; i < kNumOps; ++i)
        operatorTabs[(std::size_t) i]->setSelected (i == op);

    envelopeCanvas.setSelectedOperator (op);
    routingDiagram.setSelectedOperator (op);
    refreshFromParameters();
}

void FloydFMAudioProcessorEditor::refreshFromParameters()
{
    std::array<floyd::EnvelopeCanvas::OpDisplay, kNumOps> display {};

    for (int i = 0; i < kNumOps; ++i)
    {
        auto& d = display[(std::size_t) i];
        d.attack  = getParam (FloydFMAudioProcessor::opParamId (i, "attack"));
        d.decay   = getParam (FloydFMAudioProcessor::opParamId (i, "decay"));
        d.sustain = getParam (FloydFMAudioProcessor::opParamId (i, "sustain"));
        d.release = getParam (FloydFMAudioProcessor::opParamId (i, "release"));
        d.amp     = getParam (FloydFMAudioProcessor::opParamId (i, "amp"));
    }

    envelopeCanvas.setOps (display);

    const int   algIndex = (int) getParam ("algorithm");
    const auto& alg      = floyd::algorithmAt (algIndex);
    routingDiagram.setAlgorithm (algIndex);

    // The canvas needs the routing too -- the brightness ramp is drawn
    // on carriers, from the operators feeding them.
    envelopeCanvas.setAlgorithm (algIndex);

    // Role labels under the tabs. These change with the algorithm, not
    // with selection -- an operator's role is a property of the routing.
    for (int i = 0; i < kNumOps; ++i)
        operatorTabs[(std::size_t) i]->setCarrier (floyd::isCarrier (alg, i));

    // op{n}_amp is one parameter with two roles: modulation depth when
    // the operator is a modulator in the current algorithm, output level
    // when it is a carrier. The label is driven by the algorithm, not
    // stored (handoff).
    const bool carrier = selectedOperatorIsCarrier();
    amountSlider.setLabel (carrier ? "LEVEL" : "DEPTH");

    const float amp = display[(std::size_t) selectedOperator].amp;
    amountSlider.setValue (amp, juce::String (amp, 2));

    const int   ratioIndex = (int) getParam (FloydFMAudioProcessor::opParamId (selectedOperator, "ratio"));
    const float ratioNorm  = (float) ratioIndex / (float) (floyd::kNumRatios - 1);
    ratioSlider.setLabel ("RATIO");
    ratioSlider.setValue (ratioNorm, juce::String (floyd::ratioAt (ratioIndex), 2));

    presetBar.setRoleText ("OP" + juce::String (selectedOperator + 1)
                           + (carrier ? " CARRIER" : " MODULATOR"));

    if (algIndex != lastAlgorithm)
    {
        lastAlgorithm = algIndex;
        rootContainer.setAlgorithm (algIndex);
    }
}

void FloydFMAudioProcessorEditor::timerCallback()
{
    refreshFromParameters();

    envelopeCanvas.setPlayhead (processorRef.playheadState.load(),
                                processorRef.playheadElapsed.load());

    // CG11: the DAW can change preset with the editor open.
    const int program = processorRef.getCurrentProgram();
    if (program != lastProgram)
    {
        lastProgram = program;
        presetBar.setPreset (processorRef.getProgramName (program), program, floyd::kNumPresets);
    }
}

void FloydFMAudioProcessorEditor::nudgeAlgorithm (int delta)
{
    const int current = (int) getParam ("algorithm");
    const int next    = (current + delta + floyd::kNumAlgorithms) % floyd::kNumAlgorithms;

    setParam ("algorithm", (float) next);
    refreshFromParameters();
}

void FloydFMAudioProcessorEditor::nudgePreset (int delta)
{
    const int current = processorRef.getCurrentProgram();
    const int next    = (current + delta + floyd::kNumPresets) % floyd::kNumPresets;

    processorRef.setCurrentProgram (next);
    refreshFromParameters();
}

//======================================================================
//  Paint / layout
//======================================================================

// The editor itself only fills behind the scaled panel -- everything
// else lives in rootContainer so it scales as a unit.
void FloydFMAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (C::background);
}

void FloydFMAudioProcessorEditor::PanelRoot::setAlgorithm (int index)
{
    if (algorithmIndex == index)
        return;

    algorithmIndex = index;
    repaint();
}

void FloydFMAudioProcessorEditor::PanelRoot::paint (juce::Graphics& g)
{
    g.fillAll (C::background);

    const auto& alg = floyd::algorithmAt (algorithmIndex);

    // "ALGORITHM" section caption, over the routing diagram.
    T::drawTrackedCentred (g, "ALGORITHM", F::of (F::caption), C::textDim,
                           juce::Rectangle<int> (L::ALG_CAPTION_X, L::ALG_CAPTION_Y,
                                                 L::ALG_CAPTION_W, L::ALG_CAPTION_H).toFloat(),
                           F::caption.tracking);

    // ALG number, centred between the two nudge buttons.
    const auto numberArea = juce::Rectangle<float> ((float) (L::ALG_NUDGE_X + L::NUDGE_W),
                                                    (float) L::ALG_NUDGE_Y,
                                                    (float) (L::ALG_NUDGE_W - 2 * L::NUDGE_W),
                                                    (float) L::ALG_NUDGE_H);

    T::drawTrackedCentred (g, "ALG " + juce::String (alg.display), F::of (F::algNumber),
                           C::textPrimary, numberArea, F::algNumber.tracking);

    T::drawTrackedCentred (g, juce::String (alg.name), F::of (F::algName), C::textSecondary,
                           juce::Rectangle<int> (L::ALG_NAME_X, L::ALG_NAME_Y,
                                                 L::ALG_NAME_W, L::ALG_NAME_H).toFloat(),
                           F::algName.tracking);

    T::drawTrackedCentred (g, juce::String (alg.label), F::of (F::faint), C::textDim,
                           juce::Rectangle<int> (L::ALG_LABEL_X, L::ALG_LABEL_Y,
                                                 L::ALG_LABEL_W, L::ALG_LABEL_H).toFloat(),
                           F::faint.tracking);
}

void FloydFMAudioProcessorEditor::resized()
{
    // Scale the whole panel as a unit. Children keep design coordinates.
    const float scale = (float) getWidth() / (float) L::PANEL_W;

    rootContainer.setTransform ({});
    rootContainer.setBounds (0, 0, L::PANEL_W, L::PANEL_H);
    rootContainer.setTransform (juce::AffineTransform::scale (scale));

    processorRef.uiScale.store (scale);

    for (int i = 0; i < kNumOps; ++i)
        operatorTabs[(std::size_t) i]->setPillBounds (L::TAB_X[i], L::TAB_Y, L::TAB_W, L::TAB_H);

    algPrev.setBounds (L::ALG_PREV_X, L::ALG_NUDGE_Y, L::NUDGE_W, L::NUDGE_H);
    algNext.setBounds (L::ALG_NEXT_X, L::ALG_NUDGE_Y, L::NUDGE_W, L::NUDGE_H);

    routingDiagram.setBounds (L::ALG_DIAGRAM_X, L::ALG_DIAGRAM_Y,
                              L::ALG_DIAGRAM_W, L::ALG_DIAGRAM_H);

    envelopeCanvas.setBounds (L::CANVAS_X, L::CANVAS_Y, L::CANVAS_W, L::CANVAS_H);

    // Each VSlider spans its label + track + value rows as one block.
    const int sliderH = L::AMOUNT_LABEL_H + L::AMOUNT_TRACK_H + L::AMOUNT_VALUE_H;
    amountSlider.setBounds (L::AMOUNT_LABEL_X, L::AMOUNT_LABEL_Y, L::AMOUNT_LABEL_W, sliderH);
    ratioSlider .setBounds (L::RATIO_LABEL_X,  L::RATIO_LABEL_Y,  L::RATIO_LABEL_W,  sliderH);

    holdButton.setBounds (L::HOLD_X, L::HOLD_Y, L::HOLD_W, L::HOLD_H);

    presetBar.setBounds (L::PRESET_BAR_X, L::PRESET_BAR_Y, L::PRESET_BAR_W, L::PRESET_BAR_H);
}
