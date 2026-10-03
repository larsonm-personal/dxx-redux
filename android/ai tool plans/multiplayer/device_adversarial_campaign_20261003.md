# Physical-device adversarial multiplayer campaign

Status: paired execution resumed at 17:59 UTC after user unlocked Samsung

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

### Unlocked paired run

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
The same 105-second scenario is being repeated twice in
`late-arrival-retroid-host` before proceeding with remaining scenarios.

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
multi-hour paired execution phase remains incomplete pending Samsung unlock.

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
