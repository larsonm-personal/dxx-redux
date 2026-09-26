# D1-in-D2 usable completion

Status: active

User goal: continue until D1-in-D2 is reasonably usable/done, rather than stop
after each isolated repair. The consolidation plan F1-F5 remains the scope;
native D1 retirement is a separate decision. Cosmetic exactness and incidental
animation history are excluded per user direction; graphics must not alter SIM

1. Run all discovered D1 recordings in two native runs and one imported run on
   frozen current binaries. This is diagnostic evidence while remaining gates
   are open, not premature qualification. Fix actual earliest divergences
2. Complete death/respawn, save/rewind and multiplayer collision lifecycle
   coverage against actual engine callers; retain gameplay timing, avoid
   presentation serialization
3. Verify D1-only and both-installed startup, optional Guide-Bot lifecycle,
   custom art retirement, D1/D2/D1 switching and checkpoint presentation
4. Recheck co-op physical exit through level-two overlay/control/Back, join and
   restore; qualify available platform runtime and record real remaining gaps
5. Review migration/resource owners and reconcile stale acceptance/checker
   descriptions with evidence, without automatically declaring a green result
6. Audit every F1-F5 requirement against current artifacts before completion

Starting evidence: current dirty tree preserves the prior flyout reset/save
admission and death-camera FX fixes, all 114 host tests and all Android ABIs
build. Last goal-preceding turn was concrete progress (reproduced/fixed camera
SIM leak). User's outstanding_bugs.md edits remain untouched


## Active evidence, first continuation

- Full eight-recording paired run: temp/d1-usable-corpus, outer execution
  session 74611. Binaries/assets/harness/source were frozen before subsequent
  death fixes. This remains diagnostic baseline evidence, not a claim that
  later fixes are covered. Do not restart a live run just because it is slow
- Android both-installed D2 -> First Strike: 70/70 steps, helper exit 0 and
  original app data restored. temp/d1-launch-runtime-20260924-125109 and
  temp/d1-usable-optional-device.log. This verifies interaction/level travel
  and package publication, not a complete Guide-Bot spawn/restore lifecycle
- Actual consecutive-death regression reproduced stale time_dead in both
  engines: the immediate second death exploded before its own deadline and
  prematurely dropped gear/lost hostages. Both entry points now reset the
  death clock. No save field was introduced
- Actual native writer accepted a 273875-byte active-death save. Both writers
  now reject active death, and rewind skips death frames without replacing
  playable history. Android memory admission checks cover both explosion states
- test_d1_death.ps1 passes seven matching native/imported gameplay observations,
  including two full deaths and early dismissal, exact explosion boundary,
  single gear drop, respawn life decrement and save rejection. Before-fix logs
  and current checks: temp/d1-death-lifecycle. Ordinary D2 campaign control
  and full host suites pass (53/53 native, 61/61 D2). Android build session
  78725 exited 1 before compilation because cleanup refused the live corpus
  process; defer Android build/device checks until corpus releases the guard

Do not modify concurrent music-editor work or user outstanding_bugs.md changes


## Multiplayer cadence and presentation follow-up

The real local multiplayer contact path now has repeated-contact, strict packet
boundary, per-peer, reversed object order, wide clock and backwards-epoch
regressions in both engines (D2 profiles 2/1/2). They exposed an existing >=
boundary bug: id MAX_PLAYERS indexed past last_player_bump and still applied an
impulse. Both engines now reject it. Before-fix failures, corrected builds,
quality and all 53/61 host passes are in temp/d1-bump-cadence

This cadence is network-local contact throttling. Its existing backwards-time
guard handles restored/travel epochs; no new save record or reset was justified
by these cases. Real peer sessions remain a separate test

Fresh host weapon-art comparison passes ten stock/custom/reloaded/retired/
after-D2 Spreadfire views, in D1-only and both-installed imported configurations
(temp/d1-usable-weapon-art.log, temp/d1-weapon-art-comparison)

Added test_d1_optional_guidebot.jsonc and -Guidebot to the isolated Android
helper. It exercises cold D1 with optional D2 data, deployment, docking,
ordinary quick-save/load, then menu-driven D1/D2/D1 switching and redeployment
Current status: Android build session 32834 completed successfully for all
three ABIs. Guide-Bot scenario passed all 45 steps on emulator-5556, including
cold deployment, docking, quick-save/load and D1/D2/D1 redeployment. Fresh
native and imported Android weapon-art runs both completed with helper exit 0,
including real memory save/rewind restore and active-death/flyout admission
checks. Imported GPU output matches the fresh native reference

Preserved device evidence is in temp/d1-usable-device-evidence: guidebot-roundtrip,
native-memory, imported-memory and the earlier both-installed-baseline. This
supersedes the earlier pending Android build/check status, not the immutable
historical death-lifecycle manifest. ARM64 runtime remains pending; only x86-64
emulators were connected. Fresh two-peer physical transition is next

## Fresh co-op and obsolete sound path

Imported D1 physical exit passed on both peers with the death/bump APK:
temp/d1-usable-lan-d2-D1LevelTransition, wrapper exit 0. Durable host/client
automation passed and both Android overlays were active in level 2; Back opened
the game menu and returned to the playable mine on each device. The isolated
wrapper restores both emulators' original files/preferences after capture

Caller inventory found d1_in_d2_apply_sounds and its error accessor entirely
unused (no production or test caller). Removed that alternate bank replacement
path, its private sound-map reader and flags. The generation reader, output
conversion and publication remain the production owner. Host build and all
61 D2 tests pass. Android rebuild was refused before compilation while ctest
held the cleanup guard; retry started after ctest exited. This cleanup does not
claim closure of F4: effect/reactor fixtures still call legacy overlay APIs,
and Guide-Bot/cockpit backup paths still need connected retirement review

Native control temp/d1-usable-lan-d1-D1LevelTransition exited 1: both peers
failed waiting for reactor destruction, before entering the exit. Host firing
did not destroy the reactor within 30 seconds. This is an unresolved control
failure, not evidence of a native level-transition regression. Both original
app directories were restored. Imported co-op death check is now running;
the wrapper also preserves the final introspection snapshot for diagnosis

CoopDeath attempt temp/d1-usable-lan-d2-CoopDeath exited 1 before gameplay:
client startup never completed, with app and Android system ANRs in preserved
logcat. Host memory was below 1 GB free during concurrent compilation. Do not
infer a gameplay death regression or dismiss the startup failure without an
idle retry. Original app directories were restored; emulator-5556 reboot was
started afterward (session 43488). Recheck native transition and imported
death after recovery, with no concurrent build

The first sound-cleanup Android build passed but exposed the now-unused
seek_d1_final_sound_maps helper. Removed it too; final host build passes and
the final three-ABI build (session 11402, temp/d1-sound-overlay-android-final.log)
has no new warning so far. No need to rerun the full host suite solely for
removing a compiler-confirmed unused static function. Await final build exit

Final build exited 0, all three ABIs, no unused-function warning. Final host
build and scoped quality also pass. Hashes/check logs are recorded in
temp/d1-sound-overlay-final-manifest.json. Emulator-5556 reboot completed and
both devices received the final APK. Native physical-transition retry is
running with no concurrent build (session 60337). Prior result/automation/
logcat evidence is copied to temp/d1-usable-native-transition-failure; the
first wrapper run.log was overwritten at retry startup. Retention prunes
families but does not rotate an existing fixed output path

Native retry again failed at reactor destruction. Its snapshot file was stale
(last written during initial sync), so added a live introspection refresh in
Write-DeviceAutomationDiagnostics before Cleanup force-stops the engines.
PowerShell quality passes. Third diagnostic run is session 40502; do not run
another emulator scenario until it finishes. Second-failure evidence is in
temp/d1-usable-native-transition-retry-failure

Checker audit found FX RNG was still part of the aggregate gameplay verdict
and object/boundary RNG comparisons. Moved effects_rng to diagnostics and
excluded only FX state/calls from compared storage; availability, unknown
fields, raw evidence validation and all SIM values remain required. All 55
comparator tests pass (temp/d1-cosmetic-rng-checker-tests.log). The live corpus
uses its older frozen checker, so any cosmetic verdict there needs explicit
recomparison, not a retroactive pass or recapture

Third native transition failure now has fresh state: button 100 held, primary
fire state 0, energy unchanged, reactor shields unchanged, active joystick
table bound primary to desktop button 0 and default axes. Startup logs show
Android profile bindings loaded correctly (secondary 101 etc). auto_create_pilot
applied Android defaults after new_player_config had populated kc_joystick,
but omitted the kc_set_controls call already present in normal new-pilot menus.
Added that refresh in shared/net/auto_net.c. Scoped quality passes; Android
rebuild and the native transition regression still need to run. Concrete before
state: temp/lan-failure-emulator-5554.json and temp/d1-native-reactor-live.json

First full corpus case (level 14, 5696 frames) completed: native repeatability
passes every check; imported SIM RNG and terminal state pass. Strict object/
world mismatch is slot 2 reactor id 8 native versus 0 imported from restore
through terminal. Four object-related diagnostic hashes differ as a result.
Recording/native per-frame and SIM RNG disagreement remains separate despite
equal terminal state. Source semantics of reactor ID still need investigation;
do not silently ignore it. Corpus continues level 15 on frozen binaries

Source confirmation: native get_reactor_definition ignores id and selects
Reactors[0]; imported d1_in_d2_fixup_level_object explicitly sets id=0 for
version-1 mines. This points to a representation difference, not a reactor
behavior change. Canonical observation/hash handling still needs a deliberate
fix with tests; do not change gameplay merely to match an unused native ID

Android build for the new-pilot input refresh started in session 61208,
temp/d1-lan-input-android-build.log. No emulator test is currently active.
After successful packaging, install on both peers, preserve the current native
failure directory before reusing its fixed path, then rerun native transition.
The imported CoopDeath startup/ANR attempt also needs an idle retry. Keep the
full corpus session 74611 running; it has advanced to level-15 native-repeat

## Continuation after process interruption

At 14:51 both previous execution handles were missing and process inventory
confirmed no corpus or emulator processes remained. The old Android build
had failed on its cleanup guard. Corpus evidence completed levels 14/15 and
both native level-16 captures; its imported level-16 capture was interrupted.
Preserved the entire original output. temp/continue-d1-usable-corpus.py verifies
the pinned assets/binary packages/frozen harness and unchanged capture helpers,
reuses completed evidence and resumes missing captures into
temp/d1-usable-corpus-continuation (session 64665). It uses the frozen old
checker and baseline binaries, not the later engine or FX-verdict fixes

Input-refresh Android retry passed all three ABIs (temp/d1-lan-input-android-retry.log).
Both emulators were booted/provisioned, then native two-peer transition passed
with helper exit 0: real firing, flyout, briefing, playable level 2, overlay and
Back on both peers. Before-fix fresh-state evidence is archived in
temp/d1-lan-input-before. Imported co-op death retry is session 80936

Level-15 strict mismatch also exposed a real respawn difference at frame 1983:
D2 init_player_stats_new_ship calls init_ai_for_ship and replaces every cloak
memory slot with the dying ship position/time. Native D1 does not. Added
assertions to the existing actual-death integration trace, including ordinary
D2's intentionally refreshing behavior, and built both host tests. Before-fix
integration is running; preserve the failure before changing the engine

The new actual-death assertion passed native and failed imported before the
fix; evidence copied to temp/d1-respawn-cloak-before. init_ai_for_ship now
returns for native-D1 gameplay, retaining ordinary D2 behavior. Both host
builds and scoped quality pass. The death integration and 21-phase campaign
comparison pass, including the ordinary D2 respawn/control expectations and
three D2 loaded-world save controls. Logs use temp/d1-respawn-cloak-*. Full D2
CTest is running (session recorded by the active tool); Android build pending

Imported co-op death also passed on both peers with the input-fix APK before
this new cloak-memory fix: temp/d1-usable-lan-d2-CoopDeath, helper exit 0.
Both isolated peer app directories were restored after both successful tests

## Latest Android qualification and unused animation loaders

The cloak-memory APK built successfully for all three ABIs. On both x86-64
emulators, imported SavedLateJoin passes: host restores alone, then the saved
client rejoins with plasma and six homing missiles. The original physical
level-1-to-2 transition also passes on this APK: host 26/26, client 23/23,
overlay active, gameplay controls, and Back menu/return on both peers. Both
helpers exit 0 and restore original app data. Evidence directories:
temp/d1-usable-lan-d2-SavedLateJoin and temp/d1-usable-lan-d2-D1LevelTransition.
The earlier transition evidence is preserved in
temp/d1-usable-imported-transition-before-cloak. APK/source hashes and logs:
temp/d1-respawn-cloak-evidence/manifest.json

Frozen level 16 completes with native repeatability, SIM RNG and terminal
result passing. Its strict imported object/world mismatch is only reactor ID
18 versus 0 (slot 16); diagnostic hashes remain strict failures. The immutable
baseline corpus continues level 18 in session 64665

Caller audit also found no production or test calls to
apply_powerup_vclips/apply_wall_anims. Removed those unused overlay APIs,
private readers/caches, and obsolete vclip copying in the legacy robot path.
The generation publication path and its Last_stats remain intact. Scoped
quality and host builds pass without new warnings; full D2 CTest runs in
session 14266. Android compilation of this cleanup remains pending

Device logs contain existing MEM_FREE_NOMALLOC warnings immediately before
TSF music starts and a MEM_FREE_NULL near initial level setup. No crash occurred,
but their owners are not established by these passing scenarios; retain them
for the resource-owner audit without changing concurrent music work

Unused-loader validation: all 61 D2 CTests pass (93.88s). The first Android
build passed but exposed unused skip_d1_vclips_and_effects after its last
caller was removed. Removed that private helper too; final scoped quality
and host build pass without warnings. Final three-ABI Android rebuild is
session 84711, temp/d1-unused-animation-loaders-android-final.log. No further
full host suite is needed solely for deletion of the unused static helper

Final Android cleanup rebuild was refused before compilation: retention waited
60s for active replay process PID 11584 and exited 1. This is the existing
build/test guard, not a compiler failure. Preserve its log, let corpus capture
finish, and retry during comparison idle time. Session 84711 is complete;
corpus session 64665 is still active and has reached level-18 imported capture.
The prior cleanup APK built all ABIs with the now-removed unused-helper warning;
the device-qualified cloak-memory APK predates only unused-loader cleanup

## Effective reactor identity in strict observations

Native D1 get_reactor_definition ignores object ID; imported source/save load
selects reactor definition zero. Native's renderer uses the separately stored
model_num. The checker now maps only the native expected reactor ID to zero
for cross-engine object/frame-boundary comparisons. Imported ID changes remain
failures; native-repeat checks raw IDs. Models, shields, gun geometry, unknown
fields and full raw records remain intact

Shared diagnostic schema 2 observes that effective native selector in object,
local-segment, whole-mine segment-list and player-contact diagnostics. D2 keeps
its actual selector for both ordinary and native-content profiles. Old captures
remain tied to their schema-1 checker; no historical verdict is retroactively
changed. C++ integration checks both engine profiles, all affected object/list
hashes and model/shield sensitivity. Python object/boundary tests cover the
mapping, rejected imported changes and immutable raw evidence

Initial builds and all 56 comparator tests pass. Native 53/53 and D2 61/61
host suites pass before the final whole-mine hash addition. Final scoped quality
and rebuild run in session 11745; targeted engine tests and a fresh level-15
paired capture remain next. README now describes actual current storage/FX
rules and boundary coverage instead of the obsolete no-mapping status

Final reactor observation host builds/quality and targeted upstream/recorder
checks pass in both engines. Android three-ABI build passes with no warnings
in changed sources; this also closes the pending unused-loader Android compile.
APK/source provenance: temp/d1-reactor-observation-android-manifest.json

Fresh paired level-15 run on current pinned binaries is session 43594,
temp/d1-reactor-respawn-current. It uses diagnostic schema 2 and the corrected
respawn behavior. Keep it separate from historical full corpus session 64665

Fresh level-15 capture issue: native-a and imported exited 0, but native-repeat
exited 1 after 161 complete frames (last frame 160), leaving truncated gzip and
no result/RNG finalization. Its full frame-state/diagnostic prefix exactly matches
native-a. The helper removed its sandbox in finally, so the original engine log
is unavailable; retain runner.log and partial trace rather than relabel failure.
The run is now comparing native/imported but cannot qualify repeatability

Diagnostic rerun of the same pinned native package uses KeepSandbox and
ReplayDebugLog in temp/d1-reactor-repeat-debug, session 3030. It has passed frame
160 and was at 489 when inspected. Do not treat a successful retry alone as an
explanation of the early exit. The existing helper's failure-log retention needs
review after the frozen corpus no longer depends on its unchanged source

Frozen level 18 finishes: native repeatability/SIM/terminal pass, imported strict
object/world only reactor ID 25 versus 0 (slot 10). Corpus advances level 5

Fresh native/imported level-15 comparison PASSES all checks across 2006 frames:
SIM RNG, frame diagnostics, object fields, world fields, checkpoint clock and
terminal result. This verifies the actual respawn AI-cache fix and effective
reactor mapping together. Recorded/native remains separately failed (historical
annotation/layout differences; RNG values themselves match in this case)

The debug native rerun exits 0. Its complete decompressed state and RNG traces
are byte-identical to native-a and its result JSON is identical; pinned native
package hashes are unchanged. Evidence: temp/d1-reactor-repeat-debug/verification.json.
The original failed repeat remains unresolved and preserved. A quiet repeat with
KeepSandbox (no extra debug logging) is running in temp/d1-reactor-repeat-quiet;
check its live session before relying on any expected result

Quiet native repeat completed with exit 0. Its entire decompressed state/RNG
traces and result JSON are also identical to native-a, with unchanged pinned
package hashes. Evidence: temp/d1-reactor-repeat-quiet/verification.json.
Thus both diagnostic and quiet repeats reproduce the full current native run;
the earlier failed attempt remains preserved/unexplained, not overwritten

Next work: continue full frozen corpus session 64665 (level 5 native-repeat as
last observed); retire the remaining connected effect/robot overlay and captured
Guide-Bot/cockpit backup paths by moving their fixture coverage onto generation
publication/real restore callers. Do not retain test-only production loaders just
to keep obsolete overlay-specific assertions. Raw recorder/helper failure-log
retention needs improvement once the frozen capture dependencies can be changed
without invalidating its manifest. ARM64 runtime is still unverified

## September 25 continuation: robot overlay and captured companion retirement

The frozen eight-demo continuation is terminal, with no live replay processes.
All eight native repeats and cross-engine SIM RNG/terminal checks pass; the two
later level-7 recordings pass all historical strict checks. Other strict failures
are reactor selectors and respawn cloak memory on binaries preceding their fixes.
The historical report stays unchanged and does not qualify current F1/F2.

Next F4 slice: migrate reactor coverage from test-only validate/apply APIs to
read/publish, the real level-object adapter, rejected preparation with an active
bank, and ordinary HAM reload. Remove the superseded robot overlay, pending bank,
D2 tuning/player backups and connected captured Guide-Bot append resources.
Retain generation-owned optional companion publication/availability/model checks.
Validate scoped quality, both host builds, full D2 integration and Android build.
Concurrent profiling/rendering edits and outstanding_bugs.md remain untouched.

Robot/captured-companion retirement passed both host builds, scoped quality and
all 114 host tests. The next connected slice is now implemented: effect fixture
coverage moved to a complete published generation (two original animation frames,
one-shot completion/orientation, invalid-reference rejection retaining the active
bank, and next-generation retirement). The legacy monitor replacement test still
covers its distinct bitmap protection/RLE duties. Removed apply_effects, its
private reader/cache, and the now-unused source-to-D2 model palette converter.
Native models retain indexed palette colors through the unchanged publication
contract. Final combined host build is live session 65726; new full tests pending.

An unrelated shared Android build failed on guidebot_route.c's undeclared
escort_set_goal_object call during concurrent routing changes. No Android or
new emulator qualification is claimed yet. Do not change or revert those edits
as part of asset retirement. Recheck current source/build before retrying.

Final shared-tree build hit two concurrent routing integration errors: a call to
nonexistent PHYSFSX_writeInt in state.c, and a missing declaration of the existing
escort_set_goal_object in the route module. Corrected only the symbol to the
existing PHYSFS_writeSLE32 and added the function declaration in its internal
header. Preserve all other routing work. Final scoped quality passes.

Current live build: session 91701, temp/d1-overlay-retirement-build-retry.log.
The previous session 65726 is terminal (link failed before correction). The retry
is confirmed live in CMake regeneration/vcpkg install, with cmake PID 6340,
regeneration PID 29744 and vcpkg PID 19636 at the last check. Poll this same
session rather than starting another build. After success, run full D2 CTest
(new effect fixture has not yet executed), then Android compile and isolated
Guide-Bot helper on emulator-5554. D1 assets: temp/d1-collision-clock-parity/d1-assets;
D2 assets available under game_data/gog installers/setup_descent_2_1.1_(16596)/extracted.
Do not claim the robot-only 114 pass covers the later effect test changes.

Further cockpit audit: apply_cockpit has no active caller, only inactive cleanup
inside reset_asset_context. That cleanup is called solely from Android mission
reset; gamedata_close/piggy registry retirement already releases the published
generation and clears its stats. The remaining cockpit/gauge capture/remap family
is therefore a concrete next removal candidate, followed by real cockpit/resource
and D1/D2/D1 reset validation. No removal of that family made in this turn.

## Cockpit backup retirement continuation

Previous turn was progress plus a verified wait. Session 91701 remains live and
has advanced from cmake --version to compiler hash detection/pwsh --version.
No new build has been started. Completed caller audit confirms no active cockpit
remap call: only reset_asset_context -> apply_cockpit(0) remained. Remove the
private cockpit/gauge capture/remap family, its API and the Android reset hook.
Normal gamedata_close -> piggy_close -> release_assets remains the single
retirement path. Extend the registry integration assertion to require all asset/
presentation diagnostics cleared, and run actual D1/D2/D1 cockpit rendering plus
the isolated Android Guide-Bot lifecycle once the build is available.

The isolated intermediate binary passed its entire integration suite (session
68334 exit 0), including the new effect fixture. Evidence and binary SHA256 are
in temp/d1-overlay-fixture-check. It predates the final cockpit removal; this is
fixture/earlier-effect evidence, not complete current-tree qualification.

The original build session 91701 is terminal. Its D1 build completed; D2 failed
with object-file permission errors because a separate build entered buildd2.
The vcpkg slowdown was scheduler starvation of Idle-priority child processes;
normalizing only the known build subtree allowed compiler detection to finish.
No unrelated process was terminated or reprioritized.

Current validation uses a separate build directory temp/d1-asset-retirement-build,
with the installed dependencies read from buildd2 and manifest installation off.
Session 32255 is live; log temp/d1-asset-retirement-build-configure-retry.log.
Initial configuration session 51993 failed because Ninja was not on PATH; the
retry explicitly selects the installed Ninja path and has passed compiler checks.
After build, run test_upstream_compat with --d1-assets, --d2-assets,
--presentation-graphics and --cockpit-dump-prefix to verify original rendering
and D1/D2/D1 resource switching, then fresh Android compilation/runtime checks.

## Non-render throughput clarification and fresh resource validation

The user's sub-25-fps observation is supported by the preserved paired timing:
2006 native D1 frames took 5.092 seconds (393.95 fps) without diagnostic traces,
and 110.951 seconds (18.08 fps) with full state/RNG traces. Both exited 0 and
produced the same terminal result. Evidence: temp/d1-replay-throughput/no-trace.log
and traced.log. This measures tracing as a whole, not individual serialization,
compression or filesystem costs. Replay bypasses calc_frame_time; there is no
25-fps limit in this path. The dedicated console runner currently supports D2
accelerated checkpoint replay, not native D1 or D1-in-D2. Do not compare its
untraced throughput against this diagnostic run or silently drop strict evidence.

Fresh isolated current-source D2 build session 32255 completed with exit 0.
Full integration with real registered D1/D2 resources, presentation graphics and
cockpit dumps is running as session 69588 in temp/d1-asset-retirement-current-check.
Binary SHA256 is recorded there. Android's shared package build is currently
owned by concurrent controller work; do not start an overlapping native build.

Session 69588 completed successfully with real D1/D2 resources and all integration
assertions. Visual review of its PNGs found a fixture sequencing error: the font/
menu checks leave MENU_PALETTE active before the D2 cockpit render. D1's menu and
level palette coincide, so only the D2 image exposed it. check_profile_cockpit
now calls the ordinary load_palette(Current_level_palette, 0, 1) before drawing,
matching the engine's return-to-level path. No production palette code changed.
Scoped quality passes; rebuilt fixture session 21308 is pending, with log
 temp/d1-asset-retirement-palette-build.log. Rerun all integration checks and
inspect fresh D2 PNGs before claiming visual qualification.

The current Android APK was packaged by concurrent controller work at 22:05.
That work now owns emulator-5554 through test_gamepad_menu_navigation_unified;
do not overlap the Guide-Bot lifecycle run. Emulator-5580 is also reserved by
other ongoing work. Fresh Android runtime qualification remains pending.

The fixture rebuild completed with exit 0 as session 97019. The first launch
(session 21308) failed before building because cmd.exe parsed a forward-slash
batch path as arguments; the retry used its absolute Windows path. Final test
is live session 65582 in temp/d1-asset-retirement-palette-check, with executable
SHA256 and eventual exit-code.txt. Poll it, then inspect cockpit-d2-0.png and
cockpit-d1-0.png. The earlier current-check integration pass stands, but its D2
PNG used the menu palette and must not be cited as visual-fidelity evidence.

Throughput evidence also verifies exact terminal JSON byte equality. The traced
run contains 742577818 uncompressed state bytes (64111051 gzip bytes), confirming
that the 18.08-fps run includes substantial full-world diagnostic output. No
simulation/frame-cap/RNG behavior was changed in response to the timing question.

## Replay failure evidence retention

Previous turn made concrete progress: current resource integration passed,
visual review identified/fixed a test-only D2 menu-palette leak, and its corrected
rerun remains live (65582). Emulator validation is owned by concurrent tests.
While that runs, close the identified F1 harness gap: retain small native logs,
result, launch arguments and process outcome on failure before sandbox cleanup;
reject a nonzero engine exit even if it wrote a result. Preserve normal successful
cleanup and avoid retaining copied executable/DLL packages. Verify actual wrapper
execution using controlled child exit, timeout, result-then-failure, mismatch and
success cases. No changes to engine simulation or diagnostic trace content.

## Current terminal results and next work

Session 65582 completed exit 0. Corrected D2 cockpit PNG now has the level's
colors; D1 original key/camera pixel assertions also pass. Its output is
 temp/d1-asset-retirement-palette-check; binary SHA256 starts 81D17EFD41CB.
The D2 gauge foregrounds in this low-level fixture look dark/absent; its D2
checks currently assert GL validity and camera bounds, not foreground pixels.
Do not claim full D2 HUD pixel fidelity from it. Investigate fixture state versus
normal renderer entry before inferring a production regression.

Android session 26526 completed exit 0: all 45 Guide-Bot lifecycle steps pass,
original app data restoration finished. Evidence is copied under
 temp/d1-asset-retirement-android/device to survive the helper's next retention
cycle. Manifest records the APK hash D6DB77965D51..., asset source hashes and
newer compiled objects for all three ABIs. Device ABI verified x86_64.
Controller testing resumed on 5554 after our helper finished; 5556 and 5580 also
belong to other work. Recheck live processes before more Android work.

Final replay failure integration session 29122 passed all six cases and scoped
quality. Its report is temp/input_demo_replay_failures_20260925_222227_483/report.json.
Real imported replay session 66400 passed strict terminal comparison over 2006
frames using the new failure handling (temp/d1-asset-retirement-replay-check).
No live task-owned build/test remains from this turn.

Next F1 audit finding: d1_replay_parity.py copies several harness files into its
manifest, but capture() still executes the live repository wrapper, and the
copied dependency list omits dot-sourced platform/compat/menu helpers. The old
frozen corpus is historical evidence and must not be silently requalified.
Before starting the fresh full corpus, make its executed harness genuinely
frozen or fully enforce hash stability for every executed dependency, with a
regression covering source mutation. Then pin fresh matching native/imported
builds and rerun all eight cases. Remaining F4 owner review, edition/platform
qualification and ARM64 runtime remain open.

## Freeze the executed replay harness

Previous turn was progress: resource removal passed fresh host/Android lifecycle
checks and failed-run evidence retention passed six process integration cases.
This slice stages the Python controller, PowerShell runner and all helpers used
by paired captures in their original relative layout. The controller then runs
from that snapshot and invokes the staged runner with the explicit workspace
root. Hash checks reject staging races or later edits to the staged package.
An explicit valid data directory must bypass live data-index generation. Verify
source mutation isolation, staged corruption rejection and actual PowerShell
loading, then launch fresh paired captures using the corrected harness.

The frozen controller/runner package is implemented and all 61 comparator and
harness tests pass, including actual Python/PowerShell execution after live
source mutation, staged dependency corruption and failed-run archive retention.
Scoped quality passes. Fresh isolated native D1 and imported D2 builds pass.
Logs: temp/d1-frozen-harness-{quality,tests}.log and
temp/d1-frozen-corpus-build.log.

The full eight-demo corpus is live in session 82504, with outer log
temp/d1-frozen-corpus-run.log and output
temp/d1_replay_parity_20260925_223823_asset_retirement. The first level-15 native
capture and repeat both completed successfully; the imported capture is running.
Native binary SHA256 is B5AF006BFA6FB79E12DEA7A4E906F9EA850D53661B7B163B19E84996B46FA435;
imported is 1EEC76B0D35132DCF5713CB15D43C05D77074A59056329163CB7121EB987C356.
Process inspection confirms that captures execute the staged controller and
runner, with explicit original repository root and copied D1-only data. Keep
this run and its historical verdicts intact; poll the same session.

While the corpus runs, investigate the dark D2 gauge foregrounds in the resource
fixture. Extend the existing source-pixel assertion to the D2 key gauge, identify
missing normal renderer state before changing production, and rerun the complete
resource integration with original D1/D2 data. No claim of full D2 visual
qualification follows from the previous GL-validity-only assertion.

The new source-pixel assertion reproduces D2's first-frame gauge loss in
temp/d1-cockpit-foreground-before (exit 1): blue key pixel expected 148 but is 0.
Both original engines' cockpit_decode_alpha draw the entire decoded cockpit just
to upload its texture, overwriting the gauges rendered earlier in that frame.
Use the existing texture-upload operation directly and retire the decoded
texture itself, matching the already-correct imported D1 implementation. Keep
the two original-engine fixes mirrored. Extend the shared graphics fixture to
assert that overlay preparation preserves existing framebuffer pixels. Also
finish the fixture's normal return-to-level palette setup with gr_palette_load;
source inspection confirms the ordinary engine performs both palette calls.
Rebuild both host games/fixtures, run both graphics suites and the complete
registered D1/D2 resource integration, then review fresh PNGs.

Current E4 caller audit narrows the old checklist: coop_travel.c's physical_enabled
explicitly excludes EMULATING_D1, so its early trigger interceptor does not consume
native D1 compound actions in the current tree. Do not move native activation
ahead of that interceptor on the assumption that it does. The surviving native
action mask is explicitly marked in the trigger record; activate/cross execute
that mask. The representative type still prioritizes secret exit over normal
exit, while escort.c::side_has_exit_trigger and route confirmation query only the
representative type. Review combined exit flags through a levels-owned semantic
query alongside the legacy single-type fallback in switch.c. The format matrix
must distinguish current version-34 trigger storage from older imported saves;
no namespace guess or recovery of lost compound flags is justified by the type.

Both mirrored cockpit fixes and fixtures build successfully; native D1's complete
graphics/integration invocation passes in temp/d1-cockpit-foreground-native.
The full D2 graphics plus registered resource test is live in session 84925 under
temp/d1-cockpit-foreground-current. The frozen corpus remains live in 82504 and
has reached level-15 native/imported comparison. Its binaries intentionally
predate this draw-only correction. The latest APK predates it as well; Android
compile/runtime for these two new original-renderer edits remains pending.
Concurrent Gradle work hit the native-retention guard while the corpus runs;
do not bypass that guard or stop the preserved corpus for another build.
