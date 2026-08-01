// =====================================================================
//  RenderPanel -- dev-only headless screenshot harness.
//
//  Verify by self-screenshot: an offscreen createComponentSnapshot
//  harness (a dev-only exe that instantiates processor+editor headless
//  and writes a PNG) makes structural and typographic drift visible
//  during development instead of at review time.
//
//  Not part of the plugin. Build explicitly:
//      cmake --build build --config Release --target FloydFMRender
//      build\Release\FloydFMRender.exe out.png [preset] [zoom] [playhead]
//
//  playhead is in seconds: positive = held, negative = released.
// =====================================================================

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../Source/PluginProcessor.h"
#include "../Source/UI/FloydLayout.h"

extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

// `--verify-layout` dumps every Layout constant AS COMPILED, so the
// Phase 1 row-by-row check against the handoff table is made against the
// binary rather than against the source text.
static void dumpLayout()
{
    namespace L = floyd::Layout;

    auto row = [] (const char* name, int x, int y, int w, int h)
    {
        std::printf ("%-22s %5d %5d %5d %5d\n", name, x, y, w, h);
    };

    std::printf ("%-22s %5s %5s %5s %5s\n", "ELEMENT", "X", "Y", "W", "H");
    std::printf ("Panel %d x %d\n", L::PANEL_W, L::PANEL_H);

    for (int i = 0; i < 4; ++i)
    {
        char n[24];
        std::snprintf (n, sizeof (n), "Operator tab %d", i + 1);
        row (n, L::TAB_X[i], L::TAB_Y, L::TAB_W, L::TAB_H);
    }

    row ("Algorithm column",  L::ALG_X,           L::ALG_Y,           L::ALG_W,           L::ALG_H);
    row ("- ALG nudge row",   L::ALG_NUDGE_X,     L::ALG_NUDGE_Y,     L::ALG_NUDGE_W,     L::ALG_NUDGE_H);
    row ("- ALG name",        L::ALG_NAME_X,      L::ALG_NAME_Y,      L::ALG_NAME_W,      L::ALG_NAME_H);
    row ("- Routing diagram", L::ALG_DIAGRAM_X,   L::ALG_DIAGRAM_Y,   L::ALG_DIAGRAM_W,   L::ALG_DIAGRAM_H);
    row ("- ALG label string",L::ALG_LABEL_X,     L::ALG_LABEL_Y,     L::ALG_LABEL_W,     L::ALG_LABEL_H);
    row ("Envelope canvas",   L::CANVAS_X,        L::CANVAS_Y,        L::CANVAS_W,        L::CANVAS_H);
    row ("Slider column",     L::SLIDER_COL_X,    L::SLIDER_COL_Y,    L::SLIDER_COL_W,    L::SLIDER_COL_H);
    row ("- Amount label",    L::AMOUNT_LABEL_X,  L::AMOUNT_LABEL_Y,  L::AMOUNT_LABEL_W,  L::AMOUNT_LABEL_H);
    row ("- Amount track",    L::AMOUNT_TRACK_X,  L::AMOUNT_TRACK_Y,  L::AMOUNT_TRACK_W,  L::AMOUNT_TRACK_H);
    row ("- Amount value",    L::AMOUNT_VALUE_X,  L::AMOUNT_VALUE_Y,  L::AMOUNT_VALUE_W,  L::AMOUNT_VALUE_H);
    row ("- Ratio label",     L::RATIO_LABEL_X,   L::RATIO_LABEL_Y,   L::RATIO_LABEL_W,   L::RATIO_LABEL_H);
    row ("- Ratio track",     L::RATIO_TRACK_X,   L::RATIO_TRACK_Y,   L::RATIO_TRACK_W,   L::RATIO_TRACK_H);
    row ("- Ratio value",     L::RATIO_VALUE_X,   L::RATIO_VALUE_Y,   L::RATIO_VALUE_W,   L::RATIO_VALUE_H);
    row ("- HOLD button",     L::HOLD_X,          L::HOLD_Y,          L::HOLD_W,          L::HOLD_H);
    row ("Preset bar",        L::PRESET_BAR_X,    L::PRESET_BAR_Y,    L::PRESET_BAR_W,    L::PRESET_BAR_H);

    std::printf ("\nEnvelope canvas internals (handoff: pad 24, plot 192,92,646x436)\n");
    row ("Plot area", L::PLOT_X, L::PLOT_Y, L::PLOT_W, L::PLOT_H);
    std::printf ("pad=%d  AD %.2f -> %.2f | SUS -> %.2f | REL -> %.2f\n",
                 L::CANVAS_PAD, (float) L::PLOT_X, L::SUS_START_X, L::SUS_END_X, L::PLOT_R);
}

// `--audio-test` plays a held C4 through every factory preset and every
// algorithm and reports peak / RMS. This is the smoke test for "does it
// actually make sound" -- a silent preset, a NaN, or a runaway level all
// show up here without opening a DAW.
static int audioTest()
{
    constexpr double sr        = 48000.0;
    constexpr int    blockSize = 512;
    constexpr int    numBlocks = 94;      // ~1.0 s held

    std::unique_ptr<juce::AudioProcessor> processor (createPluginFilter());
    if (processor == nullptr)
        return 1;

    processor->setPlayConfigDetails (0, 2, sr, blockSize);
    processor->prepareToPlay (sr, blockSize);

    auto* apvts = dynamic_cast<FloydFMAudioProcessor*> (processor.get());
    if (apvts == nullptr)
        return 1;

    // Holds `notes` and returns { peak, rms, sawNaN }. A single C4 shows
    // the per-voice gain staging; the C-major triad + octave shows what
    // coherent polyphonic summing does on top of it. Both are the
    // handoff's own ear-audit criteria.
    // `wheel` < 0 leaves the pitch wheel alone; otherwise it is sent as a
    // 14-bit pitch-bend message one block after the note-on, i.e. into a
    // sounding voice, which is the case that exercises the mid-note
    // re-push rather than just the note-on snapshot.
    // Rising zero crossings over the SECOND HALF of the run, i.e. after
    // any pitch-wheel message has landed. It is a crude pitch proxy, but
    // it is the only thing here that can prove the wheel actually changed
    // the frequency rather than merely failing to crash.
    int lastCrossings = 0;

    auto run = [&] (const std::vector<int>& notes, int velocity, int wheel,
                    float& peak, float& rms, bool& sawNaN)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        peak = 0.0f; rms = 0.0f; sawNaN = false;
        double sumSq = 0.0;
        int    total = 0;
        int    crossings = 0;
        float  prev = 0.0f;
        apvts->resetPreLimiterPeak();

        for (int b = 0; b < numBlocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                for (int n : notes)
                    midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) velocity), 0);

            if (b == 1 && wheel >= 0)
                midi.addEvent (juce::MidiMessage::pitchWheel (1, wheel), 0);

            buffer.clear();
            processor->processBlock (buffer, midi);

            for (int s = 0; s < blockSize; ++s)
            {
                const float v = buffer.getSample (0, s);
                if (std::isnan (v) || std::isinf (v)) sawNaN = true;
                peak = juce::jmax (peak, std::abs (v));
                sumSq += (double) v * (double) v;
                ++total;

                if (b >= numBlocks / 2 && prev <= 0.0f && v > 0.0f)
                    ++crossings;

                prev = v;
            }
        }

        lastCrossings = crossings;

        juce::MidiBuffer off;
        for (int n : notes)
            off.addEvent (juce::MidiMessage::noteOff (1, n), 0);

        // Recentre so the next case starts from an unbent state. Note Off
        // deliberately does not do this (MIDI spec), so the harness must.
        if (wheel >= 0)
            off.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 0);

        buffer.clear();
        processor->processBlock (buffer, off);

        // Let the release finish so the next case starts from silence.
        for (int b = 0; b < 200; ++b)
        {
            juce::MidiBuffer empty;
            buffer.clear();
            processor->processBlock (buffer, empty);
        }

        rms = (float) std::sqrt (sumSq / juce::jmax (1, total));
    };

    int failures = 0;

    // GR = gain reduction the master limiter applied, in dB. Anything
    // above ~0.5 dB on a sustained note means the limiter is acting as a
    // distortion stage, not a safety net -- and the host meter will not
    // show it, because the limiter asymptotes at unity.
    const std::vector<int> single { 60 };
    const std::vector<int> chord  { 60, 64, 67, 72 };   // C major + octave

    // A limiter that only catches transients is a safety net; one that
    // reduces a sustained note by more than a fraction of a dB is a
    // distortion stage, and the host meter will never reveal it.
    // A chord is allowed a little more -- catching chord-onset peaks is
    // the limiter's actual job.
    constexpr float kMaxGRSingle = 1.5f;
    constexpr float kMaxGRChord  = 3.0f;

    // Velocity 127, NOT the 100 this table used through v0.1.0. Velocity
    // now scales carriers too, so full velocity is the loudest a preset
    // can be -- a gain-staging gate has to measure the worst case. It is
    // also the case that must be IDENTICAL to v0.1.0: at velocity 1.0 the
    // sensitivity term collapses to 1 whatever op.velSens holds.
    constexpr int kGateVelocity = 127;

    auto measure = [&] (const std::vector<int>& notes, float limit, float& gr, bool& bad)
    {
        float peak = 0.0f, rms = 0.0f;
        bool  nan  = false;
        run (notes, kGateVelocity, -1, peak, rms, nan);

        const float pre = apvts->getPreLimiterPeak();
        gr = (pre > peak && peak > 0.0f)
                ? juce::Decibels::gainToDecibels (pre) - juce::Decibels::gainToDecibels (peak)
                : 0.0f;

        bad = nan || (peak < 0.001f) || (peak > 1.001f) || (gr > limit);
        return peak;
    };

    auto report = [&] (const char* label, const char* sub)
    {
        float grOne = 0.0f, grChord = 0.0f;
        bool  badOne = false, badChord = false;

        const float peakOne   = measure (single, kMaxGRSingle, grOne,   badOne);
        const float peakChord = measure (chord,  kMaxGRChord,  grChord, badChord);

        if (badOne || badChord) ++failures;

        std::printf ("%-14s %-6s %7.4f %7.2f %8.4f %7.2f  %s\n",
                     label, sub, peakOne, grOne, peakChord, grChord,
                     (badOne || badChord) ? "FAIL" : "ok");
    };

    std::printf ("Limiter knee %.2f, output trim %.2f. GR = gain reduction applied.\n",
                 floyd::kSoftKneeThreshold, floyd::kOutputTrim);
    std::printf ("Budget: %.1f dB on one note, %.1f dB on a chord.\n\n",
                 kMaxGRSingle, kMaxGRChord);
    std::printf ("--- factory presets, velocity %d (loudest case) ---\n", kGateVelocity);
    std::printf ("%-14s %-6s %7s %7s %8s %7s  %s\n",
                 "PRESET", "ALG", "C4 PEAK", "GR dB", "CHD PEAK", "GR dB", "VERDICT");

    for (int i = 0; i < floyd::kNumPresets; ++i)
    {
        processor->setCurrentProgram (i);
        report (floyd::presetAt (i).name,
                juce::String ("ALG " + juce::String (floyd::presetAt (i).algorithmIndex + 1))
                    .toRawUTF8());
    }

    std::printf ("\n--- all 8 algorithms on preset 3 (BRASS, sustaining) ---\n");
    processor->setCurrentProgram (2);

    for (int a = 0; a < floyd::kNumAlgorithms; ++a)
    {
        if (auto* p = apvts->apvts.getParameter ("algorithm"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) a));

        report (floyd::kAlgorithms[(std::size_t) a].name,
                juce::String ("ALG " + juce::String (floyd::kAlgorithms[(std::size_t) a].display))
                    .toRawUTF8());
    }

    // ------------------------------------------------------------------
    //  Velocity response (per-operator sensitivity, added 2026-08-01)
    // ------------------------------------------------------------------
    //  Proves three things the peak/GR tables cannot:
    //    - a soft note is genuinely quieter than a hard one, i.e. carrier
    //      velocity is actually wired (v0.1.0 would print a flat row);
    //    - the response is monotonic, so no preset gets LOUDER when played
    //      softer through some sign error;
    //    - ORGAN stays flat, because its four op velSens are all 0. That
    //      row is the regression check that velSens = 0 still reproduces
    //      v0.1.0 behaviour exactly.
    std::printf ("\n--- velocity response, single C4 (peak by velocity) ---\n");
    std::printf ("%-14s %8s %8s %8s  %s\n", "PRESET", "VEL 20", "VEL 64", "VEL 127", "VERDICT");

    for (int i = 0; i < floyd::kNumPresets; ++i)
    {
        processor->setCurrentProgram (i);

        float p[3] { 0.0f, 0.0f, 0.0f }, rms = 0.0f;
        bool  nan = false, anyNaN = false;
        const int vels[3] { 20, 64, 127 };

        for (int k = 0; k < 3; ++k)
        {
            run (single, vels[k], -1, p[k], rms, nan);
            anyNaN = anyNaN || nan;
        }

        const bool isOrgan  = juce::String (floyd::presetAt (i).name) == "ORGAN";
        const bool flat     = std::abs (p[2] - p[0]) < 1.0e-5f;

        // 0.5% slack: the onset ramp and limiter make exact equality the
        // wrong test for "did not get quieter".
        const bool monotonic = (p[1] <= p[2] * 1.005f) && (p[0] <= p[1] * 1.005f);
        const bool responds  = isOrgan ? flat : (p[0] < p[2] * 0.95f);
        const bool bad       = anyNaN || ! monotonic || ! responds;

        if (bad) ++failures;

        std::printf ("%-14s %8.4f %8.4f %8.4f  %s%s\n",
                     floyd::presetAt (i).name, p[0], p[1], p[2],
                     bad ? "FAIL" : "ok",
                     isOrgan ? "   (flat by design -- no velocity on a drawbar organ)" : "");
    }

    // ------------------------------------------------------------------
    //  Pitch bend (added 2026-08-01)
    // ------------------------------------------------------------------
    //  ORGAN is the probe: ADDITIVE, all four operators sustaining, OP1 at
    //  ratio 1.00, so the output is periodic enough for a zero-crossing
    //  count to track pitch. The wheel is sent one block AFTER the note-on,
    //  so this exercises the mid-note re-push, not just the note-on
    //  snapshot. Expected ratio at the default +/-2 semitones is
    //  2^(2/12) = 1.1225 up and 0.8909 down.
    std::printf ("\n--- pitch bend, ORGAN held C4, bend_range default ---\n");
    processor->setCurrentProgram (6);   // ORGAN

    {
        const int   wheels[3] { 0, 8192, 16383 };
        const char* labels[3] { "wheel down", "wheel centre", "wheel up" };
        int   cross[3] { 0, 0, 0 };
        bool  anyNaN = false;

        std::printf ("%-14s %10s %10s  %s\n",
                     "POSITION", "PEAK", "CROSSINGS", "VERDICT");

        // Ratios are printed AFTER the loop -- the centre count is the
        // divisor and it does not exist until its own row has run.
        for (int k = 0; k < 3; ++k)
        {
            float peak = 0.0f, rms = 0.0f;
            bool  nan  = false;
            run (single, 127, wheels[k], peak, rms, nan);
            cross[k] = lastCrossings;
            anyNaN   = anyNaN || nan;

            const bool bad = nan || peak < 0.001f || peak > 1.001f;
            if (bad) ++failures;

            std::printf ("%-14s %10.4f %10d  %s\n",
                         labels[k], peak, cross[k], bad ? "FAIL" : "ok");
        }

        // The crossing count must actually move, and in the right
        // direction. A wheel wired to nothing prints three identical rows.
        const bool  moved   = cross[0] < cross[1] && cross[1] < cross[2];
        const float ratioDn = cross[1] > 0 ? (float) cross[0] / (float) cross[1] : 0.0f;
        const float ratioUp = cross[1] > 0 ? (float) cross[2] / (float) cross[1] : 0.0f;

        if (! moved || anyNaN) ++failures;

        std::printf ("measured %.4f down / %.4f up (expect 0.8909 / 1.1225 at +/-2 semitones): %s\n",
                     ratioDn, ratioUp,
                     moved ? "ok" : "FAIL -- wheel had no effect or is inverted");
    }

    processor->releaseResources();
    std::printf ("\n%s (%d failure(s))\n", failures == 0 ? "ALL PASS" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}

int main (int argc, char* argv[])
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc > 1 && juce::String (argv[1]) == "--verify-layout")
    {
        dumpLayout();
        return 0;
    }

    if (argc > 1 && juce::String (argv[1]) == "--audio-test")
        return audioTest();

    const juce::String outPath = (argc > 1) ? juce::String (argv[1])
                                            : juce::String ("panel.png");

    std::unique_ptr<juce::AudioProcessor> processor (createPluginFilter());
    if (processor == nullptr)
    {
        std::fprintf (stderr, "createPluginFilter() returned null\n");
        return 1;
    }

    processor->prepareToPlay (48000.0, 512);

    // Optional second arg: 1-based factory preset to render.
    if (argc > 2)
        processor->setCurrentProgram (juce::jlimit (0, floyd::kNumPresets - 1,
                                                    juce::String (argv[2]).getIntValue() - 1));

    // Optional fourth arg: playhead position in seconds, so the playhead
    // line and the per-operator dots can be verified headlessly. Positive
    // = note held (seconds since note-on), negative = released (seconds
    // since note-off). The editor seeds the playhead in its constructor,
    // so this must be set before the editor exists.
    if (argc > 4)
    {
        const float t = (float) juce::String (argv[4]).getDoubleValue();

        if (auto* fm = dynamic_cast<FloydFMAudioProcessor*> (processor.get()))
        {
            fm->playheadState.store (t < 0.0f ? 2 : 1);
            fm->playheadElapsed.store (t < 0.0f ? -t : t);
        }
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorIfNeeded());
    if (editor == nullptr)
    {
        std::fprintf (stderr, "createEditorIfNeeded() returned null\n");
        return 1;
    }

    // Optional third arg: zoom factor, to exercise the resize path.
    if (argc > 3)
    {
        const float scale = juce::jlimit (0.75f, 2.0f, (float) juce::String (argv[3]).getDoubleValue());
        editor->setSize ((int) (floyd::Layout::PANEL_W * scale),
                         (int) (floyd::Layout::PANEL_H * scale));
    }

    // The editor sizes itself in its constructor; snapshot at whatever it
    // chose so the harness cannot mask a wrong panel size.
    const auto w = editor->getWidth();
    const auto h = editor->getHeight();
    std::printf ("editor size: %d x %d\n", w, h);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);

    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (outPath));
    out.deleteFile();

    juce::FileOutputStream stream (out);
    if (! stream.openedOk())
    {
        std::fprintf (stderr, "cannot open %s\n", out.getFullPathName().toRawUTF8());
        return 1;
    }

    juce::PNGImageFormat png;
    if (! png.writeImageToStream (image, stream))
    {
        std::fprintf (stderr, "PNG write failed\n");
        return 1;
    }

    stream.flush();
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());

    // Editors must be released before the processor.
    processor->editorBeingDeleted (editor.get());
    editor.reset();
    processor.reset();

    return 0;
}
