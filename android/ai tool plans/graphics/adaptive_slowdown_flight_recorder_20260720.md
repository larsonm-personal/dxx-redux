# Adaptive slowdown flight recorder design

## Plan

- [x] Inventory the existing Android profiler, render counters, and exportable log path
- [x] Define low-overhead trigger and capture states that distinguish frame caps from stalls
- [x] Bound steady-state CPU cost, capture-time work, log rate, and total file size
- [x] Specify the smallest D1/D2 hooks and shared Android implementation
- [x] Define configuration, log category behavior, output schema, and validation
- [x] Record a recommended implementation sequence and unresolved tradeoffs

## Recommendation

Add an automatic slowdown flight recorder to the existing Android profiler. It
should be enabled by a separate `Automatic slowdown capture` switch, but it
must not open or write a log while merely armed. It continuously retains a
small amount of coarse frame history in native memory. A sustained slowdown
dumps that history and records bounded summaries for 60 seconds under the
existing Profiling log category. Profiling can remain unchecked in the debug
category list.

The first rollout should leave the new switch off by default so its overhead can
be measured on the phone against a clean baseline. If the armed overhead meets
the budget below, turn it on by default in a later change. This retains a switch
for completely disabling it when doing controlled performance comparisons.

## Existing pieces to reuse

- `android_profile.c` already receives game frame begin/end, level and segment,
  broad D2 wait/simulation/render/replay timings, GL swap/GPU/resolve/error
  timings, slow texture and storage operations, rendered-object identity, and
  renderer counters such as textured polygons, water faces, texture binds,
  shader changes, mask draws, and merged-wall cache activity.
- The current manual Profiling mode samples every frame for one second out of
  ten and batches text through JNI. It is useful for an explicitly requested
  run, but continuous periodic output is not an appropriate flight recorder.
- `DebugLog` already produces launcher-exportable files and retains five files.
  Native batched logging is synchronous today, so automatic capture needs a
  bounded asynchronous batch path to avoid putting storage latency in a game
  frame.
- D2 has all broad phase hooks and GL metrics. D1 has frame begin/end and object
  hooks, but is missing the broad phase wrappers and the GL metric handoff. D1
  should be brought to parity with the same small Android-only hook pattern.

## States

`DISABLED`

- One flag check at each frame hook
- No clocks, ring writes, formatting, JNI, or file activity
- Manual Profiling behavior remains available and independent

`ARMED`

- Record fixed-size numeric frame records into a 1,024-entry circular buffer
- Perform no allocation, formatting, JNI calls, or file writes
- Keep detailed per-object clocks disabled
- Retain up to the last five seconds by timestamp when dumping. The larger ring
  accommodates the 200 FPS maximum without dynamic allocation

`CAPTURING`

- Emit the pre-trigger history once
- Continue collecting for 60 seconds
- Emit one bounded batch per second through an asynchronous logger
- Enable detailed object timing for the first two seconds, then for one second
  out of every ten. This provides useful model/object evidence without adding
  per-object clocks for the entire minute
- End early on game exit, and emit a final partial summary on flush

`COOLDOWN`

- Return to the cheap armed monitor for five minutes
- Do not create overlapping or immediately repeated captures
- Record the continuing slow state in the capture-end line, but do not extend
  the capture indefinitely

## Frame record

Use only fixed-width numeric fields in the hot-path ring. Strings belong in the
formatted output, not in every record. A record should include:

- monotonic end timestamp, frame ID, level, and viewer segment
- total, intentional wait, simulation, render, replay, and unaccounted time
- swap, GPU, resolve, and GL error time
- textured polygons, water faces, texture binds and reuses, shader switches,
  mask draws, and merged-wall cache hits/misses
- rendered object count, plus the slowest object identity and time when detailed
  sampling is active
- configured maximum FPS and VSync state

Keep the structure at or below 96 bytes, for a maximum ring allocation of 96
KiB. Existing GL timer queries and renderer counters are reused without adding
GL calls or renderer loops.

Slow texture and storage events should use separate small event rings while
armed, limited to the most recent 16 events of each type. During capture they
can be included in the next one-second batch. No disk logging should occur when
one of these events happens before a frame-rate trigger.

## Trigger design

The detector must not use raw frame duration alone. At a configured 25 FPS,
roughly 39 ms of an otherwise 40 ms frame is intentional wait time. That is a
healthy frame and must never trigger.

Maintain one-second rolling windows and a stable delivered-FPS baseline:

- With VSync off, expected FPS is no greater than the saved `maxFps` value
- With VSync on, learn expected FPS from the best stable one-second window in
  the last 30 seconds. This accommodates 60, 90, and 120 Hz presentation without
  adding Android display-mode plumbing
- Preserve a good baseline across ordinary segment and level changes, but
  suppress triggering for three seconds after a level change
- Treat a gap of 500 ms or more between game draw callbacks as a lifecycle or
  loading discontinuity. Reset the current trigger window instead of counting
  the gap as a slowdown
- Reset the baseline warmup when frame cap or VSync configuration changes

Trigger on either condition:

1. Sustained slowdown: delivered FPS is below 70 percent of expected FPS for
   two consecutive one-second windows
2. Severe stalls: at least three non-wait frames reach 100 ms within two
   seconds, even if a stable baseline has not been learned yet

Comparing delivered FPS to the learned, cap-aware baseline rejects the
intentional 25 FPS sleep while still catching scheduler or event-loop starvation
that occurs outside the measured frame buckets. A slow swap, simulation, render,
driver call, or otherwise unaccounted game-thread stall is also eligible. The
log should state the trigger condition, expected and observed FPS, non-wait
ratio, frame cap, VSync state, and baseline age.

Thresholds should initially be compile-time constants in the shared profiler,
not launcher controls. The log schema records all of them so a device run can
show whether they need adjustment.

## Output rate and size

Do not write every frame for a minute. Convert the native frame history to:

- 100 ms aggregate bins for five seconds before and five seconds after trigger
- one aggregate line per second for the remaining 55 seconds
- the three worst non-wait frame records from each second
- detailed object/model maxima only during the short detail windows
- sparse texture/storage event lines and start/end metadata

Use the existing space-separated `key=value` format with `prof_v=2`. Suggested
types are `capture_start`, `window`, `worst_frame`, `detail`, `texture`,
`storage`, `capture_end`, and `capture_drop`.

Set these hard bounds:

- at most 64 KiB in one JNI batch
- at most 128 KiB queued to the background writer; drop new diagnostic batches
  instead of blocking the game thread when full
- at most 256 KiB contributed by one automatic capture
- at most one capture per five minutes
- five retained debug files, making the automatic recorder's worst-case retained
  contribution no more than 1.25 MiB when no other category is enabled

Every capture-end line reports formatted bytes, queued batches, dropped batches,
ring overwrites, and detail windows completed. A `capture_drop` line is emitted
later if the queue had to discard data.

## Overhead budget

Armed mode targets:

- no heap allocation or locks on the game thread
- no formatting, JNI, logcat, or filesystem calls
- no per-object clock calls
- no more than 128 KiB persistent native memory
- less than 10 microseconds added per frame at 120 FPS, measured at p99
- less than 0.2 percent of one CPU core averaged over a stable scene

Capture mode targets:

- under 20 microseconds per ordinary frame outside short detail windows
- one formatting pass and one asynchronous handoff per second
- no synchronous flush or file open on the game thread
- less than a 1 FPS difference between recorder-off and armed A/B windows in a
  stable uncapped scene

The current disabled path should also be tightened: check whether neither manual
profiling nor the flight recorder is active before reading clocks. In particular,
`android_profile_bucket_end()` currently reads the monotonic clock before its
inactive-frame check. Reordering that check removes profiler cost when both
modes are disabled.

## Code shape

Keep state, thresholds, ring storage, aggregation, and output formatting in the
shared Android profiler. If deterministic detector tests make `android_profile.c`
unwieldy, extract only the pure numeric state machine to
`android_slowdown_detector.c/.h`; do not duplicate it in D1 and D2.

Small Android-only changes in both games should:

- give D1 the same wait, simulation, render, replay, and GL metric hooks as D2
- pass current max-FPS and VSync state as frame-pacing context
- preserve the existing object hooks, with the shared profiler deciding whether
  detailed timing is active

Kotlin changes should:

- add `Automatic slowdown capture` in Advanced settings, separate from the
  Profiling category checkbox
- synchronize that preference to native through one boolean JNI setter
- add a forced asynchronous batch entry point that can lazily create the normal
  exportable debug log without enabling continuous Profiling
- use a single bounded background writer queue and retain current launcher open,
  save, share, refresh, and deletion behavior

Do not add a new dependency or a second log viewer/export mechanism.

## Validation

Add deterministic native tests that feed synthetic frame records:

- 25 FPS frames with about 39 ms wait and 1 ms work never trigger
- a cap change from 120 to 25 FPS resets warmup and never triggers
- a stable 120 FPS stream falling to 20 FPS due to render time triggers after
  two slow windows and captures pre-trigger history
- a stable VSync 60 FPS stream with isolated hitches does not trigger
- three 100 ms non-wait stalls trigger the severe path
- loading, background, and resume gaps do not trigger
- capture ends at 60 seconds, observes cooldown, and respects byte and queue caps
- D1 and D2 records contain the same broad phase and GL fields

Then run the normal Android unit tests, all-ABI debug build, D1/D2 Windows builds,
and existing graphics regression scenarios. On-device validation should compare
three otherwise identical uncapped windows: recorder disabled, recorder armed,
and a forced capture. Report CPU frame buckets, FPS, file bytes, and dropped
batches. The manual Profiling category should remain off during this A/B so its
periodic sampler does not contaminate the result.

## Implementation sequence

1. [x] Add and test the pure trigger/ring state machine, including cap-aware
   wait handling and all hard limits
2. [x] Integrate cheap armed collection into the shared profiler and fix the
   fully disabled fast path
3. [x] Add D1 phase/GL parity and the small pacing-context hook in both games
4. [x] Add the Advanced setting and bounded asynchronous forced-batch logging
   that is independent of the manual Profiling category
5. [x] Add aggregation/detail output, detector tests, and file-size limits
6. [x] Build and run automated regressions, then prepare the three-way phone A/B
7. [ ] Decide whether the switch should default on based on measured armed
   overhead

## Implementation result

The feature is implemented behind `Advanced -> Automatic slowdown capture` and
defaults off for the first phone A/B. It has a dedicated native enable flag.
Neither detection nor triggered output checks or enables the manual Profiling
category. Triggered output uses a forced asynchronous batch path, so a log file
is created lazily even when every debug logging category is unchecked.

The detector retains 1,024 fixed-size records in about 106 KiB, ignores the
intentional portion of frame-limit waits, emits five seconds of 100 ms history,
then emits one-second summaries and the three worst frames for 60 seconds. The
output is tagged `PROFILING`, uses `prof_v=2`, has a 256 KiB capture limit, and
enters a five-minute cooldown. Normal armed operation performs no formatting,
JNI, logcat, or file I/O. Detailed rendered-object clocks run only in short
windows after a trigger.

Validation completed:

- Android debug APK built for arm64-v8a, armeabi-v7a, and x86_64
- all Android debug unit tests passed
- Windows D1 and D2 builds and complete native CTest suites passed
- new detector tests passed in both suites, including intentional 25 FPS wait,
  120-to-25 cap change, sustained render and scheduler slowdowns, severe stalls,
  60-second end, cooldown, and the 128 KiB detector-memory ceiling
- scoped clang-format, ktlint, CMake format/lint, BOM lint, and `git diff --check`
  passed

Phone use: enable only `Automatic slowdown capture` in Advanced. Leave the
`Profiling` logging category off. Reproduce a sustained slowdown, keep playing
for about one minute after it begins, then return to Advanced and export the
new log. A successful independent trigger begins with
`prof_v=2 type=capture_start ... manual_profiling=0`.

## Healthy coop baseline review

- [x] Identify enabled categories and profiling record types in the supplied log
- [x] Quantify frame, phase, GL, renderer, object, texture, and storage distributions
- [x] Separate intentional frame pacing and instrumentation cost from actionable work
- [x] Record low-risk optimization candidates or conclude that no change is justified

The 1.20 MB, roughly 20-minute log has manual Profiling enabled. It contains 104
one-second samples, 2,619 sampled gameplay frames, 950 UI-poll summaries, 33
texture bursts, 11 individually slow level-load textures, and no `slow_frame`,
storage, or automatic `prof_v=2` capture records.

All sampled gameplay stays at the intentional 25 FPS cap. Frame time averages
39.85 ms, of which 38.16 ms is limiter wait. Simulation averages 0.163 ms and
has a 0.431 ms p99. CPU render averages 1.52 ms, with 3.26 ms p95, 4.91 ms p99,
and a single 7.66 ms maximum. GPU time averages 1.62 ms, reaches 3.63 ms p99 and
4.65 ms maximum, while swap reaches only 0.252 ms p99. GL errors are zero.

Render time correlates primarily with ordinary scene complexity: textured
polygons `r=0.767`, GPU time `r=0.741`, and texture binds `r=0.695`. Water-face
count has a much weaker `r=0.298`. Source inspection confirms `TMI_WATER` only
increments the diagnostic counter in this render path; water faces do not use a
special draw routine. Animated effects are advanced once in simulation rather
than once per visible face, and the sub-millisecond simulation measurements
rule that loop out as a concern in this run. Higher render time in water-bearing
views is therefore best explained by the rest of those scenes, not animation
thrash.

The only conspicuous costs occur during level loading. Two large bursts load
3,843 textures in 0.97 seconds and 2,955 textures in 0.64 seconds. A few stock
textures take 31-35 ms in upload or mask work, but they occur during cache flush
and level setup, not sampled gameplay, and caused no frame-rate incident. This
could be a separate level-transition latency investigation, but it does not
justify a normal-renderer change from this evidence.

No performance code change is recommended from the healthy run. For ordinary
testing, leave manual Profiling off and Automatic slowdown capture on. That
avoids the manual sampler's 2,619 per-frame text records, 950 once-per-second UI
records, and 1.20 MB file while retaining automatic evidence if performance
actually falls. The new `prof_v=2` capture already includes level, segment,
object, render, GL, and pacing context that this healthy v1 frame stream lacks.
