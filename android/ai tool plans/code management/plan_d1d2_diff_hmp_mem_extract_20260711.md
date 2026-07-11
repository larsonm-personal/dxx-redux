# Plan: D1/D2 HMP Memory Parser Extraction 2026-07-11

## Goal
- Remove the duplicated Android HMP memory-buffer parser from the original D1/D2 engine files while preserving each file's original cross-platform HMP-to-MIDI converter

## Existing work to preserve
- Preserve the completed PhysFS extraction and pending upstream-sync changes
- Preserve all unrelated workspace and mission metadata changes
- Keep desktop HMP file loading and conversion behavior unchanged

## Steps
- [x] Compare the D1/D2 Android HMP blocks, allocation differences, ownership, and failure paths
- [x] Identify the smallest shared parser API and local converter callback boundary
- [x] Move only branch-added Android memory-buffer behavior into a shared helper
- [x] Add focused parser/conversion coverage for both games or the shared implementation
- [x] Run scoped formatting and refresh D1/D2 diff metrics
- [x] Build both Windows targets and all configured Android ABI/game combinations
- [x] Run focused Android MIDI playback/preview integration coverage and record results
- [x] Record deferred HMP work and the next recommended cleanup candidate

## Guardrails
- Do not move the inherited static `hmptrk2mid` implementation out of either engine file
- Preserve byte validation, track allocation, buffer ownership, and every cleanup path
- Keep D1 and D2 call sites minimal and mirrored
- Do not change TinySoundFont playback or launcher preview policy in this tranche
- Avoid adding an abstraction whose adapter code approaches the duplicated body size

## Baseline
- Aggregate D1/D2 diff: 343 files, +50206/-3880 against `upstream/main`
- `d1/misc/hmp.c`: +126/-0
- `d2/misc/hmp.c`: +125/-0
- Expected combined reduction: approximately 220 to 235 additions

## Current result
- Aggregate D1/D2 diff: 341 files, +49969/-3880 against `upstream/main`
- `d1/include/hmp.h` and `d2/include/hmp.h`: clean against upstream
- `d1/misc/hmp.c`: +12/-0
- `d2/misc/hmp.c`: +12/-0
- Combined reduction: 237 additions from the original D1/D2 files

## Implementation
- Added `shared/hmp_android_shared.c` as the single memory parser and MIDI assembly implementation
- Kept each inherited static `hmptrk2mid` converter in its original engine file and passed it through a narrow callback
- Kept each game's `tempo` bytes local and passed them to the shared converter
- Compiled the helper separately into `d1_misc` and `d2_misc`, preserving each allocator family and engine headers
- Removed the private `hmp_open_mem` API and restored both upstream `hmp.h` files
- Replaced handwritten launcher declarations with `hmp_android_shared.h`

## Validation
- Scoped format, lint, UTF-8 BOM, CMake format, and CMake lint checks pass
- `git diff --check` passes with only existing CRLF normalization notices
- Automation catalog validation passes: 47 standalone JSON tests, 15 support scripts, and 36 PowerShell entries
- All nine pinned D2 dependencies exist and match their declared SHA-256 values
- Windows `run-windows-build.ps1 -Target both` passes for D1 and D2
- Forced `:app:externalNativeBuildDebug --rerun-tasks` passes for D1 and D2 on arm64-v8a, armeabi-v7a, and x86_64
- `:app:assembleDebug` passes and packages the rebuilt native libraries
- `test_midi_preview_hmp_unified.json5 -Game d2` passes 10/10 steps: seven built-in tracks enumerate, `descent.hmp` plays, position reaches 6058 ms, duration is 183460 ms, and stop state is confirmed
- `test_music_track_controls_unified.json5 -Game d1` passes 36/36 steps
- `test_music_track_controls_unified.json5 -Game d2` passes 35/35 steps
- The first APK install attempt timed out on an unresponsive overnight emulator; a cold emulator restart restored adb/package-manager health and the unchanged APK passed all tests

## Deferred HMP work
- Launcher preview loads the D2 library explicitly, so the focused test directly covers the D2 wrapper and shared implementation
- D1 receives full desktop and Android compile/link coverage plus file-based in-game music regression coverage, but not a direct runtime call to D1 `hmp2mid_mem`
- A dual host synthetic-HMP test would require expanding host test registration and making the Android adapter test-callable; defer that infrastructure work unless the memory converter gains more behavior
- Preserve the inherited signed-overflow-prone length arithmetic, unchecked reallocations, and limited event bounds checking in this extraction-only tranche

## Next candidate
- Prefer the paired Android save/load dispatch in `d1/main/gamecntl.c` and `d2/main/gamecntl.c`
- The two current 155-line blocks are identical after normalizing D1/D2 labels and the two state function signatures, for an exact 310-addition engine-file payoff
- Move them into the existing `android_meta_actions.c` with two direct handlers and small `DXX_BUILD_DESCENT_II` adapters; no new CMake target or callback layer is required
- Preserve dispatch priority, pause-window behavior, multiplayer/dead-player restrictions, autosave quit/minimize behavior, and rewind host/joiner routing
- Validation should include both host and Android matrices, paired pause-menu coverage, quick-record sidecar coverage, and a new unified save/load dispatch integration script
