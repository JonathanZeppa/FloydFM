#pragma once

// =====================================================================
//  FloydLayout.h -- absolute positions, transcribed from
//  FLOYD-FM-HANDOFF.md > "Layout (Absolute Positions)".
//
//  Top-left origin, absolute pixels. Panel 1024 x 600, FIXED (no resize
//  in v1). Every constant below carries the handoff table row it came
//  from in a trailing comment; the verification pass that produced those
//  comments is recorded in floydfm-logfile.md (Session 2026-07-26).
//
//  Handoff "Build": "write component dimensions as constants rather than
//  literals so resize can be added without a rewrite". Hence this header
//  exists even though nothing scales yet. No literal pixel value may
//  appear in a setBounds() call anywhere in this plugin.
// =====================================================================

namespace floyd::Layout
{

// --- Panel -----------------------------------------------------------
inline constexpr int PANEL_W = 1024;
inline constexpr int PANEL_H = 600;

// --- Operator tabs ---------------------------------------------------
// Handoff rows: "Operator tab 1..4". Radio behaviour, UI-only state.
inline constexpr int TAB_W = 241;
inline constexpr int TAB_H = 42;
inline constexpr int TAB_Y = 12;
inline constexpr int TAB_X[4] = { 16, 266, 516, 766 };

// Component spec: "Pill: border-radius: 21px (half height)".
inline constexpr float TAB_RADIUS = 21.0f;
inline constexpr float TAB_BORDER = 2.0f;

// CARRIER / MODULATOR role label under each tab (added 2026-07-26 for FM
// education). Drawn inside the tab component, in the band between the
// pill's bottom edge (y=54) and the algorithm column's top (y=68).
// TAB_ROLE_GAP drops the band 2px clear of the pill (support ticket
// 2026-08-09: label sat tight against the tab and was hard to read).
// The band still ends inside the tab component's glow margin (y=70),
// and the 10px text bottoms out at y=68, flush with but not into the
// algorithm caption row.
inline constexpr int TAB_ROLE_GAP = 2;
inline constexpr int TAB_ROLE_H   = 14;

// --- Algorithm column ------------------------------------------------
// Handoff row: "Algorithm column".
inline constexpr int ALG_X = 16;
inline constexpr int ALG_Y = 68;
inline constexpr int ALG_W = 138;
inline constexpr int ALG_H = 484;

// "ALGORITHM" section caption (added 2026-07-26 for navigability). Not
// in the handoff table. It heads the column, so the group reads
// caption -> selector -> name -> diagram; everything below shifts down
// by its height, absorbed from the routing diagram's dead space (the
// boxes are vertically centred and occupied ~100px of 408).
inline constexpr int ALG_CAPTION_X = 16;
inline constexpr int ALG_CAPTION_Y = 68;
inline constexpr int ALG_CAPTION_W = 138;
inline constexpr int ALG_CAPTION_H = 16;

// Handoff row: "-- ALG nudge row", shifted down by the caption.
inline constexpr int ALG_NUDGE_X = 16;
inline constexpr int ALG_NUDGE_Y = 68 + ALG_CAPTION_H;      // 84
inline constexpr int ALG_NUDGE_W = 138;
inline constexpr int ALG_NUDGE_H = 24;

// Handoff row: "-- ALG name", shifted down by the caption.
inline constexpr int ALG_NAME_X = 16;
inline constexpr int ALG_NAME_Y = 92 + ALG_CAPTION_H;       // 108
inline constexpr int ALG_NAME_W = 138;
inline constexpr int ALG_NAME_H = 20;

// Handoff row: "-- Routing diagram", top edge down by the caption, and
// its height reduced by the same so the column's outer row is unchanged.
inline constexpr int ALG_DIAGRAM_X = 16;
inline constexpr int ALG_DIAGRAM_Y = 112 + ALG_CAPTION_H;   // 128
inline constexpr int ALG_DIAGRAM_W = 138;
inline constexpr int ALG_DIAGRAM_H = 408 - ALG_CAPTION_H;   // 392

// Handoff row: "-- ALG label string".
// NOTE: the JSX mockup's CSS flow puts this at y=520 (112 + 408, no
// margin); the handoff table says 524. Handoff table wins -- it is the
// authored absolute-position spec, the mockup value is emergent. Logged
// as DISC-1 in floydfm-logfile.md for Jonathan's adjudication.
inline constexpr int ALG_LABEL_X = 16;
inline constexpr int ALG_LABEL_Y = 524;
inline constexpr int ALG_LABEL_W = 138;
inline constexpr int ALG_LABEL_H = 16;

// --- Envelope canvas -------------------------------------------------
// Handoff row: "Envelope canvas".
inline constexpr int CANVAS_X = 168;
inline constexpr int CANVAS_Y = 68;
inline constexpr int CANVAS_W = 694;
inline constexpr int CANVAS_H = 484;

// Handoff "Envelope canvas internals": inner padding 24px on all sides,
// plot area 192, 92, 646 x 436 ABSOLUTE.
inline constexpr int CANVAS_PAD = 24;
inline constexpr int PLOT_X = 192;
inline constexpr int PLOT_Y = 92;
inline constexpr int PLOT_W = 646;
inline constexpr int PLOT_H = 436;

// Banded time axis -- FIXED absolute windows shared by all four
// operators. Handoff: "Do NOT auto-scale the axis to the slowest
// operator." (Phase 2 consumes these; declared here so the axis has one
// source of truth from day 1.)
inline constexpr float AD_FRAC  = 0.46f;   // 192 -> 489
inline constexpr float SUS_FRAC = 0.22f;   // 489 -> 631
inline constexpr float REL_FRAC = 0.32f;   // 631 -> 838
inline constexpr float AD_WINDOW  = 5.0f;  // seconds
inline constexpr float REL_WINDOW = 4.0f;  // seconds
inline constexpr float SUS_SWEEP  = 0.45f; // playhead sustain crossing, seconds

// Derived band edges (float -- the fractions do not land on integers).
inline constexpr float PLOT_R      = (float) (PLOT_X + PLOT_W);          // 838
inline constexpr float SUS_START_X = (float) PLOT_X + AD_FRAC * (float) PLOT_W;                 // 489.16
inline constexpr float SUS_END_X   = (float) PLOT_X + (AD_FRAC + SUS_FRAC) * (float) PLOT_W;    // 631.28

// --- Slider column ---------------------------------------------------
// Handoff row: "Slider column".
inline constexpr int SLIDER_COL_X = 876;
inline constexpr int SLIDER_COL_Y = 68;
inline constexpr int SLIDER_COL_W = 132;
inline constexpr int SLIDER_COL_H = 484;

// Handoff rows: "-- Amount label / track / value". Left slider; its label
// reads DEPTH or LEVEL depending on the selected operator's role in the
// current algorithm (handoff "op{n}_amp -- contextual meaning").
inline constexpr int AMOUNT_LABEL_X = 882;
inline constexpr int AMOUNT_LABEL_Y = 68;
inline constexpr int AMOUNT_LABEL_W = 54;
inline constexpr int AMOUNT_LABEL_H = 21;

inline constexpr int AMOUNT_TRACK_X = 882;
inline constexpr int AMOUNT_TRACK_Y = 89;
inline constexpr int AMOUNT_TRACK_W = 54;
inline constexpr int AMOUNT_TRACK_H = 366;

inline constexpr int AMOUNT_VALUE_X = 882;
inline constexpr int AMOUNT_VALUE_Y = 455;
inline constexpr int AMOUNT_VALUE_W = 54;
inline constexpr int AMOUNT_VALUE_H = 20;

// Handoff rows: "-- Ratio label / track / value". Right slider, always RATIO.
inline constexpr int RATIO_LABEL_X = 948;
inline constexpr int RATIO_LABEL_Y = 68;
inline constexpr int RATIO_LABEL_W = 54;
inline constexpr int RATIO_LABEL_H = 21;

inline constexpr int RATIO_TRACK_X = 948;
inline constexpr int RATIO_TRACK_Y = 89;
inline constexpr int RATIO_TRACK_W = 54;
inline constexpr int RATIO_TRACK_H = 366;

inline constexpr int RATIO_VALUE_X = 948;
inline constexpr int RATIO_VALUE_Y = 455;
inline constexpr int RATIO_VALUE_W = 54;
inline constexpr int RATIO_VALUE_H = 20;

// Handoff row: "-- HOLD button".
inline constexpr int HOLD_X = 876;
inline constexpr int HOLD_Y = 485;
inline constexpr int HOLD_W = 132;
inline constexpr int HOLD_H = 30;

// VSlider internals (handoff "VSlider (54 x 407)" + mockup VSlider()).
// The 54 x 407 in the component spec is label(21) + track(366) + value(20).
// Inside the 366px track region the drawn rail is inset by thumb radius
// + 4 at both ends, exactly as the mockup does it. Phase 2 consumes these.
inline constexpr float SL_RAIL_W    = 10.0f;   // "Track: 10px wide"
inline constexpr float SL_THUMB_R   = 14.0f;   // "Thumb: circle r = 14"
inline constexpr float SL_THUMB_PAD = 4.0f;    // mockup: top = SL_THUMB + 4
inline constexpr float SL_STROKE    = 3.2f;    // "stroke-width: 3.2"

// --- Preset bar ------------------------------------------------------
// Handoff row: "Preset bar".
inline constexpr int PRESET_BAR_X = 16;
inline constexpr int PRESET_BAR_Y = 560;
inline constexpr int PRESET_BAR_W = 992;
inline constexpr int PRESET_BAR_H = 40;

// PresetBar internals. The handoff table gives only the bar itself and
// "Display: 210px min width"; the rest is flow-derived from the mockup's
// preset strip (gap 8, nudge padding 2px/8px, counter marginLeft 6) and
// expressed here so no literal lands in a setBounds() call.
inline constexpr int PRESET_DISPLAY_MIN_W = 210;
inline constexpr int NUDGE_W = 24;
inline constexpr int NUDGE_H = 24;

inline constexpr int PRESET_PREV_X    = 0;
inline constexpr int PRESET_DISPLAY_X = PRESET_PREV_X + NUDGE_W + 8;          // 32
inline constexpr int PRESET_DISPLAY_W = PRESET_DISPLAY_MIN_W;                 // 210
inline constexpr int PRESET_DISPLAY_H = 26;
inline constexpr int PRESET_NEXT_X    = PRESET_DISPLAY_X + PRESET_DISPLAY_W + 8;  // 250
inline constexpr int PRESET_COUNTER_X = PRESET_NEXT_X + NUDGE_W + 8 + 6;          // 288
inline constexpr int PRESET_COUNTER_W = 70;

// "PRESET" caption (added 2026-07-26). The bar is 40px tall and the
// display 26px, so the caption takes the 13px band above it and the
// display drops to sit under it -- the row's outer bounds do not move.
inline constexpr int PRESET_CAPTION_H = 13;
inline constexpr int PRESET_ROW_Y     = PRESET_CAPTION_H;   // display + nudges top

// Right-hand text gap (mockup: marginLeft 14 between the two spans).
inline constexpr int PRESET_RIGHT_GAP = 14;

// Algorithm column internals: the nudge row is `<`  ALG n  `>`.
inline constexpr int ALG_PREV_X = ALG_NUDGE_X;                                // 16
inline constexpr int ALG_NEXT_X = ALG_NUDGE_X + ALG_NUDGE_W - NUDGE_W;        // 130

// --- Shared component metrics ---------------------------------------
// RoutingDiagram spec: "Boxes: 26 x 26, border-radius: 4px, 12px
// horizontal gap, 22px vertical gap between rows." (Phase 2.)
inline constexpr int DIAGRAM_BOX   = 26;
inline constexpr int DIAGRAM_HGAP  = 12;
inline constexpr int DIAGRAM_VGAP  = 22;
inline constexpr float DIAGRAM_BOX_RADIUS = 4.0f;

// DragHandle spec: "r = 17 ... stroke-width: 3.2". (Phase 2.)
inline constexpr float HANDLE_R      = 17.0f;
inline constexpr float HANDLE_STROKE = 3.2f;

// EnvelopeCanvas stroke spec. (Phase 2.)
inline constexpr float ENV_STROKE          = 9.0f;
inline constexpr float ENV_STROKE_SELECTED = 9.0f * 1.6f;   // 14.4
inline constexpr float ENV_OPACITY         = 0.70f;

// Playhead spec: "1.8px vertical line ... plot height + 6px overhang". (Phase 2.)
inline constexpr float PLAYHEAD_W        = 1.8f;
inline constexpr float PLAYHEAD_OVERHANG = 6.0f;

// Playhead dot -- Floyd, 2026-07-30: "a small circle that moves along
// the ADSR curve so you can exactly see at which point in the graph you
// are currently". One per operator, riding its own line. Deliberately
// well under HANDLE_R (17) so a dot can never be mistaken for a drag
// handle, and outlined in the panel background so it separates from the
// same-coloured line underneath it.
inline constexpr float PLAYHEAD_DOT_R          = 4.5f;
inline constexpr float PLAYHEAD_DOT_R_SELECTED = 6.0f;
inline constexpr float PLAYHEAD_DOT_OUTLINE    = 1.6f;

// Brightness gradient resolution. The metric is continuous in x, but a
// juce::ColourGradient is piecewise-linear between stops, so the ramp is
// sampled across the plot width.
//
// The COLOUR is quantised to Floyd's 12 shades, so consecutive samples
// inside one shade are identical and interpolate to nothing; the only
// place interpolation happens is where two shades meet, over a single
// sample interval. This count therefore sets how hard those 11 edges
// are, not how smooth the ramp is: 128 stops puts them at ~5px, which
// reads as a step rather than a fade. Cheap to build per paint -- no
// image, no blur, so no CG19 exposure.
inline constexpr int BRIGHTNESS_STOPS = 128;

} // namespace floyd::Layout
