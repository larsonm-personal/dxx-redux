# Single-player level-load freeze log review

- [x] Read repository debugging instructions and identify the attached log scope
- [x] Locate the reported single-player level-start and launcher save-load intervals
- [x] Correlate long timestamp/frame gaps with profiling, lifecycle, texture, and load events
- [x] Trace implicated events to the relevant source paths and report evidence, confidence, and gaps

## Findings

- The single-player capture records one 4.647960-second gap between consecutive
  frame-begin callbacks at about 23:18:13.122 through 23:18:17.770. The app
  remained foreground/running, and surrounding simulation/render work was only
  tens of milliseconds.
- The gap is outside the profiled draw callback. This matches the synchronous
  fresh-level path entered after a briefing closes: `StartNewLevel` calls
  `StartNewLevelSub`, which performs `LoadLevel`, texture paging/caching, sound
  setup, music transition, and the remaining level initialization before the
  next draw event.
- Startup resume calls `state_restore_all_path` before the main event loop and
  before the first profiled gameplay draw. Its perceived freeze cannot be
  measured by the current frame flight recorder.
- The export enabled Coop Desync and Dormancy, not Game. Single-player restore
  milestones use the Game category, while the useful level texture phase logs
  are currently forced only for multiplayer. The log therefore proves the
  game-thread stall and its outer path but cannot identify the slow inner phase.
- The same file contains several long cooperative waits, including a 6.299-second
  level synchronization at 23:06:41.926, but those are unrelated to the reported
  single-player freeze.

## Diagnostic implementation

- [x] Add paired D1/D2 structured level-load and level-initialization timings
  under the existing Profiling category
- [x] Add paired successful save-restore phase timings, including startup restore
  before the first game frame
- [x] Run scoped formatting, host builds, and relevant tests

## Validation

- Scoped Android code-quality checks and `git diff --check` passed.
- Paired Windows D1/D2 builds passed.
- Android debug builds passed for arm64-v8a, armeabi-v7a, and x86_64.
- Host tests passed: D1 32/32 and D2 38/38.
- Emulator smoke testing emitted the new Profiling records. The observed D2
  level-one load reported 1,800,099 microseconds total, including 1,694,518
  microseconds in texture paging.
