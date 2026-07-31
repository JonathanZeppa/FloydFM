#pragma once

#include <cmath>

#include <juce_graphics/juce_graphics.h>

// =====================================================================
//  FloydColours.h -- every hex from FLOYD-FM-HANDOFF.md > "Colors",
//  in table order, plus the handful of values that exist only in the
//  canonical mockup (zedlab-fm-brightness-v11.jsx). Mockup-only entries
//  are marked; the handoff Colors table has no row for them.
//
//  RULE (handoff, non-negotiable): "Operator colour is identity and NEVER
//  changes. Selection is communicated by stroke weight and bloom only."
//  Nothing in this plugin may tint, dim, desaturate, or swap an operator
//  colour to indicate state.
//
//  ONE SANCTIONED EXCEPTION (2026-07-30): a MODULATED CARRIER's envelope
//  line is drawn in the brightness ramp below instead of its identity
//  colour. This is Floyd's gradient, and the handoff itself specifies
//  the integration as "feeding a juce::ColourGradient into the carrier's
//  stroke". Identity is preserved everywhere else -- tab, tab glow, role
//  label, diagram box, playhead dot -- and unmodulated carriers keep
//  their identity line, so "which line is which" is still answerable.
// =====================================================================

namespace floyd::Colours
{

// --- Handoff "Colors" table, row for row -----------------------------
inline const juce::Colour background     { 0xFF08080A };  // Main panel
inline const juce::Colour op1            { 0xFF3FA9FF };  // Operator 1 identity
inline const juce::Colour op2            { 0xFFFF8F2E };  // Operator 2 identity
inline const juce::Colour op3            { 0xFF34D058 };  // Operator 3 identity
inline const juce::Colour op4            { 0xFFC86BFF };  // Operator 4 identity
inline const juce::Colour selected       { 0xFFFFFFFF };  // Tab oval + text + diagram box when selected; drag handles
inline const juce::Colour textPrimary    { 0xFFDFE7F3 };  // Slider labels, values, ALG number
inline const juce::Colour textSecondary  { 0xFF8FA3C8 };  // Algorithm name
inline const juce::Colour textDim        { 0xFF5B6272 };  // Algorithm label string
inline const juce::Colour textFaint      { 0xFF3E3E46 };  // Preset counter, credit line
inline const juce::Colour lcdText        { 0xFF9FD6C8 };  // Preset name
inline const juce::Colour lcdBackground  { 0xFF0E1218 };  // Preset display inset
inline const juce::Colour border         { 0xFF2B3040 };  // Preset display, nudge buttons
inline const juce::Colour diagramEdge    { 0xFF4A5570 };  // Routing lines
inline const juce::Colour diagramArrow   { 0xFF6A7A9C };  // Direction triangles
inline const juce::Colour carrierBoxFill { 0xFF1B2436 };  // Diagram box fill for carriers only
inline const juce::Colour playhead       { 0xFFFF8F2E };  // Note position line

// "Sustain Band | #FFFFFF @ 1.8%" -- vertical band marking the sustain region.
inline const juce::Colour sustainBand = juce::Colour (0xFFFFFFFF).withAlpha (0.018f);

// Operator identity lookup. Index is the 0-based operator (displayed OP1..OP4).
inline const juce::Colour opColour[4] = { op1, op2, op3, op4 };

// --- Mockup-only values (no handoff Colors row) ----------------------
// Handoff PresetBar spec names "#8E96A8 text" for nudge buttons in prose
// but the Colors table omits it; the mockup's nudgeBtn style agrees.
inline const juce::Colour nudgeText      { 0xFF8E96A8 };

// HOLD button, mockup only (JSX line ~432). Idle vs. sounding states.
inline const juce::Colour holdIdleText   { 0xFF8E8E96 };
inline const juce::Colour holdIdleBorder { 0xFF2B2B30 };
inline const juce::Colour holdActiveFill { 0xFF2A1A0C };
inline const juce::Colour holdActiveInk  { 0xFFFF8F2E };  // == playhead

// --- Brightness ramp (Floyd's gradient, 2026-07-30) ------------------
// Floyd's own description of the scale, from the video where he showed
// the interface: "The result is normalized and mapped to a color scale
// from pure blue to pure red. The carrier's ADSR curve is then drawn in
// 12 possible color shades from left to right."
//
// So: PURE blue to PURE red, and QUANTISED to 12 shades rather than a
// continuous ramp. Both matter to the look. Green stays at zero the
// whole way, so the RGB path runs blue -> violet -> magenta -> red
// fully saturated -- there is no muddy midpoint to design around.
//
// The 12 steps are Floyd's, and they are not a limitation he worked
// around: banding at this width reads as a deliberate scale, the way a
// heat map's legend does, and it makes "this section is one shade
// brighter than that one" a thing the eye can actually judge.
inline constexpr int BRIGHT_SHADES = 12;

inline juce::Colour brightnessColour (float t)
{
    t = juce::jlimit (0.0f, 1.0f, t);

    // Shade index 0..11, then spread across the full scale so shade 0 is
    // pure blue and shade 11 is pure red.
    const int   shade = juce::jlimit (0, BRIGHT_SHADES - 1, (int) (t * (float) BRIGHT_SHADES));
    const float q     = (float) shade / (float) (BRIGHT_SHADES - 1);

    return juce::Colour::fromRGB ((juce::uint8) juce::roundToInt (q * 255.0f),
                                  (juce::uint8) 0,
                                  (juce::uint8) juce::roundToInt ((1.0f - q) * 255.0f));
}

// --- Component-spec alphas -------------------------------------------
// OperatorTab: "Idle glow 0 0 9px {colour}44" / "Selected 0 0 15px #FFFFFF66".
// 0x44 = 0.267, 0x66 = 0.400.
inline constexpr float TAB_GLOW_ALPHA_IDLE     = 0.267f;
inline constexpr float TAB_GLOW_ALPHA_SELECTED = 0.400f;
inline constexpr float TAB_GLOW_RADIUS_IDLE     = 9.0f;
inline constexpr float TAB_GLOW_RADIUS_SELECTED = 15.0f;

// RoutingDiagram: identity colour @ 0.8 for idle box strokes. (Phase 2.)
inline constexpr float DIAGRAM_IDLE_ALPHA = 0.8f;

// Playhead: "#FF8F2E @ 0.85". (Phase 2.)
inline constexpr float PLAYHEAD_ALPHA = 0.85f;

// --- Phase 1 scaffold ------------------------------------------------
// NOT a handoff colour. Dim guide outlines drawn around the three
// container regions so Phase 1 layout can be verified by eye against the
// handoff table. Compiled out by FLOYDFM_PHASE1_SCAFFOLD=0 and deleted
// at the start of Phase 2 -- see CMakeLists.txt.
inline const juce::Colour scaffoldGuide = juce::Colour (0xFF2B3040).withAlpha (0.55f);

} // namespace floyd::Colours
