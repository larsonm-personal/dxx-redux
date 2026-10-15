# input-based demo file regression tests

- these are demo files which consist of player inputs rather than game element positions (which is what the classic .dem files encode)
- these are meant to form a set of regression tests so we can replay them and check the final state produced. with a large enough body of demo files we should be able to lock down game engine behavior and enable larger refactoring passes, such as to de-duplicate d1/ and d2/

# todo

- remove debug state tracing from the early demos

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
