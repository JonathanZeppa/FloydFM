#pragma once

#include <array>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../DSP/Algorithm.h"
#include "../DSP/EnvelopeShape.h"
#include "Brightness.h"
#include "FloydColours.h"
#include "FloydLayout.h"

// =====================================================================
//  EnvelopeCanvas -- FLOYD-FM-HANDOFF.md > "EnvelopeCanvas (694 x 484)".
//
//  All four operator envelopes on one shared absolute time axis, plus
//  drag handles on the selected operator, plus the playhead.
//
//  Envelope geometry -- five points, straight segments, hard corners:
//     origin (AD_start, 0) -> peak (AD(attack), amp)
//       -> susIn (AD(attack+decay), amp*sustain)
//       -> susOut (SUS_end, amp*sustain) -> end (REL(release), 0)
//
//  "Amplitude scales the whole envelope: peak height is amp, not 1.0."
//
//  The axis uses FIXED absolute windows shared by all four operators.
//  "Do NOT auto-scale the axis to the slowest operator. Two reasons: the
//   display lurches during handle drags, and per-envelope normalization
//   would hide a fast modulator dying under a slow carrier -- which is
//   the entire reason this editor exists."
//
//  BLOOM. The handoff specs an SVG Gaussian blur and warns (CG19) that
//  it must be a cached juce::Image, never generated in paint(). It also
//  says: "If bloom proves too expensive, drop it. It is cosmetic. The
//  envelope geometry is not." What is drawn here is the cheap middle
//  option -- a single wider low-alpha under-stroke -- which needs no
//  cached image, cannot stall the message thread, and survives handle
//  drags at full rate. No Gaussian, no CG19 exposure.
//
//  BRIGHTNESS GRADIENT (2026-07-30). A modulated carrier's line is
//  stroked with Floyd's brightness ramp instead of its identity colour:
//  the handoff's own integration note is "feeding a juce::ColourGradient
//  into the carrier's stroke". The metric lives in Brightness.h; this
//  file only samples it. Modulators, and carriers with nothing
//  modulating them, are unchanged.
//
//  THE SHADES RUN ALONG THE LINE, not across the screen (second fix,
//  same day). The first pass filled the whole path with one HORIZONTAL
//  gradient, so a zero-attack preset -- where the entire bright moment
//  sits in a few pixels of x -- painted every near-vertical segment a
//  single solid colour and the feature read as a red pillar at the
//  onset. Floyd's own renderer reads the DRAWN curves ("the modulators'
//  values on the Y-axis"), whose attacks rise from y = 0, so on his
//  screen the shades always march along the line: blue sweeping up the
//  rise to the peak's shade, then stepping back down the falls
//  (confirmed by Jonathan against the video). The metric is therefore
//  evaluated at the envelope NODES and each drawn segment sweeps
//  between its endpoint shades along its own direction: the rise ramps
//  silence-blue -> shade-at-peak, a fall walks its whole drawn length
//  from its top shade to its bottom one, and a sustained plateau whose
//  ends are both bright stays solid red. The metric still owns every
//  anchor; the sweep between anchors is geometry, not a repaint of the
//  metric.
// =====================================================================

namespace floyd
{

class EnvelopeCanvas : public juce::Component
{
public:
    EnvelopeCanvas() = default;   // Pattern 11: MSVC needs this explicitly

    // Defined in Brightness.h -- the brightness metric and the canvas
    // read the same operator snapshot. Aliased here so existing call
    // sites keep working.
    using OpDisplay = floyd::OpDisplay;

    // Handle callbacks map 1:1 onto the handoff's DragHandle table.
    std::function<void (float attack, float decay, float amp)> onPeakDrag;
    std::function<void (float decay, float sustain)>           onSusInDrag;
    std::function<void (float release)>                        onEndDrag;

    void setOps (const std::array<OpDisplay, kNumOps>& newOps)
    {
        ops = newOps;
        repaint();
    }

    void setSelectedOperator (int op)
    {
        if (selectedOp == op)
            return;

        selectedOp = op;
        repaint();
    }

    // The canvas needs the routing to know which lines are carriers and
    // which operators feed them. Read from Algorithm.h -- never a
    // parallel table.
    void setAlgorithm (int index)
    {
        if (algorithmIndex == index)
            return;

        algorithmIndex = index;
        repaint();
    }

    // state: 0 = off, 1 = held, 2 = released. elapsed in seconds.
    void setPlayhead (int state, float elapsed)
    {
        if (state == playheadState && juce::approximatelyEqual (elapsed, playheadElapsed))
            return;

        playheadState   = state;
        playheadElapsed = elapsed;
        repaint();
    }

    //==================================================================
    void paint (juce::Graphics& g) override
    {
        // Sustain band -- #FFFFFF at 1.8% across the full plot height.
        g.setColour (Colours::sustainBand);
        g.fillRect (juce::Rectangle<float> (xSusStart(), pad(),
                                            xSusEnd() - xSusStart(), plotH()));

        // One brightness scale for the whole picture, computed once --
        // carriers stay comparable to each other.
        const float scale = brightnessScale (algorithmAt (algorithmIndex), ops);

        // Unselected first, so the selected envelope reads on top.
        for (int i = 0; i < kNumOps; ++i)
            if (i != selectedOp)
                drawEnvelope (g, i, false, scale);

        drawEnvelope (g, selectedOp, true, scale);

        drawPlayhead (g);
        drawHandles (g);
    }

    //==================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = handleAt (e.position);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging == Handle::none)
            return;

        const auto& o = ops[(std::size_t) selectedOp];

        const float mx = e.position.x;
        const float yv = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - pad()) / plotH());

        switch (dragging)
        {
            case Handle::peak:
            {
                // X sets attack, clamped to [0, attack + decay]; dragging
                // right shortens decay so susIn holds position. Y is
                // op{n}_amp -- the same parameter the Amount slider edits.
                const float span = o.attack + o.decay;
                const float t    = juce::jlimit (0.0f, span, timeFromAD (mx));

                if (onPeakDrag)
                    onPeakDrag (t, span - t, yv);
                break;
            }

            case Handle::susIn:
            {
                // X sets decay (clamped >= 0); Y sets sustain as y / amp.
                const float t = timeFromAD (mx);
                const float d = juce::jlimit (0.0f, Layout::AD_WINDOW - o.attack, t - o.attack);
                const float s = (o.amp <= 0.0f) ? 0.0f : juce::jlimit (0.0f, 1.0f, yv / o.amp);

                if (onSusInDrag)
                    onSusInDrag (d, s);
                break;
            }

            case Handle::end:
            {
                // X sets release. Y is PINNED -- release always ends at
                // silence, so its vertical position has no honest meaning.
                const float r = juce::jlimit (0.0f, Layout::REL_WINDOW, timeFromREL (mx));

                if (onEndDrag)
                    onEndDrag (r);
                break;
            }

            default: break;
        }
    }

    void mouseUp (const juce::MouseEvent&) override { dragging = Handle::none; }

    juce::MouseCursor getMouseCursor() override
    {
        return dragging != Handle::none ? juce::MouseCursor::DraggingHandCursor
                                        : juce::MouseCursor::NormalCursor;
    }

private:
    enum class Handle { none, peak, susIn, end };

    //--- axis ---------------------------------------------------------
    static constexpr float pad()   { return (float) Layout::CANVAS_PAD; }
    static constexpr float plotW() { return (float) Layout::PLOT_W; }
    static constexpr float plotH() { return (float) Layout::PLOT_H; }

    static constexpr float xAD (float t)
    {
        return pad() + (t / Layout::AD_WINDOW) * Layout::AD_FRAC * plotW();
    }

    static constexpr float xSusStart() { return pad() + Layout::AD_FRAC * plotW(); }

    static constexpr float xSusEnd()
    {
        return pad() + (Layout::AD_FRAC + Layout::SUS_FRAC) * plotW();
    }

    static constexpr float xREL (float t)
    {
        return xSusEnd() + (t / Layout::REL_WINDOW) * Layout::REL_FRAC * plotW();
    }

    static constexpr float yOf (float v) { return pad() + plotH() - v * plotH(); }

    static constexpr float timeFromAD (float x)
    {
        return ((x - pad()) / (Layout::AD_FRAC * plotW())) * Layout::AD_WINDOW;
    }

    static constexpr float timeFromREL (float x)
    {
        return ((x - xSusEnd()) / (Layout::REL_FRAC * plotW())) * Layout::REL_WINDOW;
    }

    //--- geometry -----------------------------------------------------
    struct Points { juce::Point<float> origin, peak, susIn, susOut, end; };

    Points pointsFor (int op) const
    {
        const auto& o = ops[(std::size_t) op];
        const auto  b = breakpointsOf ({ o.attack, o.decay, o.sustain, o.release });

        return { { xAD (0.0f),        yOf (0.0f) },
                 { xAD (b.peakT),     yOf (o.amp * b.peakV) },
                 { xAD (b.susInT),    yOf (o.amp * b.susInV) },
                 { xSusEnd(),         yOf (o.amp * b.susInV) },
                 { xREL (b.endT),     yOf (0.0f) } };
    }

    static juce::Path pathOf (const Points& p)
    {
        juce::Path path;
        path.startNewSubPath (p.origin);
        path.lineTo (p.peak);
        path.lineTo (p.susIn);
        path.lineTo (p.susOut);
        path.lineTo (p.end);
        return path;
    }

    // A carrier that something modulates carries the brightness ramp;
    // everything else keeps its identity colour. An UNMODULATED carrier
    // (ADDITIVE, the bare sines in PAIR+SINE) has no brightness to show
    // and would otherwise be painted the cold end of a scale it is not
    // on -- and in ADDITIVE all four lines would go identically blue.
    bool usesBrightnessRamp (int op) const
    {
        const auto& alg = algorithmAt (algorithmIndex);
        return isCarrier (alg, op) && isModulated (alg, op);
    }

    // Shade position (0..1 through Floyd's 12 shades) for operator `op`
    // at pixel x -- the metric at that point's time, normalised and
    // through the perceptual curve. The lower clamp is ONE PIXEL in, not
    // zero: envHeldAt returns 0 at exactly t = 0, so a zero-attack
    // preset would otherwise evaluate its peak anchor -- which sits at
    // x = pad -- to silence and paint the whole line blue. One pixel
    // after note-on a zero-attack modulator is at full level and a
    // slow-attack one is still honestly dark.
    float shadeAt (int op, float xPix, float scale) const
    {
        const float t = juce::jlimit (1.0f / plotW(), 1.0f, (xPix - pad()) / plotW());
        return brightnessShade (brightnessAt (op, algorithmAt (algorithmIndex), ops, t), scale);
    }

    // Floyd's ramp, along the LINE. The metric is evaluated at the five
    // envelope NODES (the origin counts as silence), and each polyline
    // segment is stroked with a linear gradient sweeping between its
    // endpoint shades along the segment's own direction. This is the
    // classic cheap polyline colouring -- value per node, gradient per
    // segment -- and it is what makes the shades march down the drawn
    // line the way Floyd's do: the rise sweeps silence-blue up to the
    // peak's shade, a long fall spends its whole length walking from
    // its top shade to its bottom one, and a sustained plateau whose
    // two ends are both bright stays solid red. The metric still owns
    // every anchor; the sweep between anchors is geometry.
    void strokeRamped (juce::Graphics& g, int op, const Points& p,
                       float alpha, float scale, float strokeWidth) const
    {
        const juce::Point<float> pts[5] = { p.origin, p.peak, p.susIn, p.susOut, p.end };

        const float anchor[5] = { 0.0f,                            // silence at the origin
                                  shadeAt (op, p.peak.x,   scale),
                                  shadeAt (op, p.susIn.x,  scale),
                                  shadeAt (op, p.susOut.x, scale),
                                  shadeAt (op, p.end.x,    scale) };

        // Sample spacing matched to the old whole-plot resolution, so
        // the 11 shade edges keep reading as ~5px steps (FloydLayout).
        const float spacing = plotW() / (float) Layout::BRIGHTNESS_STOPS;

        for (int s = 0; s < 4; ++s)
        {
            const auto a = pts[s], b = pts[s + 1];
            const float len = a.getDistanceFrom (b);

            if (len < 0.5f)
                continue;

            auto valueAt = [&] (float f)
            {
                return anchor[s] + (anchor[s + 1] - anchor[s]) * f;
            };

            auto colourAt = [&] (float f)
            {
                return Colours::brightnessColour (valueAt (f)).withAlpha (alpha);
            };

            juce::ColourGradient grad (colourAt (0.0f), a, colourAt (1.0f), b, false);

            const int n = juce::jlimit (2, 192, (int) (len / spacing));
            for (int i = 1; i < n; ++i)
            {
                const float f = (float) i / (float) n;
                grad.addColour (f, colourAt (f));
            }

            juce::Path seg;
            seg.startNewSubPath (a);
            seg.lineTo (b);

            g.setGradientFill (grad);
            g.strokePath (seg, juce::PathStrokeType (strokeWidth, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
        }
    }

    void drawEnvelope (juce::Graphics& g, int op, bool selected, float scale) const
    {
        const auto points = pointsFor (op);
        const auto col    = Colours::opColour[op];   // identity, never changes

        const float w = selected ? Layout::ENV_STROKE_SELECTED : Layout::ENV_STROKE;
        const float a = selected ? 1.0f : Layout::ENV_OPACITY;

        const bool ramp = usesBrightnessRamp (op);

        // Cheap bloom: one wider under-stroke at low alpha. On a ramped
        // carrier this is the glow, and it carries the same colour as
        // the line -- so a bright section glows red.
        const float bloomAlpha = a * (selected ? 0.22f : 0.13f);

        if (ramp)
        {
            strokeRamped (g, op, points, bloomAlpha, scale, w * 2.0f);
            strokeRamped (g, op, points, a,          scale, w);
            return;
        }

        const auto path = pathOf (points);

        g.setColour (col.withAlpha (bloomAlpha));
        g.strokePath (path, juce::PathStrokeType (w * 2.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        g.setColour (col.withAlpha (a));
        g.strokePath (path, juce::PathStrokeType (w, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    void drawPlayhead (juce::Graphics& g) const
    {
        if (playheadState == 0)
            return;

        const float x = playheadX();
        if (x < 0.0f)
            return;

        g.setColour (Colours::playhead.withAlpha (Colours::PLAYHEAD_ALPHA));
        g.drawLine (x, pad() - Layout::PLAYHEAD_OVERHANG,
                    x, pad() + plotH() + Layout::PLAYHEAD_OVERHANG,
                    Layout::PLAYHEAD_W);

        drawPlayheadDots (g, x);
    }

    // Floyd: "a small circle that moves along the ADSR curve so you can
    // exactly see at which point in the graph you are currently."
    //
    // One dot per operator rather than one for the selected line: the
    // whole reason four envelopes share an axis is to compare them, and
    // the instant a modulator dies under a still-sounding carrier is
    // precisely what the dots make legible.
    //
    // Position comes from envDisplayAt -- the same function the
    // brightness metric samples and the same math the drawn breakpoints
    // come from -- so a dot cannot drift off its own line.
    void drawPlayheadDots (juce::Graphics& g, float x) const
    {
        const float xn = (x - pad()) / plotW();

        for (int i = 0; i < kNumOps; ++i)
        {
            const auto& o = ops[(std::size_t) i];

            // Past this operator's release end there is no line left to
            // ride, so there is no dot either.
            if (xn > (xREL (o.release) - pad()) / plotW() + 1.0e-4f)
                continue;

            const bool  selected = (i == selectedOp);
            const float r        = selected ? Layout::PLAYHEAD_DOT_R_SELECTED
                                            : Layout::PLAYHEAD_DOT_R;

            const juce::Point<float> centre { x, yOf (o.amp * envDisplayAt (o, xn)) };
            const auto box = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre);

            // Identity hue: on a gradient-stroked carrier the dot is the
            // thing still answering "which line is this". Brightened
            // well past the line it sits on, and ringed in the panel
            // background -- an identity-coloured dot on an
            // identity-coloured line reads as a hole, not a bead.
            g.setColour (Colours::background);
            g.fillEllipse (box.expanded (Layout::PLAYHEAD_DOT_OUTLINE));

            g.setColour (Colours::opColour[i].brighter (selected ? 0.9f : 0.6f));
            g.fillEllipse (box);
        }
    }

    // Note on: sweeps the A+D region in real time from t=0 to
    // max(attack + decay) across all four operators, then crosses to the
    // right edge of the sustain band over 0.45 s and parks.
    // Note off: sweeps the release region, disappears at max(release).
    float playheadX() const
    {
        float adEnd = 0.01f, maxRel = 0.01f;

        for (const auto& o : ops)
        {
            adEnd  = juce::jmax (adEnd,  o.attack + o.decay);
            maxRel = juce::jmax (maxRel, o.release);
        }

        if (playheadState == 1)
        {
            if (playheadElapsed < adEnd)
                return xAD (playheadElapsed);

            const float f = juce::jlimit (0.0f, 1.0f,
                                          (playheadElapsed - adEnd) / Layout::SUS_SWEEP);
            return xAD (adEnd) + (xSusEnd() - xAD (adEnd)) * f;
        }

        if (playheadElapsed >= maxRel)
            return -1.0f;

        return xREL (playheadElapsed);
    }

    void drawHandles (juce::Graphics& g) const
    {
        const auto p = pointsFor (selectedOp);

        for (const auto& centre : { p.peak, p.susIn, p.end })
        {
            const auto box = juce::Rectangle<float> (Layout::HANDLE_R * 2.0f,
                                                     Layout::HANDLE_R * 2.0f)
                                 .withCentre (centre);

            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillEllipse (box);
            g.setColour (Colours::selected);
            g.drawEllipse (box.reduced (Layout::HANDLE_STROKE * 0.5f), Layout::HANDLE_STROKE);
        }
    }

    Handle handleAt (juce::Point<float> pos) const
    {
        const auto p = pointsFor (selectedOp);
        const float r2 = Layout::HANDLE_R * Layout::HANDLE_R;

        // Test end-first so overlapping handles resolve to the one whose
        // drag is least destructive.
        if (pos.getDistanceSquaredFrom (p.peak)  <= r2) return Handle::peak;
        if (pos.getDistanceSquaredFrom (p.susIn) <= r2) return Handle::susIn;
        if (pos.getDistanceSquaredFrom (p.end)   <= r2) return Handle::end;

        return Handle::none;
    }

    std::array<OpDisplay, kNumOps> ops {};
    int    selectedOp      = 3;
    int    algorithmIndex  = 0;
    int    playheadState   = 0;
    float  playheadElapsed = 0.0f;
    Handle dragging        = Handle::none;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeCanvas)
};

} // namespace floyd
