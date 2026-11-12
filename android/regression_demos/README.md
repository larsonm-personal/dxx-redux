# input-based demo file regression tests

- these are demo files which consist of player inputs rather than game element positions (which is what the classic .dem files encode)
- these are meant to form a set of regression tests so we can replay them and check the final state produced. with a large enough body of demo files we should be able to lock down game engine behavior and enable larger refactoring passes, such as to de-duplicate d1/ and d2/

# todo

- remove debug state tracing from the early demos

# Recorded asset compatibility

New input recordings include SHA-256 fingerprints for the HOG/PIG/HAM assets
actually loaded. Every input playback path verifies these dependencies; missing
or unknown historical fingerprints require an explicit `-AllowUnverifiedAssets`
investigation and are excluded from verified shareware CI. Malformed metadata or
known mismatches always fail. Use `run_input_demo_regressions.ps1 -SharewareOnly
-RunMode headless` to run every recognized recording, with a nonzero failure if
none are selected. See [the asset-verification contract and execution-path audit](../tests/INPUT_DEMO_ASSETS.md).

# Guide-Bot routing for new regression recordings

Use **Original** Guide-Bot routing for new D2 recordings intended to protect
general gameplay and engine behavior. Keep **Enhanced** recordings for cases
that specifically exercise its prerequisite planning, route recovery, and
other added behavior, alongside the physical route and live-navigation tests

Choose Original before starting or loading a single-player game. The launcher
setting applies to new games and loaded single-player saves. Verify the new
recording's header contains `player_cfg.guidebot_routing_mode`: `0` for Original
or `1` for Enhanced

New saves preserve Guide-Bot commands, timers, HUD messages and Enhanced planner
progress. The opt-in `android/tests/test_guidebot_original_navigation.ps1
-SaveContinuity` probe compares uninterrupted navigation with a midpoint save/load
in both modes: Scram, Next, Exit, Hostages and Enhanced Unexplored. It checks
state immediately around saving/loading and every subsequent frame, including
movement and RNG state, on levels 1 and 11 with repeat runs

This uses real saves and live navigation/physics on level geometry, not the full
game frame loop. It does not certify full-world demo equivalence or Android's
wall-clock-budgeted planner scheduling. Older saves retain the legacy restore
fallback because they lack the new runtime fields. Passing checkpoint playback
alone does not certify ordinary save/load continuity

This is a recording policy, not a playback override. Playback must honor the
recorded mode regardless of launcher defaults or checkpoint settings. Do not
force an Enhanced recording to Original, edit historical headers to make a
test pass, or copy a replay's final result into the recorded expectation

The May 2026 D2 recordings predate the explicit mode field. Their Guide-Bot
behavior is not identical to current Original or Enhanced routing. The nine
divergent recordings and their classic-demo/RNG companions were removed at the
user's request on 2026-09-27; the two passing recordings remain. The saved-world
Guide-Bot test retains its standalone checkpoint under `android/tests/fixtures`.
Accept a replacement only after it matches its recorded
state on immediate playback, repeated clean playback, and the supported headed
and headless paths. Include Android-to-host checks for recordings captured on
Android. Repeated replay results alone do not establish recording fidelity

# Create and verify automatic level recordings

The 17 public level replay fixtures were recaptured with the current native
producer after the ordered-input changes. Independent captures matched byte for
byte, and each scene passed detailed native replay checks twice on both graphics
and headless backends. Their gameplay action declarations, terminal results and
simulation RNG expectations remain unchanged. Imported coverage retains the
manifest's explicit scope; only flight gameplay-state/SIM parity is qualified.
Historical format-4 conversion metadata is retained under
`../tests/fixtures/public_level_replays/provenance/format4_migration/` and describes
the previous corpus rather than the current native captures.

The automatic level-recording integration runner is
`android/tests/test_auto_level_demos.ps1`. Select `-Game d1`, `d2`, or `d1-in-d2`
and the owned device with `-Serial`. Add `-DataEdition shareware` for public data,
`-Cheats` to exercise recorded weapon grants, or `-RecordState` for per-frame
diagnostic comparison. General D2 gameplay scenarios select Original routing
before recording and verify that setting in the exported header.

`-Game d2 -DataEdition retail -Secret` checks fresh secret-level recording,
independent replay without a save checkpoint, and automatic-recording suppression
when returning to the saved base level or revisiting the saved secret level.

# Watch one recording

Run `android/run_all_tests.ps1` without arguments and choose **R. Replay a demo**.
Choose D1, D2 or D1-in-D2, then the recording, then headed or headless playback.
Headed playback uses normal speed and a visible game window. Headless playback
runs accelerated through native D1/D2 console binaries without initializing a
video device. D1-in-D2 still uses the no-present windowed runner and needs a
display; it is not a true-headless backend.
This menu starts only the selected replay, not the test suite

From the repository root, jump straight to the picker:

```powershell
.\android\run_all_tests.ps1 -ReplayDemo
```

The picker keeps your existing `regression_demos/` recordings first, in their
original newest-first order, then appends the tracked public fixtures in stable
path order. It includes all 28 compact demos from:

- `android/tests/fixtures/public_level_replays/` (17)
- `android/tests/fixtures/public_d1_fault_probes/` (5)
- `android/tests/fixtures/public_d1_fan_probes/` (3)
- `android/tests/fixtures/public_all_weapons/` (3, native engine only)

Choose **1. Headed** to watch at recorded speed in a normal 640x480 window.
Tracked fixtures exit the game cleanly when playback completes or aborts, so a
known fault is reported promptly rather than leaving the game at its menu.
Press **Space** to pause/resume, or **Right arrow** to pause and advance one frame.
The existing render-profile and robot-label options remain available through the
direct helper. Short ordered-input and all-weapons recordings finish quickly;
pausing lets you inspect them. The helper allows recording duration plus startup
time, with a five-minute minimum timeout; pass `-TimeoutSeconds` to the direct
helper when planning a longer paused inspection.

These tracked fixtures automatically select the pinned public shareware edition,
including D1 assets for D1-in-D2. If they are missing, the interactive helper offers
to extract a cached public package, or explicitly download and extract that public
package. Enter or Q cancels without setup. The existing setup helper verifies
package and extracted-file hashes, uses bounded extraction, and builds the native
extractor if needed; it never executes the DOS installer or downloads retail data.
You can also prepare both public editions in advance:

```powershell
.\android\helpers\setup_shareware_test_data.ps1 -Game both -Download
```

Omit `-Download` to require already cached public installers. Use `-DataDir` on the
direct replay helper for another already extracted, hash-matching directory. The
normal host build guard automatically builds missing or stale game executables;
listing or cancelling the picker does not build games.

Imported D1 viewing stages the checked-in native-written comparison pilot when
its projection requires it, using only the disposable replay sandbox. Personal
pilots and recorded projection checks stay unchanged. The monitor-phase,
mine-blast and fan-transparency entries are visibly marked as known imported
faults. Monitor and mine terminal summaries can match despite their detailed
faults. Fan-transparency is expected to stop at its strict carried-RNG mismatch
before input 301; its nonzero exit is preserved and is not a fault-verification
pass. Causal controls and cases excluded from imported qualification are labelled
separately. All-weapons compact replay is native-only and is omitted from the
imported picker.

The viewing helper checks the embedded terminal summary. It does not run the
separate full-state/RNG/detail oracle checkers, so a successful viewing run is not
full imported parity. Old recordings remain visible with a legacy-format warning;
the current engine requires format 5, and does not silently convert them.

The existing single-demo helper also works directly. For a tracked native D1
all-weapons recording:

```powershell
.\android\tests\run_input_demo_replay.ps1 -DemoPath android\tests\fixtures\public_all_weapons\d1-bigred.dximdemo -Runner visual -Mode realtime
```

To watch the level-14 recording in native D1:

```powershell
.\android\tests\run_input_demo_replay.ps1 -DemoPath android\regression_demos\d1_descent_level14_20260920_184354.dximdemo -Game d1 -Runner visual -Mode realtime -RenderProfile default -ReplayRobotLabels hide
```

For demos recorded by D2 while playing D1 missions, use `-Game d2 -D1Mission`
with D1 data. The built-in `descent` mission is recognized automatically. This
keeps the recorded D2 state and comparison rules; it does not translate a D1
engine recording. `-DataEdition shareware` uses the pinned public D1 assets.

Use `-Game d2 -D1InD2` to watch that D1 recording in D2. The helper's
`-Interactive` switch opens the same picker; add `-SearchRoot <directory>` for
recordings outside this corpus. Result comparisons still run after playback,
so viewing a divergent recording can end with a nonzero test result

The shared reader accepts recordings up to 1 GiB and reads bounded JSON lines
(8 MiB per record), avoiding whole-file text copies. The decoded checkpoint
limit remains 2 MiB. Parsed frame data still occupies memory until replay setup
finishes; this is not a constant-memory replay engine

Replay RNG tracing spools fixed-size events beside the requested trace as
`<trace>.events.tmp`, then writes the existing JSONL format and removes the
temporary file on normal cleanup. This bounds replay trace memory even when
presentation generates millions of draws. Recording still retains its trace
until a destination is chosen. Truncated traces fail finalization explicitly.
The native result is published after RNG and state traces are finalized, so a
large export does not appear to the runner as a result followed by a hung exit.

# D1-in-D2 verification

From the repository root, the existing runner can execute the D1 corpus in
both engines:

```powershell
.\android\tests\test_input_demo_regressions.ps1 -RecordedGame d1 -Game d1 -ResultArchiveRoot temp/d1-parity/native
.\android\tests\test_input_demo_regressions.ps1 -RecordedGame d1 -D1InD2 -ResultArchiveRoot temp/d1-parity/imported
```

Keep native and imported results separate, including failed results. Both
commands currently compare against the recordings; they do not yet provide a
strict, paired native-versus-imported state comparison. The imported runner
now selects D1 startup explicitly and needs only the same D1 HOG/PIG as native D1

The paired evidence runner executes every selected recording twice in native D1
and once in D1-in-D2, sequentially, against an isolated copy of those D1 assets:

```powershell
.\android\tests\test_d1_replay_parity.ps1
# Restrict an investigation without silently skipping a required filename:
.\android\tests\test_d1_replay_parity.ps1 -DemoFileName d1_descent_level14_20260920_184354.dximdemo
```

It is also available through `run_all_tests.ps1 -Filter test_d1_replay_parity`.
It is an explicit investigation while its coverage gate remains incomplete, so
the routine and full unattended suites do not run a knowingly incomplete qualification.
The timestamped output under `temp/d1_replay_parity_*` contains a manifest of recording/checkpoint,
asset and executable hashes, recorded player settings, launch commands, raw results,
compressed state/RNG traces and a report with three independent relationships:
recording versus native, native repeatability, and native versus imported. Every
available frame field and diagnostic is compared with explicit engine/storage
mappings described below. Terminal player values are never
copied into an expectation. Missing/truncated evidence is not a pass

The output also archives the working source patch and a frozen harness under
`harness/android/`. Both the Python controller and the PowerShell replay runner
execute from that copy, including the runner's platform, menu, compatibility and
retention helpers. `harness.json` lists their hashes; staging races, missing
dependencies or later changes to the staged files invalidate the capture.
Live workspace edits do not replace the executed scripts. The runner receives
the original repository root explicitly for data/output paths and uses the
supplied staged data bank without regenerating the workspace data index.

State-trace
metadata describes the recording in both engines; terminal results identify the
executing engine. RNG reports retain context differences in their strict verdict
and separately show the first value/timing/count difference, so an annotation
cannot hide a later simulation difference. SIM and effects RNG streams have
independent reports. FX state/count differences are cosmetic diagnostics and do
not fail gameplay parity; SIM state, draws and counts remain strict

Frame reports also inventory every diagnostic field, including its compared and
differing frame counts and first differing value. These entries supplement the
strict verdict: an early hash/layout difference cannot hide a later mismatch in
weapon orientation or another diagnostic. No diagnostic exclusion list is used

Replay state traces also contain version-2 `object_state` records after each
`frame_state`. Frame zero resets a slot map; subsequent `slots` entries replace a
complete object, or delete it with `null`. Unlisted slots retain their preceding
state. Slot reuse carries the new signature and complete replacement. Each record
also contains the full allocator list, segment heads, simulation clock and both
actual RNG streams. The paired runner compares reconstructed objects by named
field and preserves the first differing slot/frame for every field, including
fields absent from the old hashes such as orientation and weapon hit history

These records expose common object fields, active movement/control/render data,
retained ghost polygon data and per-robot local AI data. They omit ABI padding
and inactive union interpretations. Raw records retain each engine's storage.
The cross-engine comparison relocates saved D1 AI fields, requires neutral D2-only
capacity/fields, and maps native D1's unused reactor ID to effective selector 0.
Imported reactor selectors, models, shields and gun geometry remain compared.
Native repeatability still checks raw IDs. Frame diagnostic schema 2 uses the
same effective selector in native object/segment hashes and collision diagnostics;
older captures require their original checker and cannot pass the new schema

Version-8 world records expose per-frame world/player/AI state. Both object and
world observers emit restored and terminal boundaries. Known exit-animation
cosmetic fields are excluded from gameplay comparison; sequence ownership and
unknown fields remain strict. No animation-history persistence is required

State-log paths ending in `.gz` are compressed by the engine as it writes. The
paired runner uses `state.jsonl.gz`; the existing PowerShell trace comparator
also accepts plain or gzip input. Original records remain available for auditing

This is still F1 work in progress: the complete simulation lifetime and private
state audit, corpus qualification and target-platform matrix remain open.
The report keeps these coverage gaps explicit and returns
nonzero for incomplete qualification (2) or observed differences (1). An exact
summary-state match alone cannot qualify full fidelity

For a single result comparison, `run_input_demo_replay.ps1 -StrictComparison`
disables the legacy terminal-exit subset. `-SkipExpectedChecks` is capture-only
and prints `CAPTURED`, not `PASS`. A required missing reference is an error

The [finishing plan](../ai%20tool%20plans/asset%20management/d1-in-d2-consolidation-plan.md#0-stocktake-and-plan-to-finish)
defines the next gate: repeatable native reference, unchanged checkpoint/input
replay in D2, exact frame-zero/per-frame/terminal semantic state and RNG
comparison, and separate recorded-versus-native and native-versus-imported
verdicts. Do not use fresh-level startup or expected-result substitutions to
make checkpoint demos pass

# Compact public corpus: primary headless and secondary headed

The same regression entry points select the pinned public scenes with
`-Corpus public`. All scenes are ordinary native level starts; both backends
run each twice and compare unchanged live-capture frame state, every diagnostic,
terminal state and SIM-RNG oracles. Each demo and each oracle file stays at or below
100,000 uncompressed UTF-8 bytes. Playback never rewrites expectations.

Prepare pinned public assets using `android/helpers/setup_shareware_test_data.ps1
-Game both -Download`, then run:

```powershell
# Primary native CI path: no window, video driver or X server needed
./android/tests/test_input_demo_regressions.ps1 -Corpus public
# Secondary rendered path, using the same oracles
./android/tests/test_input_demo_regressions_graphics.ps1 -Corpus public
```

Explicit `-D1Exe` and `-D2Exe` select matching console or desktop binaries;
`-D1DataDir` and `-D2DataDir` select the respective pinned shareware directories.
Without explicit executables the existing build guard chooses/builds them.
Headless selects `-Runner headless-console` and never falls back to a no-present
window. Graphics selects `-Runner visual` and does run rendering/presentation.
Both native compact backends require finalized result/traces and natural process exit.
`-RequireCleanExit` enables opt-in desktop `-inputdemo-exit`. The tracked public
fixtures also use it for prompt completion/abort reporting in visible realtime
playback; other user recordings retain their existing menu behavior.

Use Xvfb and software GL on Linux for the graphics pass and for route-development
screenshots. Xvfb is a virtual display for the headed engine, not the headless
implementation. CI runs the console pass with DISPLAY/WAYLAND_DISPLAY absent
and an intentionally invalid video driver. A separate graphics job runs under
Xvfb in parallel, reusing the same hashed binaries and pinned assets from one
build job. Both remain automated PR checks.

Only the graphics pass also runs the existing paired D1 investigation. Required
imported scope remains exact D1 flight gameplay-state/SIM-RNG; other imported
scenes remain diagnostic probes. Imported diagnostic/object/world differences
and full simulation qualification remain explicitly incomplete. The headless
report says imported parity was not run rather than treating it as a pass. The
paired imported harness retains its existing process-lifetime policy; the new
natural-exit assertion covers the native compact paths.

`test_compact_public_replay_failures.ps1 -RunMode headless|graphics -D1Exe ...
-D2Exe ...` runs deliberately wrong frame expectations and changed controls
through the same full regression-wrapper chain and real engines. It requires
all admitted scenes/repeats to fail at the intended runtime comparisons. The
wrong-oracle run's actual traces must still pass against the original immutable
oracles, providing matched positive evidence. Setup/admission failure, missing
artifacts or an incomplete inventory cannot count as a successful negative.
Negative controls explicitly request `-NativeOnly`: their reports mark imported
parity `not_run`, avoiding two redundant imported investigations. The positive
graphics pass still requires the existing paired scope once. All mutations are
disposable copies; checked-in fixtures stay byte-identical.

## XL environmental timeline (format 6, under qualification)

The codec recognizes a required `xl-clock-input-v1` capability for D2. Normal
Windows and Linux OpenGL builds record and replay this format for the bounded
profile below, including Omega and time equipment. Ordinary F5 recording and
level-start recording use the same ordered timeline. Android, true-headless and
other production backends remain rejected pending their qualification; developer
probes retain access for that work. This does not establish complete XL gameplay,
campaign progression or presentation independence. Existing format-5 stock
fixtures are unchanged; legacy XL playback without the required equipment
timeline remains rejected.

The existing `android/tests/xl_omega_damage.py` owner accepts `--recording`
for a developer-only live Omega witness. It warms the original level for 66
ordinary frames, sets an approach pose and initial inventory before capture,
and moves an original Omega pickup to that approach. Capture starts without Omega
ownership or charge. The first ordinary frame acquires and selects the pickup;
61 released-fire frames then let the one-second selection rearm delay expire.
The remaining 90 held-fire and 90 released-fire frames exercise ordinary damage
and recharge, for 242 recorded frames total. The receipt requires damage while
Omega is still selected, charge consumption and recharge. It discloses the two
position assists and one inventory setup assist and requires
zero assists during recording. The original eight component/rate controls remain
the default mode. A passing capture receipt does not qualify playback: compare
the recorded detailed state and SIM RNG with clean-process replay before freezing
the demo. The acquisition Warp 5 capture matches all 242 object/frame observations,
246 world/boundary records, 1616 SIM RNG events and 488 FX RNG events against
three clean-process playbacks, including accelerated and realtime pacing.
This includes restored clocks/counters, texture UVs and
AI chatter timers. Presentation RNG differs and remains reported separately.
This bounded assisted witness is not complete XL state/platform qualification or
a frozen corpus. The recording probe reports phase timings and the runner reports
total process time; its 45-second clean-exit deadline remains unchanged.

The existing `xl_render_clock.py --time-equipment` probe now reports startup,
recording/save phase endpoints and aggregate delay/simulation/render/flip costs.
A fresh original Warp 6 combat capture exits cleanly in 32.70 seconds against the
unchanged 45-second deadline. Immediate and repeated accelerated/realtime replays
each match 231 object/frame observations, 235 world/boundary records, 1193 SIM RNG
events and 236 FX RNG events.
The probe warms the level for 66 ordinary frames, finds a wall-clear approach
outside original robots, then records the actual pickup, ordinary steering and
84 firing frames during slow motion, followed by reversal. The existing damage
trace must attribute a shield decrease to a player projectile in the observed
slowed frame; a shield drop alone is insufficient. World observations describe
frame starts, so the following boundary verifies the factors after the collision.
One approach-pose assist precedes recording; inventory and in-record assists are
zero. Later bullet-time controls and movement remain live checks outside that
recording; five total pose assists prevent a connected-progression claim.
The full world observer avoids deep-copying
changed JSON sections before serialization. All 724 decoded trace records from a
fixed before/after replay are byte-identical; no field, delta or flush was removed.
Earlier timed-out captures and failed setup/verifier attempts remain failed
evidence. These Windows results do not establish cross-platform acceptance.

XL transient particles, lightning bolt/draw geometry and flare illumination use
RNG stream 2 (`D_RNG_PRESENTATION`). These owners previously consumed stream 1
(`D_RNG_FX`), which also drives simulation-owned sound deadlines and some stock
object effects. Rendering extra views or rebuilding clouds could therefore change
later FX decisions. The new stream uses the existing LCG, independently even in
libc RNG builds. Stock SIM/FX sequences and stock flare selection are unchanged.
Every presentation draw/reseed remains in the complete RNG trace, with a separate
`presentation_rng` comparison report. The two-entry object/boundary `rng` array
continues to describe SIM and FX; native D2 comparison retains both exactly.
Presentation sequence/state is not saved: transient clouds and bolts already
rebuild on ordinary restoration. This is an RNG ownership boundary, not blanket
qualification of optional graphics/audio. Lightning linked-sound ownership and
all other gameplay dependencies still require the G3 audit and no-device proof.

The header `environment` object contains `required: ["xl-clock-input-v1"]`,
`slow_motion_speedup: 6`, `clock_ms`, `pickup_ms`, `cached_ms` and `render_frame`. Unknown or
duplicate required capabilities and unsupported profiles reject. Build identity
remains provenance rather than an exact-commit playback whitelist. The existing
difficulty metadata retains its range validation. Format 6 requires `lcg_state`,
explicit `player_cfg`, `view_projection` and `rear_view` objects, plus an XL clock
baseline for `new_level`. Every ordinary D2 player-config field emitted by the
recorder must be present, including headlight default, homing, Guide-Bot routing,
autoselection options and both weapon orders. Boolean settings require integer
0 or 1. Missing settings do not inherit legacy defaults. Both complete-file
admission and recorder startup enforce this contract before activating a session;
format-5 stock/default behavior remains unchanged.

The settings audit currently distinguishes these owners. Open rows are
qualification work, not implied support:

| Setting/input                                                                                          | Existing ownership and remaining qualification                                                                                                                                                               |
| ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Difficulty                                                                                             | Header 0-4, restored before startup; ordered difficulty commands retain later changes                                                                                                                        |
| Auto-leveling, persistent debris, headlight default, homing, weapon selection/order, Guide-Bot routing | Explicit startup configuration plus complete ordered `player_cfg` operations; Guide-Bot changes use the ordinary setter and retain its AI routing reset; the current profile survives pilot reload           |
| Keyboard, joystick, mouse and touch scaling/bindings                                                   | Applied before captured control samples; playback dispatches those scaled samples, rather than reading current device bindings                                                                               |
| Rear view/cockpit and projection/depth/missile view                                                    | Explicit startup baselines and ordered view operations; projection changes cancel capture, and incompatible playback projection rejects                                                                      |
| Extra/guided/camera render views                                                                       | Existing gameplay-view admission applies; broader renderer independence remains G3 work, since visibility and render counts can affect gameplay                                                              |
| XL speedup and process clocks                                                                          | Required speedup 6, bounded recorded transition/pickup/cached clocks and render-event count; no configurable alternate speedup is currently implemented                                                      |
| RNG and floating point                                                                                 | Required `lcg_state` and supported strict C/C++ build profile; altered runtime FP controls reject without replay-only repair. Cross-build/architecture gameplay qualification remains open                   |
| Cheats and checkpoint globals                                                                          | Ordered all-weapons commands are retained. Active other native cheat flags or suspended simulation subsystems reject XL recording before startup; complete gameplay-state and disk continuation belong to G2 |
| Audio backend and optional visual effects                                                              | Some runtime state still depends on backend availability; G3 must establish simulation independence before treating these as presentation-only                                                               |

Low-memory bitmap substitution (`-lowmem`) is outside this profile: it changes
the `GameBitmaps` pixels used by transparency collision checks. XL capture
rejects it before creating a checkpoint, and format-6 playback rejects it before
restoring the world. Other active native cheat flags and debug subsystem
suspension also reject; the native end-level robot suspension remains allowed.
The existing modal integration owner checks all 22 rejected startup profiles,
unchanged player/controls/SIM RNG, normal end-level admission and successful
ordinary recording after restoring the settings.

Format 6 admission requires the existing CMake strict-FP profile in both C and
C++: MSVC `/fp:strict`, or GCC/Clang no fast math and no contraction. Compiler
markers additionally reject fast-math overrides and excess-precision evaluation.
The checked hardware profiles are SSE2 (plus 53-bit x87 on MSVC x86) and ARM
hardware floating point. Unknown compiler/hardware profiles reject. These are
admission prerequisites, not cross-platform gameplay qualification.

Capture startup checks FP controls before checkpoint capture; replay checks before
session publication and world restore, then again after restore. Live input/frame
boundaries and replay steps require nearest rounding, masked exceptions and
gradual underflow. ARM also requires its default NaN/vector controls. Accrued
exception status is not a control change. A changed control mode cancels the
companion capture or stops playback with a diagnostic; it is not silently repaired.
Format 6 no longer uses the historical Windows replay-only FP reset. Stock format
5 retains that behavior. The build profile is part of the format-6 contract;
changing its numerical semantics requires renewed fidelity qualification.

Format 6 records configuration changes as `{k: "player_cfg", ft, value}` in the
existing ordered dispatch list. `value` is the complete, validated D2 `player_cfg`
field map, including both weapon orders. Missing fields, invalid booleans/orders,
unknown keys, D1 operations and use in format 5 reject before execution. The
initial header remains unchanged; playback retains the current ordered profile
when a later pilot reload reapplies settings. Format-5 recording/playback behavior
is unchanged.

The recorder observes settings before input consumers and simulation, deduplicates
unchanged snapshots, and captures a final change in the no-simulation stop tail.
A preference mutation first discovered inside simulation cancels the companion
rather than emitting an operation after its consequences. Guide-Bot routing is
captured at its ordinary setter before resetting AI routing; two changes between
frames remain two operations even when the final mode equals the original mode.
Playback uses that same setter, not the save-restoration path that preserves AI.

XL world observations use schema 16 and require `xl_player_cfg`, `xl_births` and `xl_native`
at restored, per-frame and terminal boundaries. Birth observations contain every
live slot/signature, birth time, creation order and the next creation counter;
the reader checks live object identities at both boundaries and validates the
times, orders and bounds on every frame. Stock world schema 10 is unchanged.
The native extension observes Omega and afterburner charge, the 16-bit wiggle
phase, every object-slot afterburner clock, all eight guided-missile pointer/slot
and signature associations, and the actual XL seismic
owner's interval, detonation scales/ages, magnitude and cadence. The native seismic
save compatibility getter returns zeros under XL and cannot supply that observation.
A null guided pointer is represented as `[-1, -1]`: every consumer checks that pointer before
its signature. Non-null bindings retain their raw signature even if stale, and
an out-of-pool pointer is emitted as slot `-2` and rejected by the reader.
The native extension also captures the base escort owner's complete fixed save
fields through one shared field list, plus all ten stolen-item bytes and the
persistent-message preference. Its 150-byte message buffer is an explicit byte
array, preserving both native text encoding and trailing storage without a
Unicode conversion. Capture performs no save encoding, cache polling, owner
repair or timer sampling. Route state uses the same approach for its 107 save
operations: goal/unexplored targets, pending recalc and notifications, liveness,
avoidance, limiter timestamps, trigger activation bytes and counters. Capture
also retains the pending restore marker. The raw cache-poll deadline remains in
its native cached-host-clock domain in receipts. Semantic comparison uses its
remaining wait from the same predicate as the real poll consumer: a future deadline
less than two seconds away delays polling; other values permit an immediate poll.
The codec intentionally rebases this deadline to the restoring process's cached
clock. Capture reads that existing cached value without sampling or advancing time.
The remaining wait is required and compared; unknown fields remain compared.
Active cache loading and full routing are still unqualified. Disabled compiled route support is explicit.
Metadata captures the existing detached typed snapshot: all canonical/live/candidate
plans, hashes and generations, decisions, certificates, both full workspaces and
pending work summaries. Its shared field list also drives the unchanged native
codec. Full fixed arrays and text storage remain visible. Long-double scores use
an exact sign, binary significand and exponent, preserving native precision and
negative zero without copying ABI padding or narrowing to double. Metadata and
active-route qualification remain incomplete.
Earlier XL schema-10/11/12/13/14/15 receipts retain their historical scope; they cannot
prove the newly required native-state observation. The Omega recording probe exercises menu-equivalent
auto-leveling, homing and weapon-order changes, two ordinary routing setter calls
at frame 102, preference restoration at frame 197, and routing in the stop tail.
Its runner checks all four recorded operation sites alongside the pickup, rearm,
combat and recharge workload. This is scripted settings qualification, not a UI menu
or connected-progression playthrough.

Native D2 comparisons retain the full raw awareness buffer in the trace. Only
the known segment/type/position fields beyond `Num_awareness_events` are ignored
semantically: `ai.c` processes and saves only the active queue, and enqueue
overwrites all three fields before increasing its count. Active entries, the
count, storage length and unknown fields remain compared. Automap discovery
remains gameplay state because Guide-Bot unexplored routing consumes it; the
native live traversal owner invokes the ordinary view-discovery boundary after
simulation, as playback and interactive drawing do.

Every frame and the mandatory input tail carry `environment: {count, samples}`.
Each sample has a contiguous zero-based `sequence`, `phase` (`input` or `sim`),
`operation`, `consumer`, `subject`, and the four clock values. Input operation
numbers are one-based: a consumer inside the second dispatched operation has
`operation: 2`. Simulation samples have `operation: 0`. A single `input_end`
sample follows all input operations before any simulation samples; a tail instead
ends with `tail_end` and cannot contain simulation samples. Empty input batches
still have their boundary sample. This preserves multiple interleaved operations
and the record-stop tail without assuming one render per simulation frame.

Consumers are closed: `slow_frame`, `slow_off`, `pickup`, `camera_frame`, `wall_trigger`, `message_resume`, `omega_blocked` and
`omega_fired`, plus the two boundary names. Pickup samples identify the object
slot in `subject`; wall-trigger samples identify an index in 0..253. Message-resume
samples identify the acknowledged text ID in 1..32767. Other
consumers require `subject: -1`. Slow-frame, camera-frame and pickup
consumers belong to simulation. Counts, phases, ordering, required boundaries,
unknown fields and duplicate JSON keys reject during complete-file admission,
without replacing an already parsed recording.

`camera_frame` supplies the cached SDL millisecond clock at the ordinary object
simulation boundary, using `cached_ms` from that ordered sample. Wall-trigger
cooldowns consume the same cached integer clock at activation, including input
dispatch and simulation. The timer owner exposes its last SDL sample without a
new timer read or conversion from fixed point. XL adds a constant saved-domain
offset after ordinary disk restoration, so subsequent cached samples advance
from the saved origin instead of the new process's uptime. Neither consumer uses
the replay renderer's clock. Detailed XL time observations read the last consumed
sample, including the checkpoint origin before frame zero; they never sample a
timer, advance a consumer or update the save clock cache. The detailed trace
retains time factors/directions/threshold ages, pending controls, weapon markers,
pickup ages, camera-controller state and pending wall-trigger/cooldown state.
Save schema 41 retains cooldown activation and its age clamped at the source's
750 ms threshold. Loading rebases that age onto the restored cached clock or
admitted replay origin. The unqualified format-6 drafts lacking `cached_ms`
reject; they are diagnostic evidence, not a released compatibility contract.
Schema 41 also preserves runtime object birth time and append order, including
the next creation counter after deleted objects. These values control newborn
robot damage protection and the order of smart-weapon/radial target selection.
The pinned XL save omits them; the forward Redux continuation contract deliberately
preserves them so loading an earned save does not change identical future play.
Authored objects retain zero birth time/order. Every live slot/signature must
appear exactly once, in slot order; nonzero creation orders must be unique and
bounded by the saved counter, and birth times cannot exceed the saved game epoch.
The complete birth table validates before that table is published. Earlier
development XL schema-40 saves reject explicitly; stock saves and classic demos
keep their existing layouts. This table guarantee alone does not establish
atomicity of the surrounding native whole-save loader.
`Drop_afterburner_blob_flag` is scratch within `object_move_one`: that owner
clears it before reading controls and consumes it before returning. It needs no
cross-frame/save persistence. The continuation case uses this complete owner;
calling `read_flying_controls` alone can create an irrelevant stale-flag difference.
The existing projectile continuation owner combines newborn damage/order/reuse
with a live guided missile, Omega charge, afterburner input/resources, wiggle,
object clocks, a pending seismic event and derived stolen inventory on an authentic
thief definition. Identical future native controls steer
the missile and recharge the afterburner across uninterrupted, warm and cold
execution. A derived route goal fills the normal recalc limiter, saves its pending
request and verifies identical rejection before expiry and release at its deadline.
The fixture persists its Enhanced routing preference through the normal player
profile writer, since ordinary level restore reloads that preference from disk.
The developer lifecycle target includes the game's planner and live escort code.
This exercises the limiter rather than a complete active route or metadata load.
Ordinary stolen-item drops create actual loot, with an empty-inventory
control that consumes no RNG or object slots. This is an explicitly derived
mechanic case with seeded inventory, not proof of actual theft or connected original
play or proof of whole-save atomicity.
Timed level messages also consume this cached clock, matching the pinned
`ShowGameMessage` use of `nSDLTicks[0]`. Message age, text and opacity no longer
read host wall time. Save/trace readers derive effective age and expiry directly
without mutating the message owner or depending on a prior draw/update call.
Native message controls record ordinary frames, reload their timeline, hold its
clock while host time advances, then verify age and expiry as recorded samples
are consumed. These derived controls do not prove original navigation or completion.
Paused XL menu draws now give `FrameTime` zero and reset the next frame's host
timer origin, matching the paused branch of pinned `CalcFrameTime`. Stock timing
is unchanged. The existing rendered clock probe opens a real menu, processes
three event loops and checks zero XL duration, unchanged game time/SIM RNG and
the three actual render-counter contributions.
An XL modal also holds the cached gameplay tick through the interrupted frame,
matching the source's return before `GetSlowTicks`. The next input/frame boundary
releases it; restore and level reset clear the temporary hold. `message_resume`
observes real elapsed clocks and render counts after the normal pause/window
lifecycle, while retaining that cached tick for chained actions in the same frame.
Both live play and replay establish zero paused duration before opening the menu.
Replay displays and automatically acknowledges this source-defined single-button
message, whose return value never selects a gameplay action. Normal window
activation still clears controls; no recorded control or world state is injected.
The existing derived message fixture exercises an autoplay master, a modal and
a following timed child inside SIM. Live captures and repeated clean-process
rendered playback compare full objects/world/frames and RNG, including message
expiry on the next sampled clock. This qualifies the tested modal continuation;
true-headless message output and broader presentation independence remain G3 work.
The schema also persists both signed texture coordinates at all four corners of
every segment face. Scrolling UVs feed hit-point transparency as well as drawing;
restoring only texture IDs changes collision. All rows and coordinates validate
before any UV is applied. The bounded trailer allowance adds the maximum encoded
UV array and bounded birth rows to the prior 8 MiB owner allowance. Older development XL saves lack this
state and reject; stock save layouts remain unchanged.
Camera scan angle, direction, per-owner brake time and the cached frame boundary
are saved separately from attachment identity. All monitor rows validate before
publication, including agreement between monitors sharing an owner. The pinned
source's unused `t0` field remains zero. New-level format-6 baselines explicitly
carry `xl_camera_ticks`, bounded by the header's cached-clock origin. These fields
preserve ordinary save continuation as well as checkpoint playback; they do not
add playback-only camera corrections.
It also preserves the nonnegative fixed-point frame duration and SIM RNG call
counter alongside the stock save's SIM seed. The counter is diagnostic and used
for ordered-input validation; it does not choose random values. Frame duration
can be consumed before the next time calculation and is the fallback for a bad
timer delta. Ordinary XL disk restores retain both instead of inheriting startup
zeros. Malformed durations/counters reject before any trailer state is applied.
These observations do not establish complete XL owner coverage; that remains a
separate save/continuation qualification gate.

The current bounded profile uses nondecreasing signed 64-bit transition-clock
milliseconds, starting at least at 26 and leaving INT32_MAX overflow headroom.
Pickup and cached milliseconds are separately nondecreasing in 0..INT32_MAX, matching the currently
qualified signed native animation-clock domain; a longer process lifetime is
not silently wrapped into that profile. Render frames use unsigned 32-bit modulo
arithmetic: each observed forward difference is at most INT32_MAX, including
UINT32_MAX to zero. There are at most 16,384 samples per frame/tail, within the
existing record/file byte limits. No clock is inferred from display rate or
substituted for elapsed time. The recorder and replay owners now preserve and
consume this stream, including exact consumer/phase/operation checks and a
fully consumed tail. Native world admission requires the complete supported
profile and origin binding before restoration; unsupported backends reject
before loading the checkpoint or level.

The D2-only ordered command `xl_time_control` carries one boolean `bullet_time`.
Each ordinary Alt+S/Alt+B request is retained, including repeated requests in a
single batch. The runtime retains the source's pending-boolean coalescing until
the next simulation step. Formats before 6 reject this command. Android Extra
actions reach the same ordinary key handler. These code paths are not yet an
accepted live recording or Android qualification.

## Ordered input recording (format 5)

New captures preserve native pre-simulation dispatch order and input-time frame
timing, with explicit marker-reset, controls-clear, sample and direct-command
operations. The final input tail is mandatory. Replay compares computed residual
controls and SIM RNG instead of overwriting discrepancies. SIM state and absolute
call count are restored once at frame zero, including an empty first batch. Every
later batch and terminal tail must match the carried SIM state/count before any
input effect, then match the computed post-input boundary. External RNG reseeding
is not a supported input operation. Zero-frame/tail-only recordings are rejected. Playback
accepts format 5 only; use `android/tests/convert_public_replay.py` for bounded,
oracle-verified public-corpus conversion, or make a fresh recording. Conversion
is not a general guarantee that missing historical event order can be recovered.

All-weapons entry is recorded as `grant_all_weapons` with an explicit `grant`
variant: `d1_scourge`, `d1_bigred`, or `d2_honestbob`. D1's `gabbagabbahey`
activation is a separate `enable_cheats` command. The native live handler and
replay use the same implementation, including score reset, cheat state, ammo,
energy, laser level and quad lasers. Repeated grants and terminal-tail grants
retain their exact position among weapon selection and firing operations.

D1-in-D2 `honestbob` resolves the active D1 profile: PC shareware uses the
scourge loadout; registered D1 uses bigred's D1 loadout. Native D1 bigred and
D2 honestbob retain their existing masks and selection policy. This does not
unlock excluded shareware weapons or supply missing retail data. Other recognized
native cheats and debug commands stop companion input capture before changing
state, while ordinary live gameplay still proceeds. Grants during dead input
also stop capture; replay rejects dead/endlevel/wrong-profile grants.

`android/tests/test_all_weapons_runtime.ps1` captures fresh shareware starts,
checks native Spreadfire/Gauss use, builds immutable compact oracles, and verifies
headed/headless replay plus missing/wrong-grant controls. D1-in-D2 live capture
is a separate diagnostic; synthetic retail-mask tests do not claim retail-asset
playback. Android's menu still reaches these native handlers through its existing
text-input route; native host tests do not establish Android device fidelity.

The isolated [all-weapons evidence corpus](../tests/fixtures/public_all_weapons/README.md)
preserves three native compact pairs, four live reports, and the complete 22-run
qualification from clean source `7145f682f4dd946ce0a8019d087eba6523794c16`.
Its manifest pins the exact source, public assets, and unchanged evidence hashes.

Rear-view records the live engine's toggle/hold/release decisions, and playback
applies them without querying wall time. Its initial view/cockpit baseline is
explicit in new production captures. Automap records ordered lifecycle and visited
segment-depth effects; playback uses a virtual paused map, with no UI window.
Controls-only updates do not fire weapons or flush device input. D2 confirmed
marker deletion retains the resolved marker slot and object signature. Pure map
camera/redraw events do not grow the recording.

Android rewind stops companion input recording because rewind snapshots do not
retain pending ordered dispatch history. Gameplay rewind and classic demo
recording still work; start a fresh input recording afterward. Map debug/reveal
and Android reactor/matcen cheats also stop companion input recording with a clear
diagnostic. Normal rear-view and automap use do not cancel recording.

F3 and the existing reverse helper changes are ordered cockpit operations. New captures also record a
projection contract: screen dimensions, exact fixed-point aspect/base zoom,
portal depth, classic-depth mode and D2 missile-view preference. Replay requires
those initialized settings to match and explains any mismatch. Changing them
while recording cancels the companion with a notice; ordinary live settings still
change normally. Existing Android custom visual FoV does not change its baseline
discovery projection. A missing projection object in converted files leaves their
old startup behavior unchanged.

Map discovery is reproduced by a separate CPU pass for the actual main camera
and default D2 missile window. It does not alter homing candidate collection or
live camera policy. Active guided and other selected auxiliary cockpit cameras
remain unsupported for companion capture and produce a named stop notice.
Endlevel special-camera map fidelity is unqualified. These camera paths were
already unmodeled in aggregate recordings; cancellation makes that limit explicit
rather than preserving a recording that may silently diverge.
