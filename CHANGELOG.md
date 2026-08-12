# Changelog

All notable changes to FloydFM are documented here.

## [0.2.1] - 2026-08-12

The changes below respond to the first user feedback report (2026-08-09,
from an LV2 user on Ubuntu Studio running jalv -- thank you!). Windows
VST3 and Windows LV2 binaries ship these fixes; the macOS package and the
Raspberry Pi (Rust) LV2 bundles are unaffected or pending (the Pi build
has no JUCE UI, and the Mac 0.2.1 build follows in a Mac session).

### Changed

- Routing diagram: direction arrowheads now rotate to lie along their
  connection line. Previously every arrowhead pointed straight down,
  which made diagonal connections ambiguous.
- CARRIER / MODULATOR labels under the operator tabs moved down, clear
  of the tab pill, and grew from 9px to 10px so they no longer crowd
  the buttons.

### Answers to the report's other notes

- **Fonts a bit small in general:** the panel zooms. Drag the window
  corner (resize the window in jalv) and the whole UI scales from 0.75x
  to 2.0x, fonts included.
- **jalv's window title only updates when a preset is picked from the
  preset menu, not from the in-plugin arrows:** correct observation, and
  it is host-side. The plugin's arrows change the program internally, and
  LV2 has no plugin-to-host program-change notification, so jalv cannot
  know. The plugin's own preset bar always shows the current name.
- **No VST3 produced by the Linux build:** the README's Linux
  instructions only build the LV2 target. On Linux,
  `cmake --build build --target FloydFM_VST3` works too and produces a
  VST3 that VST3 hosts (Bitwig, REAPER, etc.) can load.

## [0.2.0] - 2026-08-01

- Per-operator velocity sensitivity (DX7 model, the `vel` column), on
  Floyd Steinberg's request.
- Pitch wheel support with adjustable bend range.
- macOS release 2026-08-08: notarized Universal AU + VST3 installer.

## [0.1.0] - 2026-07-31

- Initial release: 4-operator FM synth, 8 algorithms, 10 factory
  presets, envelope brightness gradient (2026-07-30), VST3 / AU / LV2.
