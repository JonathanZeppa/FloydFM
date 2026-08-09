#pragma once

#include <juce_graphics/juce_graphics.h>
#include <BinaryData.h>

// =====================================================================
//  FloydFonts.h -- Montserrat, the font family LVGL ships as its
//  built-in face (lv_font_montserrat_8 .. _48).
//
//  Replaced VT323 wholesale on 2026-07-26 at Jonathan's request, for
//  visual consistency with an LVGL hardware panel. The handoff's Fonts
//  table specified VT323 only; that section is superseded.
//
//  SIZES ARE NOT THE HANDOFF'S NUMBERS. VT323 is a narrow terminal face
//  with a small cap-height relative to its em; Montserrat is a wide
//  geometric sans. Carrying the VT323 px values over verbatim renders
//  both too large and too wide -- "RATIO" at the handoff's 16px overflows
//  the 54px slider column. Every size below was re-tuned against the
//  actual render. A font family change invalidates every size constant:
//  audit ALL widths when the face changes.
//
//  WEIGHTS. LVGL's default is Regular. Medium is used as the workhorse
//  here because small light text on a near-black panel reads thin, and
//  SemiBold carries the two identity roles (tab labels, preset name).
//
//  SIZING METHOD: FontOptions::withPointHeight(), NOT withHeight().
//  withHeight sizes by ascent+descent and renders ~1.3-1.4x small
//  relative to a CSS px value; withPointHeight sets the em size and
//  matches a mockup 1:1.
//
//  LETTER-SPACING: drawText applies none. Anything with tracking > 0 must
//  go through floyd::Text::drawTracked* (TextHelpers.h).
//
//  ASCII ONLY in every plugin-facing string. Use "--", never an em dash.
// =====================================================================

namespace floyd::Fonts
{

struct Typefaces
{
    juce::Typeface::Ptr regular, medium, semiBold;

    Typefaces()
    {
        regular  = juce::Typeface::createSystemTypefaceFor (
            BinaryData::MontserratRegular_ttf,  BinaryData::MontserratRegular_ttfSize);
        medium   = juce::Typeface::createSystemTypefaceFor (
            BinaryData::MontserratMedium_ttf,   BinaryData::MontserratMedium_ttfSize);
        semiBold = juce::Typeface::createSystemTypefaceFor (
            BinaryData::MontserratSemiBold_ttf, BinaryData::MontserratSemiBold_ttfSize);
    }
};

inline const Typefaces& faces()
{
    static Typefaces t;
    return t;
}

enum class Weight { regular, medium, semiBold };

inline juce::Font montserrat (float pxHeight, Weight w = Weight::medium)
{
    const auto tf = (w == Weight::semiBold) ? faces().semiBold
                  : (w == Weight::regular)  ? faces().regular
                                            : faces().medium;

    return juce::Font (juce::FontOptions (tf).withPointHeight (pxHeight));
}

// { size px, letter-spacing px, weight }
struct Spec
{
    float  px;
    float  tracking;
    Weight weight = Weight::medium;
};

// --- roles from the handoff's Fonts table, re-tuned for Montserrat ---
inline constexpr Spec tabLabel     { 15.0f, 2.0f,  Weight::semiBold };  // OP1..OP4
inline constexpr Spec sliderLabel  { 11.5f, 1.0f,  Weight::medium   };  // DEPTH / LEVEL / RATIO
inline constexpr Spec sliderValue  { 12.5f, 0.0f,  Weight::medium   };
inline constexpr Spec algNumber    { 13.5f, 1.0f,  Weight::semiBold };  // "ALG 5"
inline constexpr Spec algName      { 11.5f, 0.5f,  Weight::medium   };  // "TWIN PAIR"
inline constexpr Spec diagramOpNum { 11.0f, 0.0f,  Weight::semiBold };
inline constexpr Spec presetName   { 14.0f, 2.0f,  Weight::semiBold };
inline constexpr Spec faint        { 10.0f, 0.0f,  Weight::regular  };  // counter, credit

// --- mockup-only roles ------------------------------------------------
inline constexpr Spec nudgeGlyph   { 12.0f, 0.0f,  Weight::medium   };
inline constexpr Spec holdLabel    { 12.5f, 2.0f,  Weight::medium   };

// --- new: navigation captions and the FM role labels ------------------
// Section captions over the ALGORITHM and PRESET groups, and the
// CARRIER / MODULATOR label under each operator tab. The role label is
// the FM-education one: it tells the player, live, whether the operator
// they have selected is heard directly or only heard through what it
// modulates -- which is also why the left slider reads LEVEL vs DEPTH.
inline constexpr Spec caption      {  9.0f, 1.6f,  Weight::semiBold };
// opRole 9 -> 10px, support ticket 2026-08-09 (reading-glasses user;
// paired with TAB_ROLE_GAP in FloydLayout.h).
inline constexpr Spec opRole       { 10.0f, 1.2f,  Weight::medium   };

inline juce::Font of (const Spec& s) { return montserrat (s.px, s.weight); }

} // namespace floyd::Fonts
