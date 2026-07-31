#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../DSP/Algorithm.h"
#include "FloydColours.h"
#include "FloydFonts.h"
#include "FloydLayout.h"
#include "TextHelpers.h"

// =====================================================================
//  RoutingDiagram -- FLOYD-FM-HANDOFF.md > "RoutingDiagram (138 x 408)".
//
//    "Auto-layout from the algorithm edge table. Level 0 = carriers
//     (bottom row); an operator's level is 1 + max(level of everything
//     it modulates). Rows stack upward, each row centred horizontally."
//
//    Boxes 26 x 26, radius 4px, 12px horizontal gap, 22px vertical gap.
//
//    | State            | Fill      | Stroke            | Text     |
//    | Carrier, idle    | #1B2436   | identity @ 0.8    | identity |
//    | Modulator, idle  | transparent | identity @ 0.8  | identity |
//    | Selected         | unchanged | #FFFFFF @ 2.2px   | #FFFFFF  |
//
//    Edges: 1.6px #4A5570 from bottom-centre of source to top-centre of
//    destination, with an 8x8 triangle at the midpoint pointing toward
//    the destination.
//
//  Reads kAlgorithms directly -- there is no parallel table for the
//  visual (handoff, "Single source of truth").
// =====================================================================

namespace floyd
{

class RoutingDiagram : public juce::Component
{
public:
    RoutingDiagram() = default;   // Pattern 11: MSVC needs this explicitly

    void setAlgorithm (int index)
    {
        if (algorithmIndex == index)
            return;

        algorithmIndex = index;
        repaint();
    }

    void setSelectedOperator (int op)
    {
        if (selectedOp == op)
            return;

        selectedOp = op;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& alg = algorithmAt (algorithmIndex);
        const auto  lv  = computeLevels (alg);

        std::array<juce::Point<float>, kNumOps> pos;
        computePositions (lv, pos);

        // Edges first, so boxes sit on top of the lines.
        for (std::uint8_t e = 0; e < alg.edgeCount; ++e)
        {
            const auto& edge = alg.edges[e];
            if (edge.src == edge.dst)
                continue;

            const auto a = pos[edge.src];
            const auto b = pos[edge.dst];

            const float x1 = a.x + kBox * 0.5f, y1 = a.y + kBox;
            const float x2 = b.x + kBox * 0.5f, y2 = b.y;

            g.setColour (Colours::diagramEdge);
            g.drawLine (x1, y1, x2, y2, 1.6f);

            // Direction triangle at the midpoint. Destinations are always
            // on a lower row, so the triangle always points down.
            const float mx = (x1 + x2) * 0.5f;
            const float my = (y1 + y2) * 0.5f;

            juce::Path tri;
            tri.addTriangle (mx - 4.0f, my - 4.0f, mx + 4.0f, my - 4.0f, mx, my + 4.0f);
            g.setColour (Colours::diagramArrow);
            g.fillPath (tri);
        }

        for (int i = 0; i < kNumOps; ++i)
        {
            const auto box = juce::Rectangle<float> (pos[(std::size_t) i].x,
                                                     pos[(std::size_t) i].y, kBox, kBox);
            const bool on       = (i == selectedOp);
            const bool carrier  = isCarrier (alg, i);
            const auto identity = Colours::opColour[i];

            if (carrier)
            {
                g.setColour (Colours::carrierBoxFill);
                g.fillRoundedRectangle (box, Layout::DIAGRAM_BOX_RADIUS);
            }

            // Operator colour is identity and never changes; only the
            // stroke and text go white on selection.
            const float strokeW = on ? 2.2f : 1.4f;
            const auto  stroke  = on ? Colours::selected
                                     : identity.withAlpha (Colours::DIAGRAM_IDLE_ALPHA);

            g.setColour (stroke);
            g.drawRoundedRectangle (box.reduced (strokeW * 0.5f),
                                    Layout::DIAGRAM_BOX_RADIUS, strokeW);

            Text::drawTrackedCentred (g, juce::String (i + 1),
                                      Fonts::of (Fonts::diagramOpNum),
                                      on ? Colours::selected : identity,
                                      box, Fonts::diagramOpNum.tracking);
        }
    }

private:
    static constexpr float kBox  = (float) Layout::DIAGRAM_BOX;
    static constexpr float kHGap = (float) Layout::DIAGRAM_HGAP;
    static constexpr float kVGap = (float) Layout::DIAGRAM_VGAP;

    void computePositions (const Levels& lv, std::array<juce::Point<float>, kNumOps>& pos) const
    {
        const float w = (float) getWidth();
        const float h = (float) getHeight();

        const float totalH = (float) (lv.maxLevel + 1) * kBox + (float) lv.maxLevel * kVGap;
        const float yTop   = juce::jmax (4.0f, (h - totalH) * 0.5f);

        for (int level = 0; level <= (int) lv.maxLevel; ++level)
        {
            // Row members, in operator order.
            std::array<int, kNumOps> row {};
            int count = 0;

            for (int i = 0; i < kNumOps; ++i)
                if (lv.level[(std::size_t) i] == level)
                    row[(std::size_t) count++] = i;

            if (count == 0)
                continue;

            // Rows stack upward: level 0 (carriers) is the bottom row.
            const float y    = yTop + (float) ((int) lv.maxLevel - level) * (kBox + kVGap);
            const float span = (float) count * kBox + (float) (count - 1) * kHGap;
            const float x0   = (w - span) * 0.5f;

            for (int k = 0; k < count; ++k)
                pos[(std::size_t) row[(std::size_t) k]] = { x0 + (float) k * (kBox + kHGap), y };
        }
    }

    int algorithmIndex = 0;
    int selectedOp     = 3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingDiagram)
};

} // namespace floyd
