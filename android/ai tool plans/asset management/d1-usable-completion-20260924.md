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

The combined --graphics/--presentation-graphics invocation (84925) exited 1
because the menu fixture closes the graphics system and the resource fixture
cannot reinitialize it in the same process. It is not a renderer-fix pass.
Separate final invocations both pass: D2 graphics session 17640 in
temp/d1-cockpit-foreground-d2-graphics, and registered resource/graphics session
72143 in temp/d1-cockpit-foreground-resources. The native D1 invocation above
also passes. Source-pixel checks now pass for both D1 and D2 blue keys, including
both D1/D2/D1 transitions. Reviewed full/status PNGs show the energy/shield/key
foregrounds and camera labels restored. Current fixture binary SHA256 is
6FB72B0F7D1352DE30487DE941429727E147ADC81733E436A32E13FFC7982E66.
Scoped mixed quality and changed-file whitespace checks pass. The concurrent
workspace commit 80af2244 includes the tested edits; this task did not create it.

The first frozen-corpus report is now written: level 15 passes all native-repeat
and native/imported checks, including all 2006 frames, object/world boundaries
and frames, SIM RNG and terminal state. Recording/native remains separately
failed on historical diagnostics (first frame fireball_changed_bucket and an
absent ctx_id at SIM event 18); SIM RNG values/order and terminal result match.
The overall gate remains incomplete and the old recording verdict is retained.
Session 82504 continues with the long level-7 144212 native capture. Next turn:
poll this same session, diagnose any actual native/imported failure at its first
divergence, continue the E3/E4 owner review above, and qualify the latest renderer
edits on Android after the retained corpus no longer blocks native builds.

## Runtime exit queries retain compound native actions

Previous turn was progress: mirrored cockpit correction, both graphics suites,
registered D1/D2/D1 source-pixel validation, and the first exact paired corpus
case completed. Session 82504 is confirmed live on level-7 144212; preserve it.

E4's next concrete defect is representative-type narrowing. A native trigger
with both EXIT and SECRET_EXIT retains both source flags but is represented as
TT_SECRET_EXIT. Guide-Bot's enhanced normal-exit search, normal-exit indicators,
guided-missile retirement, the exit cheat and route confirmation must query
actions rather than that lossy representative field. Add one neutral engine
query returning TRIGGER_EXIT/TRIGGER_SECRET_EXIT flags, with a narrow native
dispatch into levels in D2 and a direct source-mask read in reference D1.
Shared metadata/indicator consumers use that same operation. Keep disabled/ON
admission separate, matching their existing query behavior, and return no exit
for invalid indices. Verify actual Guide-Bot destination selection for compound
native actions, original routing mode, ordinary D2 and serialized trigger reads.

Retain explicit serialized-format consumers for the subsequent E3 review:
newdemo.c's TT_SECRET_EXIT check consumes D2-only ND_EVENT_SECRET_THINGY bytes;
blindly replacing that check with a runtime action query would change framing.
The Android physical co-op protocol is explicitly D2-only. Its implementation
and tests stay unchanged while this slice closes runtime query ownership.
The metadata scan's single-type projection and old imported-save fallbacks also
remain explicit follow-ups; this slice does not claim all of E3/E4 complete.

Runtime-query implementation and validation are complete for this slice. Both
host builds pass (session 52646 terminal 0), native D1 integration passes, and
D2 integration with registered D1/D2 resources passes (81402 terminal 0).
Redux-reference Original routing passes twice each on levels 1 and 11, including
save-mode checks (42706 terminal 0). Logs and binary hashes are retained under
temp/d1-trigger-exit-{native,imported,original}; scoped quality passes. The new
fixtures verify the real enhanced Guide-Bot exit destination for compound
native actions, unchanged original/ordinary-D2 behavior, and semantic queries
after all existing trigger flag/layout round trips. No Android build was started
while the pinned corpus owns the native-run retention guard.

The implementation ledger now contains the E3 format/namespace inventory and
its concrete next evidence: an actual imported save with a companion, followed
by cold resource restoration without that optional source (and with changed
source definitions). Existing same-assets save/load cannot close that gate.
The Android mission key is explicitly revision-independent; it is not a
definition-content identity. The legacy .dem header and secret-trigger framing
also have concrete unresolved source findings, recorded in the ledger.

Only task-owned live job at this handoff is still corpus session 82504. Native
level-7 144212 PID 8800 was verified responsive at frame 7398/16873 at 23:13;
window progress is authoritative while Windows directory metadata can retain
the gzip file's initial length until handles close. Poll the same live run,
not a replacement capture. It remains frozen before the cockpit/query changes.
Android validation for both later slices, the rest of the corpus, E3/E4 closure
and the wider F5 platform/edition gates remain open.

## E3 actual companion restore with unavailable or changed source

Previous goal turn was a verified wait: live corpus PID 8800 advanced to frame
10934/16873. The same run is still active; preserve its pinned inputs. Extend
the real host integration fixture to save a deployed registered companion,
retire/reload the generation and restore against unchanged, changed and absent
optional source data. Retain a normalized report of loader status and actor
identity before fixing the demonstrated failure. The changed HAM is confined
to the isolated fixture directory; installed packages remain untouched. After
reproduction, put definition identity/admission in the asset/format owners,
verify real restores and ordinary D2 controls, then run scoped quality/builds.

The actual companion save reproduction failed as expected: changed HAM mass
silently replaced the saved actor definition. The current imported writer now
uses version 39 and binds optional source bytes plus runtime mapping before the
object array; ordinary D2 remains version 38. Missing/changed identity fails
before saved actors are installed. D2's existing event-boundary menu recovery
now covers host identity rejection as well. The real window/event-loop fixture
verifies return to the menu and a subsequent valid restore.

Host builds, both integration suites, registered resource cycling, seven exact
native/imported resumed checkpoint cases and repeated Original Guide-Bot controls
pass. Final targeted source/codec/menu recovery passes in
`temp/d1-companion-save-assets-recovery-final` (session 75972 terminal 0).
Detailed scope, hashes and evidence are in the implementation ledger. The mixed
quality invocation was corrected to pass an actual PowerShell path array; its
complete log is `temp/d1-companion-save-assets-quality-complete.log`. Explicit
fixture/new-owner formatting and the final formatting rebuild complete the
source checks.

Next: old imported saves lack this identity. Use the archived version-38 save
in `temp/d1-companion-save-assets-repro/companion.sav` to demonstrate the current
legacy reader outcome, then make admission explicit without guessed namespaces.
The current change does not solve legacy optional/custom identity or prove
Android rewind/platform behavior. Keep the preserved eight-demo corpus running:
82504 advanced through the first long native capture to native-repeat PID 12084,
verified responsive at frame 16039/16873. It still uses earlier pinned binaries.
All new Android compilation/runtime remains pending, and F1-F5 remain open.

Final formatted host rebuild session 81001 completed with exit 0. Corpus session
82504 was polled directly and remains live; both long level-7 native captures
are finished and its imported capture is now PID 33784, verified responsive at
frame 40/16873. No verdict for this case is written yet. All other task-owned
build/test sessions in this slice are terminal. Preserve the same corpus run.

## E3 legacy imported-save admission

Previous goal turn was progress: current optional-source identity and actual
menu recovery were implemented and verified. The preserved corpus is still live
in its long imported capture (PID 33784, frame 848/16873 on this turn's check).
First run the archived version-38 companion save through the current loader,
with unchanged, changed and missing optional packages. Then define admission
from recorded format and asset provenance, without guessing a namespace from
numeric ranges. Preserve native D1 checkpoint import and ordinary D2 legacy
formats; retire any imported migration path that cannot meet its contract.

The archived old save confirmed silent companion-definition replacement even
with the current reader. D2-format saves targeting D1 now require version 39,
with explicit rejection before world replacement and visible menu recovery.
Version 31 existed on both sides of the old overlay/native-resource transition,
so it cannot identify a namespace. Native D1 checkpoint translation and ordinary
D2 formats retain their readers; no guessed migration is introduced.

The complete validation in `temp/d1-legacy-save-admission-recovery` passes
(95702 terminal 0): archived version-38 same/changed/missing source rejection,
current companion identity and real menu recovery, actual ordinary D2 saves
36/37/38, both host integration suites, registered resource cycling and seven
exact native/imported dynamic checkpoint scenarios. Legacy rejection retains
the current objects, clock and SIM RNG; a subsequent current save works.
An initial test caught hidden-window recovery on the new early failure path;
its failing `d1-legacy-save-admission-final` output is preserved. The final
implementation restores visibility before requesting the existing menu path.
Scoped quality uses an explicit path array and passes; pinned clang-format also
covers the excluded fixture and new owner CPP. See the ledger for hashes and
precise remaining persistence limits.

The same preserved corpus is now comparing long level-7 144212 after completing
both native captures and the imported capture. Python PID 35700 was verified
live and accumulating CPU; session 82504 still owns this run. Do not restart it.
Next: inspect its actual native/imported verdict when written, continue E3
base/custom identity and E4 compound metadata/serialized-demo consumer review,
then Android qualification once the native-retention guard is free. Current
Android artifacts still predate cockpit/query/identity slices; F1-F5 stay open.

Final formatted host build 48453 completed with exit 0; log is
`temp/d1-legacy-save-admission-formatted-build.log`. It also picked up concurrent
graphics edits, so the exact integration evidence remains bound to the hashes
above. Corpus session 82504 was polled again and is live, now comparing native
repeatability for long level 7. All save-validation jobs from this slice are
terminal; the corpus is the only remaining task-owned live job.

## September 26: base and custom definition identity

Previous goal turn was progress: older imported-save admission and its complete
host validation finished. The pinned corpus is still comparing long level 7.
Next extend the actual custom-checkpoint fixture to save its imported world,
change/remove the HX1 robot definitions in the isolated write directory, reload
resources and attempt restore. Preserve the reproduction before fixing it.
Definition identity belongs to the prepared asset generation and existing save
adapter, without hashing mutable simulation, rendered output or host structs.
Keep ordinary D2 framing and native D1 checkpoint translation unchanged.

The reproduction confirmed silent substitution with both changed and absent
HX1. Imported save version 40 now binds base PIG/palette and selected PG1/DTX/HX1
bytes in the prepared generation, plus the existing optional companion digest.
The record is checked before saved actors are installed. Paths, output audio
conversion, world/animation state and RNG are excluded. Whole selected source
files are bound conservatively; unrelated metadata/geometry is outside this
definition identity. Native D1 checkpoint translation and D2 save version 38
retain their contracts. Version 39 is explicitly unsupported by the new writer's
reader because it lacks mandatory base/custom identity.

The nine-case actual custom-source matrix and seven exact native/imported custom
checkpoint scenarios pass in `temp/d1-custom-save-identity-fixed` (13066 terminal
0). The broader companion/legacy/ordinary-D2/integration/resource/dynamic suite
passes in `temp/d1-base-custom-identity-validation` (44841 terminal 0), including
actual menu recovery and identical resumed frame records. The before-fix failure
is retained in `temp/d1-custom-save-identity-before`. The ledger records the
tested binary hashes and precise scope. Scoped quality and explicit fixture/new
CPP formatting pass.

Next persistence work is full swapped-save execution and Android memory rewind;
E4's metadata and legacy rendering-demo consumers also remain. The same corpus
process 35700 is verified live while comparing native repeatability for long
level 7. Preserve session 82504 and its pinned binaries/assets/harness. Do not
start Android builds while that run holds the retention guard. F1-F5 remain open.

Final formatting rebuild 19233 completed with exit 0 in
`temp/d1-base-custom-identity-formatted-build.log`. All new validation/build
sessions are terminal. Corpus 82504 was polled directly and remains live; Python
35700 continued accumulating CPU in the same native-repeatability comparison.

## September 26: complete save-reader audit and endian fields

Previous goal turn was progress: mandatory base/custom identity and its complete
host source/restore checks passed. Full opposite-endian save execution remains
unproven. Reader inspection found both original object_rw_swap implementations
using SWAPINT on 16-bit physics.turnroll and polygon animation angles. Reproduce
this with independently constructed opposite-endian object bytes in the shared
host fixture, fix both original readers, and run both integration suites before
building the complete swapped-save fixture. Do not claim that individual codec
tests close the full save requirement.

The same audit found an additional ordinary D2-save-path boss shield repair in
state.c that still runs for imported D1 objects. Native checkpoint translation
has its own preservation logic; inspect and exercise an imported re-save on
boss levels before claiming all boss health variants survive ordinary reload.

The object-width reproduction failed in both engines. Corrected the four
SWAPINT uses to SWAPSHORT per engine; both full host integration suites now pass
with registered resource cycling. Before/fixed evidence and binary hashes are
recorded in the ledger. Full swapped-save qualification remains open.

Next extend each native boss checkpoint case with an actual ordinary save and
reload, preserving a native D1 control. Cover both boss levels, every difficulty
and over-default/full/partial/one/zero/negative health with saved physics and
actual damage history. Reproduce before changing the D2-only repair boundary.

This reproduction confirmed 4000 saved native boss shields became 1000 on an
ordinary imported reload. The existing actor-role dispatch now limits D2's
repair to engine actors. Both native/imported boss levels pass all 60 paired
health/difficulty cases, with exact physics and damage history, and all 56
resumed frames from 14 scenarios match. Actual Counterstrike level-8 controls
pass all 30 cases with unchanged D2 repair, alongside the existing companion,
menu recovery and ordinary D2 legacy save checks. See the ledger for before/fixed
evidence and tested hashes. Build and quality sessions are terminal and passing.

All new test sessions are terminal: boss reproduction 7666 failed as expected;
fixed matrix 29175 and D2 control 51705 passed. The sole task-owned live job is
the preserved corpus 82504, controller PID 35700 still accumulating CPU in long
level-7 native-repeatability comparison. Do not restart it or bypass its Android
retention guard. Next: complete swapped-save fixtures and remaining E3/E4 review;
Android rewind/format-40 qualification remains pending. F1-F5 remain open.

## September 26: full opposite-endian save fixtures

Previous turn was progress: the object angle-width and ordinary imported boss
health fixes completed their actual host controls. Continue E3 with independent
full-file conversion of current native D1 18 and D2 38/imported 40 saves, checking
every byte through EOF and restoring through the real engine entry points.
Preserve explicit little-endian subrecords (vectors emitted by the LE helper,
secret identities and Guide-Bot routing) while reversing host-order fields.
Compare complete observed world and object state plus SIM seed, excluding
non-resumable exit-animation cosmetics. Reproduce reader failures before edits.

Source review found mixed-order vector writes versus swap-aware reads in AI
awareness and morph state, and explicit-LE Guide-Bot mode writes versus host-order
reads. These are hypotheses pending actual complete-save execution, not yet fixed.
Keep the frozen corpus live and its staged source/binaries unchanged.

The independent full-file matrix reproduced the mixed-order defects, plus a
missing CT_MORPH static-AI swap in native D1. Fixed the original readers and the
native checkpoint owner without changing any writer layout or version. The
final matrix passes all 23 complete world/object/SIM-seed comparisons across
native 18, imported 40, native checkpoint import, companion and ordinary D2 38.
All seven native/imported scenarios and 28 resumed frames match. Both complete
host integration suites, registered resource cycling, companion identity/menu
recovery, D2 36/37/38 and 30 D2 boss-health controls pass. See the ledger for
before/fixed evidence, hashes and scope limits; all new sessions are terminal.

The preserved corpus is still live (82504, controller 35700), now comparing long
level-7 native-to-imported. Next: E4 compound metadata/snapshot consumers and E3
imported rendering-demo framing, then current Android memory rewind and platform
checks when the retention guard is free. Full current single-player opposite-
endian file execution is now covered, but cooperative/historical/platform scope
is not inferred from it. No completion claim for F1-F5.

## September 26: compound trigger metadata and route snapshots

Previous goal turn was a verified wait on preserved corpus controller 35700,
plus confirmation of the existing trace-throughput measurements. The E4 review
confirms native metadata selects one D1 action and imported metadata exposes a
single D2 type. Replace that loss with ordered navigation actions in the shared
snapshot, preserve source action order and terminal exit behavior, and update
all route consumers and labels. Keep ordinary D2 actions unchanged. Exercise
compound illusion/door actions, exit selection, snapshot identity and actual
native/imported engine metadata before claiming this gate covered. Also move
flyout metadata and exit diagnostics to the existing engine exit query. Do not
change the active frozen corpus, Original Guide-Bot routing, or SIM RNG.

The compound metadata/snapshot host implementation is now validated. It retains
ordered navigation actions, applies native door/illusion effects, hashes all
actions, preserves both exit bits and keeps route destination recovery explicit.
Native repeated-crossing admission no longer inherits D2 one-shot consumption.
The initial reproduction and an intermediate alternative-exit failure are both
retained; the corrected final source passes 54 D1 and 62 D2 CTests, both full host
integrations, all four repeated Original Guide-Bot runs (levels 1/11), and actual
native/imported level-1 metadata publication. Extended compound-exit certifier
checks also pass in both builds. Full build and scoped quality are complete.
Evidence, exact hashes and output-comparison limits are in the ledger and
`temp/d1-compound-metadata-fixed`; all new sessions are terminal.

The preserved corpus finished long level 7 and exposed 45 frames differing in
primary_weapon_picked_up (first frame 2908), despite exact world/object/SIM-RNG
and terminal results. Native repeatability passes all six checks. The corpus is
now on level-14 native-repeat (live process 3636). Do not restart or modify it.
Next reproduce native/imported quad/laser acquisition and autoselection behavior,
then fix the actual latch/selection boundary rather than weakening diagnostics.
Continue E3 rendering-demo framing and Android rewind/platform qualification
when the corpus releases its retention guard. No F1-F5 completion claim.

## September 26: native pickup and autoselection parity

Previous goal turn was progress: compound-trigger metadata/routing and its full
host checks completed. The frozen corpus has an actual level-7 mismatch in the
primary pickup latch, while all world/object/SIM RNG and terminal checks pass.
Reproduce quad, laser, normal primary and duplicate pickups with default/custom
ordering, firing policies, first-pickup admission and existing pending choices.
Compare actual native/imported do_powerup and delayed_autoselect outcomes before
editing. Preserve ordinary D2 behavior and pending native quad identity through
save/checkpoint restore. Keep substantive native policy in d1_in_d2, not the
replay comparator. Do not modify the active corpus or suppress its diagnostics.

The pickup slice is now implemented and host-validated. Frame 2908 is a laser
upgrade while quads are selected; native logical weapon 16 versus acquired 0
explains the first-only latch difference. Actual do_powerup/release/followup
tests reproduced 360 mismatching cases; an intermediate run caught 48 remaining
ammo-selection cases caused by D2's outer helper undoing a native switch. Both
failures remain retained. Final 2112-case native/imported matrix matches, with
ordinary D2 byte-identical to its baseline and SIM RNG untouched throughout.

Native queued quad identity survives native checkpoint translation and imported
file/memory runtime records, including opposite-endian data. The complete save
matrix passes 23 comparisons and 28 resumed frames, both full host integrations
pass, and both engine/fixture builds plus final quality checks complete. See
the ledger for exact evidence, hashes and limits. Android runtime and a current
full corpus are not inferred from host checks or the older preserved capture.

All newly started jobs are terminal. The sole ongoing task job remains frozen
corpus 82504, controller 35700 accumulating CPU in level-14 native-to-imported
comparison. Next reproduce native weapon-order consumption by cycling,
empty-weapon fallback and reorder UI: those still use D2 domains even though
native replay configuration can contain logical quad index 16. This pickup
change handles ordinary D2 pilot orders with the native below-cutoff quad
default, but does not implement a separate pilot quad-order UI. Continue E3
rendering-demo framing and Android rewind/platform gates afterward. F1-F5 stay
open and outstanding_bugs.md remains untouched.

## September 26: complete native weapon-order consumers

Previous goal turn was progress: pickup/queue/save semantics now pass the actual
native/imported matrix and complete host checks. Current source confirms that
cycling and empty-weapon fallback still index D2 weapon tables with a native quad
entry from replay settings. Reproduce those real operations before editing.
Then give native ordering an explicit owner and persistent pilot fields, route
native reorder menus/cycling/fallback through that domain, and make replay
configuration/recording observe the same settings without overwriting D2 orders.
Preserve ordinary D2 profile layout and behavior. Validate native comparisons,
profile roundtrips, mission switching, recorded settings and unchanged pickup
and pending-save behavior. Continue the existing corpus without changing its
staged files; Android qualification remains behind its live retention guard.

The weapon-order slice is now implemented and host-validated. Separate native
pilot fields and text persistence feed real reorder menus, selection, availability,
recording and replay settings without putting quad 16 into D2's arrays. Initial
cycling/fallback reproduction failed 2234 of 8640 cases; native Fusion energy
admission explained the remaining intermediate mismatches. Expanded coverage
also reproduced a true native classic-fallback infinite loop with all six primary
choices above the cutoff. Removed the wrong five-slot wrap and guarded an empty
secondary cutoff. This does not identify the cause of every loading-screen hang.

Real replay startup exposed a pilot reload that overwrote recorded preferences
in both engines. Recorded configuration is now restored at the level-entry
boundary after that reload; regular gameplay is unaffected. Actual profile and
recorder tests pass for conflicting saved orders, optional invalid/missing fields,
native recordings imported into D2, and D1 -> D2 -> D1 mission entry.

Final 18240 selection cases and 2112 pickup/queue cases match native D1 with
ordinary D2 byte-identical to both retained baselines and SIM RNG untouched.
Both complete host integrations, all 23 complete endian restore comparisons,
28 resumed checkpoint frames, and parser/recorder/replay CTests in both builds
pass. Both game executables and fixtures build; scoped quality and whitespace
checks pass. Exact evidence and binary hashes are recorded in the ledger.

All newly started jobs are terminal. The preserved corpus remains active as
82504/controller 35700 on level-18 native-repeat; levels 14 and 16 pass all six
native-repeat/native-imported checks. These older binaries do not qualify this
new implementation. Continue E3 rendering-demo trigger framing and current
Android format-40/rewind/platform qualification after its retention guard is
free. F1-F5 remain open; outstanding_bugs.md is untouched.

## September 26: rendering-demo trigger framing

Previous goal turn was progress: native weapon ordering, persistence, recording
configuration and startup now pass focused and full host checks. Continue E3 by
reproducing actual rendering-demo trigger writer/reader behavior. Current D2
reader unconditionally consumes a status record for a representative secret-exit
type, but native activation never emits that record and some rejected ordinary
D2 exits return before emitting it. Establish exact byte-boundary evidence, fix
the format owner rather than relabeling semantic exit flags, and cover ordinary
D2 playback and endian rewrite controls. Keep native D1 .dem format support
separate and explicit. Do not alter the active frozen corpus or SIM RNG.

The rendering-demo framing fix is now host-validated. A terminated actual writer
fragment reproduced the old reader consuming byte 22 instead of reaching the
frame boundary at byte 31. The format owner now detects an optional status event
from the bytes; rejected ordinary D2 secret crossings do not become exits during
playback. No native action-mask projection is used to infer a record length.

Ten native/ordinary-D2 trigger scenarios pass exact frame-boundary and rewrite
checks without SIM RNG consumption. The regression is registered with D2 CTest.
Actual First Strike level-10 and Counterstrike level-3 files pass export, three
normal no-interpolation playback boundaries, and four complete object-free
endian conversions through the public converter. Eight truncated status records
fail without replacing prior output. Full optional-source/custom-demo identity
and every historical/object opcode are not inferred from these focused checks.

The malformed-file integration also reproduced removal of a caller-owned source
mount by the exporter. Existing mounts are now reused, source shadows rejected,
and only exporter-owned mounts/handles are retired. Success/failure/shadow tests
pass. A real registered native D1 version-13/type-2 demo remains explicitly
unsupported by D2 and is rejected without altering prior export output.

Final evidence is in `temp/d1-classic-final-missions`, the final CTest/dump logs,
and `temp/d1-classic-final-integration`; exact hashes and verification limits are
in the ledger. Both full host integrations, both engine/fixture builds and scoped
quality pass. All newly started jobs are terminal. Frozen corpus 82504/controller
35700 remains live on level-5 comparisons; level 18 passed all six native-repeat
and native/imported checks using its older staged binaries. Continue the remaining
optional/custom rendering-demo identity and network format review while it runs,
then current Android format-40 rewind/platform checks once the guard is free.
F1-F5 remain open and outstanding_bugs.md is untouched.

## September 26: rendering-demo source identity

Continue E3 with actual optional/custom object recordings, cold definition
loads, normal playback, export and endian rewrite. Reproduce changed/missing
source behavior before edits. Imported D1 rendering records need an explicit
format discriminator and source identity before object decoding, including
random frame access and level changes. Preserve ordinary D2 format behavior;
keep source identity policy in the existing asset owner. Do not infer the
namespace from IDs or change SIM RNG for presentation. Validate matching,
changed, missing and restored sources and retain malformed-file evidence.

This source-identity slice is implemented and host-validated. Actual pre-fix
companion recordings exported successfully even with changed or absent optional
sources. Imported native rendering demos now have explicit type 4/version 16
framing and per-frame source identity before object decoding. The shared asset
owner validates the same base/custom/optional identities used by saves; ordinary
D2 stays type 3. Older imported type-3 and native D1 type-2 files are explicitly
unsupported. No exit animation history or SIM RNG compensation was introduced.

Final actual recorder/playback/export/endian tests pass for optional and custom
sources, absent/changed/restored definitions, eleven malformed/unsupported cases,
normal and interpolated companion playback, frame seeking and a custom-to-stock
level transition. Rejected playback now returns to the menu without creating a
game window. Endian conversion reports failed publication and retains backups.
The actual console exporter also exposed and now fixes audio service callbacks
being uninitialized during native asset startup. The reusable asset runner can
qualify both the fixture and actual game executable with -GameExe.

Evidence and limits are in the ledger and
`temp/d1-classic-assets-interpolation-verified`. Both complete host integrations,
23 complete endian save comparisons, 28 resumed frames, ordinary D2 demo controls,
both builds and scoped quality pass. All new jobs are terminal. The preserved
corpus remains live as 82504/controller 35700 on final level-7 181652 imported capture;
level 5 and level-7 144813 passed all six native-repeat/native-imported checks
using older frozen binaries. F1-F5 and Android format-40/rewind/platform remain
open, with outstanding_bugs.md untouched. Continue applicable network format and
travel review while the corpus holds the build retention guard.

## September 26: network source admission

Previous turn was progress: rendering-demo source admission and headless console
startup now pass actual host tests. Continue E3/E4 at the live network boundary.
The current late-join object packet installs raw runtime IDs before the final
sync/checksum packet, with no optional/custom source identity. Reproduce using
the existing engine packet writer and reader with a real companion and cold
same/missing source generations. If confirmed, bind the prepared generation
before object installation and before accepting initial level sync, sharing
asset-owner identity policy. Keep ordinary D2 semantics and native D1 transport
separate; version any changed D2 wire framing. Qualify actual packet mutation,
malformed/source failure and normal controls, then live Android join/travel and
format-40 rewind once the frozen corpus releases the build guard.

The network defect is reproduced and fixed. Actual localhost UDP writers and
initial/final sync readers now admit the asset owner's fixed namespace before
object or metadata mutation. Matching/restored optional and custom assets pass;
changed/missing/reverse-optional availability and D1/D2 namespace mismatches
fail without world changes. Sixty-four malformed packet cases, ordinary D2,
both desktop network layouts, SIM RNG invariance, both complete host
integrations and eight focused CTests pass. D2 wire versions are 30078 Android
and 30025 desktop; native D1 framing is unchanged. See the ledger and
`temp/d1-network-assets-verified` for evidence and limits.

The frozen eight-case corpus finished without modification. All native-repeat
checks pass; seven native/imported cases pass all six checks, with only the
previously identified diagnostic-only mismatch in long level-7 144212. The
historical recording comparisons and explicit qualification gaps remain open;
these old binaries do not certify subsequent fixes. Session 82504 is terminal.

Current Android assembleDebug passes for ARMv7, ARM64 and x86-64. The updated APK
is installed on both x86-64 emulators. After preserving a run interrupted by a
four-hour clock gap and an unhealthy launcher retry, both emulators were cold
restarted. The actual imported D1 initial sync and level-1-to-level-2 transition
now pass, including gameplay input, Android overlays and Android Back on both
peers (`temp/d1-network-assets-android-transition-cold`). Ordinary D2 save,
cold host-only restore and late join also pass, as does native D1 initial sync
(`temp/d1-network-assets-android-d2-latejoin` and `-native`). All jobs are now
terminal and the current APK remains installed on both healthy emulators.
Continue imported format-40 save/rewind and remaining E4/F1-F5 qualification.
The user's outstanding_bugs.md is untouched.

## September 26: Android format-40 save and rewind

The previous goal turn was progress: source admission, complete host packet
tests and real Android transition/ordinary-D2/native-D1 controls passed. The
current installed APK hash still matches that qualification. Continue with
actual imported D1 cooperative save, cold host-only restore and returning
client inventory, then host/client memory rewind through the real transfer
and restore paths. Inspect emitted save headers and runtime state so a generic
green LAN check cannot stand in for format-40 coverage. Audit the shared test
oracles for native semantics and collect durable failure diagnostics before
changing engine code. Keep original D1, ordinary D2, SIM RNG and cosmetic
presentation history separate; do not alter outstanding_bugs.md.

The performance follow-up rechecked existing evidence rather than advancing the
remaining goal gates. Resume from current APK/device state. Actual imported D1
cooperative save, cold host-only restore and returning client inventory passed
in `temp/d1-format40-android-latejoin`; the retrieved DGSS file is version 40.
Two successive client-requested rewinds also passed their world/clock/private
inventory/history checks in `temp/d1-format40-android-client-rewind`. The runner
had disabled game logging and omitted native diagnostic/automation tags from
its saved logcat, so that run alone does not directly record memory format.
Enable those existing diagnostics in rewind tests and preserve both native tags;
qualify the rerun and the host-requested path, then current optional companion
cold deployment, actual save/restore and D1/D2/D1 resource switching. No engine
behavior changes are justified by the passing initial rewind run.

This slice now passes actual imported D1 file restore/late join, consecutive
client-requested network rewinds and host-requested rewind with exact snapshot
time and pause/packet-loss checks. Both peers directly record memory version 40
in `temp/d1-format40-android-{client-rewind-verified,host-rewind}`. The original
client run remains preserved separately. Evidence and limits are in the ledger.

The optional companion script now covers natural memory rewind as well as cold
deployment, file save/restore and D1/D2/D1 switching. The isolated helper pins
the ten-second rewind preference and captures native logs continuously, since
the final logcat ring had evicted early restore evidence. The final 54-step run
in `temp/d1-launch-runtime-20260926-090800` passes, with format-40 file and memory
restore records and a docked companion restored after redeployment. Original
app files/preferences are restored. The same logger and current APK pass all
67 D1-only interaction/level-transition steps in
`temp/d1-launch-runtime-20260926-090959`, reaching playable level 2 with no D2
assets. Mixed scoped quality and diff checks pass; no native behavior changed.

All new jobs are terminal. These results close the named x86-64 Android
format-40 checks, not all of F1-F5. Continue missing/changed-source Android
rewind recovery and deployed-companion cooperative persistence, plus the
independent state/lifetime/mapping audit and remaining platform/edition gates.
The completed older corpus remains intact; outstanding_bugs.md is untouched.

## September 26: cooperative companion rewind and source-failure recovery

The previous turn made progress: current x86-64 file/memory format-40 restores,
single-player companion rewind and D1-only travel now pass. The installed APK
and both devices still match that evidence; no prior test process remains live.
Extend the actual LAN rewind runner to collect client-owned deployed companion
history, dock it after recording, then verify restoration of its actor, health,
ownership and local control on both peers. Reuse existing deployment and rewind
actions, with an ordinary-D2 control; diagnose any failure before production
edits. Then exercise missing/changed source admission and usable recovery in an
isolated Android installation. Keep the independent F1-F5 gates open and do not
replace complete-scope qualification with these focused persistence checks.

The focused companion/source-recovery slice now passes. Two consecutive
client-requested companion rewinds pass imported D1 and ordinary D2 controls
with actual memory formats 40 and 38 respectively. The default host-requested
imported rewind also passes, including exact loaded snapshot time, retained
history and three pause/packet-loss stages. Explicit state on both peers
confirms one restored companion, 37 shields and client ownership/local control.
Evidence is in `temp/d1-coop-companion-rewind-{client,d2-control,host}`.

Actual Android missing and changed optional-source memory rewinds reject the
snapshot, close the game window and return to the main menu. Restoring the
source then permits a new game and the original valid format-40 file load,
with a released companion and active/unpaused game, all in the same process.
The corrected fixture uses visible menu/window state rather than requiring
Current_level_num to reset on error. Both cases pass all 21/6/9 phase steps;
stable evidence and the earlier menu-expectation failure are retained under
`temp/d1-rewind-source-device-evidence`. The appended-byte source case checks
identity admission, not malformed sample decoding. The ledger records limits.

No native behavior changed in this slice. Mixed scoped quality and diff checks
pass; all jobs are terminal, both emulators online, and isolated files and
preferences restored with no logcat child left. The APK is unchanged. The
earlier single-player and D1-only device evidence is additionally preserved
under `temp/d1-format40-device-evidence`. Continue the independent F1/F2
state/lifetime/mapping audit and remaining platform/edition/presentation gates;
F1-F5 remain open, the finished frozen corpus stays unchanged, and
outstanding_bugs.md remains untouched.

## September 26: private wall-blast lifetime audit

The preceding goal turn made progress: Android companion persistence and both
optional-source rejection/recovery cases passed. All jobs are terminal. Resume
the independent F1/F2 audit against current source rather than interpreting the
checker’s explicit incomplete status as a request to relax qualification.

One concrete uncovered state is the exploding-wall pool. Its timer controls
when WALL_BLASTED becomes passable and schedules damaging, SIM-randomized
explosions. Ordinary D2 saves store the pool, while native D1's current writer
appears to omit it. Reproduce a real blastable-wall save/load before deciding on
production changes. Compare uninterrupted execution with restored execution,
native D1 with imported D1, and ordinary D2; inspect boundary timing and RNG.
If confirmed, repair persistence in the existing native/save-translation owners
and extend the shared observation contract. Cosmetic exit animation history
remains excluded. Preserve old-save admission policy and ordinary D2 framing.

The omission is confirmed and fixed. The actual campaign fixture saved during
five blast phases (0, 8, 24, 25 and 31 of 32 frames). Before the fix, native
restores lost the remaining damaging explosions and SIM draws; the first three
phases also left the wall permanently blocked. Native format 19 now preserves
the active pool, the native-to-imported adapter validates and restores it, and
world schema 9 observes every active slot. Ordinary/imported D2 formats stay
38/40. Cosmetic exit history remains excluded.

`temp/d1-wall-blast-verified` passes all five phases, both byte orders and all
three engine modes, including actual native checkpoint imports and 60 malformed
suffix rejections without live-state mutation. Both host builds, 54 native and
63 D2 CTests, 62 comparator tests and scoped quality pass. The complete-save
integration rerun in `temp/d1-wall-blast-endian-verified` finishes with exit 0:
23 byte-order comparisons have no differences, and native/imported resumed-frame
files match exactly. The earlier failed run is preserved; it exposed a stale
test-only secret-record offset, now corrected for the new suffix.

Android builds all three ABIs. The current APK is installed on both emulators;
native and imported x86-64 memory restore, rewind and Spreadfire comparison
pass 16/16 steps each. Stable evidence and APK identity are in
`temp/d1-wall-blast-device-evidence`. Active wall-blast timing is host evidence;
the Android cases exercise ordinary memory restore with the new native suffix.
All jobs are terminal. outstanding_bugs.md remains untouched.

Next continue the independent state audit: pending weapon autoselect indices
are persisted but not yet in the complete observer; death-phase state and boss
network effect caching need classification against their consumers. F1-F5
remain open and the completed earlier frozen corpus is unchanged. The confirmed
non-render throughput figures remain 393.95 fps without traces versus 18.08 fps
with full state/RNG output on the same 2006-frame recording. The launcher cannot
currently use its dedicated D2 checkpoint console runner for native/imported D1.

## September 26: pending selection and death observation

The preceding turn completed the wall-blast integration verdict and preserved
its evidence. Continue F1 with the next consumer-backed gaps: the two pending
weapon indices and pickup latches influence selection after trigger release;
the active death clock, explosion/drop/abort flags and saved control state
influence ship destruction and respawn. Extend the existing world observer and
strict comparator, with actual death lifecycle and checkpoint continuation
checks. Observe active gameplay state without serializing death or camera
history. Classify boss effect-send caching from its consumers. Then capture a
fresh whole D1 corpus on frozen current binaries; retain the earlier corpus.

This slice passes its focused verification. World schema 10 now requires both
pending weapon selections/pickup latches and active death gameplay state.
The actual native/imported death regression matches twelve phases; ordinary D2
passes the same lifecycle check. There is no new death save state or cosmetic
history. Sixty-four comparator tests pass, and the full host suites pass after
correcting stale schema assertions in the observer fixture. Twenty-one campaign
phases, three ordinary D2 cadence restores and 23 complete-save endian
comparisons pass. Native/imported resumed frames match exactly. Stable host
evidence is in `temp/d1-selection-death-host-evidence`; complete checkpoint
evidence is in `temp/d1-selection-death-checkpoints`.

All Android ABIs build. Native/imported x86-64 memory restore, rewind and GPU
checks pass 16/16 each, with exact native-reference Spreadfire pixels. Both
devices have the current APK, and isolated files/preferences are restored.
Stable device evidence and APK identity are in
`temp/d1-selection-death-device-evidence`. No active death world trace was
collected on device; that behavior is verified by the host lifecycle tests.

The boss effect-send cache controls only remote animation start/stop and stays
out of gameplay history. Updated the authoritative section-0 stocktake to remove
the obsolete expectation of exit-animation carryover; the user's cosmetic/SIM
boundary remains authoritative. No outstanding_bugs.md edits were made.

The fresh all-eight paired corpus is now live in `temp/d1-selection-death-corpus`
(outer log `temp/d1-selection-death-corpus.log`, execution session 35090).
The controller snapshots binaries/assets/harness before capture; retain the
older completed corpus. Recheck this specific job to its terminal result before
launching any replacement or claiming full-corpus success. F1-F5 remain open.

## September 26: edition readiness mismatch

Previous turn made verified progress on world observation. The frozen corpus
is live and advancing normally; do not restart it. Independent F5 review found
native and imported readiness both reduce to D1 file presence. Actual Mac demo
data (PIG size 2714487) is reported ready for D1-in-D2 on the current APK, then
fails during startup with invalid vclips. Preserved before evidence is in
`temp/d1-edition-scope-evidence/mac-demo-before`; original device data is restored.

Keep edition knowledge in the native asset owner. Expose its known unsupported
layout admission result to launcher readiness and preflight, keeping native D1
available. Test the actual rejection and recovery with registered data, local
and SAF readiness, and unchanged ordinary D2 admission. This declares unsupported
editions explicitly; it does not add a new format decoder or claim Mac/shareware
fidelity. Preserve the frozen current corpus while implementing this boundary.

The native owner now supplies the known unsupported-edition policy to Android
readiness and launch preflight. Host builds, all 63 D2 CTests, the native
upstream fixture and all 14 launch-readiness JVM tests pass. The three-ABI APK
build passes. Actual Mac-demo Android admission now rejects before creating a
game process, gives the edition reason and preserves native D1 readiness.
Registered PC data subsequently passes the isolated 67-step interaction and
physical level-one-to-two control. These are separate launches, not evidence
of recovery within one game process. Stable before/after evidence and source
hashes are in `temp/d1-edition-scope-evidence`.

The two initial admission-fixture failures were test errors: a one-step script
serialized as an object, then a query for launch_error in setup introspection
when only multiplayer introspection exposed it. The fixture now uses a script
array and setup introspection exposes the existing preflight error. Both failed
fixtures are preserved separately. No new game error state was introduced.

The support matrix now records tested, unsupported and unverified edition and
platform scope. The live corpus was frozen before the edition changes and
remains separate evidence; leave its binary, harness and source capsule intact.
F1-F5 remain open. Outstanding bugs were not edited.

Edition admission also passes with D2 content installed; ordinary D2 readiness
is retained. Both emulators have the final APK, isolated data/preferences are
restored and no device test or logcat capture remains. The only live job is
the frozen corpus (execution session 35090), now comparing the first recording's
native repeatability. Continue that existing job, not a replacement capture.

## September 26: registry metadata boundary

The preceding turn completed edition admission and its Android positive
control. E2 review finds piggy_read_level_bitmap_flags unconditionally returns
unavailable for imported D1, preventing concealed-liquid texture facts even
though the published base/custom generation has authoritative source flags.
Reproduce through the existing asset integration fixture, then dispatch the
read-only query through the D1 owner. Keep ordinary PIG/POG mechanics unchanged
and remove mission/palette inference from that dispatch. Verify custom/stock
publication, rejected staging, descriptor changes, renderer flag mutation and
retirement; retain the tested destroyed-light paging retirement hook. Exercise
real registered D1/D2 switching and available host/Android builds. The live
corpus remains frozen and predates this metadata change.

The query regression failed before the repair. The implemented owner query and
profile dispatch pass all 63 D2 CTests, the native upstream integration and
actual registered D1/D2 lifecycle checks. The retained converted-light cleanup
is public through the facade; piggy.c no longer includes its private layout.
The only other original-file private-generation include was newdemo.c, for two
identity queries. Their existing declarations now live in the facade as well;
the rendering-demo integration passes after the move. Both host executables
and all three Android ABIs build. Scoped quality and diff whitespace checks
pass; existing DGSS/Gradle warnings remain.

Final Android APK AC700AD52C6D97F8880A229538595262E298B9D1F85967619C3DFA5851F3B029
passes the 67-step D1-only interaction and physical level transition in 57.023
seconds. Three earlier attempts are preserved: ADB dispatch disconnect,
60-second launcher startup timeout and a subsequent 180-second game timeout.
The emulator was showing one-second launcher frames and very slow class
verification. The same final APK/script passed after restarting only the
primary emulator, without wiping its data. Stable evidence and source hashes
are in temp/d1-bitmap-facts-evidence. Both emulators now have the final APK;
isolated files/preferences are restored, backup directories are absent and no
device test/logcat process remains.

The frozen corpus has advanced: level 14 passes all six required native-repeat
and native/imported checks, with FX diagnostics also passing. Historical
recording/native diagnostics and RNG context metadata still fail, while the
terminal result passes. Level 15 is comparing; session 35090 remains live.
Keep its pre-edition/pre-bitmap source boundary explicit and do not restart it.

Next E5 evidence to investigate: the slow-startup logs show the native D1
metadata worker failing with "PhysicsFS initialization failed: already
initialized" on a later request. init_levelmeta_runtime initializes PhysFS
before several fallible steps, but only marks runtime_ready at the end; a
failed first initialization can therefore enter PHYSFSX_init again. Identify
the first failure and reproduce sequential requests through the actual worker,
then make the partial initialization/restart lifetime explicit. This is not yet
fixed or established as the cause of the emulator-wide slowdown. F1-F5 remain
open; outstanding_bugs.md is unchanged.

## September 26: metadata initialization failure lifetime

Continue E5 from the preserved native D1 worker failure. Rebuild the actual
native D1 and D2 metadata workers, reproduce a failed initialization followed
by another request, and preserve the first error rather than only the later
PhysicsFS double-initialization symptom. Inspect Android request ownership and
host worker reuse before choosing recovery. Add targeted Android failure
diagnostics, make partial initialization retirement explicit, and verify valid
requests after recovery plus healthy worker reuse. Keep this separate from the
replay throughput measurement and the unexplained emulator slowdown. Preserve
the live frozen corpus (session 35090); its level 15 comparison has finished
and level 16 capture has begun.

The controlled Android reproduction now preserves the first failure: a valid
request pointing at registered D1 files returns "could not find descent.hog",
then a second request terminates with PhysFS already initialized. Android's
normal startup search paths did not consume metadata's -hogdir argument and
instead depended on a selected-game publication. The original timeout's first
result was deleted, so this is a new controlled reproduction, not recovery of
that missing result. Evidence: temp/d1-metadata-init-evidence/device-diagnostic.

Metadata now initializes search paths from its explicit data directory, with
that directory's SAF manifest and without selected-game mods/catalogs. Normal
game and preview startup retain their existing path selection. A partially
initialized runtime is marked unusable before mutation and emits the native
worker_restart_required flag. The Android service drains accepted work, replies
busy to queued requests, and retires; the host helper retires before returning
the original error. Healthy workers and ordinary request failures remain
reusable. Sparse failure logging preserves the reason after request disposal.

Actual native D1 and D2 host worker tests pass: two requests in an unusable
process return structured failures, the helper retires after the first error,
valid requests recover, and healthy reuse gives identical level metadata. Both
Android services pass failed initialization -> fresh-process valid request ->
ordinary request failure -> valid repeat with the same healthy PID. The shared
path test covers requested-root/SAF isolation from selected game/mod paths;
its main now propagates test failure as nonzero. The first added fixture had
a local/global variable typo, corrected before its passing build. All 59
metadata JVM tests, protocol watchdog tests and the three-ABI APK build pass.
Scoped quality passes. Final host workers are rebuilt from formatted sources.

APK A24451D814A12A5A75108D00FD76AF5E8F3548BCAB37F90C64D4721CF491A5A6 is installed
on both emulators. The existing D1-only interaction/physical level-transition
control passes 67/67 steps in 50.926 seconds. Stable evidence, source hashes,
test reports and the positive control are under temp/d1-metadata-init-evidence.
Original device files, preferences and cache are restored; test backup
directories are absent. outstanding_bugs.md is unchanged.

Next concrete remaining issue: the positive control's logs show imported D1
in-game active-level metadata still selecting the D2 worker, which requires
descent2.hog/d2demo.hog even in a D1-only installation. See game-control/logcat.txt
and native-logcat.txt in that evidence directory, especially request
b850b1ce-ab13-4ff4-9152-c3da73799f65. RouteMetadataBackground forwards its game
argument into the metadata target. Trace the engine/content identity and cache
publication contract before selecting a worker or teaching its initialization
about imported content; avoid filename guesses or silently changing route-cache
ownership. This is separate from the initialization retry failure fixed here.

Frozen corpus session 35090 remains live, capturing level 18. Levels 14, 15 and
16 pass all six native-repeat and native/imported checks; historical recording
comparisons still fail frame diagnostics/RNG context while terminal state passes.
The partial leaf-status summary is preserved as corpus-partial.json. The corpus
predates edition, bitmap metadata and this worker repair; do not restart it or
claim that it covers those changes. F1-F5 remain open. No cause has been proved
for the earlier emulator-wide slowdown, and tracing throughput is unchanged.

## September 26: imported active-level metadata content identity

The preceding turn made verified progress on initialization failure/recovery.
The frozen corpus remains live. Active imported-D1 metadata currently carries
only engine="d2"; startup therefore requires D2 assets. Keep the D2 worker and
D2 route-cache ownership/encoding, which the live game consumes. Pass an
explicit content_game from the native committed profile through the existing
JNI/target/request path, and reuse d1_in_d2_init_base_resources plus owned
per-level asset preparation in the worker. Include content in request/cache
identity and retire/retry when an initialized worker's context changes.
Verify actual imported host/service scans with D1-only and both-installed
assets, custom/stock preparation, ordinary D1/D2 controls and live cache
publication/adoption. Do not substitute a native-D1 cache or infer content
from a level filename. Preserve targeted failure logs and extend the existing
worker/device runners. F1-F5 remain open.
