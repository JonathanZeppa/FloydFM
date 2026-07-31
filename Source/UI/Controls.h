#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

#include "FloydColours.h"
#include "FloydFonts.h"
#include "FloydLayout.h"
#include "TextHelpers.h"

// =====================================================================
//  Controls.h -- NudgeButton and HoldButton.
//
//  NudgeButton is used by both the ALG row and the preset bar. Handoff,
//  PresetBar: "Nudge buttons: transparent, border: 1px solid #2B3040,
//  #8E96A8 text, wrap around at both ends."
//
//  The glyphs are ASCII '<' and '>' drawn as TEXT, not juce::Path
//  triangles. Unicode arrow glyphs are avoided as a rule, because most
//  Google fonts lack them -- but '<' and '>' are plain ASCII, the font
//  has them, and the mockup renders exactly those characters. Drawing
//  triangles here would be a deliberate departure from the mockup.
// =====================================================================

namespace floyd
{

class NudgeButton : public juce::Component
{
public:
    explicit NudgeButton (bool pointsRight) : right (pointsRight) {}

    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        g.setColour (Colours::border);
        g.drawRect (b, 1.0f);

        const auto ink = down ? Colours::textPrimary : Colours::nudgeText;
        Text::drawTrackedCentred (g, right ? ">" : "<", Fonts::of (Fonts::nudgeGlyph),
                                  ink, b, Fonts::nudgeGlyph.tracking);
    }

    void mouseDown (const juce::MouseEvent&) override { down = true;  repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        down = false;
        repaint();

        if (getLocalBounds().contains (e.getPosition()) && onClick)
            onClick();
    }

private:
    const bool right;
    bool down = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NudgeButton)
};

// ---------------------------------------------------------------------
//  LinkLabel -- tracked text that opens a URL.
//
//  Drawn glyph-by-glyph like every other tracked string here, so it needs
//  to be a real Component (not painted text) purely to get hit-testing
//  and a hand cursor. Underlines on hover.
// ---------------------------------------------------------------------
class LinkLabel : public juce::Component
{
public:
    LinkLabel() = default;   // Pattern 11

    void setLink (const juce::String& newText, const juce::String& newUrl,
                  Fonts::Spec newSpec, juce::Colour newColour)
    {
        text   = newText;
        url    = newUrl;
        spec   = newSpec;
        colour = newColour;
        repaint();
    }

    // Width this label needs, tracking included. The parent uses it to
    // right-align a row of labels without guessing at glyph metrics.
    int preferredWidth() const
    {
        return (int) std::ceil (Text::trackedWidth (Fonts::of (spec), text, spec.tracking));
    }

    void paint (juce::Graphics& g) override
    {
        const auto font = Fonts::of (spec);
        const auto ink  = hovered ? colour.brighter (0.6f) : colour;
        const auto area = getLocalBounds().toFloat();

        const float w = Text::drawTracked (g, text, font, ink, 0.0f, area.getCentreY(),
                                           spec.tracking);

        if (hovered)
        {
            const float y = area.getCentreY() + font.getHeight() * 0.5f;
            g.setColour (ink);
            g.fillRect (0.0f, y, w, 1.0f);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (getLocalBounds().contains (e.getPosition()) && url.isNotEmpty())
            juce::URL (url).launchInDefaultBrowser();
    }

    juce::MouseCursor getMouseCursor() override
    {
        return juce::MouseCursor::PointingHandCursor;
    }

private:
    juce::String text, url;
    Fonts::Spec  spec { 13.0f, 0.0f };
    juce::Colour colour { Colours::textFaint };
    bool hovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LinkLabel)
};

// ---------------------------------------------------------------------
//  HoldButton -- "Hold to preview the envelope timing" (handoff tooltip).
//  Momentary: held while the mouse is down. Drives the playhead only.
//  Styling is mockup-only; the handoff Colors table has no rows for it.
// ---------------------------------------------------------------------
class HoldButton : public juce::Component
{
public:
    HoldButton() = default;   // Pattern 11: MSVC needs this explicitly

    std::function<void (bool)> onHoldChanged;

    void setActive (bool shouldBeActive)
    {
        if (active == shouldBeActive)
            return;

        active = shouldBeActive;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        if (active)
        {
            g.setColour (Colours::holdActiveFill);
            g.fillRect (b);
        }

        g.setColour (active ? Colours::holdActiveInk : Colours::holdIdleBorder);
        g.drawRect (b, 1.0f);

        Text::drawTrackedCentred (g, "HOLD", Fonts::of (Fonts::holdLabel),
                                  active ? Colours::holdActiveInk : Colours::holdIdleText,
                                  b, Fonts::holdLabel.tracking);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (onHoldChanged) onHoldChanged (true);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (onHoldChanged) onHoldChanged (false);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (isMouseButtonDown() && onHoldChanged) onHoldChanged (false);
    }

private:
    bool active = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HoldButton)
};

} // namespace floyd
