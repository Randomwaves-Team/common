# randomwaves-common

Code shared by Drumboy Pro and Synthgirl Pro (STM32H723), used as a git submodule named `common` in both firmware repos.
Include path is this folder, so includes carry the folder name: `#include "lcd/Lcd.h"`, `#include "ui/TabStrip.h"`.
Full description, interfaces and how a product uses them: `docs/shared/common-code.md` in the Randomwaves workspace.

Contents:
- `lcd/` — NT35510 driver (fast column / curve / dotted-line drawing), fonts, icons, `LcdTypes.h` (colors, RGB565 types, LCD bus macros)
- `panel/` — center panel: `Panel.h/.cpp` (Surface, View, Renderer), `Graph.h/.cpp` and `GraphMath` (column composer, axes, math),
  one `…Graph` class per shared panel (FILTER, EQ, REVERB, MIXER, LFO, EFFECT, RHYTHM, METRO, SYNC); products add their own
  graphs through a `GraphList`
- `ui/` — `Layout.h` (tab / bar / status box / panel geometry), `EncoderInput` (AB decoding, acceleration), `Param.h`
  (ParamDef, ParamSource, Page), `TabStrip` (8 tabs: headers, values, units, bars, labels; diff redraw), `Menu.h`

Planned: `Dsp/` (Filter, Eq, Effect, Reverb, Lfo), `Io/` (MIDI, SYNC), `File/` (file versioning and conversion).

Rules: see `docs/shared/HANDOFF.md`. One product edits a module at a time; every change is a separate commit with a HANDOFF note
for the other product.
