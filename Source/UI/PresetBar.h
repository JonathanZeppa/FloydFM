#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Controls.h"
#include "FloydColours.h"
#include "FloydFonts.h"
#include "FloydLayout.h"
#include "TextHelpers.h"

// =====================================================================
//  PresetBar -- FLOYD-FM-HANDOFF.md > "PresetBar (992 x 40)".
//
//    "Left to right: < nudge, preset name display, > nudge, counter,
//     then right-aligned plugin name and credit."
//
//    Display: 210px min width, #0E1218, 1px #2B3040 border, VT323 17px
//    #9FD6C8, 2px letter-spacing, centred.
//    Counter: n/10, VT323 13px #3E3E46.
//    Nudge buttons wrap around at both ends.
//
//    "No save button. No dirty indicator." Factory presets only.
//
//  The right-hand side carries TWO spans, following the canonical
//  mockup: the live OP role readout, then the attribution. The mockup's
//  own credit string ("gradient deferred -- concept: Floyd Steinberg")
//  and the handoff's ("ZEDLAB FM -- ...") are both stale -- the gradient
//  note is a mockup-era artefact and the plugin has been renamed -- so
//  the attribution is composed by the editor from the live plugin name.
//  Logged as DISC-2.
//
//  ASCII only: "--", never an em dash. VT323 has no em dash glyph.
// =====================================================================

namespace floyd
{

class PresetBar : public juce::Component
{
public:
    PresetBar()
    {
        addAndMakeVisible (prev);
        addAndMakeVisible (next);
        addAndMakeVisible (builtBy);

        builtBy.setLink ("Built By Zedtronics LLC", "https://zedtronics.net",
                         Fonts::faint, Colours::textFaint);
    }

    std::function<void()> onPrev;
    std::function<void()> onNext;

    void setPreset (const juce::String& name, int index, int total)
    {
        presetName  = name;
        presetIndex = index;
        presetTotal = total;
        repaint();
    }

    // "OP3 CARRIER" / "OP1 MODULATOR" -- explains whether the left
    // slider currently reads LEVEL or DEPTH.
    void setRoleText (const juce::String& text)
    {
        if (roleText == text)
            return;

        roleText = text;
        repaint();
    }

    void setCreditText (const juce::String& text)
    {
        creditText = text;
        repaint();
    }

    void resized() override
    {
        // Nudges centre on the display row, which now sits below the
        // "PRESET" caption rather than in the middle of the bar.
        const int rowH = getHeight() - Layout::PRESET_ROW_Y;
        const int y    = Layout::PRESET_ROW_Y + (rowH - Layout::NUDGE_H) / 2;

        prev.setBounds (Layout::PRESET_PREV_X, y, Layout::NUDGE_W, Layout::NUDGE_H);
        next.setBounds (Layout::PRESET_NEXT_X, y, Layout::NUDGE_W, Layout::NUDGE_H);

        prev.onClick = [this] { if (onPrev) onPrev(); };
        next.onClick = [this] { if (onNext) onNext(); };

        // Rightmost element in the bar: the Zedtronics link, aligned to
        // the display row rather than the whole bar.
        const int w = builtBy.preferredWidth();
        builtBy.setBounds (getWidth() - w, Layout::PRESET_ROW_Y, w, rowH);
    }

    void paint (juce::Graphics& g) override
    {
        const float h    = (float) getHeight();
        const float rowY = (float) Layout::PRESET_ROW_Y;
        const float rowH = h - rowY;

        // "PRESET" caption, left-aligned over the display.
        Text::drawTracked (g, "PRESET", Fonts::of (Fonts::caption), Colours::textDim,
                           (float) Layout::PRESET_DISPLAY_X, rowY * 0.5f,
                           Fonts::caption.tracking);

        // Preset name display -- LCD inset.
        const auto display = juce::Rectangle<float> ((float) Layout::PRESET_DISPLAY_X,
                                                     rowY + (rowH - (float) Layout::PRESET_DISPLAY_H) * 0.5f,
                                                     (float) Layout::PRESET_DISPLAY_W,
                                                     (float) Layout::PRESET_DISPLAY_H);

        g.setColour (Colours::lcdBackground);
        g.fillRect (display);
        g.setColour (Colours::border);
        g.drawRect (display, 1.0f);

        Text::drawTrackedCentred (g, presetName, Fonts::of (Fonts::presetName),
                                  Colours::lcdText, display, Fonts::presetName.tracking);

        // Counter: n/10.
        const auto faint = Fonts::of (Fonts::faint);

        Text::drawTracked (g, juce::String (presetIndex + 1) + "/" + juce::String (presetTotal),
                           faint, Colours::textFaint,
                           (float) Layout::PRESET_COUNTER_X, display.getCentreY(),
                           Fonts::faint.tracking);

        // Right-hand run, laid out right to left:
        //   [OP role]  [Floyd attribution]  [Built By Zedtronics LLC]
        // The Zedtronics link is a child component (it needs hit-testing),
        // so the painted spans stop where its bounds begin.
        float rightEdge = (float) builtBy.getX() - (float) Layout::PRESET_RIGHT_GAP;
        const float textY = display.getCentreY();

        const float creditW = Text::trackedWidth (faint, creditText, Fonts::faint.tracking);
        Text::drawTracked (g, creditText, faint, Colours::textFaint,
                           rightEdge - creditW, textY, Fonts::faint.tracking);

        rightEdge -= creditW + (float) Layout::PRESET_RIGHT_GAP;

        const float roleW = Text::trackedWidth (faint, roleText, Fonts::faint.tracking);
        Text::drawTracked (g, roleText, faint, Colours::textFaint,
                           rightEdge - roleW, textY, Fonts::faint.tracking);
    }

private:
    NudgeButton prev { false };
    NudgeButton next { true };
    LinkLabel   builtBy;

    juce::String presetName { "GLASS BELL" };
    juce::String roleText;
    juce::String creditText;
    int presetIndex = 0;
    int presetTotal = 10;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBar)
};

} // namespace floyd
