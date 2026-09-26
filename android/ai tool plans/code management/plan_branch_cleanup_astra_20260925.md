# Branch cleanup and original-file diff reduction, 2026-09-25

## Goal and approach

Review all branch changes for simplification and cleanup, and reduce avoidable
changes to inherited D1/D2 files. Start with a validated batch and leave a ranked,
current continuation list. This is an Astra review; older worker/model dispatch
instructions describe previous campaigns and are not adopted for this round.

Prefer deleting duplication and unnecessary mechanisms over simply relocating
lines. Shared feature owners should need fewer interfaces than the code they
replace. Preserve game behavior, ordering, formats, diagnostics, and desktop
builds. Keep canonical engine policy and private structures with their owners.
Do not format inherited files broadly or deduplicate upstream implementations.

## Recovered history

- `../reusable/cleanup.md`: behavior-preserving slices, scoped quality/builds,
  focused integration coverage, and separate inherited-file metrics
- `d1d2_diff_shrink_study.md` and the May/July refresh plans: earlier extraction
  boundaries and explicit warnings against moving tightly coupled engine policy
- `plan_d1d2_diff_minimization_campaign_20260711.md`: completed low-risk campaign
- `plan_d1d2_high_coupling_cleanup_campaign_20260712.md`: completed player-file,
  controller, host migration, menu, classic-demo, and replay command work
- `d1d2_diff_minimization_ledger_20260811.md`: only chunks 001 (texture lookup)
  and 002 (FOV policy) completed; overlay, virtual gamepad, scene profiling, and
  D2 replay helper residue remain candidates, subject to current-source review
- `../gameplay/guidebot_minimize_upstream_diff_20260823.md` and
  `../gameplay/guidebot_route_architecture_simplification_20260827.md`: newer
  Guidebot extraction and compiled selection supersede older routing candidates
- `../networking/coop_recovery_simplification_20260906.md`: current recovery
  semantics require preserving drop identity, remaining quantities, and fencing

## Baseline and ownership

- Branch: `cmake`
- Starting HEAD: `2ebbacf3a464ee8a3359a9f3915706d606205815`
- Branch base: `fb555eec75e1ed12c8348805ab335afb4c721b06`
- Local upstream reference: `9fd90f03513663ce1372c8cfa723b7a73c4219fa`
- Branch total: 4,683 changed files, +1,542,851/-6,868 lines, including data
- D1/D2 total: 414 changed files, +73,417/-6,864 lines, including new feature files
- Existing untracked `../gameplay/guidebot_original_routing_scope_20260925.md`
  belongs to separate research and must remain untouched
- Scratch inventories, comparisons, and build logs: `temp/cleanup_astra_20260925/`

## Steps

- [x] Recover earlier plans and establish branch/worktree boundaries
- [x] Inventory the complete branch by ownership, source/data, and changed area
- [x] Recheck outstanding August candidates and newer high-churn changes
- [x] Implement coherent first batch with measured inherited and total reduction
- [x] Run scoped quality, Windows and Android builds, and focused validation
- [x] Review resulting diff and record evidence and ranked follow-up work
- [x] Obtain a fresh complete APK and retry the focused launcher tests
- [x] Complete D1/D2 emulator input and overlay validation when the device window is stable

## Initial priorities

1. Texture-label rendering: duplicated bodies in `gamerend.c`; existing shared
   texture debug owner already owns labels and RGB state, so one draw entry
   point can replace both bodies without callback infrastructure
2. Scene profiling: duplicated read-only object scan in `game.c`; assess placing
   it with profiling while compiling against each game's canonical object types
3. Virtual gamepad registration: inspect a compact shared initializer that takes
   only mapping/name arrays and returns counts; reject private-layout adapters
   larger than the removed initialization or changes to Kotlin index semantics
4. New networking automation bodies: significant growth since August, but
   private packet/queue state may make a mechanical extraction a poor tradeoff
5. Branch-owned code outside D1/D2: use the whole-branch inventory to identify
   repeated mechanisms and obsolete wrappers, rather than only ranking raw size

## Validation and outcomes

### Expanded first batch from whole-branch duplicate scan

- Remove the no-op `CrashLog.install` method, its private flag, and two activity
  calls; `DxxReduxApp.attachBaseContext` already owns xCrash initialization and
  native breadcrumb setup remains in place
- Remove the multiplayer copy of `TvButtons.kt`; its four composable bodies are
  exactly identical to the app package implementations. Import those existing
  wrappers explicitly so TV focus behavior and all defaults remain unchanged
- Remove nine tracked `mission-metadata-{core,cli}/bin/**/*.kt` artifacts after
  byte-for-byte comparison with their `src/main/kotlin` or `src/test/kotlin`
  originals. Gradle uses the standard source directories and no build/script
  references point to these bin copies. Add only module-specific ignore rules

### Current inventory

The complete tracked branch inventory contains 4,683 paths. Ownership breakdown:
414 engine paths, 509 Android native paths, 234 Android JVM paths, 911 test paths,
36 server paths, 1,856 plans, and 723 other paths (including generated mission
metadata, fixtures, scripts, build configuration, and metadata JVM modules).
The largest raw files are generated mission JSON, so raw added-line ranking is
not a useful cleanup order. No generated mission baselines were changed.

At the frozen branch base, the engine surface is 352 inherited paths with
+43,710/-6,864 and 62 new feature files with +29,707/-0. Against the local upstream
tip, it is 344 inherited paths with +43,007/-6,717 plus the same new files.
The duplicate scan inspected branch-added C/C++/Kotlin/PowerShell/Rust sources;
its overlapping 12-line matches are discovery signals, not proof of equivalent
semantics. The whole inventory is not a claim that every line has been reviewed.

### Continuation queue after the first batch (historical)

1. **Virtual gamepad initialization** (`d1/arch/sdl/joy.c`, paired D2 file)
   remains the best next original-file candidate. Both still build the same
   eight base axes, three combiner axes, ten buttons, twelve axis buttons, and
   four D-pad buttons. A shared initializer can take the five mapping/name
   arrays while local code retains SDL handles/counts and joystick lifetime.
   Aim for roughly 100 inherited lines removed; preserve axis-button `-1`
   sentinels, deadzones, allocation/free ownership, and Kotlin indices. Validate
   descriptor output and both games' maintained axis/controller scripts before
   accepting the boundary. Do not expose the whole private joystick structure.
2. **Duplicated SHA-1 implementation** in `extract/extract_cd.c` and
   `extract/fingerprint_cd.c`: each has its own context, transform, update,
   finalization, and hex formatting. Share the hash mechanism without changing
   fingerprint semantics or introducing another dependency. First check byte-order,
   chunking, formatting, and failure paths with known vectors and existing CD
   fingerprint/extraction fixtures. Roughly 170-200 duplicate lines are plausible.
3. **Preview audio ring buffer/OpenSL mechanics** in `shared/cd_preview.c` and
   `shared/midi_preview.c`: substantial exact overlap, with a third ring-buffer
   implementation in `rbaudio_bin.c`. Start with an explicit buffer instance,
   not a generic playback framework. Preserve acquire/release ordering, reset
   locking, callback lifetime, and each decoder's transport behavior. Requires
   concurrent preview/seek/stop/underrun coverage, so deferred from this batch.
4. **Dual-emulator setup scripts** (`test_dual_emu.ps1` and
   `test_dual_emu_setup.ps1`): repeated provisioning/log/process helpers remain
   despite the common helper imports. Both are interactive; preserve their
   different NAT/server lifetimes when sharing setup and owned-process cleanup.
5. **D2 replay diagnostic residue** from August chunk 006 remains small and
   coherent, but requires exact final-state/RNG comparison. It should follow
   simpler duplication removal rather than reopening simulation policy.

Do not reactivate broad Guidebot or D1-in-D2 movement from an old ledger: both
have substantial newer design work and concurrent edits in this worktree.
Networking has also grown: roughly 180 lines of paired introspection fixtures
now live beside private reliable-queue state. Moving them is only worthwhile
if their queue/ACK hooks are compact; exporting private packet structures merely
to lower original-file line counts would make maintenance worse.

Gyro single-field migration, launcher file-set migration, and schema fallbacks
are separate simplification candidates under the pre-release-format policy.
They were not deleted based on a `legacy` name alone: some similarly named audio
IDs and accessibility actions describe supported formats/APIs, not obsolete app
compatibility. Audit current producers/defaults before changing persisted values.

### First-batch evidence

- Overlay drawing now has one implementation in `android_texture_debug.c`;
  render sites retain their original ordering, label reset, and Android guards
- Scene-object scanning now has one implementation in `android_profile.c`,
  compiled against each game's canonical headers; no callback, allocation,
  second traversal, new source file, or CMake registration was needed
- Whitespace-normalized comparison against frozen HEAD confirms both games'
  scan and overlay statement sequences are unchanged, apart from removal of
  the redundant function-local RGB `extern` declaration
- Nine generated metadata copies were byte-identical to canonical source/tests
- Four TV button wrapper bodies were identical before consolidation; nine
  multiplayer consumers now import the existing app functions explicitly
- Whole-batch owned-path manifest: `temp/cleanup_astra_20260925/owned_paths.json`
- Scoped mixed-language quality passed; Windows D1/D2 builds passed
- Initial Android compile exposed the still-needed label-reset header; restored
  it in both original render files. Build startup retention also requires serial
  build execution, so overlapping attempts were discarded and rerun serially

### Final first-batch metrics and validation

- Cleanup-only patch: **+99/-1,040, net 941 lines removed**, excluding this plan
- Original engine files: **120 fewer branch-added lines**. Each `game.c` loses
  31 added lines; each `gamerend.c` loses a net 29 after its new include/call
- No new product source/header/test file or abstraction was introduced
- Metadata output cleanup removes 786 redundant lines; TV wrapper consolidation
  removes 93 net lines; the dead crash-install hook removes another 15 lines
- Scoped mixed-language formatting/lint passed, and final owned-path
  `git diff --check` passed
- Windows D1 and D2 builds completed successfully before the later concurrent
  Guidebot/controller edits. Subsequent cleanup-only changes to original source
  are an Android-guarded include restoration and original EOF whitespace
- All **24 relevant Android CMake object targets** passed: both games' `game.c`,
  `gamerend.c`, `android_profile.c`, and `android_texture_debug.c` for arm64-v8a,
  armeabi-v7a, and x86_64. No new cleanup-caused compiler warning was observed
- Android production Kotlin and Java compilation passed with the consolidated
  wrapper imports. A warning in concurrent controller code was left untouched
- All **7 existing renderer contract tests passed**
- All **8 metadata-core JVM tests passed on a fresh `--rerun-tasks` run**;
  metadata CLI assembly/build also completed in the earlier combined invocation
- Full APK assembly remains blocked by concurrent Guidebot changes:
  `guidebot_route.c:447` calls `escort_set_goal_object`, whose declaration is no
  longer present in the live header. This is outside the cleanup-only patch
- The requested two launcher test classes could not run because Kotlin compiles
  all test sources before applying the runtime test filter. Concurrent controller
  changes removed `shouldDispatchGamepadButtonDown`, `shouldDispatchGamepadButtonUp`,
  and `shouldRouteControllerBToNativeBack` while `GamepadButtonDispatchPolicyTest`
  still references them. Production Kotlin compilation itself passed
  The cleanup run's Gradle client was stopped after this recorded compile
  failure remained pending; no unrelated build process was stopped
- No emulator smoke result is claimed: a fresh APK could not be assembled.
  `render_smoke.jsonc` in the scratch directory is ready for both games once the
  unrelated compile errors are resolved. It uses the maintained axis test's
  launch/dependency setup, then enables/disables texture labels during gameplay
- Scratch `cleanup-only.patch` separates this work from later overlapping edits;
  a non-mutating reverse-apply check passed against the live worktree

### Concurrent work excluded

During this task, unrelated edits appeared in `android/outstanding_bugs.md`,
the D1 usability plan, `test_upstream_compat.cpp`, D2 AI/polygon/D1-in-D2 asset
sources, and new Guidebot/OpenGL plans. Preserve them and report this batch's
metrics using the cleanup-only patch rather than the whole worktree. Later
controller and Guidebot work also changed `MainActivity.kt` and `d2/main/game.c`:
only the removed crash-install call and moved scene scan in those files belong
to this batch. Their other edits were preserved, not formatted or reverted.

## Batch 2: fixed virtual-gamepad initialization

Previous goal turn classification: progress (first-batch edits and verification).
The live tree still contains the first batch and substantial concurrent feature
work. The Guidebot full-build blocker remains; the obsolete controller test has
now been removed by that work, so its earlier failure is historical.

The paired `joy.c` initialization blocks remain clean and identical. Extract
their fixed labels and mappings to one small C implementation. Its only inputs
are the three map arrays and two owned-name arrays; no private engine structure,
callback table, or new lifetime state is exposed. Retain `d_strdup` allocation,
all physical/virtual indices, unused slots, and local SDL counts/reset/deadzones.

- [x] Add shared initializer and paired compact call sites
- [x] Verify the persisted virtual-ID/name contract and untouched array tails
- [x] Run scoped quality, both host contract builds, and Android builds
- [x] Run maintained D1/D2 input integration when a fresh APK can be assembled
- [x] Record metrics, validation, and remaining blockers

## Batch 3: one extraction SHA-1 implementation

Live inspection found a third SHA-1 implementation in `inno_reader.c`. Reuse
that smaller byte-order-safe implementation for the two CD tools as well, with
an explicit incremental context and namespaced functions. Keep its copyright
notice with the moved code. No new dependency, checksum format, parser policy,
or file-I/O behavior is needed. Preserve the existing lowercase hex output.

- [x] Move the installer implementation to `extract/sha1.c/.h` and wire all users
- [x] Verify known digests, padding boundaries, chunked updates, and million-byte input
- [x] Build CD/fingerprint/installer tools and run synthetic fingerprint and installer coverage
- [x] Compile Android users for all configured ABIs and run scoped quality
- [x] Record net removal and remaining validation

### Batch 2/3 evidence so far

- Shared gamepad initializer preserves allocation order, all fixed IDs and labels,
  the unused button-map slots, virtual-axis `-1` sentinels, and untouched tails
- The registered gamepad contract fixture passed in two isolated Windows CMake
  targets against the real D1 and D2 headers
- Each inherited `joy.c` has +12/-59, a combined net reduction of 94 lines
- SHA-1 known-vector coverage includes 12 binary lengths around padding/block
  boundaries with six update sizes, `abc`, and one million `a` bytes
- Added reusable `cd_sha1_cli_tests`: each built CD tool reads a synthetic CUE
  with data and audio tracks, and its two digests must match CMake `file(SHA1)`
- All five focused extraction CTests passed: SHA-1 vectors, CD CLI integration,
  CD read contracts, incomplete-read rejection, and the existing GOG reader suite
  against both local D1/D2 GOG installers. The two CLI tools, installer CLI,
  installer test, and hash test all compiled and linked on Windows
- The shared hex formatter writes two lowercase digits directly, avoiding a new
  CRT formatting dependency. No checksum or parser policy changed
- Scoped quality passed for both batches; the focused final hash pass also passed
- Cleanup-only batch 2/3 patch: +481/-603, net 122 lines removed including the
  new tests and all registrations. Product SHA-1 bodies alone lose 301 net lines
  before build wiring and tests. The gamepad extraction reduces inherited
  differences while preserving a smaller product implementation overall
- Combined first three batches: net 1,063 lines removed and 214 fewer added
  lines in inherited files. These exclude this plan and concurrent feature work
- `temp/cleanup_astra_20260925/batch23-only.patch` and its owned-path/numstat
  manifests preserve exact attribution; reverse-apply validation passed
- All 24 Android object targets passed across ARM64, ARMv7, and x86_64 for both
  games' `joy.c`, shared initializer, installer reader, and shared SHA-1
- A complete native build linked both x86_64 game libraries successfully,
  including the first-batch profiling/overlay edits. No warning points to this
  cleanup. Further APK and remaining-ABI work is currently running in another
  thread's Gradle invocation; do not overlap its native builds or stop it
- The concurrent Guidebot task has restored `escort_set_goal_object`'s declaration
  in its internal header, removing the previously observed full-build blocker
- A fresh APK from the concurrent Gradle build was copied into this run's scratch
  directory. All six packaged D1/D2 native build IDs match the current linked
  artifacts across ARM64, ARMv7, and x86_64. Isolated runtime validation is being
  prepared on `Nexus5X_Light_2`, serial `emulator-5556`, to avoid other test sessions

### Closing validation for this goal turn

- The final SHA-1 build and vector CTest passed after removing an unnecessary
  CRT warning suppression. Remaining MSVC D9025 warnings are the suite's existing
  assertion-enable and third-party warning options, not new source diagnostics
- `GameProcessExitDiagnosticsTest` and `LogFileRetentionTest` passed all 10 tests
  through JUnit using snapshots of the freshly built production runtime JAR and
  compiled test classes, plus the pinned JUnit/Kotlin/xCrash dependencies and
  installed Android 37.0 API JAR. This avoids racing another Gradle compilation
  in the live classes directory; no source or test was altered or disabled
- Both owned patches pass non-mutating reverse-apply checks against the worktree
- On-device validation did not begin: another task's launcher recovery invoked
  `emu_health.ps1`, whose `Stop-Emulator` kills every emulator and restarts the
  ADB server. This shut down the separate `emulator-5556` during boot. Its bounded
  boot/install wait exited with failure; no cleanup-owned emulator remains alive
- The controller test's log records repeated launcher timeouts and global
  recovery. Do not claim input/overlay smoke passed, restart that task, or keep
  launching competing emulators. Retry serially after its recovery/testing ends
- This goal turn is progress: two more cleanup chunks are implemented and
  host/native/package validation is complete. The broad cleanup goal remains
  active; runtime validation and the ranked whole-branch audit are unfinished

### Further audit notes

- Gyro runtime consumes the per-axis fields. `GyroConfig.deadzone` survives only
  in duplicated legacy parsing, validation, serialization, and unused editor
  state. Both config readers also retain a single-angle migration. Current
  app writers already emit all per-axis values. Audit bundled presets and current
  serialization tests, then remove the obsolete fields under the pre-release
  format policy, preserving modern per-axis values and reference orientation
- The bundled `touch_controller_menus.json` is an exception: it still supplies
  only `deadzone: 0.1`, which the current reader interprets as approximately
  X/Y 0.2293578 and Z 0.6. Upgrade that maintained preset to explicit per-axis
  values before removing the reader's old conversion, preserving its behavior
- Both dual-emulator scripts are interactive; the earlier automatic/manual
  description was inaccurate. Their main difference is the NAT menu and server
  lifecycle. Existing managed-emulator helpers already cover much of their
  repeated boot/setup code. Both also contain broad stale-process cleanup that
  can kill unrelated `cl.exe` and PowerShell processes; do not run these scripts
  unchanged while other work is active. Prefer the existing managed helpers
  and explicit ownership when simplifying this area
- File-set migration is distinct from native pilot repair. Do not delete all
  similarly named routines together: check current import producers and native
  player-file locations before retiring the old root-to-set migration
- Android Gradle's observed Ninja command explicitly builds upstream FluidSynth
  tests, CLI, and examples on every ABI despite the wrapper's directory-level
  `EXCLUDE_FROM_ALL`. `defaultConfig.externalNativeBuild.cmake` has no target
  selection. Investigate a minimal explicit target list, including standalone
  fingerprinting and every required shared dependency; compare APK native entries
  and run import/audio/game smoke before accepting a build-only simplification

## Batch 4: current touch-layout format without migration machinery

Previous goal turn classification: progress (gamepad/hash cleanup, host tests,
native/package validation, and fresh evidence about shared emulator recovery).

Current source still has ten touch-layout upgrade stages and six tests dedicated
to historical upgrades. New `TouchLayout` instances even default to version 2,
while the current bundled default is version 11. The repository explicitly treats
Android formats as pre-release and disposable. Remove these migrations and make
new layouts and both readers use the current schema directly. Keep validation,
slot normalization, current Guidebot controls, and engine player-file compatibility.

- [x] Remove obsolete gyro single-deadzone/single-angle readers, state, and writers
- [x] Remove the old layout migration chain and its call sites/compatibility tests
- [x] Update the older Controller Menus preset to the current format, retaining
      its effective gyro values; preserve the default preset's current controls
- [x] Exercise current bundled presets through human/internal/slot round trips,
      verify gyro calibration and current Guidebot/long-press bindings, and keep
      malformed-schema validation coverage
- [x] Run scoped quality, Kotlin compilation, focused tests, and a fresh APK
- [x] Record exact owned metrics and finish pending runtime tests when available

### Batch 4 evidence

- Scoped quality and owned whitespace checks passed. Fresh APK assembled with
  all configured native ABIs, installed on emulator-5556, and the reusable
  `android/tests/test_touch_layout_format.ps1` device runner passed
- The runner loads the actual bundled assets from the installed APK and invokes
  its codecs with real Android key-name lookup. Both presets round-trip through
  internal JSON, readable JSON, and slot export/import without changing values
- The device probe verifies current schema defaults, both presets' effective gyro
  deadzones, and the default Guide wheel's current actions and empty center
- All 50 focused JVM cases passed across six suites, including custom gyro and
  long-press round trips, recenter calibration, schema/numeric validation,
  config import, slot handling, and current Guidebot controls
- The first build command failed because PowerShell split an unquoted Gradle
  property; the corrected quoted invocation built the APK and ran the suites
- Cleanup-only patch is +201/-493, net 292 lines removed, including the new
  host/device tests and reusable runner. Reverse-apply validation passed
- First four batches total 1,355 net lines removed, with 214 fewer added lines
  in inherited engine files. Counts exclude this plan and concurrent work

## Batch 5: retire obsolete file-set startup migration

Audit of current import producers found UI file imports, automated GOG/CD/ISO
imports, content reconciliation, and native launch path publication all use
`getSetDir()` beneath the active import root. The old v0 root-to-default and v1
set-root transfers serve only historical Android storage formats. Their four
tests exercise only those old formats. Remove them under the pre-release policy.

The startup root sweep is coupled to that migration and removes its leftovers;
remove it with the migration. Retain explicit user-requested clear-game-data
cleanup, active-root relocation through ImportLocationManager, and native pilot
repair. The native preferred directory still carries current configs and saves
and remains a search-path mount; do not change engine path policy in this batch.

- [x] Remove old root/set transfers, obsolete startup sweep, and migration tests
- [x] Preserve current set creation, storage-root selection, active-path
      publication, content ownership, and pilot handling
- [x] Exercise current storage and imports with scoped quality and focused tests
- [x] Validate fresh launcher/game startup and record owned metrics

### Validation correction and follow-up audit

- The documented space-separated `-Paths` invocation binds only its first path
  in PowerShell. Re-ran batches 2-5 with an explicit array after all formatters
  exited. The scoped wrapper passed for every listed path; because its Kotlin
  helper only includes `src/main/java`, also ran pinned ktlint directly on the
  changed Kotlin test files. That passed too
- Final formatting changes batch 2/3 to +491/-605 (114 net removed), and batch 4
  to +231/-511 (280 net removed). Earlier figures above are pre-final-format
  snapshots. The inherited reduction remains 214 added lines
- Batch 5 is +50/-300 (250 net removed), including a current-storage test that
  persists two sets, reopens the manager, switches the active set, and checks
  both games' published native paths and unchanged per-set data
- Combined five-batch net removal is 1,585 lines before any further change
- Batch 5 snapshots isolate its two-line SetupActivity deletion from both the
  first-batch crash-hook removal and concurrent controller work
- Initial batch-5 Gradle startup was rejected by native-generation retention
  because another build/test process was active. Retried after it exited
- Audio audit confirms the ring read/write/availability bodies are identical,
  but preview reset rewinds both cursors while in-game discard advances only
  the reader. A future shared buffer must expose those distinct operations and
  preserve locking around preview seek/stop and in-game transport diagnostics
- Native setup still mounts the preferred directory for current save/config
  paths. Retiring Android root-data migration is not permission to remove that
  mount or change native save/pilot search rules

### Batch 5 build and input validation

- Fresh all-ABI APK and focused Gradle invocation completed successfully. Eight
  file-set/import/storage suites report 47 passed cases and one existing skipped
  disc fixture. All six packaged D1/D2 ELF build IDs match the linked artifacts
- The maintained input test completed D1. D2 passed all routing and keyboard
  phases but failed its later assumption of seven untouched missiles: an earlier
  trigger-axis phase had fired one. Moved the existing one-frame secondary-fire
  check ahead of the trigger phases, preserving all assertions and delays
- The corrected D2 input script then passed all 97 steps. The test-only move is
  +24/-24 and does not change the five-batch net removal
- All four cleanup patch groups still pass non-mutating reverse-apply checks
- The metadata CLI/JNI duplicate signal is largely shared include lists, not an
  equivalent operation worth extracting. Server STUN and NAT simulator response
  encoding also overlap, but keep their authorization/lifetime policy separate;
  wire-fixture independence is useful and this is below the completed priorities

## Round result

Completed five cleanup batches plus one integration-fixture ordering fix. The
whole-branch inventory informed the priorities; this is not a claim that every
line of this large branch has been audited or that every candidate should be
changed. No commits or staging were performed, and concurrent work was preserved.

- Exact owned total: +895/-2,480, net 1,585 lines removed across 63 unique paths,
  excluding this plan. Inherited engine additions reduced by 214 lines
- Every owned patch passes reverse-apply validation; all owned paths pass
  whitespace checks. Final scope includes all files through explicit arrays
- Windows game/tool checks, three Android ABIs, SHA-1/CD/installer contracts,
  descriptor contracts, launcher JVM tests, and the fresh APK checks passed as
  detailed above. Touch/config suites passed 50 cases; file-set/storage suites
  passed 47 with the existing Anniversary extraction-fixture case skipped
- Both bundled presets passed Android codec/slot round trips. The fresh APK's
  maintained file-set integration passed all 17 steps, including content
  ownership, disabled-state persistence across switches, and cleanup
- D1 input routing passed; D2 passed all 97 steps after fixing the fixture's
  ammo precondition order. D1 and D2 each passed the 26-step fresh-launch and
  texture-overlay enable/disable smoke on the final APK
- The Android format removals intentionally stop upgrading obsolete touch and
  file-set formats, following the repository's pre-release/disposable policy

### Next-round priorities

1. Shared audio ring instance: keep producer/consumer atomics and distinguish
   reset from discard. First establish runnable seek/stop/underrun behavior
   coverage; source-token tests alone are insufficient
2. Emulator helpers: replace duplicated provisioning with existing helpers and
   restrict recovery/process cleanup to owned emulators. Global emulator kills
   observed during this round are a concrete concurrency problem
3. Native target selection: investigate building only the two game targets and
   their dependency graph instead of unrelated FluidSynth executables. Both
   games already link dxx_fingerprint; verify packaged library parity and audio
   and import smoke before accepting a narrowed AGP target list
4. D2 replay diagnostics and network fixtures: revisit after concurrent feature
   work stabilizes, preserving exact replay state/RNG and private engine policy

These are assessed future candidates, not unfinished validation for the five
completed batches. Retain the frozen inventory and exact owned patches when
starting another round so concurrent feature growth is not counted as cleanup.

## Continuation requested after the first round

The user asked to continue the cleanup plan until finished. The four priorities
above are now active scope. The preceding turn was progress: five implemented
and validated batches. Current HEAD is `80af2244`; it contains the first-round
work. The graphics opportunity plan is already modified by other work.

- [x] Unify the four audio ring copies, preserve reset/discard and synchronization,
      and pass executable concurrent-buffer and real preview/game audio coverage
- [x] Consolidate emulator provisioning/recovery and verify ownership boundaries
      with independent emulator sessions plus maintained multiplayer setup checks
- [x] Select required native build targets and verify packaged libraries and
      audio/import/game behavior across supported ABIs
- [x] Resolve the replay diagnostic and network fixture candidates against current
      source, with behavioral parity checks for accepted extractions
- [x] Audit the complete continuation against the plan, review owned diffs, and
      record verified results and reasoned dispositions for rejected candidates

### Audio implementation boundary

Current source has a fourth equivalent buffer in `digi_tsf_music.c`. Share only
the fixed-size SPSC PCM buffer as an explicit instance with read/write/available,
quiescent reset, and reader-side discard operations. Keep each player's locks,
OpenSL/SDL callbacks, decoder state, lifetime, and transport policy local. A
small inline header preserves the existing GCC/Clang atomics and avoids adding
link dependencies. Exercise wraparound, cursor overflow, partial reads,
underruns, reuse after reset/discard, independent instances, and actual producer/
consumer threads. The maintained launcher media integration already drives real
MIDI and CD start/pause/seek/resume/stop and will be reused for device coverage.

### Current audio and build evidence

- Executable NDK/CMake ring test passed on emulator-5556: complete/partial reads,
  underflow, array and integer-cursor wraparound, discard/reset, separate instances,
  and four concurrent producer/consumer streams totaling 33,554,432 samples
- All 18 affected production object targets compiled without errors across three
  Android ABIs: both games' synth/CD backends and the launcher's two D2 previews
- All 37 audio/preview source contracts passed after updating call names and one
  stale constructor assertion to include the current explicit file-set argument
- Scoped quality passed; the new standalone C/CMake test lies outside the legacy
  helper allowlists and was additionally formatted/linted with the pinned tools
- Native target-selection trial now requests the two game libraries. Their
  CMake dependencies include dxx_fingerprint, SDL, PhysFS, synth, codec, texture,
  and compression libraries. APK entry parity and real audio/import tests remain
  required before this trial is accepted
- Current replay audit: the FVI weapon/robot logger is already in input_demo_hooks;
  the remaining collide.c homing-bump environment gate still needs disposition

### Continuation decisions and validation

- Shared PCM storage replaces four duplicate ring implementations. The product
  change is +108/-230, including the new header. Each player retains its own
  instance, producer/consumer synchronization, callbacks and transport policy
- Device buffer testing passed wraparound at array and integer limits, underruns,
  partial reads, reset/discard, instance isolation, and 33,554,432 samples delivered
  by concurrent producer/consumer threads. All 18 production objects compiled for
  arm64-v8a, armeabi-v7a and x86_64
- Fresh APK media integration passed real MIDI, CD and MP3 pause/seek/resume/stop.
  D1 music controls passed 41 steps; D2 passed 48, including HMP memory conversion.
  The GOG Windows installer test passed all 55 steps: extraction, fingerprinted
  audio registration, MIDI/CD preview, and in-game redbook playback
- Emulator startup and recovery now share test_helpers. Every managed launch
  binds its requested console port; health checks inspect that serial's boot and
  package service. Recovery reconnects only that ADB transport and stops only the
  selected console port. It no longer resets the global ADB server or removes all
  devices' temporary files. Unscoped destructive stale cleanup is rejected
- Both manual multiplayer setup scripts reuse the managed launcher and track only
  emulators they started. Removed global PowerShell/compiler/emulator kills and
  unconditionally killing the owner of port 9000. An occupied server port now
  produces a clear failure; the no-server setup bypasses that prerequisite
- The suite now requires its configured primary/secondary serials, rather than
  treating another task's emulator as its first or second device. Teardown stops
  only owned serials, including a crashed/offline owned emulator
- Executable recovery coverage passed a selected reboot while the guard device's
  boot ID and temporary marker survived. Three other sessions were present during
  this validation. A shutdown race found by the test was fixed with a bounded
  completion wait. Android may clear its own /data/local/tmp during reboot, so
  preservation is asserted on the guard device, whose lifecycle must not change
- Actual suite and both setup cleanup functions passed owned/reused/offline
  selection checks. The no-server setup completed with KillOnExit and preserved
  both reused emulators. Maintained direct-LAN integration passed for D1 and D2
- Fifty Python source/deployment contracts and the PowerShell helper contract
  runner passed. Updated stale assertions for the current file-set constructor
  and shared mission-metadata policy owner; no product behavior was changed to
  satisfy those stale assertions
- AGP now selects only dxx-redux-d1 and dxx-redux-d2, retaining their dependency
  graph. Its logged build task names confirm those two targets for all ABIs.
  The first continuation APK had exactly the previous 45 native-library entries;
  all six packaged game build IDs matched their linked artifacts
- Full assemble startup was rejected by retention while another task's copied
  replay executable was active. packageDebug built the same native dependencies
  using the existing 4p5c715t generation, without pruning active build directories

### Inherited-engine candidate disposition

- FVI weapon/robot diagnostics were already in input_demo_hooks before this
  continuation. That part of the old DMR1-CHUNK-006 proposal is superseded
- Moved only the 15-line homing-player-bump environment gate from collide.c into
  the existing D2 input-demo owner. Its body, first-call caching, predicate and
  call-site ordering are unchanged. Added the declaration and explicit stdlib
  include; no collision policy or RNG operation moved
- The May 5 level-4 homing fixtures named by the old investigation are absent
  from the current corpus. Used current level-9 checkpoint recording
  d2_descent2_level9_20260511_192533 instead. It executes all 66 frames. Its old
  recorded RNG comparison already fails before this extraction, so that failure
  was preserved rather than changing expectations. Current-branch before/after
  results match exactly: final result, all 203 state records, and 887 RNG records
  (886 events plus metadata). Only the known 15-line source-location shift for
  collide.c RNG annotations is normalized. The moved gate body is byte-identical
- D2 Windows game and headless replay targets built, and both affected production
  objects compiled across all three Android ABIs. The broader initial Windows
  build hit concurrent test_upstream_compat linkage work; a subsequent rebuild
  completed once those concurrent definitions were available
- Rejected network fixture extraction for this round. The paired MDATA fixtures
  mutate UDP_MData sequence numbers, inject the retry queue, cache game-info/sync
  packets, deliberately drop a full packet, and collect ACK state inside receive
  processing. The state is spread across the top-level fixture block, game-info
  parsing, ACK handling and send/drop paths. Moving only the large block would
  require exporting fixture state/private helpers or a source-fragment include;
  moving the whole facility adds capture/ACK/drop interfaces solely for test code.
  Keep this policy and queue observation beside the private transport machinery.
  No queue, packet, retry, or validation behavior was changed or weakened
- A final combined Android build exposed a missing declaration in concurrently
  changed state.c. Added only d1_in_d2/d1_in_d2.h; its save-format and identity
  changes remain owned by the other work. The separate include patch is recorded
  against a saved preimage so those changes are not counted as cleanup

### Continuation audit

- Owned changes, excluding this plan: +624/-1,130, net 506 lines removed across
  26 paths. Tests account for much of the added code. Audio product code removes
  122 net lines; emulator product/setup code removes 636. The replay relocation
  adds two lines overall while removing 15 inherited additions
- The one necessary state.c include makes this continuation's inherited-engine
  reduction 14 lines. Cumulative two-round totals: 2,091 net lines removed and
  228 fewer added lines in inherited engine files
- Exact ownership manifest, new-file copies, replay comparisons, APK library
  manifest and reverse-applicable tracked patch are in
  temp/cleanup_astra_20260925. All owned tracked hunks passed reverse-apply and
  whitespace checks. Concurrent graphics, Guidebot and D1-in-D2 edits are excluded
- Scoped formatting/lint passed. New standalone PCM C/CMake tests were also run
  through the pinned formatters outside the wrapper's legacy allowlists. The
  inherited C files retain their existing style, which that wrapper excludes
- No commits or staging were performed

### Final continuation result

All four continuation priorities are resolved: the accepted audio, emulator,
build-target and replay-gate changes are implemented and verified; the already
extracted FVI logger and retained network fixtures have explicit dispositions.
No required cleanup or validation remains in this plan.

- Final Windows D2 game and headless replay targets pass after the include fix
- Final all-ABI APK package build passes; all six game ELF build IDs match the
  linked artifacts and the native entry set remains exactly 45 libraries
- Final APK D1 and D2 launch/game/texture-overlay smoke checks each pass 26 steps
- Final before/after replay result, state and RNG parity passes again
- Final owned patch reverse-apply and whitespace checks pass; metrics remain
  +624/-1,130 for this continuation, excluding the plan and concurrent changes
