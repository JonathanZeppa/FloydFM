# ZedLab FM — HANDOFF

> **Target:** JUCE 8 VST3 + AU (Mac)
> **Manufacturer:** Zedtronics
> **Manufacturer Code:** `Zedt`
> **Plugin Code:** `Zlfm`  ⚠️ **PLACEHOLDER — confirm with Jonathan before project creation**
> **Plugin Name:** ZedLab FM  ⚠️ **PLACEHOLDER — confirm before project creation**
> **Canonical mockup:** `zedlab-fm-brightness-v11.jsx` (plugin root)
> **License:** MIT — this plugin ships open source

---

## Plugin Overview

| Property | Value |
|----------|-------|
| Name | ZedLab FM (placeholder) |
| Type | Instrument (synth) |
| Tier | Free — factory presets only, no user save, no MIDI Learn, no FX |
| Panel Size | 1024 × 600 px (fixed, no resize in v1) |
| Polyphony | 8 voices |
| Description | Four-operator FM synthesizer with a direct-manipulation envelope editor: all four operator envelopes on one shared absolute time axis, edited by dragging breakpoints. |

**Attribution (non-negotiable, appears in three places):** The interface concept originates with Floyd Steinberg (YouTube), who published it as a design idea and explicitly invited reuse. Credit appears in the preset bar, the Help > About tab, and the repository README. Floyd published no source code — this is an independent implementation from his description.

---

## ✅ SUPERSEDED 2026-07-30 — Brightness Gradient IS NOW IMPLEMENTED

**This section is history.** Floyd supplied the metric himself, the deferral was
lifted, and the gradient plus a per-operator playhead dot shipped on 2026-07-30.
The live specification is `Source/UI/Brightness.h`; the rationale and the
decisions taken are in `floydfm-logfile.md` (session 2026-07-30) and `README.md`.
Do not re-defer it, and do not restore the `return 0` hook.

The original deferral text follows, unedited, because the integration note in it
is the one the implementation actually followed.

---

## ⚠️ DEFERRED FEATURE — Brightness Gradient (original text)

Floyd's signature feature (carrier envelope drawn as a dark-blue→bright-red gradient showing where the tone is brightest) is **intentionally NOT in v1**, pending clarification from Floyd himself.

**Code must NOT implement it, and must NOT design around its absence.** Provide exactly one integration point:

```cpp
// Reserved. Returns 0..1 brightness for the operator `target`
// at normalized display position x (0..1). v1 always returns 0.
float brightnessAt (int target, const Algorithm& alg,
                    const OperatorParams* ops, float x);
```

Nothing else in the DSP, the envelope editor, or the layout may depend on it. It reconnects later by feeding a `juce::ColourGradient` into the carrier's stroke — **note: the JUCE method is `addColour (double proportion, Colour)`, NOT `addColourStop()`**, which does not exist.

---

## Colors

| Name | Hex | Usage |
|------|-----|-------|
| Background | `#08080A` | Main panel |
| OP1 | `#3FA9FF` | Operator 1 identity (line, tab, diagram box) |
| OP2 | `#FF8F2E` | Operator 2 identity |
| OP3 | `#34D058` | Operator 3 identity |
| OP4 | `#C86BFF` | Operator 4 identity |
| Selected | `#FFFFFF` | Tab oval + tab text + diagram box when selected; drag handles |
| Text Primary | `#DFE7F3` | Slider labels, values, ALG number |
| Text Secondary | `#8FA3C8` | Algorithm name |
| Text Dim | `#5B6272` | Algorithm label string |
| Text Faint | `#3E3E46` | Preset counter, credit line |
| LCD Text | `#9FD6C8` | Preset name |
| LCD BG | `#0E1218` | Preset display inset |
| Border | `#2B3040` | Preset display, nudge buttons |
| Diagram Edge | `#4A5570` | Routing lines |
| Diagram Arrow | `#6A7A9C` | Direction triangles |
| Carrier Box Fill | `#1B2436` | Diagram box fill for carriers only |
| Playhead | `#FF8F2E` | Note position line |
| Sustain Band | `#FFFFFF` @ 1.8% | Vertical band marking the sustain region |

**Operator colour is identity and NEVER changes.** Selection is communicated by stroke weight and bloom only. This rule matters: colour is the only thing that answers "which line is which" when four envelopes overlap.

---

## Fonts

| Usage | Font | Weight | Size | Spacing |
|-------|------|--------|------|---------|
| Tab labels | VT323 | 400 | 19px | 2px |
| Slider labels | VT323 | 400 | 16px | 1px |
| Slider values | VT323 | 400 | 15px | 0 |
| ALG number | VT323 | 400 | 17px | 1px |
| ALG name | VT323 | 400 | 15px | 0 |
| Diagram op numbers | VT323 | 400 | 15px | 0 |
| Preset name | VT323 | 400 | 17px | 2px |
| Faint text | VT323 | 400 | 13px | 0 |

VT323 only — SIL OFL 1.1, commercial use confirmed. Downloaded as .ttf, embedded via BinaryData.

**ASCII only in every plugin-facing string.** VT323 lacks em dash and most special Unicode. Use `--` not `—`. Verify before build.

---

## Parameters

~~32 APVTS parameters.~~ **37 as shipped** — see the SUPERSEDED note below.
`{n}` = 1..4.

| ID | Name | Type | Range | Default | Unit | Group |
|----|------|------|-------|---------|------|-------|
| `op{n}_attack` | OP{n} Attack | float | 0.0–5.0 | see presets | s | Operator {n} |
| `op{n}_decay` | OP{n} Decay | float | 0.0–5.0 | see presets | s | Operator {n} |
| `op{n}_sustain` | OP{n} Sustain | float | 0.0–1.0 | see presets | — | Operator {n} |
| `op{n}_release` | OP{n} Release | float | 0.0–4.0 | see presets | s | Operator {n} |
| `op{n}_amp` | OP{n} Amount | float | 0.0–1.0 | see presets | — | Operator {n} |
| `op{n}_ratio` | OP{n} Ratio | choice | 0–18 (see below) | 3 (=1.00) | × | Operator {n} |
| `op{n}_detune` | OP{n} Detune | float | -50.0–50.0 | 0.0 | cents | Operator {n} |
| `op{n}_vel` | OP{n} Velocity | float | 0.0–1.0 | see presets | — | Operator {n} |
| `algorithm` | Algorithm | choice | 0–7 | 0 | — | Global |
| `feedback` | Feedback | float | 0.0–1.0 | 0.0 | — | Global |
| `velocity_amt` | Velocity Amount | float | 0.0–1.0 | 0.7 | — | Global |
| `bend_range` | Pitch Bend Range | int | 0–12 | 2 | semitones | Global |
| `master_level` | Master | float | 0.0–1.0 | 0.8 | — | Global |

> **SUPERSEDED 2026-08-01 — `op{n}_vel` and `bend_range` added (rows above).**
> Floyd asked for velocity on the carriers as well as the modulators
> ("it makes the pianos and pads come alive"), and Jonathan chose the
> DX7/TX81Z model to deliver it: a per-operator sensitivity, four new
> parameters, stored per preset. `velocity_amt` keeps its id, range and
> default and becomes the global master depth over the four. See the
> superseded Velocity paragraph under DSP Notes.
>
> `bend_range` is a second addition of the same date: this document never
> mentions the pitch wheel at all, and the plugin ignored it through
> v0.1.0. Global, not stored per preset, like `velocity_amt` and
> `master_level`.

### `op{n}_amp` — contextual meaning

One parameter, two roles depending on the operator's position in the current algorithm:

- **Modulator** → modulation depth (index). UI label reads `DEPTH`.
- **Carrier** → output level. UI label reads `LEVEL`.

The label is driven by the algorithm, not stored — it must reflect what the DSP actually does in the current routing.

### `op{n}_ratio` — snapped choice, not continuous

19 musical values. Free ratios are deliberately excluded: non-simple ratios throw inharmonic sidebands that read as aliasing (banked lesson). `op{n}_detune` covers the in-between territory.

```
0.25, 0.50, 0.75, 1.00, 1.25, 1.50, 2.00, 2.50, 3.00, 3.50,
4.00, 5.00, 6.00, 7.00, 8.00, 9.00, 10.00, 11.00, 12.00
```

### `algorithm` — canonical Yamaha 4-op set

Same connections as DX21 / DX27 / DX100 / TX81Z / YM2151 (OPM connections 0–7). Operators are 0-indexed internally, displayed as OP1–OP4. `edges` are `[src, dst]` meaning src modulates dst.

| Index | Display | Name | Edges | Carriers |
|-------|---------|------|-------|----------|
| 0 | ALG 1 | STACK | (0,1)(1,2)(2,3) | 3 |
| 1 | ALG 2 | TWIN FEED | (0,2)(1,2)(2,3) | 3 |
| 2 | ALG 3 | SPLIT MOD | (1,2)(0,3)(2,3) | 3 |
| 3 | ALG 4 | BRANCH | (0,1)(1,3)(2,3) | 3 |
| 4 | ALG 5 | TWIN PAIR | (0,1)(2,3) | 1, 3 |
| 5 | ALG 6 | FAN | (0,1)(0,2)(0,3) | 1, 2, 3 |
| 6 | ALG 7 | PAIR+SINE | (0,1) | 1, 2, 3 |
| 7 | ALG 8 | ADDITIVE | — | 0, 1, 2, 3 |

**Single source of truth.** This table lives in exactly one header (`Algorithm.h`). The routing diagram component and the DSP voice both read from it. Never maintain a parallel table for the visual.

---

## Layout (Absolute Positions)

Top-left origin, absolute pixels. Panel 1024 × 600.

| Element | X | Y | W | H |
|---------|---|---|---|---|
| Operator tab 1 | 16 | 12 | 241 | 42 |
| Operator tab 2 | 266 | 12 | 241 | 42 |
| Operator tab 3 | 516 | 12 | 241 | 42 |
| Operator tab 4 | 766 | 12 | 241 | 42 |
| Algorithm column | 16 | 68 | 138 | 484 |
| — ALG nudge row | 16 | 68 | 138 | 24 |
| — ALG name | 16 | 92 | 138 | 20 |
| — Routing diagram | 16 | 112 | 138 | 408 |
| — ALG label string | 16 | 524 | 138 | 16 |
| Envelope canvas | 168 | 68 | 694 | 484 |
| Slider column | 876 | 68 | 132 | 484 |
| — Amount label | 882 | 68 | 54 | 21 |
| — Amount track | 882 | 89 | 54 | 366 |
| — Amount value | 882 | 455 | 54 | 20 |
| — Ratio label | 948 | 68 | 54 | 21 |
| — Ratio track | 948 | 89 | 54 | 366 |
| — Ratio value | 948 | 455 | 54 | 20 |
| — HOLD button | 876 | 485 | 132 | 30 |
| Preset bar | 16 | 560 | 992 | 40 |

### Envelope canvas internals

Inner padding 24px on all sides. Plot area: **192, 92, 646 × 436** absolute.

Time axis is banded, with **fixed absolute windows shared by all four operators**:

| Region | Fraction of plot width | Absolute X span | Time window |
|--------|------------------------|-----------------|-------------|
| Attack + Decay | 0.46 | 192 → 489 | 0 to 5.0 s |
| Sustain | 0.22 | 489 → 631 | indefinite (fixed display width) |
| Release | 0.32 | 631 → 838 | 0 to 4.0 s |

**Do NOT auto-scale the axis to the slowest operator.** Two reasons: the display lurches during handle drags, and per-envelope normalization would hide a fast modulator dying under a slow carrier — which is the entire reason this editor exists.

Sustain band is marked with `#FFFFFF` at 1.8% alpha across the full plot height.

---

## Component Specifications

### OperatorTab (241 × 42)

Pill: `border-radius: 21px` (half height), `background: transparent`, `border: 2px solid`.

| State | Border | Text | Glow |
|-------|--------|------|------|
| Idle | operator identity colour | operator identity colour | `0 0 9px {colour}44` |
| Selected | `#FFFFFF` | `#FFFFFF` | `0 0 15px #FFFFFF66` |

Text: VT323 19px, 2px letter-spacing, centred, `OP1`–`OP4`.

Radio behaviour — exactly one selected. This is **UI-only state**, never reaches the DSP.

### EnvelopeCanvas (694 × 484)

Renders all four operator envelopes on the shared axis, plus drag handles on the selected operator, plus the playhead.

**Envelope geometry.** Five points, straight segments, hard corners — no curvature:

```
origin  (x = AD_start,            y = 0)
peak    (x = AD(attack),          y = amp)
susIn   (x = AD(attack + decay),  y = amp * sustain)
susOut  (x = SUS_end,             y = amp * sustain)
end     (x = REL(release),        y = 0)
```

Amplitude scales the whole envelope: peak height is `amp`, not 1.0. Turning an operator's amount down visibly shrinks its curve.

**Stroke:**

| State | Width | Opacity | Bloom |
|-------|-------|---------|-------|
| Unselected | 9px | 0.70 | soft |
| Selected | 14.4px (9 × 1.6) | 1.00 | strong |

`stroke-linecap: round`, `stroke-linejoin: round`, colour always the operator's identity colour.

**Bloom (CG19 CRITICAL).** The mockup uses SVG Gaussian blur. In JUCE this is a per-stroke blur and **must not be generated in `paint()`**. Render the blurred layer to a cached `juce::Image` in `resized()` and in parameter setters; `paint()` may only call `drawImageAt()` for the bloom layer plus direct stroke calls for the crisp layer. During an active handle drag, draw the crisp stroke only and regenerate the bloom on drag-end — regenerating a blur per drag frame will stall the message thread.

If bloom proves too expensive, drop it. It is cosmetic. The envelope geometry is not.

### DragHandle (r = 17)

`fill: rgba(0,0,0,0.45)`, `stroke: #FFFFFF`, `stroke-width: 3.2`. Three handles, on the selected operator only.

| Handle | X drag | Y drag |
|--------|--------|--------|
| peak | attack time, clamped to `[0, attack + decay]` | `op{n}_amp` |
| susIn | decay time, clamped `>= 0` | sustain level, as `y / amp` |
| end | release time | **pinned** — release always ends at silence |

Dragging `peak` right shortens decay so `susIn` holds position (standard point-editor behaviour). `peak` can never cross `susIn`.

`peak` Y and the Amount slider are two views of the same parameter — moving either updates the other.

### RoutingDiagram (138 × 408)

Auto-layout from the algorithm edge table. Level 0 = carriers (bottom row); an operator's level is `1 + max(level of everything it modulates)`. Rows stack upward, each row centred horizontally.

Boxes: 26 × 26, `border-radius: 4px`, 12px horizontal gap, 22px vertical gap between rows.

| State | Fill | Stroke | Text |
|-------|------|--------|------|
| Carrier, idle | `#1B2436` | identity colour @ 0.8 | identity colour |
| Modulator, idle | transparent | identity colour @ 0.8 | identity colour |
| Selected | (unchanged) | `#FFFFFF` @ 2.2px | `#FFFFFF` |

Edges: 1.6px `#4A5570` from bottom-centre of source to top-centre of destination, with a 8×8 triangle at the midpoint pointing toward the destination (signal direction).

### VSlider (54 × 407)

Track: 10px wide, full height, `border-radius: 5px`, `fill: #DFE7F3` @ 0.9, centred horizontally.
Thumb: circle r = 14, `fill: rgba(0,0,0,0.55)`, `stroke: #FFFFFF`, `stroke-width: 3.2`.

Deliberately the same visual language as the envelope drag handles.

Vertical drag; click anywhere on the track jumps. Label above, value below (2 decimals).

Left slider label is `DEPTH` or `LEVEL` per the operator's current role. Right slider is always `RATIO`, displaying the snapped value.

### Playhead

1.8px vertical line, `#FF8F2E` @ 0.85, spanning plot height + 6px overhang top and bottom.

Driven by note state, not by parameter changes:
- Note on → sweeps the A+D region in real time from `t=0` to `max(attack + decay)` across all four operators
- Then crosses to the right edge of the sustain band over 0.45 s and parks
- Note off → sweeps the release region in real time, disappears at `max(release)`

Polyphonic: track the **most recent** note only. Do not attempt to render one playhead per voice.

**CG17 CRITICAL.** The playhead is driven by the audio thread. Use a lock-free atomic for the position and a UI-side `juce::Timer` at 30 Hz to read it — do NOT call back from the audio thread into the editor. If any processor-owned callback captures editor `this`, it must use `juce::Component::SafePointer` AND clear the callback in the editor destructor. Both, or pluginval segfaults on Automation tests.

### PresetBar (992 × 40)

Left to right: `<` nudge, preset name display, `>` nudge, counter, then right-aligned plugin name and credit.

Display: 210px min width, `background: #0E1218`, `border: 1px solid #2B3040`, VT323 17px `#9FD6C8`, 2px letter-spacing, centred.

Nudge buttons: transparent, `border: 1px solid #2B3040`, `#8E96A8` text, wrap around at both ends.

Counter: `n/10`, VT323 13px `#3E3E46`.

Right side, VT323 13px `#3E3E46`: `ZEDLAB FM -- interface concept: Floyd Steinberg`

**No save button. No dirty indicator.** Factory presets only.

---

## DSP Notes

Signal flow:

```
MIDI note -> Voice (x8) -> 4 sine operators routed by algorithm
          -> carrier sum -> master level -> soft-clip limiter -> output
```

**Operators.** Phase-accumulator sine only. No wavetables, no alternative waveforms. Frequency = `noteHz * ratio * detuneFactor`, where `detuneFactor = 2^(cents/1200)`.

**Modulation (CG-FM-1 CRITICAL).** Sum modulator output **directly** into the downstream phase accumulator:

```cpp
phase += baseFreq + modInput;        // RIGHT
phase += baseFreq + modInput / twoPi; // WRONG -- caps index at 1.0 rad
```

The `1/2π` scaling caps per-stage modulation index and makes cascades sound thin against DX7-lineage references. A/B against Dexed on ALG 1 to verify depth.

**Feedback.** OP1 self-feedback, one-sample delayed output summed into its own phase. Re-verify the feedback clamp after confirming the modulation scaling above — a clamp calibrated against scaled modulator output will behave differently.

**Velocity.** ~~Scales modulator `amp` only, never carrier `amp`.~~ `effectiveAmp = amp * (1 - velAmt + velAmt * vel)`. Play harder → brighter, not just louder. This is the expressive core of FM and the reason `velocity_amt` exists as a parameter.

> **SUPERSEDED 2026-08-01 — velocity applies to EVERY operator, weighted per operator.**
> The formula above is unchanged and still the only one; what changed is
> which operators it applies to and where its depth comes from. Floyd:
> *"velocity for the carriers and modulators. It makes the pianos and pads
> come alive: the stronger you hit the keyboard, the stronger the
> modulation."* Shipped as the DX7/TX81Z model —
>
> ```
> s            = velocity_amt * op{n}_vel
> effectiveAmp = op{n}_amp * (1 - s + s * velocity)
> ```
>
> Sensitivity is a per-operator property of the PATCH, not of the
> operator's current role in the algorithm. The carrier/modulator test
> that used to gate this is gone deliberately: with it, changing algorithm
> silently changed how hard the patch responded to the keyboard. Both old
> behaviours remain reachable — `op{n}_vel = 1` under the default
> `velocity_amt` of 0.7 is the old modulator response exactly, and
> `op{n}_vel = 0` is the old carrier response exactly (ORGAN ships with
> all four at 0, since a drawbar organ has no velocity response).
>
> At full velocity the sensitivity term collapses to 1 regardless of
> `op{n}_vel`, so peak levels — and therefore the whole gain-staging
> chain — are bit-identical to v0.1.0. Velocity can only ever attenuate.

**Pitch bend.** Not in the original spec; added 2026-08-01. 14-bit wheel × `bend_range` semitones resolves to a single frequency multiplier `2^(norm * range / 12)` applied alongside ratio and detune. Computed once per patch per block rather than per operator per voice, and re-pushed at the wheel event itself so a bend lands mid-note rather than at the next block boundary. No smoothing — see the rationale at `FloydFMAudioProcessor::updatePitchBend`.

**Envelopes.** `juce::ADSR` is fine for audio, but it has no "value at time t" query, so the display needs its own evaluation function. **These two must not drift.** Put the envelope shape math in one shared header consumed by both the voice and the editor.

**Limiter (REQUIRED, not optional).** Algorithms 5–8 are multi-carrier and sum coherently at chord onset — roughly +9.5 dB above the monophonic case. Per-algorithm gain trimming cannot fix this because it depends on what the player plays. Soft-knee master limiter: linear at |x| ≤ 0.8, `tanh(x * 2.5)` above. Post-master-level, pre-output.

**Parameter delivery.** Per-block push from APVTS to voices via `getRawParameterValue()` (lock-free atomic read), then setters into plain voice members. Envelope times and initial ratio resolve once at note-on, not per block. Algorithm changes write `std::atomic<bool> needsReset` consumed at the top of `processBlock()`.

---

## Presets

10 factory presets, hardcoded. No user save.

| # | Name | Algorithm |
|---|------|-----------|
| 1 | GLASS BELL | ALG 5 (TWIN PAIR) |
| 2 | E PIANO | ALG 5 (TWIN PAIR) |
| 3 | BRASS | ALG 1 (STACK) |
| 4 | WOOD FLUTE | ALG 7 (PAIR+SINE) |
| 5 | METAL HIT | ALG 2 (TWIN FEED) |
| 6 | SUB BASS | ALG 7 (PAIR+SINE) |
| 7 | ORGAN | ALG 8 (ADDITIVE) |
| 8 | PLUCK | ALG 1 (STACK) |
| 9 | GROWL | ALG 3 (SPLIT MOD) |
| 10 | AIR PAD | ALG 6 (FAN) |

Parameter values are in the `FACTORY` array of `zedlab-fm-brightness-v11.jsx`. Transcribe them exactly.

⚠️ **These presets have never been heard.** They were authored from FM knowledge, not from listening. They are starting points, not audited patches. **Ear-audit is a hard gate before any public build** — every preset played on a held C4 and through a chord, checked for level consistency and musical usefulness. Expect to revise most of them.

Selecting a preset also sets `algorithm`.

---

## Tooltip Strings

| Control | Text |
|---------|------|
| OP tab | Select operator {n} for editing |
| Attack handle | Drag: horizontal sets attack time, vertical sets operator amount |
| Sustain handle | Drag: horizontal sets decay time, vertical sets sustain level |
| Release handle | Drag horizontally to set release time |
| DEPTH slider | Modulation depth -- how much this operator brightens the one it feeds |
| LEVEL slider | Output level of this carrier |
| RATIO slider | Frequency multiplier relative to the played note |
| ALG nudge | Select FM algorithm -- which operators modulate which |
| Preset nudge | Step through factory presets |
| HOLD button | Hold to preview the envelope timing |

ASCII only. No em dashes.

---

## Known Behaviour (do not report as bugs)

- **ALG 8 (ADDITIVE):** all four operators are parallel carriers with nothing modulating them. Four bare sines, no FM. Correct, and looks static.
- **Release handle Y is pinned.** Release always ends at silence; there is no honest meaning for its vertical position.
- **Operator colour never changes on selection.** By design — see Colors.

---

## Build

- JUCE 8, CMake, VST3 + AU
- `CMAKE_OSX_DEPLOYMENT_TARGET=10.13` on every Mac build — without it, minos defaults to the build OS and the plugin silently fails on older machines
- PKGs served directly, never ZIP (ZIP propagates quarantine flags)
- No LV2
- Fixed 1024 × 600, no resize in v1 — so Pattern 1 `localScale` does not apply yet, but write component dimensions as constants rather than literals so resize can be added without a rewrite
- **CG24:** never construct `juce::DropShadow` with `radius = 0` — crashes Reaper on Direct2D
- **CG21:** if resize is added later, debounce cache regeneration behind a timer
- Git: all commits attributed solely to Jonathan Zeppa `<jonathan@zedtronics.net>`. No `Co-Authored-By` trailers.
- Repository is public and MIT licensed — code quality is part of the pitch here in a way it isn't for closed-source plugins.

---

## Halt Conditions

Stop and report rather than guessing:

1. The plugin name or 4-character code has not been confirmed — **do not create the project without them**
2. ~~Any temptation to implement the brightness gradient~~ — **lifted 2026-07-30**, Floyd supplied the metric; see the superseded section at the top of this file
3. Any parameter that cannot be made audible on a held C4 across its full range (CG-P2-AUDIBLE — if you cannot hear it, it is not deployed)
4. Cascade modulation depth that does not match Dexed on ALG 1
5. Bloom rendering that requires work inside `paint()`
