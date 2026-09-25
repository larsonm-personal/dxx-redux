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
