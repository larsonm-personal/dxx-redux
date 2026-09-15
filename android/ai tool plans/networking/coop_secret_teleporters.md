# Co-op secret teleporter investigation and proposed design

Status: implementation handed off for player testing (2026-09-14)

The user requested finishing the cross-world save/load gap and stopping further
edge-case expansion. The historical unchecked audit/acceptance items below are
deferred follow-up work, not a queue to continue automatically. Do not resume
the exhaustive audit without a new request or a player-reported issue

Final cross-world save/load test passed at 09:47:58 on 2026-09-14:
`test_lan.ps1 -Game d2 -InitialLevel 8 -SecretCrossRestore -AllowSecretWarps
-NoCoopQol -TimeoutSeconds 180 -SkipBuild`. The new reusable case saves normal
level 8 in slot 1, enters secret -2 and saves it in slot 0, restores the secret
in place, loads the normal save from the secret, then loads the secret save
from the normal mine. Both peers verify each saved campaign checksum and their
distinct saved scores/ammunition, rather than retaining pre-load inventory.
The real return teleporter then brings both peers back to base 8 with the
expected dormant world. No production fix was required. Results and both
native captures are in `temp/coop-cross-restore-{live,host-logcat,client-logcat}.log`.
Scoped formatting, Android x86_64 assembly and both Windows builds passed in
`temp/coop-cross-restore-{format,build,windows}.log`

Player testing should cover the independent server switches, seven-second
entry/return warning, competing exits with the normal winner preserving the
live evacuation countdown, settled saves/loads, and rewind within a visit.
Broader ordinary-observer/large-roster tests, the remaining reconnect/migration
and interruption combinations, and a full single-player/custom-mission control
matrix are deferred. Existing coverage below remains evidence for the cases
actually exercised; it does not imply exhaustive coverage

Normal-exit arbitration passed on the current APK at 09:35:48 on 2026-09-14
in `temp/coop-core-normal-race-live.log`, with both native captures retained.
The authored normal exit won against a delayed secret request. The first
exiter waited on the post-level screen while the other player remained in
the live mine: game time advanced and the destroyed reactor's timer decreased.
The secret exit rejected both physical passage and trigger activation. After
the remaining player used the normal exit, both peers advanced to level 9.
This verifies the user's requirement that a normal winner does not evacuate
or freeze the team

Protocol/trigger implementation audit (2026-09-14): `d2/main/switch.c` routes
exit triggers through `coop_travel_handle_exit_trigger` before legacy trigger
side effects. `physical_enabled` limits this path to supported D2 co-op with
AllowSecretWarps enabled, while allowing return from an already active secret.
Observers, remote legacy trigger broadcasts and shots cannot authorize travel.
Full UDP game info writes/reads the option independently of briefing settings;
the normal full-info version check rejects incompatible protocol versions.
Android now uses D2 30067 (D1 30064) after simplifying briefing progress; the
desktop protocol branch is unchanged. The physical entry/return and combined
secret-advance tests below passed at 08:12:15 and 08:10:07 respectively, and
current Android/Windows builds passed in `temp/coop-briefing-simple-*` logs.
This closes the protocol capability/trigger implementation item without claiming
the broader recovery and campaign acceptance audit is finished

Current-build travel/briefing integration passed on 2026-09-14 at 08:10:07 in
`temp/coop-options-advance-live.log`: the secret exit won against a delayed
normal exit, both players entered -2 with the base destroyed, then advanced to
level 9. Both enabled settings, matching briefing plans, the client host-waiting
notice and host Skip/Launch now were verified before the settled campaign and
inventory checks. Physical secret entry/return with briefings disabled passed
at 08:12:15 in `temp/coop-options-secret_only-live.log`; both settings remained
as requested, no presentation opened, and both peers agreed on campaign,
checkpoint and arrival state after each leg. Both cases used co-op QoL disabled
and retained full host/client native captures. These runs use D2 Android
protocol 30066 after briefing content agreement; no native change was needed

Current restore-failure coverage (2026-09-14): host/client native load errors
passed in D1 and D2, including both recovery dialogs, unchanged manual saves,
same-process new game and cold co-op reload. Real synchronous-loader deadline
expiry passed in both engines; D2 also repeated cold reload after that timeout.
The post-sync-change rollback/revisit regression passed at 02:37:23 on
2026-09-14 in `temp/coop-empty-briefing-rollback-retry.log`: D2 level 8,
`-SecretRollback -SecretRevisit -AllowSecretWarps -NoCoopQol -Briefings`.
Both peers recovered from an injected client destination-load failure, then
completed entry, return, revisit and return, with matching campaign/checkpoint/
arrival checksums and old gameplay/end-level packet rejection at visits 3-7.
Physical-trigger settled secret save/load also passed at 02:40:35 in
`temp/coop-sync-secret-save-revisit-live.log`, with
`-SecretSaveRestore -SecretRevisit -AllowSecretWarps -NoCoopQol -Briefings`.
The test entered through the real trigger, opened save/load menus, saved and
changed both inventories, restored the distinct player inventories and dormant
base, then returned, revisited and returned again with matching committed
campaign/checkpoint/arrival state. Neither run required native changes or a new
APK after the synchronous-loading fix. Deliberate UDP loss/reordering, larger
rosters and remaining briefing failure cases are still open. Detailed evidence
and fixture limitations follow

The first regression attempt stopped before travel with a client startup ANR
(`temp/coop-sync-secret-rollback-client-anr.txt`): the UI thread waited in
`RenderProxy::setStopped` while RenderThread waited in the emulator's
`rcCreateWindowSurface_enc`/QEMU graphics pipe. A subsequent recovery-helper
attempt restarted AVD 1 while waiting for serial 5556 and was explicitly
stopped. Both emulators were booted before the successful retry. The LAN runner
now recognizes Counterstrike level 8's empty briefing for all travel fixtures,
and rejects named deadline/input cases there so absence of pages cannot count
as coverage of those cases. Scoped formatting and that rejection check passed

The campaign-ending secret departure is implemented and passed the two-player
run at 03:09:01 on 2026-09-14 in
`temp/coop-secret-endgame-menu-ready-live.log`. Run it with
`test_lan.ps1 -Game d2 -InitialLevel 8 -SecretEndgame -AllowSecretWarps -NoCoopQol`.
The fixture uses Counterstrike's actual base/secret triggers and a destroyed
base, then sets the mission's last-normal-level bound to that base on both
peers. It does not test a separate authored custom mission file. Both peers
accepted the same terminal operation, retained their final inventory and score,
credited carried hostages once, kept game time frozen, rejected autosaves and
returned to an unpaused main menu. Native logs retain both successive frozen
state checks and final script PASS results

`COOP_CAMPAIGN_ENDGAME` now passes live preparation/validation and commits
through the existing host gate. The prepared campaign envelope includes an
eight-byte terminal visit reservation, introduced with D2 Android protocol 30065.
Briefing content agreement subsequently advances that protocol to 30066; see
`coop_briefings_and_launch.md` for its validation status.
There is no destination world load and the actual secret level number stays
unchanged. Every peer applies the agreed portable snapshot before acknowledging,
commits the terminal visit and enters normal end-game scores only after the
commit/release handshake. Scores keep gameplay and save/rewind actions blocked,
while score-status and release packets continue on that terminal visit. Pure
packet-policy tests reject old live/frozen visits and reject ordinary gameplay,
save and rewind traffic even when terminal score status is allowed

The first live attempt exposed periodic score-status sends gated solely on a
destroyed reactor, which timed out peers when the secret reactor was intact.
Those sends now also run during campaign completion. Subsequent fixture fixes
corrected its seeded energy expectation (70), handled the optional built-in
high-score confirmation, and removed an extra Escape after returning to menus.
Android assembly, Windows D1/D2 builds, scoped formatting and the campaign,
transition, world-visit and gameplay-fence tests in both engines passed; see
`temp/coop-secret-endgame-fence-{build,windows,native-d1,native-d2}.log`.
Terminal release loss/reordering, larger rosters and host loss before terminal release remain
separate validation gaps; the ordinary normal-exit waiting contract still applies

Long high-score prompt coverage is available through `-SecretEndgameModal`.
Its baseline failed on both peers at 03:27:34 on 2026-09-14 in
`temp/coop-endgame-modal-baseline-live.log`, with matching native captures.
After 25 seconds in the actual first-place prompt, each local player had been
restored to CONNECT_PLAYING and the last teammate packet was over 25 seconds
old. The mine stayed paused and final inventory/hostage checks still passed.
The Android terminal path now retains CONNECT_END_MENU after the co-op score
screen and uses the existing end-level polling callback in both high-score
prompts. The diagnostic reads the engine's high-score table to choose a
repeatable qualifying score, without rewriting or deleting that table.
The fixed run passed at 03:32:59 in `temp/coop-endgame-modal-fixed-live.log`,
with host/client native captures. Both peers retained CONNECT_END_MENU and
fresh teammate packets after the 25-second prompt wait, preserved final
inventory/hostage totals and frozen game time, rejected autosaving, and returned
to an unpaused main menu. Android assembly (1m 24s), Windows D1/D2 builds and
scoped formatting passed in `temp/coop-endgame-modal-fix-{build,windows,format}.log`.
This test keeps both peers at their prompts; it does not cover a host closing
the session before a teammate finishes

The separate `-SecretEndgameHostLeaves` case has the host leave normally while
the client remains at the actual high-score prompt. Its baseline failed at
03:37:32 on 2026-09-14 in `temp/coop-endgame-host-leaves-baseline-live.log`,
with host/client native captures. The host reached its main menu, but when the
client detected its departure, the released terminal operation became active
again through the host-disconnect failure path. Released campaign completion
now treats host departure as an ordinary end to the network session without
changing the local result or setting the quit flag. The frame timeout check
uses the same exception between release and opening scores; unfinished
operations, including a host still awaiting release acknowledgments, keep
their failure handling. The fixed two-player run passed at 03:42:40 in
`temp/coop-endgame-host-leaves-fixed-live.log`, with both native captures.
After the host reached its main menu, the client detected its missing heartbeat
and logged `host left completed campaign`. It stayed at the high-score prompt
with phase SETTLED, no active transition, and its own CONNECT_END_MENU status.
Final inventory/hostage totals, frozen game time and blocked autosaving passed;
the client then returned to an unpaused main menu. Android assembly (1m 22s),
Windows D1/D2 builds and scoped formatting passed in
`temp/coop-endgame-host-leaves-fix-{build,windows,format}.log`.
Host loss before release and deliberate interruption between local release and
opening the score window remain separate validation cases

The normal-winning exit race passed with the new protocol at 03:10:49 in
`temp/coop-secret-endgame-normal-race-live.log`. The host waited on the normal
exit screen while the client remained in the active mine, verified advancing
game/reactor clocks, rejected its delayed secret request and the blocked secret
trigger, then used the normal exit and joined the host in level 9. This keeps
normal exits individual; only the secret-winning route moves the whole team

The post-endgame-change physical save/load/revisit run passed at 03:14:11 in
`temp/coop-secret-endgame-save-revisit-live.log`, using D2 level 8 with
`-SecretSaveRestore -SecretRevisit -AllowSecretWarps -NoCoopQol -Briefings`.
It verified real entry, settled secret save/load with distinct inventories and
the dormant base, then return, revisit and return with matching campaign,
checkpoint and arrival state. This also exercises ordinary travel with the
extended prepared-campaign header, where the terminal reservation is zero

The D1 host-load failure run failed at 01:17:07 on 2026-09-14 in
`temp/coop-restore-load-error-d1-host-live.log`. The host reached its error
dialog, but the client remained inside synchronous level sync with its restore
active and the mine frozen. Inspection found that level sync flushes queued
packets, acknowledges MDATA without dispatch while WAITING, and does not tick
the restore deadline. Preserve queued restore control and reliable history,
allow stamped control dispatch during restore sync, and check the finite
load deadline/cancellation in both sync menus. Unwind the save reader before
closing the game window, because closing it inside sync longjmps past cleanup.
Retain retries to required peers temporarily marked WAITING. Validate both
loader roles, same-process new game and cold save retry on both engines

Initial patched validation: scoped formatting, Android assembly (1m 41s),
Windows D1/D2 builds and ten selected native checks per game passed in
`temp/coop-restore-sync-{format,build,windows,native-d1,native-d2}.log`.
The D1 host-error rerun verified both failure explanations, idle usable menus,
zero frozen-clock drift and unchanged save bytes at 01:30:09. The client
aborted while still in the synchronous loader (`local_loaded=0`). The run
then failed at 01:30:21 because the new-game fixture incorrectly selected
an Ok button in D1's input-only start-level menu. Use the established D1
Enter / D2 Ok navigation and rerun the complete scenario. Evidence:
`temp/coop-restore-sync-d1-host-live.log` and host/client native captures
The paired runner also needs the existing `Resolve-TestScript` preprocessing:
the native interpreter does not filter per-game `when` steps itself
The first resolver integration stopped before gameplay at 01:34:00 because
the shared helper assumes optional JSON properties may be absent, whereas
the LAN runner enables strict mode. Invoke that helper in a child scope with
strict mode off; verify resolution for both games before rerunning. The
runner formatter must be invoked in a fresh shell: inheriting the strict-mode
preflight made its single-path Count check fail and widened formatting scope.
That formatter was stopped; its unrelated changes to previously clean files
were reverted and the fresh-shell scoped run passed in
`temp/coop-restore-sync-scoped-format.log`. No native behavior changed

The complete D1 host-load-error scenario passed at 01:40:00 in
`temp/coop-restore-sync-d1-host-resolved-live.log`, terminal zero with both
native captures free of failed script results. Both players left frozen visit
2 with the correct failure explanation and unchanged selected save. The
failed host started a new game in the same process, then both processes
restarted and restored the saved reactor countdown and six/seven homing
missiles without replaying briefings. Simulation clocks and bidirectional
position traffic resumed

The equivalent D2 host-load-error scenario passed at 01:45:30 in
`temp/coop-restore-sync-d2-host-live.log`, terminal zero with both native
captures free of failed script results. Both failure reasons, idle unpaused
menus, unchanged save bytes, same-process new game and cold reactor/inventory
restore passed, with briefing suppression and subsequent bidirectional
position traffic. The patched APK now covers host-loader failure in both
engines. Both client-loader roles, a synchronous-loader deadline when no
error packet arrives, and secret travel/rollback regressions still need
coverage; the existing connected-stall test only stalls after loading

Next loader-deadline check: add a `sync_stalled` case that transfers the complete
save and receives the client's APPLY acknowledgement, then holds the host's
local application for up to 90 seconds while networking and its frozen-source
checks continue. Keep the client's real 60-second loader deadline unchanged.
The fixture refreshes only the artificial host hold's transfer-progress time
so the chunk-transfer idle timer cannot mask the client loader deadline.
Require native evidence of entering synchronous level sync and deadline expiry,
zero simulation/countdown drift, both recovery dialogs, the client's visible
load-failure report, unchanged save bytes and a usable same-process new game.
This covers a stalled loader with no prior abort message; deliberate UDP loss
and reordering of abort traffic remain separate coverage

The first D2 loader-deadline run reached the client's actual sync wait, but
the artificial host hold had not entered normal local-load liveness grace.
The host timed out the client after 15.8 seconds and closed its mine. The
client then reached its own synchronous-loader deadline and both reached
menus. The runner correctly failed the host's expected remote-error reason
at 02:00:30 in `temp/coop-restore-loader-deadline-d2-live.log`, with native
captures. Refresh the host fixture's local-load grace while holding it outside
the reader, then rerun. The client's real deadline and timeout logic stay
unchanged. Initial formatting and Android (2m 59s)/Windows-both builds passed
in `temp/coop-restore-loader-deadline-{format,build,windows}.log`

With the fixture grace corrected, Android (59s) and Windows-both builds and
scoped formatting passed in `temp/coop-restore-loader-grace-{build,windows,format}.log`.
D2 `-RestoreFailure sync_stalled -RestoreLossResume -Briefings -TimeoutSeconds
180 -SkipBuild` passed at 02:08:52 in
`temp/coop-restore-loader-grace-d2-live.log`, terminal zero with both native
captures free of failed script results. The client entered synchronous sync
at 02:05:08.371 and reached its actual loader deadline at 02:06:01.873 after
the earlier local load work. Both peers failed with `local_loaded=0`, paused
clocks and zero measured drift. No peer-liveness timeout or host migration
masked the deadline. The client received the deadline explanation and the
host the remote-failure explanation; menus were usable, the native report
identified the interrupted sync with its window visible, and the manual save
bytes were unchanged. Same-process new game and cold co-op reload passed,
including saved reactor/inventory, briefing suppression, simulation clocks
and bidirectional position traffic

D1 `-RestoreFailure sync_stalled -TimeoutSeconds 180 -SkipBuild` passed at
02:13:05 in `temp/coop-restore-loader-grace-d1-live.log`, terminal zero with
both native captures free of failed script results. The real synchronous
loader deadline, both failure explanations, idle unpaused menus, unchanged
manual save, visible native failure report and same-process client new game
passed. This run did not repeat cold reload; D1 cold recovery was verified
with the client-load-error scenario above. The runner's trailing EMU state
lines describe the initial connection snapshot, not the recovery result;
label them explicitly. Use the scenario's native assertions and captured
failure/new-game introspection as final-state evidence

D2 client-load-error validation completed at 02:20:20:
`-RestoreFailure load_client -RestoreLossResume -Briefings -TimeoutSeconds 180
-SkipBuild` passed in `temp/coop-restore-sync-d2-client-live.log`, terminal
zero with both native captures free of failed script results. Both cleanup
dialogs, frozen clocks, unchanged manual save, native visible-window report,
same-process client new game, and cold co-op reload passed. The saved reactor
and inventory, briefing suppression, advancing simulation clocks and resumed
bidirectional position traffic were verified after reload. The runner now
labels its original connection snapshots as Initial sync

Client-loader validation: D1 `-RestoreFailure load_client -RestoreLossResume
-Briefings -TimeoutSeconds 180 -SkipBuild` passed at 01:51:57 in
`temp/coop-restore-sync-d1-client-live.log`, terminal zero with both native
captures free of failed script results. Both failure dialogs and frozen
cleanup passed, the failed client started a new game in the same process,
and cold reload restored the saved reactor and inventory without briefing
replay. Subsequent simulation-clock and bidirectional position checks passed

Coordinated local load errors (2026-09-14): add `-RestoreFailure load_client`
and `load_host` to the paired failure runner. Arm the existing native
fail-after-hide hook on only the selected peer, after saving the real reactor
state. Require both peers to reach the frozen source before the failure,
then require local-load and remote-failure explanations, no resumed mine or
host migration, visible usable menus and unchanged manual save bytes. Verify
the loader's native failure report identifies the injected phase and restored
window visibility. Use the existing cold-retry option to verify the save still
loads after process restart. Reuse the current native injection and production
cleanup paths; change production only if live evidence reveals a defect

The first D2 client-load run passed at 01:11:49 in
`temp/coop-restore-load-error-d2-client-live.log`, including both failure
explanations, the native fail-after-hide report, unchanged save bytes and a
cold reload with restored countdown/inventory and no briefing replay. Before
the next run, add a same-process new-game check on the failed loader after
dialog acknowledgement. Verify the game remains active for three seconds
and the process ID is unchanged. This checks whether a pending request to
leave a failed load leaks into the next game, which a cold restart could hide

Connected restore stall (2026-09-14): extend the existing failure runner with
a stalled case that leaves both processes and network pumps alive while the
existing loaded fixture withholds completion. Keep the real 60-second loaded
deadline unchanged. Poll both peers to reject resumed gameplay, require a
deadline explanation on at least one peer and a recoverable failure on both,
verify no network liveness timeout caused the failure, preserve the manual
save hash and acknowledge both dialogs. Use the generic command name
`-RestoreFailure client|host|stalled`, retaining the previous participant-loss
name as a CLI alias for the already documented runs. Results follow

Completed connected-stall evidence: scoped runner/plan formatting passed in
`temp/coop-restore-stall-format.log` and the label-only follow-up passed in
`temp/coop-restore-stall-label-format.log`. The D2 and D1 runs of
`test_lan.ps1 -Game d2|d1 -RestoreFailure stalled -TimeoutSeconds 180 -SkipBuild`
passed at 00:57:52 and 01:02:26 in `temp/coop-restore-stall-{d2,d1}-live.log`,
terminal zero with full native captures and no failed script results. Both
processes remained alive and connected while completion was withheld. The
host reached its loaded deadline and the peer received the remote failure;
neither run logged a network liveness timeout or replacement-host promotion.
All four native failure records show loaded visit 2, phase 1, paused state and
zero fixture drift. Both peers showed persistent failure dialogs and returned
to a usable main menu after acknowledgement. The manual save hash remained
unchanged. Per-peer dialogs and native evidence are retained in
`temp/coop-restore-loss-{d1,d2}-stalled-emulator-{5554,5556}-final.json` and
`temp/coop-restore-failure-{d1,d2}-stalled-emulator-{5554,5556}-native.log`.
No production code or timeout changed: these runs used the previously built
and installed restore-recovery APK. This closes connected loaded-barrier stall
coverage for two peers, not a hung synchronous loader, a stall during release,
3+ peers or actual local load errors

Restore failure explanation and cold recovery (2026-09-14): retain a reason
when the ordinary restore barrier fails and present it once from the stable
main menu after mine teardown. Keep the message until acknowledged and flush
old game input before opening it. Explain that players can host or join a
saved co-op game to continue. Extend participant-loss tests to inspect and
dismiss the actual message, then optionally restart both app processes,
select the preserved manual save and verify restored timers, both private
inventories, resumed networking and no replayed briefings. This follows the
completed disconnect/migration fix below; results follow

Completed explanation/recovery evidence: scoped formatting, Android (1m 39s),
both Windows builds and ten selected native checks per engine passed in
`temp/coop-restore-recovery-{format,build,windows}.log` and
`temp/coop-restore-recovery-native-{d1,d2}.log`. Full live runs used
`test_lan.ps1 -Game d2 -RestoreParticipantLoss host -RestoreLossResume
-Briefings -TimeoutSeconds 180 -SkipBuild` and the equivalent D1 client-loss
case. They passed at 00:43:15 and 00:48:39 in
`temp/coop-restore-recovery-{d2-host,d1-client}-live.log`, terminal zero with
post-restart native captures and no failed script results. Before restart,
the runner captured the actual reason and recovery instructions, required
the dialog to persist for two seconds, acknowledged it through normal menu
input and asserted a usable unpaused main menu. The preserved manual save
hash matched its pre-failure hash. Native failure evidence is retained before
launcher log clearing in `temp/coop-restore-failure-{d2-host,d1-client}-native.log`
and dialog introspection in `temp/coop-restore-loss-{d2-host,d1-client}-final.json`

After restarting both app processes and selecting manual slot 0, both peers
restored six/seven homing ammo and the running destroyed-reactor countdown
with its saved 240-second original duration. Native checks required the
remaining time to be in the saved range and to advance with simulation time.
Bidirectional PDATA resumed. Briefings remained enabled, were suppressed for
the restore and had presentation count zero. Final recovered snapshots are
`temp/coop-restore-recovered-{d2-host,d1-client}-emulator-{5554,5556}.json`.
This also supplies live D1 cold countdown-load coverage. The reason is retained
through teardown and shown once from the stable main menu; it is consumed
before opening the modal to avoid reentrant duplicate dialogs. Failure reasons
also cover interrupted transfers, local load errors, remote failure and
deadlines, but those non-disconnect cases still need live coverage. Cold
recovery here starts through the normal LAN launch/save-slot automation; it
does not verify the launcher's physical saved-game selection UI

Ordinary restore participant-loss validation (2026-09-14): expose the local
restore barrier phase, loaded flag and visit in introspection. Add a paired
LAN scenario that saves a running reactor, mutates the mine, restores and
holds the loaded barrier while the runner force-stops one peer's actual
process. Require both peers to have loaded the same visit before injection,
then require a finite return to menus without advancing the survivor's
simulation or reactor while waiting. Run host-loss and client-loss cases
for both engines. Preserve the selected save bytes and report any failure
UX or cleanup defect revealed by the live run. Production timeout values
remain unchanged; the fixture delays release only. Results follow

The initial runner failed before injection because manual saves use the host
callsign, not the autosave basename. After correcting that path, the first
D2 client-loss injection returned to menus with frozen clocks but the runner
filtered out the DXX-DLOG evidence. Correct the diagnostic tag and retain
both failed runner attempts in `temp/coop-restore-loss-d2-client-live.log`
and `temp/coop-restore-loss-path-d2-client-live.log`. Fully checked D2 client
and host loss passed at 00:15:36 and 00:18:28 in
`temp/coop-restore-loss-checked-d2-{client,host}-live.log`: same loaded visit
2, no clock drift, menu return about 45 seconds after injection and unchanged
manual save. Host-loss native logs nevertheless reveal an unwanted promotion
and LAN advertising restart immediately before abort. Add a restore-specific
disconnect gate before host migration and require no promotion in the test.
The menu return at that stage lacked a persistent player-facing failure reason;
the explanation and cold recovery work above addresses that gap for the two
live disconnect cases

Completed participant-loss evidence: `test_lan.ps1 -Game d1|d2
-RestoreParticipantLoss client|host -TimeoutSeconds 180 -SkipBuild`.
D1 client loss passed at 00:21:10 in
`temp/coop-restore-loss-checked-d1-client-live.log`. After the migration gate,
host loss passed in D2 at 00:26:05 and D1 at 00:29:52 in
`temp/coop-restore-loss-migration-{d2,d1}-host-live.log`. All four final
survivor snapshots at `temp/coop-restore-loss-{d1,d2}-{client,host}-final.json`
show the main menu, no active game/network, an idle transfer and no remaining
time pause. Each injected failure logged loaded visit 2, paused state and
zero fixture drift; the selected manual save hash was unchanged. Host-loss
reruns assert no native promotion, launcher notification or replacement LAN
proxy. Their full native captures contain no promotion or failed script
results. Failures were driven by real network liveness timeouts after the
existing restore grace period, approximately 43-47 seconds after process
loss. This is not coverage of the barrier deadline with a still-connected
stalled peer, a load failing before local completion, 3+ peers, or cold resume
after failure. The fixture holds loaded peers for up to 60 seconds to make
the injection point observable; it does not alter production timeout values

Initial scoped formatting, Android (1m 36s), both Windows builds and nine
selected native checks per engine passed in
`temp/coop-restore-loss-{format,build,windows}.log` and
`temp/coop-restore-loss-native-{d1,d2}.log`. The migration fix passed scoped
formatting, Android (1m 5s), both Windows builds and the existing host-migration
policy test per engine in `temp/coop-restore-loss-migration-{format,build,windows}.log`
and `temp/coop-restore-loss-migration-native-{d1,d2}.log`. Runner path, log-tag
and no-promotion assertions passed their scoped formatting runs. Client-loss
cases preceded the host-only gate; host-loss cases exercised the rebuilt APK

Secret-level entry checkpoint restart (2026-09-13): add
`test_lan.ps1 -Game d2 -InitialLevel 8 -AllowSecretWarps -SecretRestart
-SecretRevisit -Briefings -NoCoopQol`. After physical secret entry, use the
natural arrival checkpoint, destroy the reactor, mutate both inventories and
scores, and request a real team restart. Require arrival inventory/score,
the authored robots and intact reactor, an unchanged canonical campaign
archive including the dormant main world, no briefing replay, a reusable
entry checkpoint and resumed gameplay. Use the existing asymmetric pause
and discarded-phase retry fixture, then physically return, revisit and
return. Share the restart runner between main and secret cases and share
the exact campaign comparison with rewind. The paired restart wait now
covers its 180-second load wait plus request/verification overhead. This
does not change production timeouts. Build and live results follow

Scoped formatting, Android (1m 1s), both Windows builds and nine selected
native checks per engine passed in `temp/coop-secret-restart-{format,build,windows}.log`
and `temp/coop-secret-restart-native-{d1,d2}.log`. The full live D2 sequence
passed at 23:56:17 in `temp/coop-secret-restart-d2-live.log`, terminal zero
with host/client native captures and no failed script results. The restart
advanced visit 2 -> 3 inside level -2. Both peers restored six homing ammo,
scores 1234/1235, an intact reactor and all 70 authored robots. Their full
342758-byte campaign archives matched the pre-restart baseline exactly
(checksum 4023004910, return level 8), and the host checkpoint remained READY.
Both peers passed source/loaded clock freeze and discarded-phase retry checks:
host source 12795ms, loaded 21317ms, slower-peer wait 17541ms and drops a8/a8;
client source 12669ms, loaded 21811ms and drops 50/50. These are application
phase discards, not proof of raw UDP packet-loss handling. Subsequent physical
return, revisit and final return passed world, portable-state, campaign,
arrival and current-visit history checks. Final snapshots
`temp/coop-secret-restart-{host,client}-final.json` show both in unpaused level
8, campaign generation 5, world visit 6 and presentation count still 1.
The travel fixtures seed their own portable state before each leg. This
proves the actual secret entry checkpoint restart and subsequent travel,
not retained/cold restart, death/request overlap or participant-loss recovery

Secret-level consecutive rewind validation (2026-09-13): add
`test_lan.ps1 -Game d2 -InitialLevel 8 -AllowSecretWarps -SecretRewind
-SecretRevisit -Briefings -NoCoopQol`. Enter through the authored secret exit,
collect natural history in the secret mine, destroy its reactor and send two
real client rewind requests. In addition to the existing player, clock,
history and presentation assertions, compare the canonical campaign archive
byte-for-byte on each peer after each rewind, including the dormant main
world. Require the engine's return level to match that campaign. Then execute
physical return, revisit and return through the existing campaign/arrival
and history checks. The shared rewind runner now serves both main and secret
scenarios. Portable-state travel fixtures still seed their own inventory
before each leg; this run verifies restored inventory inside the secret and
the existing portable-state contract during travel, not unchanged rewind
inventory carried across every leg. Build and live evidence follow

Scoped formatting, Android (1m 27s), both Windows builds and nine selected
native checks per engine passed in `temp/coop-secret-rewind-{format,build,windows}.log`
and `temp/coop-secret-rewind-native-{d1,d2}.log`. The first live attempt ended
at 23:37:31 in `temp/coop-secret-rewind-d2-live.log`: the runner expected
nonempty reading screens on level 8, although both peers had correctly
reported zero pages, all-ready launch reason 1 and unpaused gameplay. Captured
introspection: `temp/coop-secret-rewind-{host,client}-start.json`. Route this
scenario through the existing level-8 empty-briefing assertion. This changes
only the harness; the completed rerun follows

The complete D2 run passed at 23:44:45 in
`temp/coop-secret-rewind-empty-d2-live.log`, terminal zero with host/client
native captures and no failed script results. Both players completed two
real client-requested rewinds inside level -2, advancing visits 2 -> 3 -> 4.
Each restored homing ammo 8/9, scores 4567/4568 and an intact reactor, with
backwards simulation clocks and resumed gameplay. Both peers' full 342758-byte
campaign archives matched the saved baseline exactly after each rewind
(checksum 2068826626, return level 8); host history retained seven then three
snapshots. Subsequent physical return, revisit and final return all passed
the existing world, portable-state, arrival and campaign assertions. Host
history checks reported one current-world snapshot after each travel leg,
with matching level and campaign generation in rewind/restart archives.
Final introspection in `temp/coop-secret-rewind-{host,client}-final.json`
shows both peers in unpaused level 8, campaign generation 5, world visit 7,
briefing inactive and presentation count still 1. This adds completed
secret-level rewind and subsequent travel coverage; actual secret checkpoint
restart, failure recovery, retained/cold restart and the broader cases below
remain open

Consecutive client rewind correction (2026-09-13): use monotonic
timer time for the one-second client cooldown instead of GameTime64, which
moves backwards after success. Validate the request's player against its
authenticated sender before processing it. Add `-ClientRewind` to the LAN
runner: collect 45 seconds of natural history, then send two real client
requests with only 1.5 seconds after the first release. The host only enables
requests and waits; it does not call rewind directly. Require two distinct
world visits, backwards simulation-clock transitions, preserved baseline
player state/history, no presentation replay and resumed gameplay

Android (2m 50s) and both Windows builds passed in
`temp/coop-client-rewind-{build,windows}.log`; scoped formatting passed in
`temp/coop-client-rewind-format.log`. All nine selected native checks passed
per engine in `temp/coop-client-rewind-native-{d1,d2}.log`. D2 passed at
22:58:48 in `temp/coop-client-rewind-d2-live.log`, terminal zero with host/client
native captures and no failed script results. Options: `-ClientRewind
-Briefings -NoCoopQol -TimeoutSeconds 180 -SkipBuild`. The host accepted requests
at monotonic times 8818983 and 10131276 while simulation time moved from
5828444 back to 4670357. The second request therefore arrived before the
old simulation-time cooldown would have expired. Both peers advanced visits
1 -> 2 -> 3, restored homing ammo 8/9 and scores 4567/4568 with an intact
reactor, and kept presentation counts 1/1. Host history retained eight then
four snapshots at verification. Both resumed-clock checks passed. D1 live
validation passed after the fixture corrections documented below. This scenario uses actual client requests and normal
transfer barriers, without the injected delays/discarded phases in the
separate host-rewind fixture. It does not test mismatched-sender rejection,
participant loss, secret-level rewind or client request duplication

Initial D1 client-request run failed at 23:01:58 in
`temp/coop-client-rewind-d1-live.log`, with both native captures. The client
lost its last shields to authored robot fire at 23:01:39.831, immediately
before requesting rewind, and the host did not begin a transfer. The fixture
now seeds enough shields to survive idle history collection and explicitly
requires a living requester; this does not alter production death handling.
Request receipt diagnostics report authenticated sender, eligibility and
the host option before early returns. The corrected fixture needs a live
rerun; death/request overlap remains separate coverage

The corrected fixture and receipt diagnostics passed scoped formatting and
Android (1m 15s) / both Windows builds in
`temp/coop-client-rewind-alive-{format,build,windows}.log`. Its first live D1
attempt timed out before briefings at 23:10:37 in
`temp/coop-client-rewind-alive-d1-live.log`. Native captures show a client
MainActivity focus-event ANR followed by a system-server ANR. The app main
thread waited in HardwareRenderer/RenderProxy::setStopped in the preserved
DropBox report `temp/coop-client-rewind-alive-d1-app-anr.txt`; texture loading
eventually finished, but the initial briefing phase never settled. This run
does not exercise the fixture correction. Preserve this startup limitation
for investigation; restart the affected emulator and retry the same APK

The rebooted D1 run reached both requests, but its outer runner wait expired
at 23:22:17 in `temp/coop-client-rewind-reboot-d1-live.log`. Native captures
prove the first completed restore (baseline inventory, score, reactor and
presentation assertions) and acceptance of the second request before the
old simulation cooldown. Client texture caching took about 63 seconds for
the first load and 75 for the second; the outer wait incorrectly allowed
only 180 seconds for a script containing two sequential 180-second restore
waits. Set this paired phase's outer bound to 420 seconds to cover both
restore waits, both 15-second request waits and verification. Per-restore
and production timeouts are unchanged. Scoped formatting passed in
`temp/coop-client-rewind-timeout-format.log`; a complete rerun is pending.
The rebooted run also released two Gradle daemons after confirming both idle;
this is not evidence that memory pressure caused or fixed the startup ANR

Complete D1 rerun passed at 23:27:03 in
`temp/coop-client-rewind-bounded-d1-live.log`, terminal zero with host/client
native captures and no failed script results. The same options used for D2
ran the corrected fixture and paired timeout on the final APK. The host
received both requests from authenticated player 1 and accepted them at
monotonic times 8241546 and 9545123, while simulation time went backwards
from 6325863 to 4888855. Both peers completed visits 1 -> 2 -> 3, restored
homing ammo 8/9 and scores 4567/4568, kept the reactor intact and presentation
counts at 1/1, and passed resumed-clock checks. Host history retained seven
then three snapshots at verification. This proves two real client requests
and completed main-mine rewinds for both engines; it does not close the
startup ANR, death/request overlap or broader recovery/secret-level cases

Actual main-level rewind correction (2026-09-13): the queued host rewind replaced
the selected history index with -1 before apply, causing the common restore
routine to discard all history on success. Retain that local index in the
send context; captures remain blocked while the barrier owns the world.
Add `test_lan.ps1 -CoopRewind`: seed distinct player ammo/scores, collect 35
seconds of natural history, destroy the reactor and mutate inventory, select
a 20-second rewind with an older point still available, then perform the real
team rewind. Assert the baseline world/player state, exact host target clock,
older selectable history, no briefing replay, pause/retry behavior and resumed
simulation. Build and live results follow

D2 actual rewind passed at 22:38:33 in `temp/coop-rewind-history-d2-live.log`,
terminal zero with host/client native captures and no failed script results.
Options: `-CoopRewind -Briefings -NoCoopQol -TimeoutSeconds 180 -SkipBuild`.
The host selected index 6 at GameTime 3890545; both peers loaded exactly
3890545. After release the host still had eight snapshots (including resumed
capture) and successfully selected an older point. Player 0 restored 8 homing
ammo/score 4567; player 1 restored 9/4568; the reactor was intact, visit advanced
1 -> 2, and presentations stayed 1/1. Both pause/retry assertions passed.
Android and Windows builds passed in `temp/coop-rewind-history-{build,windows}.log`
(Android 2m 59s), along with nine selected native checks per engine. D1 passed
at 22:42:32 in `temp/coop-rewind-history-d1-live.log`, terminal zero with
host/client captures and no failed script results. Its host selected index 5
at GameTime 4191092; both peers loaded exactly that value, restored 8/9 homing
ammo and scores 4567/4568, and kept presentation counts 1/1. The host retained
seven snapshots by verification, including resumed capture, and selected an
older point successfully. Both pause/retry checks and resumed-clock checks
passed. This verifies host-initiated rewind and retained selectable history
in the tested main mine; it does not execute a second rewind or a client request

The client cooldown issue found during the host-rewind review is addressed
by the consecutive-request work above. Rewind across secret travel and
participant-loss recovery remain open

Actual main-level restart validation (2026-09-13): add `test_lan.ps1
-LevelRestart` for D1/D2. Remember each player's initial homing ammo/score and
visit after the natural level-entry checkpoint exists; destroy the authored
reactor and change both inventories/scores; request the real checkpoint restart.
Use the existing asymmetric pause and discarded-message fixture during the
restart, then require an intact reactor, the original per-player values, a
new visit of the same level, no briefing and resumed gameplay clocks. This
tests execution rather than only the busy-state guards. Build/live results
follow; secret-level/campaign checkpoint behavior remains additional work

Initial actual-restart run passed for D2 at 22:17:14 in
`temp/coop-level-restart-d2-live.log`, terminal zero with host/client captures
and no failed script results. It restored the original reactor, homing ammo
and scores, incremented the world visit, passed the delayed-peer/retry checks
and resumed simulation. Android (`temp/coop-level-restart-build.log`, 1m 26s)
and Windows both (`temp/coop-level-restart-windows.log`) builds passed. Briefings
were disabled in that run, so the fixture now also records the presentation
counter and server option. With `-Briefings`, it requires an observed initial
presentation and no counter increase or option change across restart

Both final enabled-briefing runs passed: D1 at 22:22:08 in
`temp/coop-level-restart-briefing-d1-live.log`, D2 at 22:25:23 in
`temp/coop-level-restart-briefing-d2-live.log`, terminal zero with each run's
host/client native captures and no failed script results. Commands use
`-Game <game> -LevelRestart -Briefings -NoCoopQol -TimeoutSeconds 180 -SkipBuild`.
Both peers restored level 1 on visit 2 from visit 1, an intact reactor and
their original zero homing ammo/score after mutation to 12/13 ammo and 7654/7655
score. Each peer's briefing option remained enabled and presentation count
remained 1/1. D1 host waited an additional 17721ms for its slower peer; D2 host
waited 17470ms. All pause/retry assertions passed (stages=3, failed=0, host
drops a8/a8 and client drops 50/50), and gameplay clocks resumed. Final Android
and Windows-both builds passed in `temp/coop-level-restart-briefing-{build,windows}.log`
(Android 1m), as did nine selected native checks per engine and scoped formatting.
This closes the tested fresh-main-level restart execution path. Actual rewind,
restart after secret entry/return, later-level carried inventory, retained/cold
restart and participant-loss recovery remain unverified

Asymmetric restore/retry validation (2026-09-13): extend the native
countdown fixture to hold the loaded host for three seconds and the client for
twenty, require at least five seconds of host waiting for that slower peer, and
check both clocks through the actual release. Discard the first authenticated
LOADED, RELEASE_ACK and RUN_ACK at the host and the first RELEASE and RUN at
the client, after reliable transport delivery. This exercises the barrier's
application retries, including a repeated RUN after the client has resumed;
it is not a dropped UDP datagram test. Require every discarded phase to arrive
again, retain the five-second source/seven-second transfer hold, and clear
test injection state at session reset. Build and live results are recorded below

The same audit found that level restart could still report READY during an
ordinary save transfer, and retained restart replaced its checkpoint before
the transfer's late busy check rejected the request. Restart now reports BUSY
and retained restart refuses before reading/replacing anything during restore,
travel or briefings. The armed host fixture attempts both restart paths while
frozen and checks the checkpoint pointer, size and checksum are unchanged

D2 passed this asymmetric/retry scenario at 22:00:54 in
`temp/coop-restore-retry-d2-live.log`, terminal zero with host/client native
captures and no failed script results. Android and Windows builds passed
(`temp/coop-restore-retry-guard-{build,windows}.log`), as did eight selected
native tests for each engine. The subsequent rewind audit found its capture
and selection entry points also lacked the ordinary-transfer guard. They now
refuse before changing/selecting history, and the fixture checks the blocked
result and unchanged history count, level and campaign generation. Validation
of the final rewind guard and both engines follows below

Final rewind-guard APK validation: Android build
`temp/coop-restore-retry-rewind-build.log` passed (1m 10s), Windows both passed
in `temp/coop-restore-retry-rewind-windows.log`, and all nine selected native
checks passed per engine, including rewind and transfer policy. D1 passed the
full two-peer countdown scenario at 22:06:07 in
`temp/coop-restore-retry-final-d1-live.log`, terminal zero with host/client
captures and no failed script results. Native results: host source hold 9921ms,
loaded hold 22067ms, extra slower-peer wait 17816ms, drops a8/a8; client source
9834ms, loaded 22452ms, drops 50/50. Both reported stages=3 and failed=0.
These include restart/retained-checkpoint blocking, rewind capture/selection
blocking, unchanged clocks until actual unpause, and retry of every discarded
phase. The final D2 rerun passed at 22:09:15 in
`temp/coop-restore-retry-final-d2-live.log`, terminal zero with host/client
captures and no failed script results. Host source hold 12475ms, loaded hold
21982ms, slower-peer wait 17734ms, drops a8/a8; client source 12069ms, loaded
22443ms, drops 50/50. Both reported stages=3 and failed=0, and saved countdown,
duration and inventory checks passed after clocks resumed. Actual rewind/restart
execution, cold restore, participant loss and dropped UDP datagrams remain
separate checks. The existing test only checks history count/identity on a
blocked rewind request; it does not establish restoration of populated rewind
history or full campaign restart behavior

Ordinary restore barrier implementation (2026-09-13, partial live validation):
restore, rewind and level restart now retain a full world-visit identity after
file transfer, own a simulation pause, and exchange loaded/release/run
acknowledgements. The game window pumps networking and renders waiting status
while paused; gameplay input and world mutations are fenced. Clients validate
the authenticated sender of readiness and accept release/abort only from the
host. A failed load, missing participant or expired barrier aborts the session
instead of releasing different worlds. Input queues are cleared at freeze and
release, and ordinary join admission is deferred while the transfer is busy.
Android and Windows-both builds passed before these last input/join additions.
Both D2 and D1 passed `-CountdownSave -NoCoopQol -TimeoutSeconds 180 -SkipBuild`
on two emulators: `temp/coop-restore-barrier-d2-live.log` (21:38:18) and
`temp/coop-restore-barrier-d1-live.log` (21:41:19), terminal zero with host/client
native captures and no failed script results. The fixture gives each frozen
source reactor five seconds, holds transfer for seven seconds, then holds each
loaded world for another seven seconds before its loaded acknowledgement.
Both engine threads verify unchanged countdown/GameTime during these holds,
then saved timer duration/inventory and resumed clocks after release.
All seven selected native tests passed for both engines after updating the
mixed-packet fixture for the expanded restore acknowledgement. Remaining
validation includes the final input/join guards, secret/cold restore, actual
rewind/restart, participant loss, dropped release packets and additional peers.
Normal-exit evacuation behavior remains unchanged

Final input/join build and secret regression (2026-09-13): scoped formatting,
Android assembleDebug (`temp/coop-restore-barrier-input-build.log`, terminal
zero, 2m 8s) and Windows both (`temp/coop-restore-barrier-input-windows.log`)
passed. Both emulators received that APK. The D2 level-8 run with
`-CountdownSave -SecretSaveRestore -AllowSecretWarps -NoCoopQol` passed at
21:47:43 in `temp/coop-restore-barrier-secret-d2-live.log`, terminal zero with
host/client native captures and no failed script results. It entered secret
-2, saved through the UI with a destroyed/counting reactor, restored both
inventories and the dormant base, verified the timer and resumed clock, then
physically returned both players to base 8. The seven-second holds were armed
for this restore; the explicit native `verify_pause` assertion is currently
in the standalone D1/D2 countdown scripts, not this secret runner branch.
This run establishes the secret restore/return regression, not additional
packet-loss, join/rejoin, failure or cross-process recovery coverage

Normal-exit regression on the final APK passed at 21:50:24 in
`temp/coop-restore-barrier-normal-d2-live.log`, with host/client native captures
and no failed script results. Options: D2, level 8, `-NormalExitRace
-AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild`. The remaining
client reported paused=0, blocked=0, reactor=1 and other_waiting=1, with
GameTime advancing by 617021 fixed units while the countdown fell by exactly
617021. Normal completion advanced both peers to the next mine. The restore
pause therefore did not freeze or evacuate the remaining normal-exit player
in this two-player regression. Next restore checks should explicitly delay
one peer longer than the other and retain clock assertions through the final
release, inject lost/duplicate release packets and participant loss, then
exercise actual rewind/restart and cold restoration under the same barrier

Cold-start reactor-countdown persistence validated (2026-09-13): combine
`-CountdownSave -SecretColdResume -InitialLevel 8 -AllowSecretWarps -NoCoopQol`
to save a destroyed secret reactor with its dormant base, close both app
processes, start a fresh lobby and restore the saved secret. The runner now
checks the live reactor state and advancing simulation clock on both engine
threads after cold restoration, in addition to its existing inventory,
campaign archive, no-briefing-replay and return-trip assertions. The full run
passed at 21:18:49 in `temp/coop-countdown-cold-d2-live.log`, with post-restart
host/client native captures. Both app processes were force-stopped, the host
reached a new lobby, both peers restored secret level -2 with the saved reactor
and six/seven homing missiles, and the physical return restored base level 8.
No briefing replay or campaign mismatch occurred. Both final native captures
contain no failed script result. Scoped runner formatting and `git diff --check`
passed. This is persistence coverage, not proof of the transaction-wide pause
guarantee described below; D1 cold countdown restoration remains unverified

Restore coordination audit (2026-09-13): the ordinary restore/rewind/restart
transport still needs a shared pause and finish barrier. In
`multi_save_transfer.c`, READY_APPLY is sent before the client's local apply;
the host waits for these applying acknowledgements, resets its send context,
then applies its own world. `multi_save_transfer_finish_restore()` ends local
restore state without waiting for all peers to finish. The regular game draw
paths gate simulation on `time_paused`, while this transport does not own a
team pause; the gameplay stamp only includes `multi_save_transfer_restoring()`,
not the earlier transfer interval. During the cold test, both live introspection
snapshots reported level 1, in_game=true, time_paused=false, restore waiting and
transfer_busy=true before applying the secret save. Secret travel supplies its own outer barrier,
which ordinary restore lacks. Existing countdown tests establish persistence
and subsequent clock advance, not a freeze across the whole transaction.
Required next work: preserve the transaction identity through post-load ready
and release acknowledgements, freeze source/loaded simulation and world packets
while continuing network/UI pumping, balance pause ownership through window
activation, and bound failure/host-loss recovery. Add a delayed-loader scenario
with a nearly expired source reactor and verify no countdown/game-time advance
or ordinary exit wins during restore. Do not infer this guarantee from the
successful broad-interval timer checks

Settled reactor-countdown saves (2026-09-13, living-team scenarios validated):
the Android co-op metadata is now v13 and records reactor destruction, exact
remaining time, displayed seconds, original countdown duration and explicit
reactor pause state. D1/D2 restore applies these fields instead of retaining
the base reader's reconstructed duration. Android manual save/load, quick
actions and autosave permit an active countdown while the team is alive;
transition/end-level guards remain. Death/drop-state saving is still pending.
The new `test_lan.ps1 -CountdownSave` scenario destroys the authored reactor,
saves through autosave and manual paths, changes countdown and inventory,
transfers/restores the save to both peers and checks that the restored timer
runs with simulation time. Scoped formatting and Android/Windows-both builds
passed (`temp/coop-countdown-save-{format,build,windows}.log`); four selected
save-format, campaign, recovery and player-session tests passed in each engine.
The final Android guard build passed in
`temp/coop-countdown-save-guard-build.log`. D2 passed at 20:55:33 and D1 at
20:57:23 with `-CountdownSave -NoCoopQol -TimeoutSeconds 180 -SkipBuild`;
logs are `temp/coop-countdown-save-{d1,d2}-live.log` plus native captures.
Both restored a destroyed reactor, original duration 240, running countdown
and six/seven homing missiles after changing the live timer/duration/inventory.
The fixture also rejects autosave with a peer in CONNECT_END_MENU. It checks
remaining time within a running-time interval, then equal timer/game-time
advance; exact release-time alignment is not established by those checks.
`-CountdownSave -SecretSaveRestore -InitialLevel 8 -AllowSecretWarps` now tests
the same state through secret save/load menus, with dormant-base restoration
and the subsequent return. That live run passed at 21:04:05 in
`temp/coop-countdown-save-secret-d2-live.log`, with native captures. Both peers
restored secret level -2, running reactor state and inventories, preserved the
dormant base, and completed the physical return to level 8. Neither capture
contains a failed script result. The v13 pre-release format replaces v12
directly, in accordance with repository format policy. Android multiplayer
versions are bumped to D1 30060 / D2 30061 so peers lacking this transferred
save format do not join; desktop versions remain unchanged. The final version
build passed in `temp/coop-countdown-save-protocol-build.log` (1m 27s), and
scoped header formatting passed. The final APK is installed on both emulators.
D2 NormalExitRace passed at 21:08:45 in
`temp/coop-countdown-save-normal-d2-live.log`, with native captures. The player
still in the mine reported paused=0, blocked=0, reactor=1, other_waiting=1,
game delta 612368 and countdown delta -612368 before ordinary advancement.
All four final scenario captures contain no failed script result, and
`git diff --check` passed. Dead-player/drop-state saves,
explicitly paused countdowns and cold-start countdown restore still need work
or live coverage

End-level packet fencing (2026-09-13, D1/D2 scenarios passed): Android D1/D2
ENDLEVEL_H and ENDLEVEL_C now append the existing 12-byte visit/level/frozen
stamp at serialization. Exact sizes are 186 and 40 bytes respectively. Receive
checks validate layout, role, token, sender and world before disconnection,
countdown, score/death statistics, kill matrix or liveness changes. Normal exit
waiting leaves this world open; secret/load/briefing freezes close it. Delayed
observer forwarding also checks the original stamp before transmission.
Android protocol versions are now D1 30058 / D2 30059; desktop versions and
wire layouts are unchanged. The paired released-world probe sends host/client
old-visit and frozen-origin copies over real UDP, poisoning connection states,
countdown and statistics. It requires both kinds rejected, no probe application,
unchanged tracked state and receipt of current packets. Same-visit packet order,
participant incarnation, object sync and live observer coverage remain open

Build/normal-exit validation: scoped formatting, Android and Windows-both builds
passed (`temp/coop-endlevel-stamp-{format,build,windows}.log`), as did all four
selected recovery/gameplay-fence/world-visit/transition-policy tests in each
engine. D2 NormalExitRace passed at 19:59:10 in
`temp/coop-endlevel-stamp-normal-d2-live.log`, with host/client native captures.
The escaping client recorded paused=0, blocked=0, reactor=1, other_waiting=1,
game delta 595788 and countdown delta -595788. Both peers advanced from level
8/visit 1 to level 9/visit 2; reliable counters continued past 149999. This
verifies current end-level packets still support ordinary individual exit waiting

Probe follow-up: the first world run stopped before travel when only the client
completed the probe (`temp/coop-endlevel-stamp-world-d2-live.log`). A diagnostic
rerun passed on both peers with rejected=3, applied=0, current=1, preserved=1
(`temp/coop-endlevel-stamp-diag-d2-live.log` and native captures), then stopped on
a staging check even though the script was present on the device. Probe sending
and final verification now use separate paired scripts: neither peer stops
transmitting until both receivers have passed. This closes the test's early-stop
window while retaining the receive/state assertions. A failed five-second staging
read gets one further fifteen-second read without restaging or relaunching.
The full D2 world rerun passed at 20:16:47 in
`temp/coop-endlevel-stamp-barrier-d2-live.log`, with host/client native captures.
Both peers passed MDATA, PDATA, end-level and full-info checks on visits
1, 3, 4, 5, 6 and 7, including failed-load rollback, secret entry, return and
revisit. Every end-level result has rejected=3, applied=0, preserved=1 and
current packets received. Neither final native capture contains a failed script
result. The final diagnostic/barrier APK build and scoped formatter passed in
`temp/coop-endlevel-stamp-barrier-{build,format}.log`. D1 overlay-touch briefing
and cold restore passed at 20:19:48 in
`temp/coop-endlevel-stamp-briefing-d1-live.log`, with host/client native captures.
Both peers passed packet checks on initial visit 1 and restored visit 2;
end-level probes each reported rejected=3, applied=0, current=4, preserved=1.
Repeated Skip taps and a drag onto Launch now did not launch; a fresh Launch
now tap released the team through synchronization. Saved inventory, no briefing
replay and transfer guards passed. Neither native capture contains a failed
script result. The outstanding cases above remain open

Full GAME_INFO/SYNC preflight (2026-09-13, D1/D2 live validation passed): both Android
engines now reject wrong-sized layouts, unexpected senders/master slots, wrong
session/recipient tokens, changed reconnect generations and older co-op world
visits before the full-info parser changes Netgame, addresses, player identities,
host ownership or observer objects. Initial game-info discovery may establish a
session; subsequent metadata must match it. Layout checks mirror each engine's
writer, including optional RetroProtocol SYNC addresses and the existing trailing
visit. The wire format is unchanged. The paired gameplay fixture now clones actual
received GAME_INFO and SYNC packets and submits ten invalid variants directly to
the parser after each tested visit, requiring complete Netgame, tokens, master,
reconnect identities/generation and visit/high-water state to remain unchanged.
This is parser integration coverage, not delayed delivery over UDP. End-level
and object-sync metadata, participant incarnation and host-migration orderings
still need their own audit and live coverage

Validation: scoped formatting and final Android/Windows-both builds passed
(`temp/coop-info-preflight-final-{format,build,windows}.log`); four selected
recovery, gameplay-fence, world-visit and reconnect-auth tests passed in each
engine. D2 rollback/revisit passed at 19:26:42 in
`temp/coop-info-preflight-d2-live.log`, checking metadata rejection on visits
1, 3, 4, 5, 6 and 7. D1 host-deadline briefing/cold restore passed at 19:30:08 in
`temp/coop-info-preflight-d1-live.log`, checking visits 1 and 2, saved inventory,
no briefing replay and the active-transfer save guard. Each run retains native
host/client captures under the same prefix; none contains a failed script result

D2 NormalExitRace also passed at 19:32:05 in
`temp/coop-info-preflight-normal-d2-live.log` with native captures. Both peers
advanced from level 8/visit 1 to level 9/visit 2 through fresh SYNC. The escaping
client recorded paused=0, blocked=0, reactor=1, other_waiting=1 and equal/opposite
game/countdown deltas of 596247. Reliable sequence numbers continued above the
previous level's 149999 (host 150005, client 150044). This run verifies the normal
winner keeps the existing individual exit wait and live reactor behavior

Follow-up rejoin investigation: the first SpewRecovery invocation incorrectly
disabled both QoL and secret warps; both scripts stopped at recovery.active=false
before a rejoin. The runner now rejects that unsupported combination up front.
With secret warps enabled and QoL off, native/current-state evidence showed a
successful rejoin but no equipment reclamation: both peers were playing, the
host retained its four picked-up homing missiles, the client had zero, and six
drop objects remained. Logs: `temp/coop-info-preflight-rejoin-enabled-d2-live.log`
and its native captures; normalized state:
`temp/coop-info-preflight-rejoin-enabled-state.json`. The inventory sender still
returned early on the general QoL flag. It now checks coop_recovery_active(),
matching secret travel's independent accounting rule. The corrected
`-Game d2 -SpewRecovery -AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild`
run passed at 19:45:49 in `temp/coop-info-preflight-recovery-fixed-d2-live.log`,
with native host/client captures and no failed script result. Both client
process rejoins restored the remaining two homing missiles and primary weapon,
kept the host's collected four missiles, and left no matching world drops.
The final Android/Windows-both builds and scoped formatting passed in
`temp/coop-info-preflight-recovery-{build,windows,format}.log`; recovery,
gameplay-fence and world-visit tests passed 3/3 in each engine. The preceding
metadata and transition results remain valid; the only subsequent native
behavior change was the inventory sender's recovery-enablement check

PDATA integration (build passed; full live validation pending): all three Android position
formats now carry the same 12-byte visit/level/frozen trailer as MDATA. Receive
checks validate size, source and visit before loss/order history, reconnection
state, player objects, or ordinary host relay can change. Observer relay retains
the original trailer and skips world-ineligible PDATA. Current-visit frozen
packets can still confirm initial sync and refresh a connected peer's liveness;
old visits cannot. Android protocol versions advance to D1 30056 / D2 30057;
desktop wire sizes remain unchanged. The paired gameplay probe now sends stale
and frozen PDATA clones over UDP after release, requires both to be rejected,
requires fresh position traffic to reach the normal path, and rejects any test
clone reaching world application. It runs beside the existing MDATA retry probe

The first PDATA run (`temp/coop-pdata-stamp-d2-live.log`) passed both peers'
initial uncompressed PDATA/MDATA probes and full 476-byte retries. It stopped
before travel when the client ledger had only one row. Native client evidence
shows the second row was received, then discarded by autosave at 17:42:34 as
unbound gear (ID 28674, signature 158, object -1). The fixture had deferred
tagging its authored powerup until the next script, after the host preparation
barrier. A new paired tagging barrier now completes before the host publishes
the ledger; the later client check verifies receipt already bound the object.
Production stale-object validation remains intact. The corrected run passed at
17:58:22 (`temp/coop-rollback-binding-d2-live.log`, with host/client native
captures). Both peers bound the fixture before travel and completed rollback,
entry, return, revisit and return on visits 1, 3, 4, 5, 6 and 7. Every released
visit reported PDATA rejected mask 3, applied probes 0 and fresh traffic present;
MDATA probes also passed. This validates the uncompressed two-player path with
synthetically stale/frozen packets; short/quaternion, real delayed datagrams,
relay and observer coverage remain open. Android build and both Windows builds
passed, as did both engines' fence/visit unit checks

D1 follow-up `temp/coop-pdata-stamp-briefing-d1-live.log` passed the host-ready
deadline, initial paired gameplay probes and 476-byte retries, then failed at
18:03:57 waiting for the cold-resume host lobby. Host native capture shows
auto-host initialization at 18:03:23 but no `files/introspect.json` afterward.
MainActivity construction appears twice during startup (18:03:17.039 and
18:03:17.803), with a leaked RuntimeGameStateBridge service connection reported
at 18:03:20.377 during rotation. Later introspection broadcasts have no receiver
log. Investigate activity recreation/receiver and service ownership before
rerunning; this is not proof that the saved world failed to load. Cold restore
remains unverified for the PDATA build. The runner clears native logs on launcher
restart, so retain initial gameplay-probe evidence before that boundary in future
runs (the current runner already prints the full reliable payload evidence).
Resolved by the lifecycle correction documented in `coop_briefings_and_launch.md`:
the D1 host-deadline/cold-restore run passed at 18:16:37 in
`temp/coop-resume-lifecycle-d1-live.log`, including saved inventory, suppressed
briefing replay, transfer guards and both peers' MDATA/PDATA checks on visits
1 and 2. The runner now preserves those native probe lines before clearing logs
The corresponding D2 overlay-touch/cold-restore run passed at 18:19:28 in
`temp/coop-resume-lifecycle-d2-live.log`. Both runs used the lifecycle-fixed APK;
short/quaternion PDATA and broader participant/metadata fencing remain open

Live visit identity work (2026-09-13, validation complete for the cases below): a shared session-owned
64-bit counter now reserves world replacements independently of campaign/save
state. Transfer BEGIN carries the reservation, actual restore/rewind/restart and
rollback activate it before loading, and initial/natural level sync publishes the
host's current visit. Staging campaign/checkpoint buffers keeps the source visit.
Cancelled reservations remain consumed, and observed reservations remain above
the active visit for a possible future host. Save readers do not restore this
counter. That identity-only stage used Android D1 30052 and D2 30053; the MDATA
integration below advances them to 30054/30055. Desktop wire formats are unchanged

Introspection exposes active/reserved visits, and paired tests now require visits
1 -> 2 after initial/cold restore and 1 -> 2 -> 3 across failed destination and
rollback, followed by one increment per trip. Unit coverage includes same-session
reset, stale-sync rejection, unused reservations, migration high-water behavior,
64-bit boundaries and exhausted reservations. MDATA integration is described
below; PDATA stamps and atomic rejection of stale end-level/object/sync metadata
still need transport integration

Live identity validation: both Windows builds and the D1/D2 identity/fence unit
checks passed. D2 rollback/revisit passed at 16:44:21 with both peers on visits
1, 3 (rollback), 4, 5, 6 and 7; D1 briefing/cold restore passed at 16:51:44 with
both peers on visits 1 and 2. Logs: `temp/coop-world-visit-rollback-d2-live.log`
and `temp/coop-world-visit-briefing-d1-live.log`, with corresponding native captures

MDATA transport integration (paired D1/D2 validation passed for the cases below): Android D1/D2 now append the
12-byte visit/level/frozen stamp to raw MDATA bodies. Buffered batches flush at
scope changes; retry storage and host relay retain the original trailer. Raw
message capacity remains 454 bytes and the reliable wire maximum becomes 476.
Receiving dispatch validates and strips the trailer, then checks each message
against the current world independently, allowing session control through mixed
bodies while rejecting stale or frozen-origin gameplay. Visits of zero cannot
mutate the world. Android protocol versions become D1 30054 and D2 30055.
The new paired fixture queues old-visit and frozen-origin score mutations with
live typing control, requires real retry acknowledgments, and verifies that the
released world rejects both scores while applying the control. Android and both Windows builds plus the D1/D2 fence/visit unit tests passed.
PDATA, atomic metadata rejection, control-handler scope audit, and 3+ player/
observer adversarial coverage remain open

Control audit findings to resolve before calling the fence complete:

- `MULTI_COOP_PEER_STATUS` contains kill/score statistics, so it has now been
  removed from session control and is covered as a world mutation in both tests
- Recovery packets now require the same nonzero visit and level before dispatch.
  They may pass while that visit is frozen, preserving collection/rejoin settlement;
  their existing epoch/revision checks remain. `coop_recovery_receive` can adopt
  epochs, intersect gear and bind/remove objects, so these messages no longer
  qualify as session-wide control. The paired MDATA probe now includes a valid
  old-visit recovery life update and requires rejection with epochs/lives unchanged.
  Android and both Windows builds plus both engines' recovery/fence/visit unit
  tests passed. D2 rollback/entry/return/revisit/return passed at 18:33:08 in
  `temp/coop-recovery-visit-d2-live.log` (host/client native captures retained).
  Both peers rejected the recovery probe and preserved epochs/lives on visits
  1, 3, 4, 5, 6 and 7; existing MDATA/PDATA probes also passed. Live death-drop /
  two-process-rejoin scenarios passed for D1 at 18:35:40 and D2 at 18:38:29
  (`temp/coop-recovery-visit-rejoin-d1-live.log` and
  `temp/coop-recovery-visit-rejoin-d2-live.log`, with native captures). Gear was
  reclaimed exactly once and old world drops were absent after both rejoins.
  These are two-player tests; freeze replies involving a third live participant
  remain outside the live coverage (the recovery harness covers freeze replies)
- Rejoin inventory now retains the received MDATA stamp through dispatch and
  its pending buffer. Receipt rejects missing/obsolete visits; pre-SYNC packets
  may wait for ordinary join synchronization without adopting their world ID.
  Application rechecks the pinned visit/level before inventory, epoch or life
  changes; closed worlds defer application, and superseded/wrong ready worlds
  discard the buffer. Pending replacement compares visits before restore serials.
  The live stale MDATA probe includes a valid inventory restore and requires the
  client to reject it with recovery epochs/lives intact. Policy tests cover
  pre-SYNC, frozen arrival, same-level rollback, future and wrong-level data.
  Android/Windows builds and both engines' fence/visit/recovery tests passed;
  D2 death-drop recovery through two process restarts/rejoins passed at 18:55:46
  in `temp/coop-inventory-visit-rejoin-d2-live.log` (native captures retained).
  Recovered inventory remained correct without duplicate drops. D2 rollback /
  entry / return / revisit / return passed at 19:03:09 in
  `temp/coop-inventory-visit-d2-live.log` (native captures retained). On visits
  1, 3, 4, 5, 6 and 7 the client rejected the stale inventory restore and both
  peers preserved recovery epochs/lives; other gameplay probes also passed.
  The host rejects non-host inventory before the visit counter, so its expected
  inventory-rejected counter is zero. D1's two-process-rejoin scenario passed at
  19:05:59 in `temp/coop-inventory-visit-rejoin-d1-live.log` (native captures
  retained), preserving recovered equipment without duplicate drops. Packet
  layouts are unchanged. Forced pre-SYNC delivery and retaining a queued packet
  across an actual world replacement still need adversarial live coverage;
  policy tests cover those orderings
- Natural `net_udp_level_sync` now preserves the session reliable sequence
  while clearing batches and receive history. The normal-exit fixture seeds
  149999 and checks that mine advancement does not reset packet identities
  (D2 normal-exit race passed at 18:00:35 in
  `temp/coop-pdata-sequence-normal-d2-live.log`: host 149999 -> 150005,
  client 149999 -> 150054, level 8 -> 9, visit 1 -> 2). The client clock
  remained unpaused with the host waiting, and reactor time decreased by the
  same amount game time advanced. Host-migration slot changes and ACK/duplicate tracking
  across participant replacement still need their own audit
- Transfer BEGIN now rejects old visits, but CHUNK/APPLY/READY still use the
  compact transfer identifier. Audit identifier reuse and authenticated sender
  checks across delayed bodies; passing session control must not imply trust

The first MDATA live run (`temp/coop-mdata-stamp-rollback-d2-live.log`) failed
at the new probe: the host reported ACK mask 3, two rejected scores, applied
control and preserved score; the client verified earlier with ACK mask 3 but
zero received probe rejections/control. The fixture used a fixed two-second
wait despite staggered script start. The revised fixture arms both peers first
and waits up to 15 seconds for explicit receipt/ACK introspection before final
verification. Keep this initial failure as evidence until the rerun resolves it

The receipt-based rerun passed the initial MDATA probes and 476-byte retries,
then exposed a real restore dependency in `temp/coop-mdata-stamp-barrier-d2-live.log`:
rollback restored both worlds/players, but the host could not build/select rewind
history. `multi_prep_level` initializes `PKilledFlags` to one; the historical
REAPPEAR packet clears that flag. Its frozen-origin packet is now intentionally
rejected. Completed successful transfers therefore publish player life bookkeeping
from restored player objects/shields, without respawn effects or inventory changes.
The briefing launch barrier uses the same helper after a fresh mine becomes ready

The corrected D2 run has passed rollback and its subsequent released-world probes.
Native evidence in `temp/coop-mdata-loaded-life-rollback-proof.log` shows the
remote flag changing from 1 to 0 at visits 2 and 3, both players alive, one current
rewind snapshot, and matching rewind/restart campaign archives. The complete
D2 sequence passed at 17:19:53 in `temp/coop-mdata-loaded-life-d2-live.log`. Both
peers reported ACK mask 3, two rejected scores, applied control, preserved score
and an open world at visits 1, 3, 4, 5, 6 and 7. Host archive checks passed after
rollback and every subsequent leg. D1 briefing/force launch, cold restore and
post-release probes at visits 1 and 2 passed at 17:23:15 in
`temp/coop-mdata-loaded-life-d1-live.log`. Corresponding native captures contain
no failed scripts; the runners preserve full 476-byte retry evidence before
launcher restarts clear logcat. The latest Android build completed successfully
in `temp/coop-mdata-loaded-life-build.log`; Windows and D1/D2 unit checks passed
with `temp/coop-mdata-stamp-barrier-windows.log` before the Android-only life fix

Further validation on the same APK: D2 `-SecretDying` passed at 17:26:44,
with the host dying during entry and the client during return; portable state,
death settlement and arrival checks passed. D2 `-NormalExitRace` passed at
17:29:15: the first exiter waited, the other player's game/reactor clocks kept
advancing, the secret exit stayed blocked, and both eventually advanced to mine 9.
Logs: `temp/coop-mdata-loaded-life-dying-d2-live.log` and
`temp/coop-mdata-loaded-life-normal-d2-live.log`, with matching native captures.
These results do not close the transport/control and broader campaign/save/
recovery gaps listed elsewhere in this plan

Packet-fence follow-up (2026-09-13): the shared stamp policy and its D1/D2 tests
now cover signed levels, 64-bit visits, frozen-origin mutations, same-level
revisits/rollback, malformed stamp rejection and mixed gameplay/control bodies.
Both Windows builds and `test_coop_gameplay_fence` passed at that foundation
stage. The MDATA integration above now consumes the policy; PDATA, end-level
traffic and object/join synchronization still need integration and adversarial
live coverage before the complete stale-world issue is closed

The audit also found a direct reliable-send rollover to zero above packet
150000. Zero matches empty receiver history, so the receiver can acknowledge
without processing that payload. Both Android engine paths now keep advancing
the 32-bit sequence and skip zero at integer wrap; desktop behavior is unchanged.
The live transfer fixture now seeds 149999 and requires an acknowledgment above
150000, crossing both the original 16-bit ACK boundary and this later rollover.
The D1 `-Briefings -BriefingRestore -NoCoopQol -TimeoutSeconds 180 -SkipBuild`
run passed at 15:49:54 in `temp/coop-packet-boundary-briefing-d1-live.log`.
The host recorded acknowledgment 150787; both native captures are free of
failed script results. This run used the rollover fix before the capacity
changes described below

Before stamps were added, Android MDATA wire buffers were corrected to use an
explicit 464-byte maximum in initial,
buffered and normal/observer retry sends. The packed bookkeeping struct is only
462 bytes. Receive checks distinguish the 6-byte normal and 10-byte reliable
headers, and outgoing buffer/queue entry points reject oversized payloads.
The paired live fixture builds a valid 454-byte body, drops its initial direct
send and requires the reliable retry to be acknowledged. It runs before
briefing restore and source rollback scenarios

D1 passed the full capacity-enabled briefing/save/cold-resume scenario at
16:07:10. `temp/coop-mdata-capacity-briefing-d1-retry.log` exited zero and
retains native acknowledgment evidence for 464-byte packets on both peers
after their initial sends were dropped. After process restart, the host recorded
acknowledgment 150778; inventories and restoration without briefing replay passed.
Neither final native capture contains a failed script result. Android build
`temp/coop-mdata-capacity-fixed-build.log` passed in 1m57s, both Windows builds
passed in `temp/coop-mdata-capacity-windows.log`, and scoped formatting passed.
D2 passed both maximum-payload probes at 16:08:37, but source rollback failed
its player-state assertion at 16:09:27. `temp/coop-mdata-capacity-rollback-d2-live.log`
exited nonzero; the client capture proves the ordering behind the stale score:
score 1234 restored at 16:09:26.849, score-zero packet applied while frozen at
16:09:26.943, release at 16:09:27.031, then verification found zero. A later
correct score arrived at 16:09:27.511. The host's boundary check passed with
acknowledgment 151611, and full checkpoint rollback completed on both peers;
the revisit portion did not run

An incremental score guard now suppresses score sends and rejects score packets
while travel owns frozen world state. Source death/drop settlement remains open
through the FREEZING drain barrier, and normal exit waiting remains open. Both
rollback scripts inject a zero-score message through the normal multiplayer
dispatch during the captured-source warning and require the nonzero score to
survive. The guard passed the full rollback/revisit scenario at 16:19:58:
`temp/coop-frozen-score-rollback-d2-live.log` exited zero, with no native failed
script result in either final capture. Both injected zeros were rejected;
the host preserved remote score 1235 and the client preserved remote score 1234.
Rollback restored both scores and the full source checkpoint despite the dropped
chunk, followed by 8 -> -2 -> 8 -> -2 -> 8 with retained worlds and player state.
The 464-byte retry probes passed again and the host acknowledged packet 151611.
Native score evidence is retained in `temp/coop-frozen-score-rollback-proof.log`

Both Windows builds and the Android build passed after the guard change
(`temp/coop-frozen-score-windows.log`, `temp/coop-frozen-score-build.log`);
Android took 2m7s. Scoped formatting passed. The dying-player settlement
regression passed at 16:23:07 with `-Game d2 -InitialLevel 8 -SecretDying
-AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild` in
`temp/coop-frozen-score-dying-d2-live.log` (terminal zero). The host died during
entry and the client during return; each death settled before capture, and
portable state, scores and arrival checks passed. Neither final native capture
contains a failed script result. This guard does not replace visit stamps:
old gameplay delayed until after release still needs the full transport fence

Keep the future wire visit independent of serialized campaign generation:
rollback deliberately restores the checkpoint's historical campaign generation.
Each world replacement, including failed-load rollback, same-mine load, rewind
and restart, needs a fresh host-owned wire visit that saved metadata cannot roll
back. Transfer BEGIN currently carries only a 32-bit recovery epoch at offset 32;
distribute/adopt the new visit under the corresponding transfer/join authority,
without assuming recovery epochs alone provide a session-wide initial visit

The first capacity-enabled D1 run passed its paired maximum-payload check at
15:59:39, then failed reopening the client's launcher during cold resume.
`temp/coop-mdata-capacity-briefing-d1-live.log` exited nonzero at 16:01:42.
The client capture contains `system_server_pre_watchdog`, and no successful
SetupActivity launch. After preserving both captures, emulator-5556 was shut
down and cold-booted without snapshots. The runner now records each peer's
native full-payload acknowledgment before launcher restart clears logcat;
the retry uses the same APK

Earlier change (2026-09-13): both engines defer join/sync requests during an
active co-op transition before rejecting requests against a destroyed source.
The forced early-sync countdown regression passed at 15:17:27, including the
deliberate five-second host-load delay, two observed deferred base-sync requests,
one death per player and exact retired-gear conservation. Details and logs below

Follow-up live regressions passed with the same APK and no native failed script
results: D2 `-NormalExitRace -InitialLevel 8 -AllowSecretWarps -NoCoopQol` at
15:21:43 (`temp/coop-early-sync-normal-live.log`) and D1 `-Briefings
-BriefingRestore -NoCoopQol` at 15:24:38
(`temp/coop-early-sync-briefing-d1-live.log`). Both used
`-TimeoutSeconds 180 -SkipBuild`. The normal-exit survivor's game/countdown
advanced by 597754 fixed-point ticks while its teammate waited, and both then
reached level 9. D1 passed host Skip/Launch, save, both app-process restarts,
restoration without briefing replay, the transfer autosave guard and full-width
ACK checks. Native captures are `temp/coop-early-sync-normal-{host,client}-logcat.log`
and `temp/coop-early-sync-briefing-d1-{host,client}-logcat.log`. This closes the
confirmed destroyed-source join rejection; early-failure score contamination
and the broader remaining requirements are still open

Destroyed-source recovery retirement is implemented. During frozen destination
apply, each peer converts gear belonging to an unavailable source into recovery
credit before acknowledging the load. IDs, owner, life and inventory survive;
object bindings are cleared and revisions advance once. An intact source keeps
its dormant gear. The source checkpoint predates this mutation, allowing rollback
to restore the original ledger. The retirement validates the whole batch before
mutation and never removes objects from the destination mine

The two-emulator `test_lan.ps1 -Game d2 -InitialLevel 8 -DestroyedGearRestore
-AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild` passed at 14:29:52
on 2026-09-13, terminal exit zero in `temp/coop-retired-gear-live-d2.log`.
The client died/dropped gear in secret -2, the host returned during the reactor
countdown, and both peers verified exactly six homing missiles in the dead
player's recovery credit, no world-bound records for the destroyed secret,
and a blocked secret entrance. A real save, inventory mutation and coordinated
restore in base 8 preserved the complete recovery records byte-for-byte,
campaign archive and both saved ship inventories. Neither native capture
`temp/coop-retired-gear-*-logcat.log` contains a failed script result

Both games' `test_coop_recovery` tests passed with source retirement in either
direction, destination object-index/signature reuse, idempotence, save/restore,
capacity-limited reclaim without duplication, and atomic revision-overflow
rejection. Android and both Windows builds passed in
`temp/coop-retired-gear-build.log` and `temp/coop-retired-gear-windows.log`;
scoped formatting passed in `temp/coop-retired-gear-format.log`. Live rejoin
after retirement, a destroyed-source rollback, and cold-process recovery still
need dedicated coverage; this does not complete the broader feature

Rollback follow-up found a stale fixture and a separate open failure case.
`temp/coop-retired-rollback-diagnostic-live.log` failed before its intended
post-load injection. The client diagnostic reported source gear ID 28674 as
LIVE but unbound (`object=-1`, `remote=157`, `bound=-1`), so the source-suspend
guard correctly refused the load. The fixture had tagged an authored powerup
only on the host. Its client setup now verifies the exact authored object
identity, mirrors the synthetic recovery flag and requires a successful local
binding before requesting travel. Production binding checks remain unchanged

In that pre-load rejection run, both source restores completed, but the client's
view of the host score was zero instead of the captured 1234. Keep this as an
open early-failure rollback issue; correcting the fixture does not prove it
fixed. Captures: `temp/coop-retired-diagnostic-{host,client}-logcat.log`.
Additional diagnostics now identify rejected portable revisions/players,
unbound source gear, restored scores and incoming score packets. A dedicated
pre-load failure regression and the remaining gameplay-visit fence are still
required

The corrected post-load rollback plus intact revisit scenario passed at 14:47:53
on 2026-09-13: `test_lan.ps1 -Game d2 -InitialLevel 8 -SecretRollback
-SecretRevisit -AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild`.
Terminal exit zero in `temp/coop-retired-rollback-fixture-live.log`; neither
`temp/coop-retired-fixture-{host,client}-logcat.log` contains a failed script
result. The client loaded the destination before the injected failure, both
peers restored the full source checkpoint despite a dropped rollback chunk,
then completed 8 -> -2 -> 8 -> -2 -> 8 with the original gear ledger and
retained world state. Android and both Windows builds passed after the fixture
correction and diagnostic additions (`temp/coop-retired-rollback-fixture-build.log`,
`temp/coop-retired-rollback-fixture-windows.log`)

The successful rollback trace also records zero-score packets arriving while
frozen, before portable scores were reinstated. For example the client received
host score zero at 14:44:46.500, then restored 1234 at 14:44:46.553. This proves
temporary load-time scores are transmitted and applied; delayed arrival after
portable restoration is a candidate explanation for the early-failure mismatch,
not yet a verified fix. Do not treat the successful ordering as a packet fence

The first new countdown-conservation run failed because the existing reactor
fixture parked the host near a return exit before starting its timer. The log
records a physical exit grant at 14:50:42.750, before `reactor_short` at
14:50:45.698, so nobody died before the unintended warp. Captures:
`temp/coop-retired-countdown-{host,client}-logcat.log`; runner:
`temp/coop-retired-countdown-live.log`. Countdown preparation now uses ordinary
world preparation without exit positioning, and asserts no active transition
before starting the timer

The corrected countdown run reached a real automatic team-death departure but
timed out before arrivals at 14:59:30 on 2026-09-13. Command:
`test_lan.ps1 -Game d2 -InitialLevel 8 -SecretCountdown -AllowSecretWarps
-NoCoopQol -TimeoutSeconds 180 -SkipBuild`; terminal exit one in
`temp/coop-retired-countdown-fixture-live.log`. Captures:
`temp/coop-retired-countdown-fixture-{host,client}-logcat.log`. Both peers
confirmed no active warp before the timer. Host reported reactor team death
departure at 14:56:12.180, then LOADING at 14:56:34.938. World transfer kind 3,
ID 6 sent its payload at 14:56:39.083; the client entered base-8 level sync at
14:56:41.306. Host never logged apply-ready for that transfer or began its own
world load before the automation's 180-second arrival timeout. Neither peer
reached retirement/conservation assertions. Do not count this scenario as
verified by the earlier accidental normal warp

The stall's confirmed cause was the ordering in `net_udp_welcome_player`:
the destroyed-source rejection preceded the active-transition guard. The client
could request base-8 sync before the host received READY_APPLY, so at
14:56:41.741 the host logged `REJECTED endlevel`, sent DUMP_ENDLEVEL and
disconnected the still-required peer. The last-player message was HUD text,
not proof of a dialog. The suspected WAITING-ready rejection was not the
confirmed cause. Host introspection JSON remained from the previous settled
entry, so it did not represent the live transfer state

Both engines now defer requests during co-op travel/briefings before checking
source destruction or level mismatch. The strengthened countdown regression
holds the host's world apply for five seconds while servicing networking and
requires an actual deferred negative-source-to-positive-destination request.
It passed at 15:17:27 on 2026-09-13, terminal exit zero in
`temp/coop-countdown-early-sync-live.log`. Host logged the deliberate hold at
15:17:02.823 and deferred player-1 requests from source -2 to destination 8 at
15:17:05.591 and 15:17:07.669. Both peers then completed one death each,
returned to base 8, retained exactly six homing missiles of recovery credit
per owner, and rejected secret reentry. Neither
`temp/coop-countdown-early-sync-{host,client}-logcat.log` contains a failed
script result. Android and both Windows builds passed in
`temp/coop-countdown-early-sync-build.log` (1m 38s) and
`temp/coop-countdown-early-sync-windows.log`; scoped formatting passed in
`temp/coop-countdown-early-sync-format.log`

The diagnostic build before this ordering fix also passed the same natural
countdown at 15:09:24 (`temp/coop-countdown-barrier-diagnostic-live.log`),
showing the original stall was timing-dependent. That run did not impose the
five-second hold or require the early-request path. Broader host-loss and
transfer-timeout recovery still require their own scenarios; this fix does
not prove all failure paths are bounded

Reactor-death return passed on two emulators at 13:54:10 on 2026-09-13:
`test_lan.ps1 -Game d2 -InitialLevel 8 -SecretReactorDeath -AllowSecretWarps
-NoCoopQol -TimeoutSeconds 180 -SkipBuild`, terminal exit zero in
`temp/coop-reactor-death-live-d2.log`. The client completed its death/drop and
waited in secret -2; the living host's game clock and reactor countdown both
advanced for ten seconds before using the return teleporter. Both returned
with the expected player state and a destroyed secret archive entry. The test
checked the entrance's collision flag and rejected a new physical activation
without starting another transition. No failed native script result appears in
`temp/coop-reactor-death-*-logcat.log`. Android build passed in
`temp/coop-reactor-death-build.log` (1m 34s); both Windows builds passed in
`temp/coop-reactor-death-windows.log`

Whole-team secret countdown passed at 13:59:50 on 2026-09-13 using
`test_lan.ps1 -Game d2 -InitialLevel 8 -SecretCountdown -AllowSecretWarps
-NoCoopQol -TimeoutSeconds 180 -SkipBuild`, terminal exit zero in
`temp/coop-reactor-countdown-live-d2.log`. Neither player touched an exit or
respawn control. The host recorded one team-death return; both peers settled
exactly one death, returned to base 8 with fresh equipment, and rejected the
destroyed secret entrance. Neither native capture under
`temp/coop-reactor-countdown-*-logcat.log` contains a failed script result

Normal-mine teammate death passed at 14:02:44 on 2026-09-13 with
`test_lan.ps1 -Game d2 -InitialLevel 8 -NormalReactorDeath -AllowSecretWarps
-NoCoopQol -TimeoutSeconds 180 -SkipBuild`, terminal exit zero in
`temp/coop-reactor-normal-death.log`. The dead client stayed in level 8 while
the host's clock/countdown continued; the host's normal exit released the
client to ordinary end-level waiting and both advanced to level 9. Only the
client had a recorded death. Captured native logs contain no failed script
result

Normal-mine whole-team countdown passed at 14:05:06 on 2026-09-13 using
`test_lan.ps1 -Game d2 -InitialLevel 8 -NormalCountdown -AllowSecretWarps
-NoCoopQol -TimeoutSeconds 180 -SkipBuild`, terminal exit zero in
`temp/coop-reactor-normal-countdown.log`. The host selected normal progression
after both countdown deaths, and both players reached level 9 with one recorded
death each. Native captures `temp/coop-reactor-normal-countdown-*-logcat.log`
contain no failed script result

The existing normal/secret exit-race regression passed on the same APK at
14:07:30 on 2026-09-13 with `-NormalExitRace -Game d2 -InitialLevel 8
-AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild`. Terminal exit
zero in `temp/coop-reactor-normal-race.log`; the captured client clock showed
equal nine-second game-time advance and countdown decrease while the host
waited. The delayed secret request was rejected and both reached normal level
9. Neither `temp/coop-reactor-normal-race-*-logcat.log` capture contains a
failed script result

D1 briefing/cold-resume regression passed at 14:11:03 on 2026-09-13 with
`test_lan.ps1 -Game d1 -Briefings -BriefingRestore -NoCoopQol
-TimeoutSeconds 180 -SkipBuild`, terminal exit zero in
`temp/coop-reactor-briefing-d1.log`. Host Skip/Launch now, saving, restarting
both app processes, restoration without briefing replay, the active-transfer
autosave guard and full-width acknowledgment checks passed. Neither native
capture `temp/coop-reactor-briefing-d1-*-logcat.log` contains a failed script
result. All four sequential regression cases (normal teammate death, normal
whole-team countdown, normal/secret race, D1 briefing/cold resume) finished
with exit zero, in addition to the two separate secret-reactor scenarios above

With shared secret travel
enabled, reactor deaths remain network participants until the host selects the
departure. A dead player's completion tap waits in the current mine while living
teammates continue playing. A normal-exit decision releases those dead players
to the ordinary end-level wait path. If the entire team dies, the host selects
one secret return/advance or ordinary normal-level progression as appropriate.
Countdown expiry enters the death/drop accounting once instead of invoking a
local secret load. Destroyed secret entry now rejects activation and blocks the
entrance before another freeze/checkpoint attempt. New `-SecretReactorDeath`
and `-SecretCountdown` LAN cases exercise these paths. `-NormalReactorDeath`
and `-NormalCountdown` cover corresponding ordinary mine behavior. Endgame,
destroyed-base secret-entry death coverage, and destroyed-world dropped
gear recovery/hostage bonus accounting remain open

Death settlement before secret travel passed on two emulators at 13:31:52 on
2026-09-13. Command: `test_lan.ps1 -Game d2 -InitialLevel 8 -SecretDying
-AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild`; terminal exit
zero in `temp/coop-dying-live-d2.log`. Neither native capture under
`temp/coop-dying-*-logcat.log` contains a failed script result. Both deaths
settled before the explosion had dropped gear (`eggs=0`), and the round trip
completed with the expected portable state on both peers

The FREEZING phase
now completes an already accepted local death/drop without advancing the mine,
waits for reliable death, gear and reappearance traffic to drain on every peer,
then permits immutable portable-player snapshots. Newly arriving damage is
ignored while this phase owns the gameplay freeze. Normal-exit waiting remains
outside that freeze, with the reactor and surviving players continuing to run.
The `-SecretDying` LAN scenario covers a dying host on entry and a dying client
on return, with no respawn tap, and checks one death, one new recovery life,
fresh equipment only for the dying player, and unchanged survivor state.
Android build passed in `temp/coop-dying-build.log` (3m 13s), both Windows
builds passed in `temp/coop-dying-windows.log`, and campaign/transition-policy
CTest cases passed for both games. Reactor-destroyed deaths, countdown expiry
and full cross-visit packet fencing remain separate open work. This test does
not yet establish item-by-item conservation of the dropped gear through a
later revisit, save/load or rewind

Normal-exit regression with the same APK passed at 13:34:13 on 2026-09-13:
`test_lan.ps1 -Game d2 -InitialLevel 8 -NormalExitRace
-VerifyAutomationFailure -AllowSecretWarps -NoCoopQol -TimeoutSeconds 180
-SkipBuild`, terminal exit zero in `temp/coop-dying-normal-race-d2.log`.
The host waited on the post-level screen while the client's game clock advanced
and reactor countdown decreased by the same nine-second interval. The delayed
secret request and opposing trigger were blocked; both players then advanced
normally to level 9. Native captures `temp/coop-dying-normal-*-logcat.log`
contain exactly the one intentional final-step-failure probe per peer, followed
by passing race scripts; no unrelated script failure was observed

D1 shared-protocol regression passed at 13:37:46 on 2026-09-13 using
`test_lan.ps1 -Game d1 -Briefings -BriefingRestore -NoCoopQol
-TimeoutSeconds 180 -SkipBuild`, terminal exit zero in
`temp/coop-dying-briefing-restore-d1.log`. Host force-launch, saving, process
restart, restoration of both inventories without briefing replay, the active
transfer autosave guard and full-width acknowledgment checks passed. Neither
native capture `temp/coop-dying-d1-*-logcat.log` contains a failed script result

Intact-secret death handling passed on two emulators at 13:06:19 on
2026-09-13 using `test_lan.ps1 -Game d2 -InitialLevel 8 -SecretDeath
-AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild`, terminal exit
zero in `temp/coop-death-location-live-d2.log`. Neither native capture under
`temp/coop-death-location-*-logcat.log` contains a failed script result

Android D2 now excludes co-op
from the single-player negative-level death/return branch, so an ordinary
death uses the existing multiplayer drop and same-mine respawn path. The new
`-SecretDeath` LAN scenario enters the authored secret, kills and respawns
the client and then the host, verifies that the other player keeps playing
with its inventory and campaign visit intact, and returns the team to the
base afterward. Each death must count once without a single-player life
penalty or extra campaign transition. Both deaths and survivor checks passed,
followed by the normal team return to base 8. This exercises departure with
death-drop records present, but does not yet prove item-by-item conservation
through a later secret revisit, save/load or rewind. Android builds passed in
`temp/coop-death-location-build.log`; both Windows builds passed in
`temp/coop-death-owner-windows.log`

D1 live coverage passed at 13:08:49 on 2026-09-13 using `test_lan.ps1
-Game d1 -CoopDeath -TimeoutSeconds 180 -SkipBuild`, terminal exit zero in
`temp/coop-death-location-live-d1.log`. Both client and host respawned in
normal level 1 with one recorded death, unchanged lives, the existing local
score penalty and fresh equipment. Both survivor checks passed. Neither
native capture under `temp/coop-death-location-d1-*-logcat.log` contains a
failed script result

The first live run respawned the client in -2 with generation 2, its lives
unchanged and one recorded death, but exposed a separate shared scoring bug:
robo_anarchy_suicide_penalty charged each receiving peer's local player when
another player committed suicide. Both Android engines now apply that call
only on the dead player's peer, preserving the existing 1,000-point local
penalty. The reusable death fixtures now cover D1/D2 through `-CoopDeath`
as well as D2's `-SecretDeath` round trip, with distinct seeded scores and
an unchanged-score assertion for the survivor. These assertions passed in
the D2 secret scenario and the matching D1 scenario above

The 12:59:19 retry verified the client respawn and both survivor checks,
including correct score ownership. The host also respawned in the same visit
with one death and unchanged lives, but immediately collected its spew at the
spawn point before the fresh-ship inventory assertion. Native logs show the
new-ship initialization followed by the normal plasma/homing/laser pickups.
The fixture now moves the dying ship away from all authored spawns and other
players before damage, retaining the strict fresh-ship assertion and leaving
the dropped gear in the mine for the subsequent return-trip check

Players already dying during intact-mine secret travel are covered by the
separate `-SecretDying` scenario above. Reactor-death waiting and whole-team
secret countdown expiry are covered by the newer scenarios at the top of this
document; destroyed-base secret-entry death coverage remains outstanding

Destroyed-base advancement passed on both emulators at 12:34:27 on 2026-09-13
using `test_lan.ps1 -Game d2 -InitialLevel 8 -SecretAdvance -AllowSecretWarps
-NoCoopQol -TimeoutSeconds 180 -SkipBuild`, terminal exit zero in
`temp/coop-advance-activation-live-d2.log`. Both native log captures under
`temp/coop-advance-activation-*-logcat.log` contain only passing script results.
The test covers secret-first arbitration against a delayed normal exit,
entry into -2 after base 8 destruction, departure to fresh level 9, optional
briefing with explicit host Skip/Launch now, and both players' campaign,
inventory, cargo, normal resource floors and recovery identities after release

The prepared
campaign can authorize ADVANCE as a secret departure, with its destination
checked against entered-from + 1 and the mission's last normal level. The
fresh-world transfer accepts a positive destination only when that exact
validated operation authorizes it. Android protocol versions become D1 30046
and D2 30047 for the extended transfer semantics

Commit applies the engine's shared normal-level resource rules to all live
players, clears carried keys and timed effects, and credits onboard hostages
once before clearing cargo. Inventory, score, lives and recovery identities
otherwise carry forward. The new mine's optional briefing runs after campaign
commit and team travel release acknowledgment, while the local travel pause
still prevents early gameplay. `-SecretAdvance` extends the destroyed-base
secret-first race through -2 to normal level 9, with the briefing option enabled
and explicit host skip/launch. Both Windows builds and the Android x86_64
build passed. During fixture verification, the 12:03:16 run
reached level 9 on both peers and completed the coordinated briefing, but
the client checked for cleared invulnerability during the normal half-second
spawn protection. The fixture now selects the existing No Invuln spawn mode
before departure and verifies that mode was retained, so carried powerup
removal is tested independently of a fresh spawn grant. It also waits for
the briefing page's screen_advance_can_activate gate before requesting Skip:
screen_advance_ready alone can still be true during tap suppression at page
entry, which rejected the request in the 12:31:01 run

Full campaign restore now reapplies the archive's entered-from level to the
engine's legacy Entered_from_level variable. The legacy save body does not
serialize this variable, so a cold secret load previously left the engine's
return context at its startup value. The cold-resume fixture now requires
the engine and campaign return destinations to agree. Validation passed at
12:39:13 on 2026-09-13 using `test_lan.ps1 -Game d2 -InitialLevel 8
-SecretColdResume -AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild`,
terminal exit zero in `temp/coop-return-context-cold-d2.log`. Both native
captures report `level=8 engine_return=8 robots=2` after process restart,
then pass the real return to base 8. Neither capture has a failed script
result. Android x86_64 and both Windows builds passed in
`temp/coop-return-context-build.log` and `temp/coop-return-context-windows.log`

The shared-code D1 regression also passed at 12:42:55 with `-Game d1
-Briefings -BriefingRestore -NoCoopQol -TimeoutSeconds 180 -SkipBuild` in
`temp/coop-return-context-briefing-d1.log`. Both peers resumed the saved
inventory without replaying briefings; the autosave/transfer and full-width
acknowledgment guards passed

This is not the completion of reactor handling: bonus scoring and hostage-origin
accounting across multiple source mines, secret-reactor death/countdown departure,
and final-level endgame still require implementation and coverage

Cold secret resume and return passed on two emulators at 10:56:36 on
2026-09-13: `test_lan.ps1 -Game d2 -InitialLevel 8 -SecretColdResume
-AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild`, terminal exit
zero in `temp/coop-secret-cold-expect-retry-live-d2.log`. The run entered -2,
saved, changed both inventories, restored in-process, force-stopped both app
processes, and resumed the same secret save through a normal level-1 startup.
It compared the campaign and dormant-world sizes/checksums, restored distinct
scores and homing ammunition, retained both session options, opened no briefing
presentations, and returned to base 8 with its measured pre-travel robot count.
This is basic inventory and campaign coverage, not a complete owner-snapshot
save handshake or a physical launcher save-selection touch test

Two fixes were necessary for that cold round trip. Both engines now refresh
the local ship object index after loading the destination during co-op restore;
the client previously reused level 1's ship index 16 in secret -2, whose local
ship is index 6. Incoming position packets retain liveness updates but cannot
move ships while secret travel owns the gameplay freeze. Without that guard,
an old secret position replaced the assigned base arrival during loading.
NORMAL_WAIT is explicitly excluded from this freeze. The 10:58:42 run with
`-Game d2 -InitialLevel 8 -NormalExitRace -AllowSecretWarps -NoCoopQol
-TimeoutSeconds 180 -SkipBuild` verified the real host waiting screen, live
client reactor countdown and rejection of the delayed secret request. Its
terminal-zero result in `temp/coop-secret-cold-normal-regression-d2.log` was
subsequently invalidated: the client's final next-level assertion failed, but
automation overwrote that last-step failure with PASS. Complete next-level
settlement was subsequently verified by the corrected regression below

Full visit/restore packet fencing remains required: the frozen-position guard
does not reject old packets arriving after gameplay release. Reactor countdown
deaths and endgame, complete private player save state, cold recovery and the
other unverified cases below remain part of the implementation scope

Secret-first physical race passed at 11:15:18 on 2026-09-13:
`test_lan.ps1 -Game d2 -InitialLevel 8 -SecretExitRace -AllowSecretWarps
-NoCoopQol -TimeoutSeconds 180 -SkipBuild`, terminal exit zero in
`temp/coop-secret-winner-position-live-d2.log`. With the base reactor destroyed,
the client invoked the authored normal exit and buffered its original request
for eight seconds. The host then invoked the secret trigger. The delayed normal
request reached the host with its old operation generation and was rejected
while capture was active. The normal collision side stayed impassable; no normal
exit was granted or taken, neither player entered the post-level screen, and
both reached -2 with entered-from 8 and base-returnable false. The fixture also
verified retained source checkpoints, portable player state, non-overlapping
arrivals and zero residual ship motion. This run covers entry from a destroyed
base; departure to the next normal mine is covered by the later advancement
run documented at the top of this plan

The first run, `temp/coop-secret-winner-live-d2.log`, failed arrival validation:
an incoming MULTI_POSITION changed the host's client ship from secret segment 17
to segment 12 with a nonzero surface mask during the frozen load. The PDATA guard
did not cover this separate handler. Both engines' Android `multi_do_position`
now ignore position mutations while `coop_travel_blocks_gameplay()` is true.
The retry log recorded the rejected MULTI_POSITION during destination loading,
then both ships passed geometry/motion checks. Normal waiting is excluded from
this guard, as it is from the PDATA guard. Full epoch fencing after release is
still required. Android and both Windows builds passed; logs:
`temp/coop-secret-winner-position-android.log` and
`temp/coop-secret-winner-position-windows.log`

The final normal-first regression in
`temp/coop-secret-winner-position-normal-d2.log` exposed the same automation
false-positive: `advance_step()` could run after `stop_script_fail()` and
replace the last step's durable FAIL with PASS. Advancement now stops when
the script is inactive or failed. `-VerifyAutomationFailure` deliberately
fails the final normal-destination assertion before any exit and requires
the durable FAIL on both peers before running the normal scenario. This
regression passed on both peers in `temp/coop-normal-ready-live-d2.log`:
intentional final-step failures remained durable FAIL. The subsequent normal
scenario exposed its wrong numeric network-status expectation. Both engines
define NETSTAT_WAITING as 3 and NETSTAT_PLAYING as 1; the old fixture therefore
ran its final assertion during initial loading and missed the settled state.
All four normal-exit scripts now require status 1, both player slots playing,
campaign level 9 and an inactive travel gate before the final assertion.
The corrected full run passed at 11:39:24 on 2026-09-13:
`test_lan.ps1 -Game d2 -InitialLevel 8 -NormalExitRace -VerifyAutomationFailure
-AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild`, terminal exit
zero in `temp/coop-normal-status-live-d2.log`. Both peers retained the expected
intentional final-step FAIL, then passed the normal-exit scenario. The client
kept playing under its reactor countdown while the host waited; its delayed
secret request lost, and both ultimately reported level/campaign 9, local
CONNECT_PLAYING, no pause and no active travel. Captured logs contain only the
intentional one-step failure, followed by successful preparation and exit
scripts. Captures: `temp/coop-normal-status-emulator-{5554,5556}-logcat.log`

Introspection from the prior run showed
both peers playing level 9 with matching generation 2 campaigns and no travel
gate; its terminal failure correctly reports expected 3 but got 1
Log audits found no SCRIPT_RESULT: FAIL in the captured passing cold secret
resume, D1 briefing/cold-restore or corrected secret-first race runs above

Physical exit integration is now wired, with the trigger-callback round trip
validated below. Android D2
intercepts exit triggers before one-shot flags, sounds or local end-level work.
An enabled co-op session establishes the host gate lazily through authenticated
HELLO/STATE messages, then submits the trigger ID with the source visit context.
The host derives the operation from the current mine's trigger and requires the
sender's player object in that trigger's segment or its adjacent segment. Legacy
remote trigger broadcasts cannot invoke local exit effects. Wire byte 47 selects
physical mode; non-PORTABLE packets use bytes 56..59 for trigger ID plus one.
Arrival layout agreement subsequently raises Android protocol versions to
D1 30044 and D2 30045. Non-PORTABLE bytes 60..63 carry the arrival checksum;
the host accepts LOADING acknowledgments only after its own placement succeeds
and the peer's layout matches. A mismatch requests coordinated rollback

A granted normal request re-enters the existing trigger handler on an engine
frame, so its player uses the ordinary end-level waiting screen. A normal winner
blocks secret-trigger sides in the collision query, including the reverse side;
other players retain ordinary mine simulation and normal exits. A losing secret
request is cleared so it cannot keep its sender's normal exit locally blocked.
Secret winners use the existing team checkpoint/warning/load/commit path.
One-shot secret flags are consumed after the rollback checkpoint, with pre-load
cancellation restoring the original flag. Natural normal progression clears the
old travel gate before arming the next mine's briefing

The new `-SecretPhysical` LAN scenario places a client at the authored trigger,
invokes `check_trigger`, and verifies entry/return without manual gate arming.
It also rejects a legacy remote `check_trigger_sub` invocation. This is trigger
callback coverage, not a recorded ship-flight collision or a hostile network
location test. Normal waiting-screen races, arrival placement/rearming, reactor
advance/endgame, full visit packet fencing, settled save/load and broader host
loss/recovery behavior remain incomplete. Earlier notes below describe the gate
and transfer probes before these physical hooks existed

Physical callback validation passed at 02:31:27 on 2026-09-13:
`test_lan.ps1 -Game d2 -InitialLevel 8 -SecretPhysical -AllowSecretWarps
-NoCoopQol -TimeoutSeconds 180 -SkipBuild`. The client invoked the authored
trigger through `check_trigger`; both peers established the gate without the
automation arm API, passed the warning/paused-transfer checks, entered -2 and
returned to 8 with matching committed campaigns and preserved player/world/history
state. The return used authored trigger 12 and operation SECRET_RETURN. Logs:
`temp/coop-physical-exits-live-d2.log` (terminal exit zero) and
`temp/coop-physical-exits-{5554,5556}-logcat.log`

Final Android build: `temp/coop-physical-exits-race-android.log`, terminal exit
zero. Both Windows builds passed in `temp/coop-physical-exits-windows.log`;
subsequent changes only affect Android collision routing and pending-request
cleanup. Initial Android builds exposed include-path errors, corrected by
keeping the travel header in `wall.c`/`switch.c` under its `coop/` path

Next physical-exit checks must cover normal-first/secret-first simultaneous
requests on the actual post-level screen, clearing a losing pending secret
request, disabled one-shot behavior, source-side capture cancellation, and
non-player trigger touches. In particular, `check_trigger` can identify a
Guide-Bot touch as the local player before calling `check_trigger_sub`; that
object identity now has an explicit guard in `check_trigger` before the local-player
call to `check_trigger_sub`. A new fixture temporarily marks a real robot type
as a companion for that callback, restores the type immediately afterward, and
requires no pending exit or active transition. This callback fixture passed in
the normal-exit run below; actual Guide-Bot flight into an exit remains untested.
Also cover dead participants during the initial freeze,
pending-request save/load or host changes, and exit rearming after arrival

The new `-NormalPhysical` scenario starts in D2 level 8. The host uses the authored
normal exit and must reach the actual post-level screen while it is still waiting
for players. The client remains in level 8 with an advancing game clock and reactor
timer, checks that the secret side is impassable and a rejected touch creates no
pending request, then uses the normal exit. Both must finish the ordinary wait and
load level 9. This tests a later client request while the host is on the waiting
screen; simultaneous competing requests and countdown deaths remain separate cases

Normal waiting-screen validation passed at 02:52:29 on 2026-09-13 with
`test_lan.ps1 -Game d2 -InitialLevel 8 -NormalPhysical -AllowSecretWarps
-NoCoopQol -TimeoutSeconds 180 -SkipBuild`. The host reached its real post-level
waiting screen. The client's game clock advanced by 211943 fixed-point units
while its reactor countdown decreased by the same amount, with no pause or
travel freeze and the host reported as CONNECT_END_MENU. The secret collision
side and callback were blocked, the later normal request was granted while the
host waited, and both peers reached level 9. Log:
`temp/coop-normal-clock-live-d2.log`, terminal exit zero; diagnostic captures:
`temp/coop-normal-clock-{5554,5556}-logcat.log`. Android build passed in
`temp/coop-normal-clock-android.log`

The initial run (`temp/coop-normal-physical-live-d2.log`) failed its combined
clock/status assertion after only about 130 ms. The test used `post_delay_ms`
on `set_debug`, which does not implement that delay. Physical preparation and
clock observation now use explicit `wait_ms` steps, and the assertion logs
each condition. The successful rerun above waited three seconds; the earlier
failure does not establish a gameplay freeze

`-NormalExitRace` implies `-NormalPhysical` and exercises a pending secret
request losing to a normal exit. A test-only fault holds the client's original
authenticated REQUEST envelope for eight seconds while permitting the nonce
handshake and host snapshots. The host exits normally after two seconds; the
client must clear its pending secret request, remain in the mine, and retain
normal exit access after the original delayed packet is delivered. This is
controlled packet reordering across a competing decision, not exhaustive
coverage of simultaneous delivery or the reverse winner

The delayed-request scenario passed at 03:00:44 on 2026-09-13:
`test_lan.ps1 -Game d2 -InitialLevel 8 -NormalExitRace -AllowSecretWarps
-NoCoopQol -TimeoutSeconds 180 -SkipBuild`. Host logs prove the secret REQUEST
for trigger 39/generation 44471 reached the host during NORMAL_WAIT generation
44472, with valid location and recovery epoch, and was rejected. The client
stayed playing in level 8 with matching advancing game clock/decreasing reactor
timer, cleared its pending secret request, and received a later normal grant
for trigger 13. Both peers then reached level 9. Logs:
`temp/coop-normal-race-live-d2.log`, terminal exit zero, and
`temp/coop-normal-race-{5554,5556}-logcat.log`. Final Android build passed with
no compiler warnings in `temp/coop-normal-race-final-android.log`; both Windows
builds passed in `temp/coop-normal-race-windows.log`. The earlier race build
reported a D1 unused variable, corrected by restricting the one-shot flag
backup to D2. Scoped formatting and `git diff --check` passed

Current integration work: `coop_travel_arm_campaign` now lets the host own source
capture, canonical campaign distribution, warning, destination load and commit.
The new CAMPAIGN transfer kind stages bytes at a frame boundary without running
a world restore or changing the recovery epoch. Every participant acknowledges
the prepared campaign before the warning begins. The host and clients decode
the same source-bound envelope and commit the same next archive. Runtime staging
also validates the embedded raw-world metadata before acknowledging readiness

Source checkpoint integration: a separate CHECKPOINT transfer now precedes the
prepared campaign. After freezing, the host captures a full source save with
the current campaign and recovery state, wrapped with the immutable portable
roster, session/operation context, lengths and checksum. Every participant
validates source mission/level/generation, roster identity, metadata bounds and
checksums without staging a restore. It writes `secret_travel_source.chk.tmp`,
closes and fsyncs it, rereads all bytes, atomically renames it to
`secret_travel_source.chk`, and fsyncs the containing directory before readiness.
The previous retained file survives failed writes or validation before rename.
Destination preparation waits for checkpoint distribution; the warning waits
for both checkpoint and prepared campaign readiness. Transfers remain separate
so two campaign archives do not exceed the existing chunk-count wire bound

The retained wrapper includes all frozen player records in addition to the
engine save. Its file survives runtime gate resets, while the in-memory copy
is released. Runtime rollback now has a distinct ROLLBACK transfer, host-owned
abort generation and retried authenticated client failure report. Every peer
requires byte-for-byte agreement with its retained checkpoint, restores the
full source save, then reapplies the canonical private player fields. Recovery
acknowledgment waits for successful restoration; ordinary destination loads
and unrelated save-transfer starts are blocked during recovery. A rollback
failure still returns to the lobby with the checkpoint retained

Rollback selects the frozen participant mask and accepts readiness from a
participant marked CONNECT_WAITING. It now sends BEGIN/CHUNK/APPLY directly to
that roster rather than the ordinary playing-player broadcast. Both games'
Android UDP retry queues retain isolated packets from the active rollback for
waiting peers, checking transfer kind, ID, exact packet length and required
recipient; unrelated or mixed gameplay packets do not gain that exception.
The rollback fault scenario now marks the failed client waiting on both peers
and drops one initial chunk while retaining it for retransmission. Its completion
must prove source restoration followed by another secret entry and return.
Live validation passed at 01:50:05 on 2026-09-13 with `test_lan.ps1 -Game d2
-InitialLevel 8 -SecretRollback -AllowSecretWarps -NoCoopQol -TimeoutSeconds 180
-SkipBuild`: waiting-peer recovery, lost-chunk delivery, source/player/history
verification, retry into -2, and return to 8. Log:
`temp/coop-rollback-waiting-verified-live-d2.log`, terminal exit zero. Host/client
COOPLOG captures are `temp/coop-rollback-waiting-verified-5554-logcat.log` and
`temp/coop-rollback-waiting-verified-5556-logcat.log`. Android and both Windows
builds passed (`temp/coop-rollback-waiting-android.log` and
`temp/coop-rollback-waiting-windows.log`). The first attempt failed only because
the paired runner started the client request before host fault arming; a separate
host preparation script now arms the fault before either request is possible.
The initial run's log is `temp/coop-rollback-waiting-live-d2.log`

This tests CONNECT_WAITING after an otherwise successful destination load, not
all partially initialized engine states. Host loss/process restart recovery
selection, commit-time failures, commit markers and file-write failure injection
also remain outstanding

The first `-SecretRollback` run injected failure after the client's destination
engine load, restored both peers to the source, and passed world/player/history
checks. Its subsequent retry failed: host recovery epoch changed from 2 to 3,
while the client had started at 3 and reused its old travel handshake. Source
level/generation equality therefore cannot justify handshake reuse after
rollback. Arming now requires a new nonce handshake on both peers whenever
rollback occurred. Failed-run evidence: `temp/coop-source-rollback-live-d2.log`
and `temp/coop-source-rollback-emulator-5554-logcat.log` /
`temp/coop-source-rollback-emulator-5556-logcat.log`. The initial rollback result
alone did not prove continued play; final retry validation appears below

The next run passed the explicit recovery pause/destination-blocking checks,
but exposed a second handshake ordering issue: a rearmed client received the
still-idle host's old completed rollback state before the host's own arm call.
The host now renews a completed rollback context before answering that peer's
new nonce. Evidence: `temp/coop-source-rollback-retry-live-d2.log` and the
corresponding `coop-source-rollback-retry-emulator-*-logcat.log` files. Retry
with this host-side renewal passed in the final run below. Also cover rearming while a release
acknowledgment is lost; ordinary restored gameplay does not prove that case

Strict source restoration now addresses two discard layers: `coop_save.c` can
discard an optional gear section during metadata reading, and recovery/pickup
application can discard individual records while still returning success.
Rollback uses a scoped source-restore mode that forbids metadata gear discard,
requires the engine's gear-application hook to complete, checks zero discarded
records and exact accepted counts, and verifies the source level/generation.
An incomplete result cannot acknowledge recovery. Ordinary save loading retains
its existing optional-section behavior. Host recovery/pickup tests exercise
the completeness predicate using actual discarded-record reports

The live rollback fixture now seeds two absent-owner recovery records: three
homing missiles as inventory credit and a four-missile pack bound to a source
powerup. Both peers verify owner, record IDs, life, quantities and live object
binding after rollback. The subsequent entry and return also check the pack's
dormant/live state and unchanged credit. This extension passed the complete
two-player run at 02:07:56 on 2026-09-13, including the waiting participant and
lost initial rollback chunk. Command: `test_lan.ps1 -Game d2 -InitialLevel 8
-SecretRollback -AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild`.
Log: `temp/coop-strict-rollback-verified-live-d2.log`, terminal exit zero;
COOPLOG captures: `temp/coop-strict-rollback-verified-5554-logcat.log` and
`temp/coop-strict-rollback-verified-5556-logcat.log`

Android builds (`temp/coop-strict-rollback-android.log` and the final fixture
correction in `temp/coop-strict-rollback-fixture-android.log`) and both Windows
builds (`temp/coop-strict-rollback-windows.log`) passed. Both games' recovery and
powerup-duplication host tests passed with the new completeness assertions;
logs use `temp/coop-strict-rollback-{d1,d2}-test_coop_{recovery,powerup_duplication}.log`.
The host assertions exercise real discarded-record reports, not a full engine
restore failure. Allocation-failure and corrupted retained-checkpoint rejection
still need live coverage, as do nonempty duplicated-pickup records

The first nonempty-fixture run stopped in FREEZING before source capture:
installing the fixture through `coop_recovery_apply_pending` reset the host's
seeded player lives, inventory revisions and Omega charge. The host then
rejected the client's unchanged portable revision. The fixture now preserves
those fields around ledger installation. Failed-run evidence:
`temp/coop-strict-rollback-live-d2.log` and
`temp/coop-strict-rollback-5554-logcat.log` /
`temp/coop-strict-rollback-5556-logcat.log`. This failure does not validate or
invalidate the new strict rollback path, which that run never reached

Final rollback/retry validation: `test_lan.ps1 -Game d2 -InitialLevel 8
-SecretRollback -AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild`
passed at 01:31:35 on 2026-09-13. It injected a client failure only after the
destination engine load succeeded; both peers stayed paused in RECOVERING,
rejected destination application there, restored source level 8/campaign
generation 1 and canonical player state, and verified retained checkpoint bytes
and source visit history. Without restarting the session, both then entered
secret -2 and returned to 8, committing generations 2 and 3 with matching
checkpoints/campaigns and preserved player/world state. Log:
`temp/coop-source-rollback-handshake-live-d2.log`, terminal exit zero

Android build: `temp/coop-source-rollback-handshake-android.log`, terminal exit
zero. Both Windows builds and both games' campaign, transfer-policy and
transition tests passed earlier in this change; subsequent fixes were confined
to Android travel/transfer code and automation. This proves the injected client
post-load case with an empty ledger, not every partial-load, return-direction,
host-failure, disconnect or disk-failure path

The same final build passed `-TravelGate -AllowSecretWarps -NoCoopQol` at
01:33:25 on 2026-09-13: seven-second warning with frozen reactor/game clocks,
pre-load cancellation preserving source history, and normal-exit individual
grants/secrets rejection without freezing gameplay. This is still the gate API
probe, not physical exit/post-level-screen integration. Log:
`temp/coop-source-rollback-normal-exit-d2.log`, terminal exit zero. Both final
live runners completed cleanup

Validation on 2026-09-13: Android builds and both Windows builds passed, as did
both games' campaign, transfer-policy, transition and recovery tests. The final
Android build includes explicit file/directory fsync and passed the two-player
four-leg level 8 -> -2 -> 8 -> -2 -> 8 scenario with QoL disabled. On each leg,
every peer reread its retained file and compared it byte-for-byte with its
staged source; the runner required matching nonzero checkpoint checksums and
preserved player/world/visit-history state. Log:
`temp/coop-source-checkpoint-live-d2.log`, terminal exit zero at 00:51:29.
Build logs: `temp/coop-source-checkpoint-final-android.log` and
`temp/coop-source-checkpoint-windows.log`. This does not yet test checkpoint
restoration, process-crash recovery, failed writes or a failed destination load

The final build also passed D1 briefing force-launch, saving, cold resume with
both players' saved inventory, and suppression of briefing replay:
`temp/coop-source-checkpoint-live-save-d1.log`, terminal exit zero at 00:54:19
on 2026-09-13. Both emulator runners completed cleanup

Player handoff integration in progress: each participant captures an immutable
172-byte portable record after freezing and settling recovery. A host cannot
accept that participant's freeze acknowledgment until it has received the
record with the current operation, nonce, life and inventory revision. The
prepared campaign now includes the fixed roster and all eight record slots;
each client requires an exact echo of its own submitted record. The host uses
these records before source capture and all peers apply the canonical records
immediately before world loading. Ship-status packets cannot overwrite the
frozen records during travel. The D2 status format also preserves the upper
secondary-weapon ownership bits after gameplay resumes

This handoff covers player inventory, carried hostages, cumulative and per-mine
statistics, lives, selected weapons, omega/afterburner, drop bookkeeping, and
relative cloak/invulnerability/fire/fusion timers. Object and connection identity
remain local to the engine. The record is not yet a durable per-player save
extension: complete private-state persistence and source rollback still need
integration. Live validation now deliberately corrupts each peer's cached remote
hostages/stats/lives before travel, then requires the authoritative values for
every player on every peer. Android protocol versions are D1 30040 / D2 30041

The first handoff runs exposed a late load-side activation reset: host logs
showed the client's correct carried hostages/weapon flags after world merge,
then `init_player_stats_new_ship` reset that remote inventory while the gate
was still LOADING. `multi_make_ghost_player` now activates the destination
object without replacing the living ship's inventory while travel freezes
gameplay. Normal respawns and normal-exit waiting retain their existing path.
The diagnostic failure is retained in `temp/coop-portable-reset-trace-live-d2.log`.
The activation fix passed the four-leg two-peer test: 8 -> -2 -> 8 -> -2 -> 8,
with deliberately incorrect remote caches reseeded before every leg. Both peers
recovered every player's hostages, rescued total, kills, lives, score, all ten
secondary weapon flags and life/inventory revisions; carried homing missiles,
keys, world changes and visit-specific checkpoint archives also passed. Log:
`temp/coop-portable-final-live-d2.log`, terminal exit zero at 00:06:46 on
2026-09-13. The host trace confirms load-side activation retained inventory:
`temp/coop-portable-final-host-trace.log`

The same build passed cancellation/source-history preservation and normal-exit
individual grants without freezing, using `-TravelGate -AllowSecretWarps
-NoCoopQol`: `temp/coop-portable-final-abort-d2.log`, terminal exit zero at
00:08:56 on 2026-09-13. D1 also passed briefing force-launch and cold save/resume
with six saved homing missiles and no briefing replay, using `-Briefings
-BriefingRestore -NoCoopQol`: `temp/coop-portable-final-verified-save-d1.log`,
terminal exit zero at 00:14:40. Android and both Windows builds passed, as did
both games' campaign, transition policy and recovery integration tests. Scoped
formatting and `git diff --check` passed

Roster application also checks every captured life/revision against the live
recovery ledger before applying any record, so newer ownership changes cannot
be rolled back by stale travel data. Fault-injected revision changes, exact
timed-effect values, host-observer travel and private-state save/restore still
need their dedicated integration scenarios. Delayed gameplay packets past
the release barrier still require the planned visit fence

The 48-byte prepared-travel codec carries source level/generation, mission,
action, next archive and detached destination, with bounded lengths and a
checksum over the complete envelope. The live transfer prefixes session ID,
travel recovery epoch, operation generation and the frozen roster block
described above. Malformed decoding leaves the previous prepared operation
intact

The revised `-SecretWorld` test only seeds/records expected player/world values,
arms the owner and requests travel. It no longer prepares or commits campaign
state itself. It checks the prepared checksum on both peers after each leg.
The host-owned round trip passed on both emulators, with QoL disabled:
level 8 -> secret -2 -> level 8, generations 1 -> 2 -> 3, identical prepared
checksums on both peers, preserved carried values and restored base robots.
Log: `temp/coop-canonical-travel-live-d2.log`, exit code zero at 22:31:10 on
2026-09-12. Both Android and Windows builds passed, along with both engines'
campaign and transition policy tests. A separate `-SecretRevisit` runner option
adds another entry/return pair to exercise restoration of the visited secret

That four-leg scenario also passed on both emulators: 8 -> -2 -> 8 -> -2 -> 8,
ending at generation 5 with identical prepared checksums on both peers after
every leg. The revisited secret retained its captured robot count rather than
being initialized afresh, and each player's carried values survived all four
transitions. Log: `temp/coop-canonical-revisit-live-d2.log`, terminal exit code
zero at 22:47:35 on 2026-09-12. The runner validates the new visit generation
before comparing checksums so cached introspection from a previous visit cannot
produce a false mismatch

This supplies a canonical campaign/player handoff and retained full source
checkpoint and coordinated rollback for the tested case; the remaining
recovery cases above and complete settled private-state saving still need work.
At this stage the runtime owner handled entry and return to an intact base.
Advancement was subsequently integrated and tested as documented above;
endgame remains incomplete. Transfer and retained restart
readers now allow the 16 MiB archive plus a 2 MiB world and 64 KiB of envelope
metadata. A compile-time check keeps that bound within the existing unsigned
16-bit chunk count at 432 bytes per chunk; the wire format is unchanged.
Transfers expire after 60 seconds without new progress and have an absolute
size-derived deadline allowing two rendered frames per second plus bounded
allocation/application waits. Repeated readiness or duplicate chunks do not
refresh progress. The campaign capture gate allows that larger bounded transfer
window, now allowing two sequential bounded transfers for source checkpoint
and destination preparation. At that stage actual triggers were still disabled;
physical hooks are now described above, with placement, visit-fencing and full
persistence work still outstanding

Transfer-bound validation on 2026-09-13: both host builds passed. Both games'
transfer-policy tests simulate the maximum envelope at two fps beyond the old
60-second timeout, check idle and absolute expiry, and check 16-bit wire bounds.
Campaign tests encode, decode and commit an operation larger than 16 MiB with
opaque raw-world fixtures. These tests do not prove a maximum-size live UDP
transfer or engine restore; those integration scenarios remain required.
Android build: `temp/coop-campaign-transfer-android.log`, terminal exit zero

The same build passed live two-player entry/return/revisit/return at level 8
with QoL disabled, including canonical roster/world checks and current-visit
restart/rewind archive checks on every leg. Log:
`temp/coop-campaign-transfer-live-d2.log`, terminal exit zero at 00:31:26 on
2026-09-13. This run uses ordinary-sized engine snapshots, not the maximum
archive bound exercised by the codec and pacing tests

D1's briefing force-launch, save and cold resume regression also passed on
this build, with restored inventory on both peers and no briefing replay.
Log: `temp/coop-campaign-transfer-live-save-d1.log`, terminal exit zero at
00:34:12 on 2026-09-13. Both live runners completed cleanup

Rewind and restart now include the campaign generation in their in-memory
mine identity. Successful secret-travel commit clears the previous visit's
history and queues a new restart checkpoint; capture waits until the travel
barrier has released and the team is alive and settled. Negative secret level
numbers use the same in-session capture path. The retained highest-normal-mine
checkpoint remains separate and is not replaced by a secret visit. Intermediate
travel/briefing states do not capture rewind history, and pre-load cancellation
keeps the source history. The live travel probe now inspects the actual restart
and rewind buffers' campaign archives after each leg; the abort probe checks
that the source restart buffer and rewind history survive

The updated two-peer entry/return probe passed with QoL disabled: level 8 -> -2
-> 8, with current-visit archives in both host checkpoint buffers after each
leg. Log: `temp/coop-travel-history-reboot-live-d2.log`, terminal exit code zero
at 23:23:37 on 2026-09-12. Android and Windows builds, scoped formatting and
both games' rewind and transition policy tests passed. These archive checks
do not yet exercise an actual rewind/restart restore after travel or prove
complete per-player state capture for such restores

The updated cancellation probe also passed on both emulators, retaining the
same source restart buffer and rewind history across a pre-load abort. It
rechecked normal-winner individual grants, secret rejection and live simulation.
Log: `temp/coop-travel-history-abort-live-d2.log`, terminal exit code zero at
23:25:36 on 2026-09-12. This does not prove rollback after world replacement or
physical normal/secret trigger arbitration

The runner now uses its configured startup timeout for initial launcher and
host-lobby readiness. Earlier attempts stopped during startup on a degraded
emulator; observed texture-loading progress ruled out a transition hang. A
cold restart of the affected emulator restored normal loading speed, after
which the unchanged native build passed. Briefing and travel deadlines were
not increased

Earlier foundation: campaign travel preparation builds an owned next
archive and detaches the destination snapshot without changing the live campaign.
Cancellation frees only the prepared operation; commit rejects a stale source
generation. Entry, return, destroyed-secret exclusion, destroyed-base advancement,
and last-level endgame have host tests. The engine adapter captures the current
raw world and uses the mission's existing secret-level grouping for preparation

First visits now have a multiplayer initialization path using StartNewLevelSub
and the portable-player merge already used for dormant-world restoration. A
host-issued transfer kind coordinates that initialization with the existing
network load. These low-level paths are now driven by the owner described above.
Portable-state capture now uses the frozen roster handoff described above.
The remaining rollback cases, full settled private-state persistence, packet
visit fencing and safe arrival placement remain required
before actual trigger entry

The earlier `test_lan.ps1 -Game d2 -InitialLevel 8 -SecretWorld -AllowSecretWarps`
probe prepared campaigns on both peers, loaded a first-visit secret, and returned
to the captured base with distinct carried player values. Preparation and commit
were invoked explicitly by automation, so that test did not prove host exit
arbitration, a canonical shared archive, warning timing, or gameplay release.
The complete probe passed on both emulators on 2026-09-12, with QoL disabled:
level 8 -> secret -2 -> level 8, campaign generations 1 -> 2 -> 3. Both peers
retained distinct score/hostage values, six homing missiles, their blue key,
and current recovery life/inventory revisions. The restored base retained its
captured robot count, including progression-critical robots. Android D1/D2 and
Windows D1/D2 builds passed, as did both campaign and transition policy tests

Passing log: `temp/coop-secret-world-verified-live-d2.log`. Two earlier runner
failures were investigated: the first incorrectly expected zero robots after
the fixture deliberately preserved progression-critical robots; the second
read cached introspection before the engine serviced the new request. The
runner now checks the captured count and waits for both committed generations.
The final run completed with exit code zero. Cross-mine timed effects, nonempty
recovery ledgers, reactor destruction, shared canonical archive capture, and
actual teleporter input remain integration checks beyond this probe

Readiness: gameplay policy is specified below; engine integration is not yet
proven. Resolve the implementation gates below before enabling the option

### Live host travel gate

`shared/coop/coop_travel.{h,c}` now carries authenticated host exit requests,
source campaign/level identity, operation generation and snapshot revision.
Client arming uses a nonce echo to bind the host's travel epoch: fresh games can
have different local recovery-reset counts, so equating those counters rejected
the first live handshake. The handshake does not mutate the recovery ledger

The two-emulator `-TravelGate -AllowSecretWarps -NoCoopQol` probe passed on
2026-09-12 after that fix. It verified first-request arbitration, frozen game
and reactor clocks during the seven-second warning, blocked autosaves, and
abort before world replacement. Its normal-winner stage verified individual
one-time grants, rejected secret requests and unfrozen gameplay. Log:
`temp/coop-travel-gate-handshake-live-d2.log`, terminal exit code zero

That probe invokes the gate API directly. It does not exercise a physical exit,
the normal post-level screen, or failure after replacing the source world

The gate now advances world transfers at a game-frame boundary while it owns
the simulation pause. World application waits for the local LOADING phase,
records that replacement has started before calling the serializer, and reports
destination-ready only after successful application. A failed application
cannot resume gameplay or acknowledge readiness; checkpoint recovery ownership
is still required before replacing the temporary lobby failure path

An earlier `-SecretWorld` scenario combined both directions with the
host gate: request, freeze, explicitly prepare source archives on both peers,
warning, world transfer, campaign commit and coordinated release. It held the
client at commit for three seconds to check continued pause/save rejection.
The expanded scenario passed on both emulators: level 8 -> secret -2 -> level 8,
with both campaign generations committed and host release masks complete.
It retained each player's carried values and the captured base robot count.
Log: `temp/coop-travel-world-gate-live-d2.log`, terminal exit code zero at
21:52:36 on 2026-09-12. Android D1/D2 and Windows D1/D2 builds passed, as did
both transition policy executables. A subsequent Android build removed two
D1 warnings caused by declarations used only when arming the D2 gate

Per-peer source preparation in that earlier test has since been replaced by
the host-owned checkpoint and campaign transfers described above, including
the canonical portable-player exchange. Full physical-trigger validation, arrival
placement, visit fencing, remaining rollback cases, destroyed-reactor progression and settled secret save/load
remain outstanding. Successful gate/transfer tests do not prove those paths

The ordinary D2 briefing/cold-resume regression also passed after the frozen
transfer-loop integration, with secret warps enabled and QoL disabled. Both
peers restored six homing missiles without briefing replay and the test rejected
an autosave during transfer. Log: `temp/coop-travel-world-gate-save-d2.log`,
terminal exit code zero at 22:00:45 on 2026-09-12

An armed gate now refuses world application after abort/settlement as well as
during warning. Returning to gameplay cannot itself authorize a delayed world
transfer. This phase check supplements, but does not replace, the outstanding
visit fence on gameplay packets and operation identity on world transfers

The strengthened `-TravelGate` probe passed on the final Android build, checking
that world application is refused during warning and after the pre-load abort,
then checking unfrozen individual normal-exit grants. Log:
`temp/coop-travel-world-gate-abort-live-d2.log`, terminal exit code zero at
22:04:47 on 2026-09-12. These assertions exercise the live phase guard; they do
not inject a delayed prior-visit payload or prove rollback after replacement

The runner rejects combinations of `-WorldRestore`, `-SecretWorld` and
`-TravelGate` before touching the emulators, since each fixture owns the mine
state. That argument validation and scoped formatting passed

Related work: [co-op briefings and synchronized mine launch](coop_briefings_and_launch.md)
adds optional pages/videos, per-player progress, a bounded reading period,
and an explicit host Launch now overlay. Briefing limits are two minutes overall
and at most 20 seconds after the host finishes, with a visible timer. Launch now
uses a second overlay button located away from Skip/Next and ends the reading
wait immediately while retaining the mine-ready barrier. Build both on the same operation,
readiness, status, and recovery infrastructure with separate server settings

Request: locate the disabled secret teleporter code, investigate the original
restriction, and plan safe co-op entry/return, including players elsewhere in
the base mine

User additions: a server setup Yes/No switch beside the co-op QoL switches,
a 5-10 second warning using the existing yellow save-loading status text,
and saving/loading in any settled state with all players transitioned

User correction: normal exits retain individual escape and the existing
waiting-for-players screen. The first accepted normal exit blocks secret
entry; it does not evacuate or freeze the remaining players

## Findings

The D2 restriction is inherited from the original game. The released Parallax
[SWITCH.C](https://github.com/videogamepreservation/descent2/blob/master/SOURCE/MAIN/SWITCH.C)
contains the same multiplayer rejection in `TT_SECRET_EXIT`, and
[GAMESEQ.C](https://github.com/videogamepreservation/descent2/blob/master/SOURCE/MAIN/GAMESEQ.C)
asserts that `EnterSecretLevel` is not running in multiplayer. These inspected
sections establish the restriction, but do not document the developers'
historical reason. The architectural explanation below is an inference from
the code, not a claim about their intent

Relevant current code, with line numbers at investigation time:

| Location | Behavior |
| --- | --- |
| `d2/main/switch.c:546` | `TT_SECRET_EXIT`; local-player-only activation, dead-player rejection, demo-data restriction, explicit `GM_MULTI` ban at 562, destroyed-secret check, then `EnterSecretLevel` |
| `d2/main/switch.c:509` | One-shot triggers become disabled before action validation; a rejected or aborted asynchronous transition must not consume the trigger |
| `d2/main/switch.c:514` | `TT_EXIT` in a native D2 secret mine calls `ExitSecretLevel` locally; return needs the same team protocol as entry |
| `d2/main/gameseq.c:1503` | `EnterSecretLevel` asserts no multiplayer, records `Entered_from_level`, saves the base, maps the destination through `Secret_level_table`, and starts the secret |
| `d2/main/gameseq.c:1347` | `StartNewLevelSecret` loads or restores the secret world but does not run the ordinary network level-sync and multiplayer preparation path |
| `d2/main/gameseq.c:1442` | `ExitSecretLevel` saves an intact secret world, restores the base if its file exists, otherwise advances from `Entered_from_level` |
| `d2/main/gameseq.c:1282` | `p_secret_level_destroyed` uses `First_secret_visit` and local secret-save existence as gameplay state |
| `d2/main/state.c:2219`, `2705` | Save/restore front doors divert multiplayer to ordinary co-op save/restore initiation and return before processing secret filenames |
| `d2/main/state.c:3139`, `3302` | Secret restore merges selected fields for `Players[Player_num]` and positions that one player at the return segment |
| `d2/main/state.c:2659` | Return uses one segment center and orientation, not a placement for a team |
| `d2/main/gameseq.c:1796`, `1820` | Death in a negative-numbered level follows individual secret return/advance logic, including after the multiplayer death handler |
| `d2/main/gameseq.c:1855` | Ordinary `StartNewLevelSub` has network player setup, level sync, and `multi_prep_level` integration worth reusing |
| `d2/main/net_udp.c:1944` | End-level handling forces the native D2 secret flag to zero |

All clients currently operate one active world represented by global level,
segment, object, and player state. Splitting the team would require multiple
world simulations, world-scoped object identities and messages, separate
reactor/AI ownership, and a way to join and leave each world. It is far beyond
enabling a trigger. A synchronized team transition fits the existing model

Removing only the visible ban would still hit the assertion, invoke the wrong
save workflow, bypass normal network startup, and leave other clients in the
source world. Broadcasting the existing trigger is insufficient: the action
checks `pnum == Player_num`, and successful local transition suppresses normal
trigger transmission

## Server setting

Add an independent Yes/No setting labeled `Allow secret area warps` beside
the co-op QoL switches in server setup. The existing UI anchor is
`android/app/src/main/java/com/dxxredux/app/multiplayer/CreateGameDialog.kt`
near `Coop QoL (guidebot, arrows, warp)`. This controls D2 secret-level team
travel, not discovery of ordinary hidden rooms or warp-to-teammate

The host's value is authoritative. Proposed initial default: No, preserving
the existing behavior until enabled; the user has not specified a default.
Remember the selection with the other host defaults and carry it through
setup, lobby advertisement/join, native launch, netgame synchronization,
resume, config import/export, saves, and host migration. Audit the existing
`coopQol` / `coop_qol` plumbing as the implementation model

Make the setting available for supported D2 co-op missions. It is independent
of the general QoL switch: test both combinations and remove incidental
dependencies on QoL-gated recovery code where secret travel needs it. With
No selected, reject new secret entry requests consistently on host and client
without consuming a one-shot trigger. Keep the value fixed during play;
restoring a save restores its saved session value before resuming. Always
provide a valid departure from an already active secret, even if an off
value is encountered, so the setting cannot strand a loaded team

## Proposed gameplay rules

One connected, living player touching a valid teleporter requests a team warp.
The host validates and authorizes it. All connected players and observers load
the same destination, regardless of distance from the activator

After acceptance, show who activated it and the destination for a 5-10 second
transition warning before changing the active mine. This is required for
the initial implementation, replacing the earlier immediate-transfer proposal.
Do not require everyone to find or physically reach the teleporter. Entry
and return use the same rule

### Timed warning and loading presentation

Use the existing persistent yellow save-loading status presentation, including
its font, color, placement, and HUD layout reservation. Current anchors are
`coop_restore_status_message` in `shared/coop/coop_save.c` (currently returns
`Waiting to restore save`) and its rendering in `d2/main/hud.c`, which uses
`BM_XRGB(31,24,0)`. Generalize the status presentation for transition text;
do not turn the warp warning into an ordinary transient HUD message

Non-activating players must see a countdown and unambiguous direction, e.g.:

- `Alex activated a teleporter: warping to secret area in 7s`
- `Alex activated a teleporter: returning to main level 4 in 7s`
- `Base mine destroyed: advancing to main level 5 in 7s`

Show the countdown to the activator and observers too. Use actual destination
names/numbers where practical; use an endgame message if no next level exists.
Proposed initial duration is 7 seconds, within the requested 5-10 second range,
as one named constant rather than another setup control

Proposed simulation policy: once the host accepts the warp, freeze gameplay
for all participants while the warning counts down, including reactor,
damage, AI, and temporary powerup timers. Continue rendering and networking.
Use monotonic real time for the warning, not paused game time. This gives
players notice without killing them during a forced transition or allowing
the initiator to start the destination while everyone else is still playing

Reliably acknowledge the warning/freeze phase before the host starts the
shared countdown. Late or unresponsive peers follow the bounded timeout and
disconnect/resync policy; do not silently skip a participating client's
warning. Coalesce repeat trigger requests without restarting the timer.
The host controls the deadline and announces subsequent phases

Prepare transfer data during the warning where safe, but keep the source
presentation until it expires. If loading takes longer, replace the countdown
with `Waiting to load secret area` or `Waiting to load main level 4` in the
same yellow status row. Zero means loading begins, not permission for a
client to resume. Clear the status only on coordinated completion or replace
it with a visible failure/cancellation result; restore the source and timers
consistently on an abort. Serialize this status with save/restore operations
so unrelated completion packets cannot dismiss the warp warning

| Situation | Proposed outcome |
| --- | --- |
| Enter from an intact base | Freeze and retain the base world; move the whole team into the secret |
| Leave an intact secret | Retain the secret world; restore the most recently entered base; move everyone to safe positions near its return point |
| Revisit | Restore the saved secret world, preserving depleted pickups, dead robots, doors, switches, and automap/secret discovery |
| Enter the same secret from a later base | Update the return destination and base snapshot; retain the secret's previous progress |
| Base reactor already destroyed when entry is accepted | Mark the base unavailable for return; on secret departure advance to that base's next normal level, or endgame |
| Secret reactor destroyed | Mark the secret permanently unavailable for re-entry in the current campaign state; the first valid return teleporter evacuates the team |
| Secret countdown expires without an exit | Resolve remaining players' deaths once, then perform one team return/advance; never let each death initiate a separate load |
| One player dies in an intact secret | Normal co-op death/drop/respawn in that same secret; death does not warp the team |
| Player is already dying when a warp starts | Resolve the death/drop once before capture, transfer the resulting ship state, and do not preserve pre-death gear or add an extra life penalty |
| Normal exit and secret entry race | Host selects one exit mode for the source generation; normal-first disables secret entry but continues accepting individual normal exits; secret-first starts the coordinated team warp |
| New join or reconnect during loading | Wait for the committed active world; never join a suspended base |

Normal exits retain existing co-op behavior with the setting on or off.
Once the first normal exit is accepted, block secret entry for the rest of
that level visit. The exited player waits on the existing waiting-for-players
screen; teammates keep flying and must escape normally or die when the
reactor countdown expires. Do not freeze the mine, pause its reactor timer,
start a 7-second warning, or grant escape credit to players still inside.
Later normal exits remain available. Suggested setting help: `Secret
teleporters move the whole team; unavailable after anyone takes a normal exit`

The secret teleporter's team evacuation rule intentionally rescues living
players who are far from the teleporter, including during a countdown.
It applies to secret entry and secret return, including advancement to the
next normal level if the base was destroyed. It does not apply to normal
exit activation, and should be described accordingly in the activation message

Place ships with collision-aware fanout near the entry/return point. Generalize
the existing `coop_start_positions.c` helper, which currently takes a player
start index, to support a return anchor. Verify narrow segments, doors, lava,
and missing authored co-op starts. Prevent immediate teleporter retriggering
until a ship has left its arrival trigger volume

Arrival implementation in progress: `coop_find_arrival_positions` adds an
anchor-based planner alongside the existing start helper, leaving ordinary
start placement unchanged. It considers the authored anchor and nearby offsets,
then up to 32 segments reachable through open portals with ship-radius wall
clearance. Candidates fit entirely inside their segments with a safety margin
and avoid other planned ships, robots, reactors and clutter. The complete plan
must succeed before any participant moves; placement failure uses coordinated
rollback. Secret entries/revisits use the authored first player start; returns
use the base mine's Secret_return_segment and Secret_return_orient. Placement
clears linear/angular velocity, thrust and turn roll and resets last_pos

Per-player arrival guards cover any exit whose wall segment or adjacent segment
contains that ship. Local callbacks and host request validation refuse that
exit until the ship leaves both segments. Rearming runs in the network frame,
and ordinary travel-gate reset does not erase the guards. Full save/rewind
persistence and stale gameplay-packet interactions remain to be addressed

Introspection exposes placement readiness, a canonical layout checksum and the
local blocked-exit count. Live tests will check geometry, separated ships,
cleared motion, full-roster planning in the loaded mine, and matching layouts
on both peers. The inspection holds the existing LOADING acknowledgment while
examining actual player objects, then releases it and checks guarded callbacks
after settling. Physical preparation leaves all exit volumes and waits for
position delivery before approaching the next exit

Partial live evidence on 2026-09-13: the first run in
`temp/coop-arrivals-live-d2.log` reached matching entry positions but failed a
combined clearance/motion assertion after gameplay resumed. The frozen inspection
in `temp/coop-arrivals-frozen-live-d2.log` passed entry on both peers: layout
checksum 2155257755, players in secret segments 18 and 17, zero surface masks,
zero linear/angular velocity and thrust, and zero turn roll. Full-roster planning
in that loaded secret also passed. Both peers then settled with preserved
portable/world/history state

That second run failed later on return, before destination placement. The host
accepted the physical SECRET_RETURN request, retained the source checkpoint,
and sent the prepared campaign, but disconnected the client before campaign
transfer completion. Logs show 1512 chunks in that transfer; they do not yet
prove the disconnect cause. Added Android D2 COOPLOG diagnostics distinguish
liveness timeout, full reliable-packet queue and unacknowledged-packet timeout.
Return/revisit placement and actual arrival-trigger guard coverage are still
unverified. Diagnostic captures: `temp/coop-arrivals-frozen-{5554,5556}-logcat.log`

Windows D1/D2 builds passed in `temp/coop-arrivals-windows.log`. Android builds
passed in `temp/coop-arrivals-final-android.log` and
`temp/coop-arrivals-frozen-android.log`; the initial new automation C++ warning
was corrected by adapting the engine's legacy mutable filename argument

The diagnostic rerun (`temp/coop-arrivals-transport-live-d2.log`) passed entry,
return and revisit with matching layouts and frozen-object checks. The base
return used segments 254 and 256, checksum 3533401712. The final return failed:
the new log identified an unacknowledged reliable save-chunk packet (type 82,
packet 665) reaching the 15-second UDP timeout, followed by DUMP_PKTTIMEOUT.
Captures: `temp/coop-arrivals-transport-{5554,5556}-logcat.log`. Both authored
arrival areas had zero overlapping exit volumes, so these runs do not yet
exercise guard rejection with an arrival inside an exit volume

Transfer flow control now limits new chunks to 128 outstanding reliable packet
slots in addition to elapsed-time pacing. Both engines expose queue occupancy;
rollback accounts for its separate copy per required peer. This supplies
backpressure when acknowledgment processing falls behind. Logging records window
waits and peak occupancy. Both engines' host policy tests pass a 1512-chunk
slow-receiver case with the bound maintained, plus the existing pacing/deadline
tests. Live verification failed on the first return in
`temp/coop-transfer-window-live-d2.log`: checkpoint and campaign transfers
completed, but world transfer kind 3/id 6 stalled and reliable packet 3265
timed out with type 82 and 440 payload bytes. The bounded window did not fix
the disconnect. Captures: `temp/coop-transfer-window-{5554,5556}-logcat.log`

Targeted Android D2 diagnostics now identify retries overdue by two seconds,
duplicate packets acknowledged by the receiver, and delayed ACK queue matches.
Use these to distinguish receive loss/starvation from ACK bookkeeping before
changing retry behavior. While auditing reliability, also check the legacy ACK
lookup's first-match behavior with unused queue slots and reused packet numbers

The diagnostic run `temp/coop-ack-path-live-d2.log` exited 1 on the first
return. This time source checkpoint kind 6/id 4 stalled. Host packet 595
logged its first retry crossing the two-second threshold at age 10521 ms and
eventually timed out. Nearby packets 592..594 matched delayed ACKs in live
queue slots at age 10444 ms. The client did not log duplicate receipt of 595.
Captures: `temp/coop-ack-path-{5554,5556}-logcat.log`. The scan always restarted
at slot zero and stopped after its small resend byte budget, allowing reused
low slots to starve older high slots. Both Android engines now retain a retry
cursor across frames and reset it with the reliable queue. The byte budget and
timeout remain unchanged

The corrected four-leg physical scenario passed at 04:14:40 on 2026-09-13:
`test_lan.ps1 -Game d2 -InitialLevel 8 -SecretPhysical -SecretRevisit
-AllowSecretWarps -NoCoopQol -TimeoutSeconds 180 -SkipBuild`. Log:
`temp/coop-fair-retry-live-d2.log`, terminal exit zero. Entry, return, revisit,
and final return passed on both peers with matching campaign and arrival
checksums, preserved portable/world state, and history checks. Captures:
`temp/coop-fair-retry-{5554,5556}-logcat.log`, with zero reliable timeouts and
zero queue-full diagnostics. Substantial duplicate traffic and delayed ACKs
remain visible, so this proves successful completion under the observed
conditions rather than absence of packet loss. Authored arrival guard counts
were still zero; guard-covered arrival testing remains outstanding

Builds passed in `temp/coop-fair-retry-android.log` (both native engines and
APK, terminal exit zero) and `temp/coop-fair-retry-windows.log` (both Windows
games, terminal exit zero). Next regressions must exercise rollback to a waiting
peer and D1 briefing/cold resume with the shared retry change. The Android D2
overdue/duplicate/delayed-ACK diagnostics remain enabled only through the
existing co-op debug log category

Reliable packet-number width is now corrected in both engines: the sender
writes a 32-bit number and `net_udp_process_mdata` now passes GET_INTEL_INT
to the validation and relay paths. The wire layout is unchanged. This is a
separate large-transfer bug; it does not explain the earlier timeout of packet 665

The rollback and cold-resume automation fixtures now jump the sender counter
forward to 65534 before save data is sent, then require an ACK above 65535
matching a live outgoing queue entry. This exercises the real receive/ACK path,
not just byte conversion. The fixture never rewinds the counter or replaces
existing queued IDs. Its ACK observation survives the tested world reload;
each explicit seed clears the previous observation. Full-width relay with three
or more peers and sequence rollover remain separate checks

Android and both Windows builds passed in `temp/coop-sequence-width-android.log`
and `temp/coop-sequence-width-windows.log`, both terminal exit zero. D2 passed
`-InitialLevel 8 -SecretRollback -AllowSecretWarps -NoCoopQol -TimeoutSeconds 180
-SkipBuild` at 04:24:19 on 2026-09-13, terminal exit zero in
`temp/coop-sequence-width-rollback-d2.log`. The live boundary seed and ACK
verification passed, the forced destination failure returned both peers to the
source with the recovery ledger intact, the waiting participant received its
rollback despite the dropped initial chunk, and a subsequent entry and return
passed. Captures: `temp/coop-sequence-width-rollback-{5554,5556}-logcat.log`

D1 passed `-Briefings -BriefingRestore -NoCoopQol -TimeoutSeconds 180 -SkipBuild`
at 04:27:33 on 2026-09-13, terminal exit zero in
`temp/coop-sequence-width-resume-d1.log`. The cold transfer crossed 65535 and
verified its ACK, rejected autosaving during transfer, restored six homing
missiles for both peers, retained the briefing setting, and replayed no
presentations. Captures: `temp/coop-sequence-width-resume-{5554,5556}-logcat.log`

A further legacy issue remains:
direct reliable sends wrap the counter to zero above 150000, while zero is also
present in cleared receive history. Review unified nonzero 32-bit sequence
rollover before maximum-archive and long-session tests

## State model and transfer

Cold secret resume coverage passed with `test_lan.ps1 -SecretColdResume`.
It extends the settled-secret save/load scenario, closes both processes,
selects the manual secret slot, starts network setup at normal level 1, and
requires the saved negative level, distinct player score/ammo, server options,
and all campaign metadata plus dormant-world sizes/checksums on both peers.
It then uses the physical return exit and verifies the preserved base.
Briefings stay enabled, with zero presentations allowed during cold resume.
The fixture records its actual base robot count before travel in a separate
test expectation file, bound to mission, level and campaign generation. It
reloads that expectation after process restart; production campaign state must
come entirely from the save

Launcher inspection found that save selection discarded negative-level saves
when matching the normal hosting level, and quick resume forwarded negative
levels to native startup, which correctly admits only normal mines. A D2 full
secret save now retains its actual destination while using normal level 1 for
startup before the full-save transfer. Create Game displays the saved secret
area as read-only instead of exposing that temporary level. Start fresh still
allows ordinary level selection. Resume-selection tests cover the signed saved
destination, positive startup level, persistence round trip and invalid secret
save types. All 25 `MultiplayerResumePrefsTest` tests passed, and the Android
build plus both desktop builds passed before live testing

The first cold fixture assumed level 8 had visible briefing pages. Live
introspection showed zero authored steps and successful all-ready release on
both peers, so the fixture now verifies that empty-content path explicitly.
The next run restored the host but failed the client's runtime-state validation.
Diagnostic logging then identified object validation: the startup mine assigned
the client object 16; loading secret -2 assigned object 6, which was also the
saved client object. Restore incorrectly reapplied the pre-load object 16
before remapping players, invalidating the saved object population. D1 and D2
Android restore now retain the destination's local ship index after loading it.
Desktop behavior is unchanged. Diagnostics:
`temp/coop-secret-cold-diagnostic-emulator-5556-logcat.log`.
The ship-index fix passed cold resume on both peers at 10:24:04 on 2026-09-13,
including exact dormant-world descriptors and saved local score/ammo. The
subsequent return exposed a position-packet race: after the client placed the
host at base segment 254, an old PDATA moved it to secret segment 18 during
the frozen arrival inspection. Its logs show correct world application followed
by that invalid position. D1/D2 Android PDATA now refreshes liveness but skips
ship position application while `coop_travel_blocks_gameplay()` is true.
Normal-exit waiting remains live. This frozen-phase guard is implemented;
packet visit tagging after release and other gameplay packet families remain
open. Logs: `temp/coop-secret-cold-index-live-d2.log` and
`temp/coop-secret-cold-index-emulator-5556-logcat.log`.
With the position guard, both peers cold-resumed successfully and returned to
base level 8 with valid frozen arrival positions. The final fixture incorrectly
expected zero robots: `clear_robots` preserves companions, bosses and key
carriers, and this base retained two robots. The fixture now reloads its actual
pre-travel measurement instead of inventing a zero count. Evidence:
`temp/coop-secret-cold-pdata-live-d2.log` and
`temp/coop-secret-cold-pdata-emulator-5556-logcat.log`.
The complete run with measured expectations passed at 10:56:36 on
2026-09-13, terminal exit zero in
`temp/coop-secret-cold-expect-retry-live-d2.log`, as summarized above

Settled secret save/load integration is in progress. Android D2's
`state_save_all` and `state_restore_all` now let co-op reach the multiplayer
dispatch before applying the single-player negative-level prohibition.
Single-player and desktop behavior is preserved. The shared Android quick-slot
helpers remain single-player-only; co-op continues through its own save path

`coop_travel_blocks_state_actions` covers both an active travel phase and a
pending local exit request. Save/load/autosave, rewind and restart/capture
entry points use it. This is separate from the gameplay freeze predicate:
waiting for normal-exit arbitration must not freeze the mine or block the
request's eventual acceptance. Physical exit automation checks immediate
autosave rejection in the same callback turn

`test_lan.ps1 -SecretSaveRestore` implies the physical secret scenario and
adds a settled save/restore after entry. It opens/cancels the normal save/load
menus, writes an actual co-op slot, mutates distinct player inventories, restores
that slot through the team transfer, compares the complete dormant campaign
archive and saved score/ammo on both peers, then completes the normal return leg.
Live validation passed at 04:51:40 on 2026-09-13 with terminal exit zero in
`temp/coop-secret-save-menu-live-d2.log`. This does not establish full private-player snapshot
fidelity, cold resume from a negative level, reactor-active saving, or actual
rewind/restart restore; those requirements remain open

The first live run (`temp/coop-secret-save-live-d2.log`, exit 1) passed entry,
opened the secret save menu and wrote its slot, then rejected the subsequent
Android load-menu request because gameplay was not the front window. The
fixture now waits for `game_window_is_front` after closing menus and after
saving, rather than treating the rendering screen mode as proof of window
closure. It also uses the save writer's full 20-byte description buffer.
The save menu's first Escape exits description editing; a second closes the
menu. The corrected fixture verifies that sequence and passed the full retry.
Captures: `temp/coop-secret-save-menu-emulator-{5554,5556}-logcat.log`

Add a co-op campaign context with mission identity, active level, actual base
of entry, explicit base returnability, secret identity and visit/destroyed
state, snapshot references, and a monotonically advancing transition
generation. Missing or corrupt files must be errors, not evidence that a mine
was destroyed. Persist the context; `Entered_from_level` alone is transient

Separate three responsibilities even if existing serializers supply most of
the bytes:

1. Destination-world state: geometry changes, objects, AI, walls, reactor,
   triggers, level-local discovery and statistics
2. Current campaign/player state: stable player identity, live inventory,
   score, life/death state, absent-player records, and recovery accounting
3. Transition state: source/destination identities, participants, generation,
   lifecycle phase, and ready/commit acknowledgements

On a level swap, restore destination-world state and overlay the current
roster and portable player state. An ordinary co-op restore rolls back player
state and cannot be used unchanged. Explicitly classify energy, shields,
ammo, weapon selection, omega/afterburner charge, keys, cloak/invulnerability
remaining duration, carried hostages, kill totals, and per-mine counters

Recommended accounting: carry live ship resources and earned keys through
native D2 teleports; preserve global score and kill gains; retain counters
with their mine; track hostage origin so rescue is credited once. Do not run
ordinary end-level bonuses merely for visiting an intact mine. Clear keys
and settle hostage/bonus accounting on actual campaign advancement under
the ordinary rules. Custom secret mines containing keys or hostages need
explicit coverage, not assumptions based on stock levels

Keep death-drop and pickup recovery consistent across suspended worlds.
`coop_recovery_level_leave` currently turns live objects into recovery credits.
Applying that unchanged and later restoring those objects could duplicate
gear. Suspended worlds must keep world-scoped recovery identities, or their
restored drops must be reconciled with the live campaign ledger. Restoring an
old world must never roll back newer consumption/reclaim records

Proposed transaction:

1. Authenticate/validate the request, server setting, capability, source
   generation, trigger, mission destination, and absence of another
   save/rewind/transition
2. Freeze participants at a defined boundary; settle pending player state,
   death/drop events, and robot ownership; capture a recoverable source;
   acknowledge and display the shared 5-10 second warning
3. After the warning, load the destination, apply the player merge, and
   distribute one host-authorized world through the existing transfer
   infrastructure; retain the yellow waiting status until ready
4. Wait for destination-applied acknowledgements with bounded timeouts; record
   the committed new generation, then release ready participants to gameplay
5. Before commit, abort to the retained source on failure; after commit,
   resync or disconnect a failed participant rather than split active worlds

Audit stale gameplay packets, not only transition packets. Returning to the
same numerical level can reuse object indices; a level number alone is not
a sufficient guard against delayed packets from an earlier visit. Integrate
with existing recovery epochs and reset robot ownership/network object maps

Existing building blocks: `shared/multi_save_transfer.c`,
`shared/coop/coop_level_restart.c`, `shared/coop/coop_restore_remap.h`,
`shared/coop/coop_recovery.c`, and `state_save_to_memory` /
`state_restore_coop_from_memory`. These are reuse candidates, not evidence
that the required transaction or portable-state merge already exists

Serializer integration findings for the next implementation stage:

- `state_save_to_memory` routes through `state_android_save_to_path` directly
  to `state_save_all_sub`, so it bypasses the UI/lifecycle negative-level
  prechecks. `state_restore_coop_from_memory` uses the ordinary co-op callsign
  remap path, not the single-player secret restore merge
- `StartNewLevelSub` accepts the requested signed level, runs multiplayer
  synchronization/preparation, and uses the existing co-op fanout for missing
  player starts. It also calls `coop_recovery_level_leave` and resets per-mine
  statistics and reactor state during fresh initialization; keys/resources and
  timed effects also reset unless the secret flag suppresses that portion.
  These side effects need explicit capture/merge control for secret swaps
- `coop_recovery_level_leave` changes every LIVE recovery entry to CREDIT but
  does not remove its world object. Capturing a dormant world before that call
  and later restoring it unchanged can duplicate reclaimed gear. Keep recovery
  entries scoped to persistent worlds and reconcile newer campaign ledger state
  when applying a dormant snapshot
- Raw dormant snapshots must exclude nested dormant-world payloads; otherwise
  each revisit would recursively embed the previous campaign bundle. Ordinary
  user saves include the complete context and required worlds atomically
- `multi_prepare_restore_sync` strips robot ownership but does not change the
  UDP session token. `valid_token` checks the same `netgame_token` for PDATA,
  MDATA, and host end-level packets; client end-level/object-sync traffic has
  additional player-token rules. A secret transaction needs an explicit
  visit fence and must clear obsolete reliable gameplay payloads before their
  queued retransmissions can be repackaged under a new token

Save/load, restart, and host migration must include the dormant world and
campaign context, not just the currently active mine. The co-op save path
currently skips the ordinary single-player secret companion-copy branch.
Transfer snapshots/context to eligible replacement hosts. During an
unfinished transaction, migrate only with a recoverable committed generation;
otherwise abort consistently. Keep rejoin identities stable across slot changes

Audit progress tracking that assumes `previous_level == current_level - 1`
in `coop_save.c`, restart checkpoint selection, rewind history, guidebot
ownership/routes, and lobby level display against non-monotonic secret travel

## Saving and loading in settled states

Required: allow saving and loading whenever the team has completed its
transition, whether in a normal mine, in a secret mine, or back in the base
after a visit. Native D2's negative-level save/load guards must not block
this supported co-op path. This is part of the feature, not deferred work

Define settled as: no normal-exit waiting phase, warp warning, transfer, load, restore, or uncommitted
world change is active, and every current participating player has acknowledged
the same committed world generation and is ready. A disconnected player
retained in campaign records does not indefinitely prevent settlement.
Normal combat is settled in this sense; it does not mean everyone is at
spawn or at full health. Preserve a settled reactor countdown in a supported
save. Audit death/drop serialization separately: if the current serializer
cannot safely represent an in-progress death or pickup reclamation, briefly
block with a specific reason until that event finishes. Do not silently revive
players to make a save possible or keep all secret-level saves disabled

Keep the existing co-op authority and save/load controls. Gate manual actions
during the warning and transfer with a clear transition-in-progress status;
defer autosaves until settlement. Clear busy state after successful completion
and after a consistent abort so both actions become available again

Each save is a self-contained campaign snapshot containing the active world,
any dormant base and secret worlds needed for return/revisit, their explicit
availability/destroyed flags, actual return destination, all player/absent-player
and recovery records, and the server setting. Write and validate the complete
bundle atomically; do not depend on mutable global single-player secret files
or save filenames left by a previous session

Loading must work between any settled source/destination combination:
normal -> secret save, secret -> normal save, secret -> secret save, and
cold launch directly into a secret save. Validate the complete bundle before
replacing the current campaign. Loading rolls back the campaign and player
state together to the save; unlike a teleporter swap, it must not overlay
the players' pre-load equipment onto the saved world. Remap saved players
to the live roster through the existing co-op identity policy

Install the active and dormant worlds, return context, recovery ledger, and
saved server option as one restore operation. Use a fresh network generation
to reject pre-load packets. Do not replay a teleporter warning for an ordinary
save load; use the existing save-loading status. Resume only after all current
participants are ready, then re-enable saving/loading. Returning or revisiting
after a cold secret load must behave exactly as after uninterrupted play

## Implementation readiness review

The following rules tighten the earlier proposal to keep one active world,
one operation owner, and one explicit recovery destination. They incorporate
the subsequent discussion about racing normal and secret exits

### Fence both exits before any local side effects

Intercept both exit types before local end-level processing. In particular,
`d2/main/endlevel.c:start_endlevel_sequence` dematerializes the guidebot,
sends `multi_send_endlevel_start(0)`, and calls `PlayerFinishedLevel(0)`.
None may run speculatively while waiting for the host. Guard the entry points
as well as the trigger caller, including `PlayerFinishedLevel`, `AdvanceLevel`,
death/countdown advancement, final-boss/endgame, and received end-level packets.
The first normal exit requires host acceptance before its local side effects
and waiting screen begin. That acceptance locks normal exit mode, not an
immediate team level change. Later normal exits use the existing individual
flow under that mode. No waiting screen can independently select a secret
destination or bypass the ordinary end-level wait

A client requesting the first exit decision briefly waits at its source-side
position with `Waiting for exit confirmation`. It does not speculatively open
a post-level screen or load a secret. Other players continue playing. If the
host accepts normal mode, only the requester begins its normal exit sequence;
if secret mode wins, the host begins the team freeze/warning. If rejected,
release the requester into the source mine without changing world state.
Use host processing order with authenticated player identity and source/session
generation; client timestamps do not decide competing exits

For a secret warp, apply death and reactor events already accepted at the
host's cutoff before validating a living activator and taking the snapshot.
Later gameplay events cannot change that chosen transition. Clients accept
authoritative reconciliation even if their local countdown/death presentation
was ahead. Define a bounded collection of final pre-freeze player/robot state
rather than assuming freezing the host alone freezes distributed simulation.
Normal exit mode has no such team freeze cutoff: gameplay, later escapes,
and reactor deaths continue normally for players still in the mine

In secret mode, reject competing normal exits; the team is frozen for the
warp. In normal mode, reject secret entry with `Secret teleporter unavailable:
a player has already exited normally`, without blocking later normal escapes
or consuming a secret trigger. If a physical barrier is used, confine it to
the secret entrance and retain the host-side activation check as authoritative;
never close normal exits. Duplicates return the current decision without
restarting a warning. An accepted mode stays locked even if its initiator
disconnects; it cannot reopen secret entry after anyone has exited normally

For normal completion, preserve existing individual escape/death, hostage,
score, and waiting-screen behavior. Each participant must finish through the
ordinary exit/death/disconnect accounting before normal progression continues.
The mode lock must not mark all players escaped, terminate the countdown,
or trigger a new team-ready barrier that freezes remaining gameplay. Keep
networking alive on the waiting screen and prevent duplicate outcome credit

### One operation gate

Use one host-owned operation record shared by exits, manual save/load,
autosave, rewind, restart, and migration. Existing transfer/status helpers
must consult it, not each acquire unrelated busy flags. Menu selections and
network requests pass the same validation at execution time

| Phase | Permitted behavior |
| --- | --- |
| Settled | Normal play; save/load and supported rewind/restart can request ownership |
| Normal exit waiting | Exited players wait; remaining players keep playing, exiting, or dying under the live countdown; secret entry and save/load/rewind/restart are blocked until ordinary progression completes |
| Optional next-mine briefing | Begins only after normal exit/death waiting completes or another authorized progression selects the destination; individual reading/progress, host launch overlay, and bounded deadline; no mine simulation or save/load/rewind/restart |
| Freeze and capture | Pump network/UI; reconcile accepted events and capture a recoverable source; no new gameplay actions |
| Warning | Frozen source visible, shared 7-second countdown; no save/load/rewind/restart |
| Apply and verify | Yellow loading status; apply destination at a safe frame boundary; no gameplay until released |
| Committed and releasing | Destination fixed; acknowledge release or remove failed peers; no competing operation yet |
| Recovering | Yellow explanation; restore one agreed checkpoint or return to the lobby; no gameplay in a partially restored world |

An accepted save/load/rewind owns the gate before an exit can be accepted,
and vice versa. Reject competing manual actions with the active operation's
reason; do not queue an old load or rewind to run unexpectedly after arrival.
Coalesce autosave requests into one save after settlement. Already-open
menus, controller bindings, automation, and client requests obey the gate too

Normal exit waiting is a distinct branch, not the warp warning/transfer
pipeline. Its gate blocks competing world-changing operations while explicitly
permitting remaining players' normal exits, deaths, and ordinary network play.
Advance only through existing end-level completion rules. Preserve this normal
mode lock in any supported host migration so a successor cannot reopen the
secret entrance while a player is already on the waiting screen

Briefing completion and mine readiness are separate acknowledgements. Host Skip
ends only the host's presentation and opens its launch/wait screen. An explicit
Launch now overlay or documented deadline/all-ready policy closes remaining
presentations, then everyone still passes load/sync before gameplay release.
Do not reuse tap-anywhere or generic screen advance to launch. Natural advancement
from a secret to a new normal mine can run that mine's authored briefing; ordinary
secret return/revisit, save-load, restart, and rewind do not replay it

Internal recovery capture is not a user save: it must be callable by the
operation owner while ordinary save controls are blocked, without recursively
entering the gate. Balance pause/input/status cleanup on every terminal path.
Do not set `Endlevel_sequence` merely to freeze play: existing packet dispatch
uses it to suppress messages and may suppress required transition traffic

### Keep rewind within one visit

For the initial implementation, do not rewind across a teleporter or normal
level change. Start a new visit ID and clear old rewind history on successful
arrival, even when returning to the same numerical level. Display `Rewind
available after new history is recorded` while there are no usable snapshots

Within a settled visit, allow existing host-controlled rewind if its ordinary
player-state checks pass. Each snapshot includes active-world state, portable
player/campaign state, recovery records, and mutable secret/base availability
flags. Reference immutable dormant-world data owned by that visit; it need not
be copied into every rewind frame. Keep it alive until the history is cleared

Rewinding across the current mine's reactor destruction must restore its
reactor timer and destroyed/returnability flags together. Destroyed is permanent
for forward play, not across an explicit save-load or supported rewind to
before destruction. A swap keeps present player gear; load/rewind restore the
snapshot's player gear and recovery ledger. Never mix those restore modes

Only enable restart for a valid full campaign checkpoint for the current
visit. Returning to a base does not grant permission to use a stale pre-secret
level-start checkpoint that could recreate pickups already carried out.
Initially block restart for a visit lacking a suitable checkpoint with
`Restart unavailable for this visit`; ordinary saved-game loading remains
the supported way to return to an older campaign point

On a successful save load, create a fresh network generation and rewind visit.
On transition abort, conservatively clear history if the source was restored
or reconciled; do not splice old history onto a changed recovery ledger.
Session/authority/operation generations never roll backward with game time

### Recover to an explicit checkpoint

Retain a complete source campaign checkpoint before tearing down the world,
including an already-destroyed reactor and its remaining countdown. A base
marked non-returnable for gameplay still needs a temporary rollback checkpoint
until the operation succeeds. This checkpoint never makes its teleporter
returnable. Keep the last good user save untouched

Prefer one versioned self-contained bundle using existing world serialization
and transfer infrastructure. Validate mission/content identity, signed level
numbers, all referenced worlds, sizes, and checksums before application.
Check disk/memory capacity before source teardown, bound bundle/transfer sizes,
and reject unsupported destinations clearly. Do not silently evict a dormant
world whose progress is still needed. Avoid maintaining multiple live worlds
or building a general distributed transaction framework for this feature

Persist the source checkpoint before loading and retain it until the committed
destination is recoverable. A single atomic bundle replacement/commit manifest
must distinguish a completed destination from partially written transfer data.
Account for peak Android memory during source retention and destination load

| Failure | Required result |
| --- | --- |
| Invalid destination or failed source capture | No world teardown; release the request and explain the failure |
| Peer times out before commit | Resolve its roster membership, then host continues with ready peers if the destination remains valid; otherwise aborts; never wait forever |
| Destination fails to load/apply before commit | Host orders restore of the retained source under a fresh generation; restore timers, gear, and outcomes once |
| Peer fails after host commits | Resync that peer to the destination or disconnect it; no unilateral return to the source |
| Host disappears during transition | Initial policy: return peers to the lobby with the last agreed recoverable checkpoint; do not elect a host into an uncertain half-transition |
| Host disappears in settled play | Existing migration may proceed only if the replacement has the complete campaign context and dormant worlds; otherwise use lobby recovery |
| Source rollback also fails | Return to the lobby with a clear recovery error and the last valid checkpoint/save still available |
| App is backgrounded or killed | A nonresponsive participant follows timeout policy; resume/rejoin queries the authoritative generation instead of completing its old local operation |

Never let a client timeout mean `resume my old mine`. A paused client can be
behind the host, but cannot act in a different generation. Across a network
partition, consistency takes priority over keeping both sides playing. A
commit broadcast alone is insufficient: peers acknowledge applied state and
release, and the host tracks the active roster. Observers see the warning but
cannot activate an exit; an observer must not indefinitely hold up the team

Reuse existing bounded transfer/liveness timeouts, explicitly mapping each
phase to them; exclude the intentional warning from transfer timeout accounting.
The 7-second warning is not the whole operation timeout. Name the stalled
participant or phase in the waiting status. If loading requires a blocking
engine call, preserve network servicing through its existing loading/sync path
and keep the UI responsive enough to display status and allow leaving

After failure, require leaving the trigger volume before re-arming it, or a
deliberate retry action if placement makes that impossible. Do not create a
loop of automatic requests, frozen warnings, and failed loads. Flush held fire,
movement, and menu-confirm input at release so a held button does not fire or
skip a screen immediately. Do not add a routine approval dialog to successful
transitions; the host setting and visible warning establish the behavior

### Concrete implementation gates still open

- Exit interception: prove all native D2 exit/death/final-boss and packet paths
  are fenced before local side effects. In addition to `switch.c`, inspect
  `endlevel.c`, `gameseq.c`, and `multi.c` callers of `PlayerFinishedLevel`
- Freeze boundary: specify how final accepted player/robot updates and
  death/pickup events reach the host before capture. A client locally frozen
  while other peers still simulate is not yet a consistent source checkpoint
- Stale traffic: identify gameplay packet families lacking world/visit
  generations. Recovery epochs alone do not prove positions, robot ownership,
  doors, triggers, and end-level packets are safe after a revisit
- Save guards: `d2/main/multi.c` rejects save/restore while the reactor is
  destroyed; `shared/state_android_shared.c` has negative-level autosave and
  quick-load guards; `coop_save.c` restore and `android_rewind_policy.c` use
  all-players-alive checks. Classify each as required, safely replaceable, or
  a temporary explicit busy reason instead of globally removing guards
- Restore payload: define field ownership and test the real serializer's
  world/player merge, death/drop reconciliation, guidebot ownership, and key/
  hostage accounting. Existing memory save support is not proof of correctness
- Recovery readiness: destination applied, destination committed, and gameplay
  released are distinct acknowledgements. Decide where the durable checkpoint
  lives and how lobby recovery discovers it after host process death. At least
  one eligible peer needs a complete recovery copy before teardown when other
  players participate; a solo host retains its local durable checkpoint. If
  the host fails earlier, use the previously agreed settled checkpoint, not
  an incomplete new capture
- Platform scope: proposed first scope is Android D2 co-op; keep unsupported
  peers out of enabled sessions and preserve desktop builds. Native D1
  transition fixes and cross-level rewind need not be part of this feature

Implement and verify these foundations before wiring the setting to enabled
gameplay. Keep this as one staged feature with reviewable intermediate work;
do not ship an enabled trigger with save/restore or recovery deferred

## D1 and compatibility boundary

Native D1 already permits secret exits through `d1/main/switch.c:148`, sends
`multi_send_endlevel_start(1)`, and calls `PlayerFinishedLevel(1)`. Its secret
is a campaign detour: base -> secret -> next normal level, with no return to
the old base. `d1/main/gameseq.c:1178` maps return through the mission table

D1 UDP currently selects secrets based on source-level eligibility even if
the actual secret exit was not found. D1 emulation in D2 has an additional
mismatch: `net_udp_endlevel` sets a secret flag, but D2 `AdvanceLevel` does
not use it to choose the secret destination. The D2 teleporter ban also runs
before its D1 emulation branch

Keep D1 transitions as a separate policy. Coordinate any correction with
`../gameplay/plan_d1_in_d2_remaining_semantics_audit_20260907.md`, section A2;
do not accidentally give D1 the D2 return-to-base behavior

Keep non-co-op multiplayer disabled. Add protocol capability/version handling
so incompatible peers cannot enter a session that will send these new
transitions. Shared Android co-op infrastructure is the natural implementation
home; add narrow engine hooks and preserve desktop builds/single-player
behavior. Decide explicitly whether the new capability ships for Android
only initially or is also built into desktop peers

## Implementation sequence and validation

Implementation progress:

- Added `shared/coop/coop_transition_policy.{h,c}` with explicit normal waiting,
  secret freeze/capture/warning, briefing, apply/release, and recovery phases
- The policy rejects stale generations and competing actions; its normal mode
  keeps unresolved players active and continues admitting normal exits. The
  physical exit hooks have since been added as described above
- Added `android/tests/test_coop_transition_policy.c` and its PowerShell runner
  to the host CMake test suite. These exercise the actual policy with multiple
  participant acknowledgements; they do not yet exercise the network transport
- Both independent settings are plumbed through Android setup, launch, network
  configuration, save metadata, and migration. The subsequent physical hooks now
  route enabled D2 co-op exit triggers through the runtime transaction
- Briefing runtime networking, deadline/status UI, and mine-release barriers
  are integrated; see the linked briefing plan for completed and remaining tests
- Shared save-transfer pacing now targets elapsed time with bounded bursts,
  allowing slow rendered frames to catch up. Both games' two-peer briefing-enabled
  save/resume tests passed after this fixed a transfer timeout. This is active
  mine save coverage, not proof of dormant-world campaign serialization
- Added a bounded flat campaign archive in `shared/coop/coop_campaign.{h,c}`.
  It records mission, active level, entry base, returnability, generation, and
  dormant/destroyed worlds. The active world remains in the outer engine save.
  The v11 co-op trailer writes and checksums the archive, stages it separately
  during preflight, and applies it with the restored state. A corrupt required
  archive rejects the save instead of taking the optional gear-discard path.
  Embedded world metadata must match its directory entry and contain no nested
  campaign archive. `coop_save_world_to_memory` provides that raw capture mode.
  Normal D2 level starts initialize the context; new sessions clear it.
- The archive host tests cover round trips for returnable and destroyed bases,
  revisits and destroyed secrets, canonical ordering, every truncated prefix,
  corrupt checksums/lengths, duplicate world IDs, and resource bounds. These are
  serializer tests with opaque world bytes, not engine travel validation.
- Remaining archive integration: populate dormant snapshots at the actual warp
  boundary, preserve the live campaign during world-only restore, reconcile the
  recovery ledger, and preserve the authored return anchor. The current archive
  bounds are 2 MiB per raw world and 16 MiB total. Transfer and retained restart
  readers now include both bounds plus envelope metadata, with size-based and
  idle deadlines. Large codec/pacing tests pass; maximum-size live transfer and
  restoration remain unverified. Never silently evict a dormant mine.
- Recovery records now include their persistent mine number and an explicit
  `DORMANT` state. `coop_recovery_suspend_world` marks source drops after the
  freeze has settled; normal level departure leaves those records intact.
  Binding, collection, removal, save validation, and network remapping are
  restricted to the record's mine. Full save restoration retains inactive
  records without searching the current mine for matching object signatures
- `coop_recovery_apply_world_pending` validates a loaded world's old bindings
  against the current campaign ledger before changing any object. It rebinds
  surviving drops, removes already reclaimed drops, and preserves current
  inventory revisions, lives, other mines, and remaining credits. An incomplete
  match rejects the reconciliation without partially mutating objects or ledger.
  It is now called by the dedicated dormant-world restore path described below;
  ordinary user save loading continues to roll back the whole ledger instead
- Recovery accounting is enabled by the D2 secret-warp option independently of
  the general QoL flag. The save trailer is now v12, progress inventory v5, and
  the 160-byte recovery packet carries a 107-byte record. Android protocol
  versions are D1 30026 and D2 30027, including the dormant-world transfer kind;
  desktop versions remain unchanged
- Both host recovery harnesses passed new two-world scenarios: identical object
  slots/signatures in different mines, save/restore while the base is dormant,
  rejoin reclamation in the secret, return without gear duplication, remapping
  surviving drops, stale receipt rejection, and atomic rejection of incomplete
  captures. These execute the actual recovery code with simulated engine worlds
- Remaining secret work: authenticated exit arbitration, campaign transactions and
  world/player merge, source capture, team warning, destination transfer,
  commit/recovery, full campaign save/rewind/rejoin integration, and real peers

Campaign archive validation on 2026-09-12:

- Android D1/D2 native and Kotlin build passed; Windows D1/D2 builds passed
  with the v11 metadata ABI fixture. No new compiler warnings were introduced
- Both games passed `test_coop_campaign`, `test_coop_save_format`,
  `test_coop_player_session`, and `test_coop_recovery`
- `test_lan.ps1 -Game d2 -Briefings -BriefingRestore -SkipBuild` and the D1
  equivalent passed with two emulators. Both peers restored six homing missiles,
  resumed gameplay, and reported zero briefing presentations on cold resume.
  The runner now compares campaign mission, active level, generation, entry
  base, returnability, and dormant-world count against the pre-save context
- The real D2 save contained the 40-byte initial campaign archive. Applying the
  recovery fixture preserved those archive bytes and the full payload checksum
- The first D2 run passed launch/save but timed out at launcher startup before
  restore; its log showed a roughly two-minute graphics-context stall. An
  unchanged retry passed. Logs are `temp/coop-campaign-resume-d2.log`,
  `temp/coop-campaign-resume-d1.log`, and
  `temp/coop-campaign-resume-d2-startup-failure.log`

These live tests cover the initialized campaign context with no dormant worlds.
Actual base/secret capture and restoration remain unverified until the travel
transaction and portable-player merge are connected

- [x] Trace local entry, revisit, return, death, save, and network paths
- [x] Verify that the released original D2 source has the restriction
- [x] Document proposed team semantics, state ownership, and failure handling
- [x] Incorporate server toggle, timed yellow warning, and settled-state save/load requirements
- [x] Review competing exits, operation ownership, rewind boundaries, and failure recovery
- [x] Correct normal-first behavior to block only secret entry and preserve individual escape/waiting/countdown behavior
- [ ] Resolve the concrete implementation gates above with focused engine probes/tests
- [x] Add independent server setting beside co-op QoL with session/save propagation
- [ ] Add campaign context and world/player merge using existing serializers
- [ ] Add host-controlled transition request, freeze, timed warning, transfer, commit/abort
- [ ] Hook native D2 entry, return, and secret death/countdown paths
- [x] Fence normal exits and end-level presentation under the same host decision
- [ ] Enable settled-state save/load in either world with atomic campaign bundles
- [ ] Integrate recovery, companion persistence, restart/rewind, rejoin, migration
- [ ] Share phase readiness/status infrastructure with the linked briefing plan while preserving distinct normal-exit, secret-warp, and briefing rules
- [x] Add protocol capability handling and enable the co-op trigger branch
- [ ] Run multi-process/device integration tests and single-player controls

The setting item is closed by the settings audit in
`coop_briefings_and_launch.md`: current UI/defaults, LAN/matchmaking launch,
native synchronization, save metadata and resume propagation were inspected,
with retained co-op tests verifying operation and restoration with QoL disabled

Minimum integration scenarios:

- Server toggle Yes/No, remembered defaults, lobby/client agreement, config
  round trip, host migration, and both general QoL switch values; No must not
  consume entry triggers or trap a team restored inside a secret
- Non-activating client receives the yellow direction/countdown for 5-10
  seconds in both directions; activator and observers agree; simulation and
  reactor timers remain paused; slow loads retain the yellow waiting status;
  duplicate requests do not restart it and stale status packets do not clear it
- Two players with distinct gear/keys/hostages and widely separated positions;
  host and client activation; both arrive with correct gear and usable controls
- Entry -> return -> revisit; consume a pickup and open a door in each mine;
  verify world persistence and no duplication or inventory rollback
- Revisit the same secret from another base; return to the latest base
- Living/dead/dying teammate; dropped gear picked up by another player;
  disconnect/rejoin before and after a swap, including changed roster slots
- Both reactor states, first return during countdown, total timeout deaths,
  last-level advancement, simultaneous normal/secret exits, trigger retries
- Packet loss/duplicates/reordering, stalled participant, failed snapshot or
  load, host departure at each phase, and delayed packets after a revisit
- Save/quit/reload and host migration while in the secret and after returning;
  verify dormant snapshots and destroyed-state history remain consistent
- Save/load immediately after settlement in both directions, all normal/secret
  load combinations, and a cold secret save followed by return/revisit; verify
  gear, consumed pickups, doors, counters, setting, and absent-player records
- Manual save/load requests during warning/loading give a clear busy result;
  autosaves defer; completion and abort re-enable both controls; reject an
  incomplete/corrupt bundle without partially replacing campaign state
- Settled saves with a reactor countdown or death/drop state retain that state
  consistently when supported; unsafe transient states show a specific busy
  reason; save loading rolls back the whole campaign while teleporting preserves
  current portable player state
- Deterministically deliver normal/secret requests in both orders and with
  duplicates/delay, plus a reactor-death request at the cutoff. Assert one
  destination, no losing post-level screen, no premature guidebot removal,
  and exactly-once hostage/score outcomes
- Normal-first: player A reaches the existing waiting screen; player B remains
  controllable, the reactor timer keeps running, and secret entry is refused.
  Test B escaping normally and B dying at countdown expiry. No team warp,
  7-second warning, global freeze, or escape credit for B without escaping
- Normal-first with multiple later normal exits, first exiter disconnect,
  and supported host migration: the normal exits remain usable and secret
  entry stays blocked until level progression completes
- Race save/load/rewind/restart/autosave against exit acceptance from menus,
  controller, client packets, and automation; assert one operation owner and
  no delayed manual action unexpectedly executing after arrival
- Rewind within a secret across reactor destruction; clear history on return
  and revisit; block stale restart; verify no duplicate equipment or mismatch
  between world state and secret availability after rollback
- Kill host during warning, partial transfer, destination application, and
  after commit/release. Assert recoverable lobby fallback or authoritative
  destination resync, no split playable worlds, and preserved last good save
- Fault source capture, bundle validation, destination load, and rollback;
  verify bounded waits, visible reasons, no retry loop, and clean input/status
  after success or abort. Include screen-off client and observer timeout
- Maximum roster in a secret with one authored start and a narrow return point
- Guidebot ownership/route state, stats, recovery, restart and rewind behavior
- Existing `android/tests/test_guidebot_secret_transition.ps1` as a native D2
  single-player control; a separate real co-op runner is required
- Native D1 and D1-in-D2 normal/secret routing controls; incompatible peers
  and non-co-op games must not enter the new teleporter protocol

Expose phase, operation/authority/visit generation, chosen exit/destination,
participant acknowledgement state, action-block reason, and recovery checkpoint
identity through introspection. Use Android-tagged diagnostics for transitions
and reason codes. Build a real two-peer integration runner with controllable
packet ordering and failure injection; source-pattern tests alone cannot prove
these race/recovery properties

Host build/policy tests are being run during implementation. The full feature
is not enabled or verified by those tests; the integration scenarios above
remain required before completion

### Save-transfer race found during v12 validation

The D2 cold-resume run found that autosaves could overwrite the selected slot
while its earlier bytes were being transferred. The host then reopened that
slot, failed its game-ID check, and left the client waiting. The shared sender
now applies its retained transfer bytes from memory; autosaves are blocked
while restore is pending or a transfer/application is active. The live runner
now attempts an autosave during a real restore transfer and requires rejection

The corrected D2 run passed with secret warps enabled and general QoL disabled,
including briefing force-launch, cold save/resume, restored campaign metadata,
and death/partial-pickup recovery. Both host engines' recovery, campaign,
transition, player-session and transfer-policy tests passed, as did Android
and Windows builds. These results cover normal-world campaign persistence and
QoL-independent recovery, not real secret travel

D1 also passed the two-emulator briefing, cold restore, active-transfer autosave
rejection and partial-pickup recovery scenario. Retained test logs are
`temp/coop-restore-race-live-d1.log` and `temp/coop-restore-race-live-d2.log`

### Dormant-world engine restore

`coop_restore_world_from_memory` now distinguishes a dormant-world restore from
full-save rollback. It accepts a raw world snapshot for the current mission,
suspends source recovery records, runs the D2 serializer/network level setup,
and overlays current portable player state onto the restored player objects.
The D2 metadata epilogue uses `coop_recovery_apply_world_pending` and preserves
the current campaign, absent-player records, life/inventory revisions and
session settings. Ordinary save loading retains its existing full rollback

Current equipment, earned keys, score, carried hostages and cumulative player
statistics survive the world swap. Exploration/map-all and per-mine counters
come from the destination. Cloak/invulnerability and weapon cooldowns are rebased
onto the destination clock; omega/afterburner and fusion charge are retained.
Raw capture balances the serializer's pause without releasing the caller's
transition pause. World reconciliation rejects both incomplete ownership
sections and physical recovery pickups without captured provenance

The shared transfer has a distinct WORLD kind and applies the same retained
snapshot bytes on host and clients. The D2 two-emulator
`test_lan.ps1 -WorldRestore -AllowSecretWarps -NoCoopQol -SkipBuild` passed:
capture a live mine, change both players and remove its robots, then restore
the captured world. Both peers recovered the robots and original exploration
while retaining six homing missiles, a blue key, distinct score/hostage values,
and current life/inventory revisions. The campaign generation stayed unchanged.
The capture probe also verified that an outer pause remained active. Log:
`temp/coop-world-restore-live-d2.log`

Android D1/D2 and Windows D1/D2 builds passed. Both host recovery harnesses
passed the additional atomic rejection case for an untracked physical pickup.
The live probe uses a normal-world snapshot with an empty ownership ledger;
it does not yet prove cross-mine clocks, hostage-origin accounting, dormant
drop reconciliation through the serializer, or an actual teleporter journey

The transition coordinator still must own the freeze, canonical player-state
capture, campaign update, safe team placement, readiness/release and rollback.
First entry now has separate multiplayer initialization with the same
portable-state contract, described at the top of this plan. The earlier live
test above covers the snapshot-restoration branch
This restore API leaves failure recovery to that caller. Do not wire a trigger
directly to it or treat its low-level transfer completion as permission to
resume gameplay. Source/destination arbitration and the real secret journey
remain required before the feature is complete

The latest build passed the world-restore probe again with strict ownership
validation and corrected recovery result reporting. The regression runner now
separates initial mine loading from subsequent save application and gives each
its own finite test budget. Native profiling recorded a host texture load near
90 seconds while the client loaded in roughly two seconds and waited; an earlier
90-second script budget stopped that valid in-progress load before completion.
These runner budgets do not alter the 120/20-second briefing policy or engine
transfer timeouts. Latest world-probe log:
`temp/coop-world-restore-verified-live-d2.log`

The same build then passed the ordinary D2 two-peer
`-Briefings -BriefingRestore -AllowSecretWarps -NoCoopQol -TimeoutSeconds 180`
regression. Both peers loaded six saved homing missiles after cold startup,
retained the campaign context and briefing setting, rejected autosaving during
transfer, and opened zero presentations on resume. This verifies that full-save
rollback still uses the ordinary restore mode. Log:
`temp/coop-world-restore-verified-save-d2.log`
