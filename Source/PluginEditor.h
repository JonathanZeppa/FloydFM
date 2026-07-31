#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

#include "UI/Controls.h"
#include "UI/EnvelopeCanvas.h"
#include "UI/OperatorTab.h"
#include "UI/PresetBar.h"
#include "UI/RoutingDiagram.h"
#include "UI/VSlider.h"

// =====================================================================
//  FloydFM editor.
//
//  RESIZE. The handoff fixed v1 at 1024 x 600 but required that
//  "component dimensions [be written] as constants rather than literals
//  so resize can be added without a rewrite" -- which is what made this
//  a drop-in addition rather than a refactor.
//
//  Scaling is by AffineTransform on a root container, NOT Pattern 1
//  localScale. Every control here draws tracked text glyph-by-glyph, and
//  JUCE's renderer rasterises glyphs at output resolution even through a
//  transform, so type stays sharp at every zoom without threading a
//  localScale through a dozen paint() methods. The usual AffineTransform
//  caveat -- PopupMenu and ComboBox spawn outside the transform and need
//  a LookAndFeel uiScale -- does not apply: this plugin has neither.
//
//  Two-way sync (Pattern 2) is a 30 Hz timer poll rather than
//  SliderAttachments: the controls are custom, the same parameter is
//  edited from two views (peak handle Y and the Amount slider are two
//  views of op{n}_amp), and the timer is needed for the playhead anyway.
// =====================================================================

class FloydFMAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit FloydFMAudioProcessorEditor (FloydFMAudioProcessor&);
    ~FloydFMAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr float kMinScale = 0.75f;
    static constexpr float kMaxScale = 2.0f;

private:
    // Holds every control at design coordinates (1024 x 600) and is
    // scaled as a unit. Also paints the panel background and the
    // algorithm column's text, which are not owned by any child.
    class PanelRoot : public juce::Component
    {
    public:
        PanelRoot() = default;
        void setAlgorithm (int index);
        void paint (juce::Graphics&) override;
    private:
        int algorithmIndex = 0;
    };

    void timerCallback() override;

    void selectOperator (int op);
    void refreshFromParameters();
    void nudgeAlgorithm (int delta);
    void nudgePreset (int delta);

    void setParam (const juce::String& id, float plainValue);
    float getParam (const juce::String& id) const;

    bool selectedOperatorIsCarrier() const;

    FloydFMAudioProcessor& processorRef;

    PanelRoot rootContainer;
    juce::ComponentBoundsConstrainer constrainer;

    std::array<std::unique_ptr<floyd::OperatorTab>, floyd::kNumOps> operatorTabs;
    floyd::EnvelopeCanvas envelopeCanvas;
    floyd::RoutingDiagram routingDiagram;
    floyd::VSlider        amountSlider;
    floyd::VSlider        ratioSlider;
    floyd::HoldButton     holdButton;
    floyd::NudgeButton    algPrev { false };
    floyd::NudgeButton    algNext { true };
    floyd::PresetBar      presetBar;

    // UI-only state -- the handoff is explicit that operator selection
    // never reaches the DSP.
    int selectedOperator = 3;   // mockup initial state: OP4

    int lastAlgorithm = -1;
    int lastProgram   = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FloydFMAudioProcessorEditor)
};
