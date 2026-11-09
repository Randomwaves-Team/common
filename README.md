# randomwaves-common

Code shared by Drumboy Pro and Synthgirl Pro (STM32H723), used as a git submodule named `common` in both firmware repos.

Planned contents (moved here step by step from drumboy-firmware):
- `Lcd/` — NT35510 driver, fast drawing (drawVSpan, drawColumn, dotted lines), fonts, colors
- `Panel/` — Panel.h / Panel.cpp (hardware independent, Surface interface) and the PC preview tool
- `Ui/` — tab, value bar, ADD second value and selection arrow rules, CPU / MEM box
- `Dsp/` — Filter, Eq, Effect (9 types), Reverb, Lfo, shared math (tables, fast functions)
- `Io/` — MIDI and SYNC
- `File/` — file versioning and conversion

Rules: see `docs/shared/HANDOFF.md`. One product edits a module at a time; every change is a separate commit with a HANDOFF note for the other product.
