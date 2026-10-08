# Immediate Video Info graphics edits

Apply Video Info edits at the next safe game-thread render boundary, protecting the accepted settings before GL work. Keep the existing 2500 ms shared confirmation quiet period and five-second confirmation countdown

1. Reuse durable preview ownership for live Video Info edits, allowing further edits during the quiet period
2. Keep the Android watchdog independent of rendering without opening a modal while editing
3. Extend the existing Video Info integration test to require applied settings before confirmation, then verify both engines, recovery, scoped quality and the Android native build

Status: implemented and verified

## Result

Video Info changes now enter a durable, nonmodal live preview and apply at the next main-view render boundary. Subsequent edits update the same owned attempt and restart only the confirmation quiet period. Presented-frame acknowledgements protect the transition to confirmation. The Android watchdog monitors application even before a modal exists; lifecycle interruption, rendering failure and process death retain rollback to the accepted tuple

The first-run editor keeps its existing behavior. Video Info still suppresses confirmation for a return to the accepted tuple or an all-off selection

## Verification

- Android x86_64 debug build passed for both engines
- Native graphics safety store CTest passed, including owned baseline/all-off preview completion
- Video Info integration passed for D1 and D2: immediate presented changes within the one-second test bound, editable controls before confirmation, Cancel, Back, timeout, edit-back suppression and tray pause ownership
- Pre-confirmation render-stall recovery passed for both engines, preserving accepted settings and mirrored configs, with process recovery in approximately 8.3 seconds
- First-run Accept and cross-engine handled-marker relaunch checks passed for both engines
- Existing graphics confirmation and deferred launcher edit tests passed for both engines
- D2 MSAA allocation-failure and capability tests passed with failures captured during preview
- Scoped formatting/lint, both automation catalog checks and git diff whitespace checks passed

Evidence: `android/temp/video-immediate-*.log`, `android/temp/graphics-video-overlay-20261007-090029/`, `android/temp/graphics-recovery-20261007-090350/`, and `android/temp/graphics-first-run-20261007-090522/`

The final integration runs used an isolated `emulator-5584` because another task was using `emulator-5582`. The emulator supports MSAA but not AF; AF uses the same shared application path, with unavailable-feature behavior covered by the capability test
