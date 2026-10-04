# Physical-device adversarial multiplayer campaign

Status: selected physical campaign completed at 21:26 UTC; cleanup verified at 21:27 UTC

## Final outcome

The requested active design hour was completed. The unlocked paired campaign
ran from 17:59:40 to 21:26:53 UTC, about 3 hours 27 minutes including diagnosis,
fixing, rebuilding and reruns. Earlier setup, the blocked launch, and standalone
Retroid checks are recorded separately below.

| Result                                                            | Count |
| ----------------------------------------------------------------- | ----: |
| Completed paired attempts, including the earlier blocked baseline |    79 |
| Passing attempts                                                  |    60 |
| Distinct case identifiers with a pass                             |    57 |
| Failed reproductions of the confirmed networking defect           |     4 |
| Harness/environment failed attempts                               |    14 |
| Unresolved startup ANR observation                                |     1 |

One product defect was found and fixed: successful late admission left its
completed join context alive, silently suppressing a later checkpoint
restore's SYNC packets. The shared completion helper is used by D1 and D2.
Physical verification covers all three content modes and both devices hosting.
The detailed cause, before/after evidence and reproduction command follow.

Coverage includes briefing departure/release, delayed admission, repeated
lobby cancellation, partial-transfer cancellation, saved-player gear recovery,
checkpoint restore after return, five interrupted-restore/cold-resume variants,
consecutive client-requested rewinds, level progression after late admission,
ten alternating-path reconnects, host migration and return, Home/resume,
short/long Wi-Fi loss, lobby replacement/readiness/chat, and secret-travel
release loss. The focused final secret-release case passed at 21:26:53 UTC.

The Retroid startup focus ANR did not recur in the unchanged retry or later
launches. Its origin remains unresolved; it is not counted as a confirmed
second product defect. This campaign is two-player LAN coverage, not exhaustive
coverage of the designed combinations, WAN/relay play, larger rosters, or
movie assets. Secure-screen interruption and native-signal cases were not run.

Both Android ARM64 engines built successfully. Scoped formatting, Python
compilation, automation catalog validation (94 standalone JSON tests, 351
support scripts, 185 standalone PowerShell tests), and master catalog validation
(283 top-level entries) passed. The original app installations/data and security
settings were preserved. Both devices ended with Wi-Fi connected at their
original addresses and the diagnostic package stopped.

Machine-readable counts, every attempted case, classifications, and cleanup
state are in `android/temp/device-adversarial-20261003-0040/current-summary.json`,
`triage.json`, and `final-device-state.json`. Each batch retains commands,
continuous device logs, fresh state and native automation results. The reusable
entry point is `android/tests/test_device_network_campaign.ps1`.

Design work resumed at 15:30 UTC after revalidating the worktree and devices.
The earlier active goal interval recorded about 20 minutes of work. Reserve
another 40 minutes of design/harness review through 16:10 UTC; do not count the
intervening unattended wall-clock gap as active design time. No game campaign
results existed before execution started. Launcher-only observations completed
through 16:09 UTC; these are preflight evidence, not gameplay passes.

The execution scope uses normal gameplay automation and Android lifecycle
actions. Security protections remain enabled. No malformed-packet, exploit,
authentication-bypass, or security-control modification tests are included.
Native process suspension/kill cases remain unselected; app force-stop provides
the abrupt-departure coverage needed for this campaign.

## Schedule and scope

- Requested design interval: 2026-10-03 00:40:16 through at least 01:40:16 America/Los_Angeles (07:40:16 through 08:40:16 UTC)
- After design: several hours of actual paired-device execution, failure reduction, and evidence collection
- Baseline source: `8a508ee7`; initially clean working tree
- Retroid Pocket 4 Pro: `JYPR42510121028`, Android API 33, initially 192.168.88.137, release versionCode 23730
- Samsung SM-G996U (S21 family): `R3CR40Q4XPK`, initially 192.168.88.21, release versionCode 23830
- Both original installations reject `run-as`; automation receivers are debug-only
- Use the existing side-by-side diagnostic package option for identical ARM64 debug builds, leaving original app data intact
- Do not use the unrelated running emulators as substitutes for the requested hardware
- Evidence directory: `android/temp/device-adversarial-20261003-0040`
- Do not edit `android/outstanding_bugs.md`, which explicitly prohibits edits

## Design process

1. Inventory existing automation, state observability, deployment constraints, and prior regression coverage
2. Map launcher and native session state machines, including host migration and authority changes
3. Define state-based triggers, observable success/failure criteria, timeout boundaries, and cleanup for each scenario
4. Prepare reproducible runners and validate their syntax/selection without running campaign cases before the design interval ends
5. Review cases against both device roles and D1, D2, and D1-in-D2 content
6. Freeze an execution manifest with deterministic case identifiers and delay seeds

## Execution principles

- Distinguish graceful departure, app force-stop, native-process death, backgrounding, screen-off, and Wi-Fi loss; they exercise different paths
- Target all ADB commands by serial and package; record exact APK hash and installed version
- Stage owned game assets into the diagnostic package only
- Use test pilots; retain session and automation artifacts before starting another case
- Capture per-device logcat continuously within a case; archive before any helper clears logcat
- Require fresh introspection, observed target phase, and advancing network/frame counters, not merely a non-null JSON file
- A test that misses its trigger is inconclusive, not passed
- A process alive without a fresh snapshot is suspect, not proof of liveness
- Record failures separately as product defects, harness defects, environment failures, or inconclusive timing attempts
- Reproduce candidate product defects at least twice, varying role or timing when useful
- Keep original installation, personal saves, game data, network configuration, and other devices intact
- Restore Wi-Fi after fault injection in a finally block and verify reconnection before subsequent tests

## Initial risk matrix

| Family              | Target phases and perturbations                                                     | Required observations                                                                                     |
| ------------------- | ----------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------- |
| Baseline            | Both host directions, D1/D2/D1-in-D2                                                | Two authenticated players, same signed level and mission, unpaused play, bidirectional PDATA advances     |
| Host departure      | Gameplay, automap, menus, scores, briefing prepare/read/release                     | Bounded migration or explicit clean failure; no immortal wait; rejoin reaches authoritative world         |
| Client churn        | Graceful leave, force-stop, repeated same-pilot reconnect                           | No ghost slots, duplicated objects, stale ready masks, or permanent admission refusal                     |
| Initial join races  | Host changes phase during info, approval, partial mine transfer                     | Pending client follows current destination; source-world transfer cannot commit late                      |
| Briefing loss       | One participant leaves/dies/backgrounds; new arrival reads; host shortens countdown | Deadline does not reset, pending player excluded from ready roster, all survivors eventually play or exit |
| Level transitions   | Reactor countdown, staggered exit, flyout, scores, destination briefing             | Exactly one destination commit; no lost controls, frozen overlay, duplicated bonuses, or wrong level      |
| Secret travel       | Entry/return/revisit, host or client lost at transaction phases                     | Correct negative level/world visit, ownership, gear and released-world network traffic                    |
| Restore/rewind      | Participant loss during prepare/freeze/load/release; joining during restore         | Rollback or bounded success, no permanent pause, stale transfer cannot overwrite newer world              |
| Connectivity        | Short and timeout-crossing Wi-Fi outages on each peer                               | Explicit timeout/migration; reconnect succeeds; no split-brain authority                                  |
| Android lifecycle   | Home/resume, screen-off, background host, repeated activity resume                  | Network policy remains coherent; controls and presentation recover without deadlock                       |
| Session replacement | Kill host then create new lobby/session at same address with same pilot             | Old session packets/state rejected; client cleanly joins new session                                      |
| Secondary stress    | Graphics confirmation during networking, guidebot ownership, death/spew recovery    | Simulation/network pumping continues; shared ownership and item state converge                            |

## Existing coverage to reuse carefully

`android/tests/test_lan.ps1` already has first-join and rejoin briefings, partial-transfer transitions, host migration, restore participant-loss, secret-travel disconnect phases, and graphics scenarios. Its setup resets configuration and hardcodes the normal package, so it must not be pointed at personal installations as-is. Shared helpers already honor `DXX_TEST_PACKAGE` but the LAN runner overrides it.

The earlier transition study reports emulator passes, including D1/D2 partial transfer, rather than coverage of physical-device Wi-Fi/lifecycle timing. New cases should expand those boundaries, not simply relabel old passes.

## Results

### Confirmed: retained checkpoint restore after a late join

At 19:30 UTC, `saved-recovery-retroid/01-d2-saved-status-return` failed after
successful saved-session admission and a further same-host return. Both peers
had advancing PDATA, working controls, the expected recovered inventory, and
no remaining recovery objects before the checkpoint restore.

The normal waiting/complete status broadcast passed. The retained checkpoint
then transferred and began loading on both peers. The host repeatedly retried
SYNC, timed out the client, and displayed `Co-op restore interrupted`; the
client remained waiting for the host in restore loading. The retained file
was the prior level 5 checkpoint, so this also exercised a cross-level restore
from the current level 1. Preserve this precondition in reproductions.

The Samsung-host reproduction failed too, restoring level 1 back to level 1;
cross-level travel is therefore not required. A third reproduction on the
diagnostic build confirmed the cause: `join payload suppressed: type=10
player=1 visit=2 current=3 committed=1 verify=-1`. The completed late join's
`UDP_sync_player.join_attempt` survives, and `net_udp_send_sync_payload` applies
its old world-visit check to later restore SYNC packets, returning without
transmitting them.

The fix retires the completed join context at the existing gameplay
confirmation boundary, through a shared helper called by both engines. It
preserves the join envelope while initial/retried admission is still pending.
The ARM64 build passed. At 19:57 UTC the physical regression passed for D1,
D2, and D1-in-D2 with the Retroid hosting; logs show transfer retirement and
no suppressed SYNC. Samsung-host validation is running. The regression now
also requires settled barriers, a common level, advancing PDATA, and working
controls after the checkpoint restore.

Build hashes are archived in the evidence root. The original diagnostic APK
is `0f8bfb39c617e893c509e363e421abad6f270c1c671e2888a63a5b66c34ea922`,
the logging-only APK is
`741341bcacac799264a1a58b432c3bdd0ffa1dc13041d87319961d16288535dc`,
and the fixed APK is
`bc6b12d9468784b13173f287214a1c6a2d30f99b0d036910a835ac1cd670f394`.
Samsung-host repeated return plus checkpoint restore passed at 20:01 UTC.
The same-role partial-transfer cancellation and later return passed at 20:04.

Two additional fixture assumptions surfaced during combined scenarios:

- The first client-requested rewind after a late join cannot compare its old
  local clock directly with the host's clock. Both peers restored target
  `3964863`, but the client began about 12 seconds behind the host. Keep the
  host's backwards-time check and both peers' second-round check, and require
  identical, decreasing authoritative restore targets on both peers
- `RestoreLossResume` selected shared slot 0 even though its manual file was
  `lanhost.mg0`, while launcher slot selection resolves `coopsave.mg0`. Capture
  the shared autosave slot at the save phase and verify its hash is unchanged
  before selecting it for the cold resume

Neither mismatch is counted as a second product defect. Their corrected cases
use APK
`e63d95e5f8987898d64e530a68e1e389255785993461dff8e06f75ae19867ece`,
which adds only the revised automation clock assertion to the prior game fix.
The combined saved-session/client-rewind case passed with Samsung hosting at
20:31:58 UTC (`cold-recovery-v2-samsung`). Both authoritative targets matched
across peers and decreased on the second rewind. An earlier successful pair
of rewinds was reported as failed because Android's rotating log buffer no
longer contained both targets; the runner now reads its continuous host-side
capture instead. Cold recovery also incorrectly required the briefing-only
suppression flag when briefings were disabled; presentation count must stay
zero, while that flag is required only when the feature is enabled.

Samsung-host cold recovery passed after client loss (20:34 UTC), host loss
(20:37), a stalled SYNC (20:40), and client load failure (20:42). Each verifies
the original shared autosave hash before relaunch, both players' inventory,
reactor countdown and advancing simulation clocks, a settled restore barrier,
and renewed bidirectional PDATA. The loader-failure fixture additionally starts
a fresh game in the failed loader's existing process before the cold recovery.
Host load failure followed by cold recovery passed at 20:44 UTC as well.

The D2 ten-reconnect case passed at 20:50 UTC with Retroid hosting. It alternates
launcher IP discovery and direct engine joins, uses restart delays from 0.25
to 16 seconds, and verifies traffic, distinct pilot membership and actual ship
rotation after every return. These delays exclude activity startup latency;
command timestamps and fresh state captures record the observed recovery.
The first Home/resume case was a harness failure: ADB cannot directly launch
the non-exported MainActivity. The corrected runner follows the existing
repository helper's normal launcher intent and Back sequence. It does not
change the manifest, permissions, or Android security configuration.
The corrected host Home/resume passed at 20:54 UTC; short host Wi-Fi loss
passed at 20:55 with a measured 6.469-second outage through reassociation.
The first custom former-host return used the original engine port 42424,
but the migrated host correctly advertised its proxy on 42425. The runner
now resolves the migrated endpoint through normal launcher IP discovery,
preserving the returning pilot's callsign. This is a harness correction,
not evidence of a second product defect.
The corrected former-host return passed at 21:00 UTC. Client Home/resume,
a measured 50.563-second Wi-Fi outage followed by rejoin, and a 35-second
force-stop/rejoin passed by 21:05. Lobby host replacement, client Home/resume,
and client Wi-Fi recovery passed by 21:07, including readiness changes and
chat delivery in both directions. All these cases used Retroid as the original
host. The final transition batch reverses the physical roles again.

### Unresolved observation: Retroid startup focus ANR

At 21:08 UTC, the first `transitions-samsung/01-d1-saved-level-transition`
attempt never launched its client engine. Android reported a no-focused-window
ANR in its home launcher, then in diagnostic SetupActivity (PID 4431), which
it killed at 21:08:36. The app still handled preference and launch broadcasts
before that kill. This is not a demonstrated multiplayer failure or a confirmed
second product defect. Continuous logs, process exit reason 6, and
`retroid-lastanr.txt` preserve the observation. The exact unchanged retry
started normally at 21:11 UTC. No speculative code or OS-setting change was
made for this observation; retain it for follow-up if it recurs.
The unchanged D1 saved-session/level-transition case passed at 21:14 UTC.
Samsung-host briefing Home/resume and short Wi-Fi loss passed next, preserving
the original generation, participant set and deadline before entering play.
D1-in-D2 saved-session admission followed by two client-requested rewinds
passed at 21:19 UTC with identical, successively earlier authoritative targets
on both peers.
D1-in-D2 saved-session/level-transition passed at 21:22 UTC, including Android
Back/menu behavior and flight controls in level 2. The first secret-release
disconnect host check passed, but the manifest's redundant `SecretWorld` flag
then invoked a separate two-player traversal after the client was gone. Remove
that flag from the disconnect family and rerun the focused scenario.

Reproduce the focused physical regression after provisioning the diagnostic
package and owned data on two normally unlocked devices:

```powershell
.\android\tests\test_device_network_campaign.ps1 -HostSerial JYPR42510121028 -ClientSerial R3CR40Q4XPK -OutputDirectory android/temp/device-network-repro -CasePatterns '*saved-checkpoint-first-return' -StopOnFailure
```

The wrapper is registered as explicit-only in the master catalog; selecting
ordinary automated suites does not take control of physical devices. Use
`-List` to inspect selected cases without touching devices, and `-Reverse` to
swap their roles. Cases stop on failures when `-StopOnFailure` is selected.

Separate fixture issue: `test_coop_restore_status_host.jsonc` used
`post_delay_ms` on `set_debug`, which does not support that delay. Both status
changes occurred about 14 ms apart. An explicit `wait_ms` fixed this stage.
The physical campaign selects `RestoreStatusFunctionalOnly` to omit the
existing packet-authentication replay subtest from its functional scope.

### Priority refinement: restore failure followed by admission failure

The user requested nearby variants of the already-fixed October 2 incident,
rather than spending the campaign repeating its exact regression. The incident
is documented in `../networking/midlevel_join_regression_20261002.md`: a cancelled
lobby join left a connected slot outside the live roster, and recovery waited
for that nonexistent participant while later admission remained stuck.

New physical variants use the existing saved-gear fixture and change:

- Three cancelled lobby admissions before the solo restore, then two returns
  to the same host process
- A level 5 save followed by two returns, instead of the fixture's level 1
- A returning saved client followed by restore-status completion and checkpoint
  handling
- A cancelled partial saved-world transfer, then another admission and a later
  return without restarting the host (existing bounded transfer-delay fixture)
- Client loss, host loss, sync deadline, client load failure, and host load
  failure, each followed by a cold resume from the preserved save
- Reverse physical host/client roles after the Retroid-host pass

Require one approval at most per new return, a settled recovery/save barrier,
two connected peers, advancing PDATA in both directions, and working controls.
Archive fresh state after each return and verify the host PID did not change.
These are app-level gameplay and lifecycle cases; no security settings change.

The delayed first-join cases passed on all three content modes at 19:08 UTC
using the explicit original-deadline oracle. The first new saved-session
variant started at 19:13 UTC.

### Unlocked paired run

Checkpoint at 18:56 UTC: 27 passed attempts covering 26 distinct paired cases.
All planned D1 core cases passed. D2 briefing loss/release/rejoin and first-join
loss/cancellation passed; remaining D2 and D1-in-D2 cases are still running.
No confirmed product defect has been found at this checkpoint.

At 17:59 UTC the user unlocked Samsung and the paired campaign resumed.
All three content baselines passed, followed by seven D1 cases: host/client
briefing loss, host/client final-release loss, briefing rejoin, first-join host
loss, and first-join cancellation (`baselines-unlocked`, `core-retroid-host`).

The first 105-second late-arrival attempt failed because Samsung idled to sleep
while waiting in the launcher. Its log records screen timeout at 18:11:38 UTC,
then Android's `TOP_SLEEPING` activity-launch rejection at 18:11:54 UTC. The host
subsequently reached its ordinary 120-second deadline, tripping the fixture's
active-briefing assertion. This is environment/harness failure, not evidence
of a game-protocol bug. The timed wait now sends ordinary wake input every
15 seconds on a physical joiner; no lock or security settings are changed.
The same 105-second scenario passed twice in `late-arrival-retroid-host`.

A later Retroid precondition failure showed that WAKEUP does not refresh the
idle timer when its display is already on. Verified through Android power
state, ordinary Shift input does refresh it. Case preparation now wakes,
refreshes user activity, and verifies the lock screen has dismissed; the
long launcher wait uses Shift as well. The restore-stall scenario then passed.

The D1 level-transition fixture reached level 2 but assumed touch controls were
enabled. Retroid defaults to controller controls, so its attached overlay was
correctly inactive. This touch-recovery fixture now explicitly enables touch
controls before launch and also checks flight input after the transition.
The full fixture passed afterward. UI introspection is now archived per case.

D2's initial briefing-loss invocation was rejected before launch because the
runner passed its base mission as a custom mission argument. The runner now
omits redundant base-mission arguments. Suite validation accepts the bundled
First Strike and Counterstrike campaigns, and restore evidence uses the
selected campaign's save directory. These changes retain the existing failure
and recovery assertions; the cross-engine cases still need execution proof.

### Earlier blocked attempt and supplemental checks

Paired execution began at 17:38 UTC. The first D1 baseline timed out because
Samsung's secure lock screen prevented its game activity from launching.
`baselines/01-d1-baseline/result.json` preserves the raw runner failure;
classification is environment-blocked, not a multiplayer product defect.
Android logged `TOP_SLEEPING` and blocked the background activity launch;
window policy reported `showing=true`, `secure=true`. The user was asked to
unlock normally. The Python runner now reports this condition explicitly and
ends the batch. Security settings were not changed.

While waiting for the unlock, the Retroid D2 standalone launch/automap/lifecycle
fixture passed all 95 steps, including main-menu, active-game, pause-window,
and game-menu background/resume checks (`solo-d2-retry`). This is supplemental
coverage, not paired networking coverage. The earlier `solo-d2` attempt lacked
the host-side background marker handler and is a harness failure; the retry
uses the repository's `Watch-AutomationResult` driver. Its initial nonsecure
lock-screen wake also needed an additional ordinary wake/dismiss cycle before
the engine started. The scratch runner now waits after waking.

Retroid D1 passed all 62 launch/automap/background-resume steps on the first
properly driven attempt (`solo-d1-retry`, 17:49 UTC). D2's 95-step pass finished
at 17:47 UTC. No confirmed product defect has been established. The requested
multi-hour paired execution phase was incomplete at that checkpoint; the user
unlocked Samsung at 17:59 UTC and execution resumed.

Installed diagnostic APK SHA-256:
`0f8bfb39c617e893c509e363e421abad6f270c1c671e2888a63a5b66c34ea922`.

Resume with a new evidence directory (do not overwrite the blocked attempt):

```powershell
python android/tests/run_device_network_campaign.py --host JYPR42510121028 --client R3CR40Q4XPK --output android/temp/device-adversarial-20261003-0040/baselines-unlocked --case '*baseline' --stop-on-failure
```

Follow successful baselines with briefing loss/rejoin, first-join cancellation,
level/transfer/scores transitions, restore participant loss, and host migration.
Then run Home/resume, app force-stop/rejoin, and Wi-Fi outage cases in each role.
Omit Samsung screen-off cases from unattended batches because they require a
normal user unlock. Native process signal cases remain unselected.

Scoped mixed-language formatting, four multiplayer source-contract tests,
Python compilation, automation catalog validation, and master catalog tests
passed. Preflight observations are not counted as adversarial gameplay passes.

## Design review findings

- The existing LAN runner can accept historical SYNC log lines when neither
  introspection endpoint responds. Restrict that emulator fallback to actual
  emulator serials; physical cases require fresh snapshots
- Physical fixtures use `com.dxxredux.app.nsdtest`, built with the existing
  `nsdDiagnosticApp` Gradle property. The Samsung's earlier diagnostic data was
  archived before updating; both original release installations remain intact
- `UDP_TIMEOUT` is 15 seconds in both engines, distinct from the 30-second
  briefing and join-wait deadlines. Lobby directed-contact loss is 10 seconds,
  followed by 120 seconds of reconnect grace
- Ten-minute preflight ICMP samples: Retroid to Samsung 596/600 received, five
  duplicates, mean 131.905 ms, max 661.878 ms; reverse 597/600, four duplicates,
  mean 152.408 ms, max 1023.997 ms. The Android ping summary rounds loss to 0%,
  so preserve the raw counts. Screen-off Wi-Fi power saving may contribute;
  this is not evidence of a game networking defect
- Native `SIGSTOP`/`SIGCONT` isolates engine liveness from launcher networking;
  whole-package force-stop and native-only kill are separate cases
- A long network partition is allowed to end a session. Verify departure and
  then an explicit rejoin instead of demanding impossible transparent healing
- Mid-game admission requires host approval. Custom reconnect checks must
  exercise F6 approval when prompted, including after host migration
- Both endpoints can report network state while input remains broken. New
  control probes require consumed heading input and actual ship rotation
- Churn uses ten reconnect cycles, exceeding the eight native player slots,
  and deliberately mixes returns before and after the 15-second timeout
- Launcher introspection now includes LAN membership, readiness, directed
  traffic counts, discovery, and chat state for bidirectional liveness checks

## Evidence and outcome rules

The reusable Python runner writes a manifest, exact commands, per-case UTC and
monotonic timing, continuous logcat, fresh snapshots, durable automation
results, and Android process-exit information. Its `--list` mode exposes the
currently designed cases without touching a device. A successful runner exit
is provisional until the expected phase and relevant liveness assertions are
present in its evidence. A missing phase, inaccessible device, stale build,
secure lock screen, or harness exception must be distinguished from a product
failure when reviewing results.

Start with the three content baselines, then briefing/transition/restore cases,
then the physical lifecycle and lobby families. Reverse the physical roles for
at least the baseline, host-loss, transition, and recovery families. Investigate
reproducible defects as they appear instead of allowing repeated identical
failures to consume the campaign.
