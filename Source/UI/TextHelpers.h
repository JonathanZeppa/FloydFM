#pragma once

#include <juce_graphics/juce_graphics.h>

// =====================================================================
//  TextHelpers.h -- letter-spaced text rendering.
//
//  juce::Graphics::drawText and drawFittedText apply ZERO letter-spacing. The handoff specs
//  tracking on four of its eight text roles (tab labels 2px, slider
//  labels 1px, ALG number 1px, preset name 2px), so those must be drawn
//  glyph-by-glyph or they render cramped.
//
//  juce::Font::withExtraKerningFactor is the other option and is what
//  GlacierFM uses; it is rejected here because it appends tracking after
//  the FINAL glyph too, which shifts centred text left by tracking/2.
//  Almost every tracked string in this plugin is centred.
//
//  Font::getStringWidthFloat() is deprecated in JUCE 8 -- measure with a
//  GlyphArrangement instead.
// =====================================================================

namespace floyd::Text
{

// Advance width of a single character in `f`, whitespace included.
inline float charWidth (const juce::Font& f, juce::juce_wchar c)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, juce::String::charToString (c), 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// Total width of `text` in `f` with `tracking` px between glyphs.
// Tracking is inter-glyph only -- there is none after the last glyph.
inline float trackedWidth (const juce::Font& f, const juce::String& text, float tracking)
{
    const int n = text.length();
    if (n == 0)
        return 0.0f;

    float w = 0.0f;
    for (int i = 0; i < n; ++i)
        w += charWidth (f, text[i]);

    return w + tracking * (float) (n - 1);
}

// Draw `text` glyph-by-glyph starting at x, glyphs vertically centred on
// yCentre. Returns the total advance width actually drawn.
inline float drawTracked (juce::Graphics& g, const juce::String& text, const juce::Font& f,
                          juce::Colour colour, float x, float yCentre, float tracking)
{
    const int n = text.length();
    if (n == 0)
        return 0.0f;

    g.setColour (colour);
    g.setFont (f);

    const float h  = f.getHeight();
    const float x0 = x;
    float cx = x;

    for (int i = 0; i < n; ++i)
    {
        const auto  s = juce::String::charToString (text[i]);
        const float w = charWidth (f, text[i]);

        // Pad the cell slightly so glyphs with side-bearing are not clipped.
        g.drawText (s, juce::Rectangle<float> (cx, yCentre - h * 0.5f - 1.0f, w + 3.0f, h + 2.0f),
                    juce::Justification::centredLeft);
        cx += w + tracking;
    }

    return (cx - tracking) - x0;
}

// Draw `text` tracked and horizontally centred inside `area`.
inline void drawTrackedCentred (juce::Graphics& g, const juce::String& text, const juce::Font& f,
                                juce::Colour colour, juce::Rectangle<float> area, float tracking)
{
    const float w = trackedWidth (f, text, tracking);
    drawTracked (g, text, f, colour,
                 area.getCentreX() - w * 0.5f, area.getCentreY(), tracking);
}

} // namespace floyd::Text
