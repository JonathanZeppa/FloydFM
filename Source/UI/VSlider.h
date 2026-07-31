#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

#include "FloydColours.h"
#include "FloydFonts.h"
#include "FloydLayout.h"
#include "TextHelpers.h"

// =====================================================================
//  VSlider -- FLOYD-FM-HANDOFF.md > "VSlider (54 x 407)".
//
//    Track: 10px wide, full height, border-radius 5px, #DFE7F3 @ 0.9,
//           centred horizontally.
//    Thumb: circle r = 14, rgba(0,0,0,0.55), stroke #FFFFFF, width 3.2.
//
//    "Deliberately the same visual language as the envelope drag
//     handles."
//
//    Vertical drag; click anywhere on the track jumps. Label above,
//    value below (2 decimals).
//
//  The component spans the full 54 x 407 block -- label (21) + track
//  (366) + value (20) -- so its three Layout rows stay in one place.
// =====================================================================

namespace floyd
{

class VSlider : public juce::Component
{
public:
    VSlider() = default;   // Pattern 11: MSVC needs this explicitly

    // Called with a normalised 0..1 position during drag.
    std::function<void (float)> onValueChange;

    void setLabel (const juce::String& newLabel)
    {
        if (label == newLabel)
            return;

        label = newLabel;
        repaint();
    }

    // `normalised` drives the thumb; `display` is the text shown below.
    void setValue (float normalised, const juce::String& display)
    {
        const auto n = juce::jlimit (0.0f, 1.0f, normalised);

        if (juce::approximatelyEqual (n, value) && display == valueText)
            return;

        value     = n;
        valueText = display;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto labelArea = juce::Rectangle<float> (0.0f, 0.0f,
                                                       (float) getWidth(),
                                                       (float) Layout::AMOUNT_LABEL_H);
        const auto valueArea = juce::Rectangle<float> (0.0f,
                                                       (float) (Layout::AMOUNT_LABEL_H + Layout::AMOUNT_TRACK_H),
                                                       (float) getWidth(),
                                                       (float) Layout::AMOUNT_VALUE_H);

        Text::drawTrackedCentred (g, label, Fonts::of (Fonts::sliderLabel),
                                  Colours::textPrimary, labelArea, Fonts::sliderLabel.tracking);

        const auto rail = railBounds();
        g.setColour (Colours::textPrimary.withAlpha (0.9f));
        g.fillRoundedRectangle (rail, Layout::SL_RAIL_W * 0.5f);

        const float cy = thumbCentreY();
        const float cx = (float) getWidth() * 0.5f;
        const auto  thumb = juce::Rectangle<float> (cx - Layout::SL_THUMB_R,
                                                    cy - Layout::SL_THUMB_R,
                                                    Layout::SL_THUMB_R * 2.0f,
                                                    Layout::SL_THUMB_R * 2.0f);

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (thumb);
        g.setColour (Colours::selected);
        g.drawEllipse (thumb.reduced (Layout::SL_STROKE * 0.5f), Layout::SL_STROKE);

        Text::drawTrackedCentred (g, valueText, Fonts::of (Fonts::sliderValue),
                                  Colours::textPrimary, valueArea, Fonts::sliderValue.tracking);
    }

    void mouseDown (const juce::MouseEvent& e) override { applyDrag (e); }
    void mouseDrag (const juce::MouseEvent& e) override { applyDrag (e); }

    juce::MouseCursor getMouseCursor() override
    {
        return juce::MouseCursor::PointingHandCursor;
    }

private:
    // The rail is inset by thumb radius + 4 at both ends so the thumb
    // never overhangs the track region (mockup: top = SL_THUMB + 4).
    float railTop() const
    {
        return (float) Layout::AMOUNT_LABEL_H + Layout::SL_THUMB_R + Layout::SL_THUMB_PAD;
    }

    float railHeight() const
    {
        return (float) Layout::AMOUNT_TRACK_H - 2.0f * (Layout::SL_THUMB_R + Layout::SL_THUMB_PAD);
    }

    juce::Rectangle<float> railBounds() const
    {
        return { ((float) getWidth() - Layout::SL_RAIL_W) * 0.5f, railTop(),
                 Layout::SL_RAIL_W, railHeight() };
    }

    float thumbCentreY() const
    {
        return railTop() + (1.0f - value) * railHeight();
    }

    void applyDrag (const juce::MouseEvent& e)
    {
        const float n = juce::jlimit (0.0f, 1.0f,
                                      1.0f - ((float) e.position.y - railTop()) / railHeight());

        if (onValueChange)
            onValueChange (n);
    }

    juce::String label     { "LEVEL" };
    juce::String valueText { "0.00" };
    float        value = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VSlider)
};

} // namespace floyd
