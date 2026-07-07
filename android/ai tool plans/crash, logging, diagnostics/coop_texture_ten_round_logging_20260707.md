# Coop texture ten round logging

## Goal
- Add a broader logging pass for the D2 level 7 coop texture issue
- Assume the current log and the next several focused logs will be insufficient
- Capture enough source, merge, upload, draw, and framebuffer evidence to reduce repeated device round trips

## Plan
- [done] Re-read project instructions and inspect existing diagnostics
- [done] Identify the largest remaining unknowns after the latest log
- [done] Add compact, gated logging for those unknowns
- [done] Run scoped formatting and code quality on touched files
- [done] Summarize the new log signals and how to use them

## Added Logging
- `[mwall_merge_ref]` recomputes the old CPU texmerge output independently and compares it against the live cached merged bitmap
- `[mwall_texread]` attaches uploaded GL textures to a small FBO, reads level 0, hashes RGBA, and compares against the expected palette-expanded RGBA when source dimensions match
- `[mwall_single_draw]` records old single-texture draw state for the target face: geometry, UV bounds, vertex colors, GL state, texture bindings, and bitmap hash
- Tap/probe logging now forces texture readbacks for the selected base, overlay, and merged textures, even if automatic one-shot readback already happened earlier
- Snapshot last-draw state is now populated by the old single-texture path, so probe snapshots can report the bypassed old-texmerge draw state

## Validation
- Passed scoped code quality:
  `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\cpp\shared\merged_wall_debug.c','android\app\src\main\cpp\shared\merged_wall_debug.h','d1\arch\ogl\ogl.c','d2\arch\ogl\ogl.c')`
- Passed Android native build:
  `.\gradlew.bat :app:externalNativeBuildDebug`
