#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

#include "FloydColours.h"
#include "FloydFonts.h"
#include "FloydLayout.h"
#include "TextHelpers.h"

// =====================================================================
//  OperatorTab -- FLOYD-FM-HANDOFF.md > "Component Specifications" >
//  "OperatorTab (241 x 42)".
//
//    Pill: border-radius 21px (half height), background transparent,
//          border 2px solid.
//
//    | State    | Border   | Text     | Glow                |
//    | Idle     | identity | identity | 0 0 9px  {colour}44 |
//    | Selected | #FFFFFF  | #FFFFFF  | 0 0 15px #FFFFFF66  |
//
//    Text: VT323 19px, 2px letter-spacing, centred, "OP1".."OP4".
//    Radio behaviour -- exactly one selected. UI-ONLY STATE, never
//    reaches the DSP.
//
//  Operator colour is identity and NEVER changes (handoff, Colors). The
//  glow and border go white on selection; the identity colour is what
//  the glow uses when idle and what the envelope line always uses.
//
//  GLOW GEOMETRY. A JUCE component clips paint to its bounds, so a
//  15px halo on a tight
//  241x42 component renders as a square. The component is therefore
//  oversized by kGlowPad on every side and the pill is drawn centred
//  inside it; hitTest() is overridden so the transparent halo margin --
//  which overlaps the neighbouring tab -- does not steal its clicks.
//
//  The halo is drawn as concentric outward strokes rather than
//  juce::DropShadow: CSS outer box-shadow is clipped to OUTSIDE the
//  border-box, so a transparent-fill pill gets a halo hugging its
//  stroke, not a blob behind its interior. This is also cheap enough to
//  run in paint() without the CG19 cached-image treatment.
// =====================================================================

namespace floyd
{

class OperatorTab : public juce::Component
{
public:
    // Halo reach beyond the pill. >= the 15px selected glow radius.
    static constexpr int kGlowPad = 16;

    static constexpr int kOuterW = Layout::TAB_W + 2 * kGlowPad;
    static constexpr int kOuterH = Layout::TAB_H + 2 * kGlowPad;

    explicit OperatorTab (int operatorIndex)
        : index (operatorIndex)
    {
        jassert (juce::isPositiveAndBelow (operatorIndex, 4));
        setName ("OP" + juce::String (operatorIndex + 1));
    }

    // Position by the pill's handoff coordinates; the component expands
    // itself outward to make room for the halo.
    void setPillBounds (int x, int y, int w, int h)
    {
        setBounds (x - kGlowPad, y - kGlowPad, w + 2 * kGlowPad, h + 2 * kGlowPad);
    }

    void setSelected (bool shouldBeSelected)
    {
        if (selected == shouldBeSelected)
            return;

        selected = shouldBeSelected;
        repaint();
    }

    // Whether this operator is a carrier in the CURRENT algorithm. Drives
    // the role label -- the FM-education cue that tells the player
    // whether they are hearing this operator directly, or only hearing
    // what it does to something else.
    void setCarrier (bool isCarrier)
    {
        if (carrier == isCarrier)
            return;

        carrier = isCarrier;
        repaint();
    }

    bool isSelected() const noexcept { return selected; }
    int  getOperatorIndex() const noexcept { return index; }

    // Only the pill itself is clickable -- the halo margin is inert.
    bool hitTest (int x, int y) override
    {
        return pillArea().contains ((float) x, (float) y);
    }

    void paint (juce::Graphics& g) override
    {
        const auto  pill     = pillArea();
        const auto& identity = Colours::opColour[index];

        const juce::Colour ink   = selected ? Colours::selected : identity;
        const juce::Colour glowC = selected ? Colours::selected : identity;
        const float glowAlpha    = selected ? Colours::TAB_GLOW_ALPHA_SELECTED
                                            : Colours::TAB_GLOW_ALPHA_IDLE;
        const float glowReach    = selected ? Colours::TAB_GLOW_RADIUS_SELECTED
                                            : Colours::TAB_GLOW_RADIUS_IDLE;

        drawOuterGlow (g, pill, Layout::TAB_RADIUS, glowC, glowAlpha, glowReach);

        // border: 2px solid. JUCE centres a stroke on its path, CSS puts
        // the border inside the box -- inset by half the stroke width.
        const float half = Layout::TAB_BORDER * 0.5f;
        g.setColour (ink);
        g.drawRoundedRectangle (pill.reduced (half), Layout::TAB_RADIUS - half,
                                Layout::TAB_BORDER);

        // "background: transparent" -- nothing is filled.

        Text::drawTrackedCentred (g, getName(), Fonts::of (Fonts::tabLabel), ink,
                                  pill, Fonts::tabLabel.tracking);

        // Role label, in the band below the pill. Drawn last so the halo
        // cannot wash over it. Uses the operator's identity colour at
        // reduced alpha -- identity colour never changes, and dimming it
        // keeps the label subordinate to the tab itself.
        const auto roleArea = juce::Rectangle<float> (pill.getX(), pill.getBottom(),
                                                      pill.getWidth(), (float) Layout::TAB_ROLE_H);

        Text::drawTrackedCentred (g, carrier ? "CARRIER" : "MODULATOR",
                                  Fonts::of (Fonts::opRole),
                                  identity.withAlpha (selected ? 0.95f : 0.55f),
                                  roleArea, Fonts::opRole.tracking);
    }

    // Radio behaviour lives in the editor, which clears the other three.
    std::function<void (int)> onSelect;

    void mouseDown (const juce::MouseEvent&) override
    {
        if (onSelect)
            onSelect (index);
    }

    juce::MouseCursor getMouseCursor() override
    {
        return juce::MouseCursor::PointingHandCursor;
    }

private:
    juce::Rectangle<float> pillArea() const
    {
        return getLocalBounds().reduced (kGlowPad).toFloat();
    }

    // CSS `box-shadow: 0 0 Npx colour` on a transparent-fill bordered
    // element: a soft halo outside the border-box, quadratic falloff.
    static void drawOuterGlow (juce::Graphics& g, juce::Rectangle<float> pill,
                               float radius, juce::Colour colour, float alpha, float reach)
    {
        const int steps = juce::jmax (1, (int) reach);

        for (int i = steps; i >= 1; --i)
        {
            const float t = (float) i / (float) steps;      // 1.0 at the outer edge
            const float a = alpha * (1.0f - t) * (1.0f - t);
            const float e = (float) i;

            g.setColour (colour.withAlpha (a));
            g.drawRoundedRectangle (pill.expanded (e), radius + e, 1.6f);
        }
    }

    const int index;
    bool selected = false;
    bool carrier  = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OperatorTab)
};

} // namespace floyd
