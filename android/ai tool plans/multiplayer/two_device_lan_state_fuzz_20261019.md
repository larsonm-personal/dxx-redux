# Two-device LAN state fuzzing plan

Date: 2026-10-19

## Conclusion

Build this as a seeded, state-aware sequence fuzzer on top of the existing
physical-device campaign, not as a second collection of fixed end-to-end tests.
The repository already has unusually broad deterministic coverage: the current
campaign catalog contains 287 cases and already exercises the motivating flow
of host/client departure followed by Join In-Game at several delays.

The useful new work is to compose those operations in longer same-process
sequences, choose actions from the state actually observed on both devices,
jitter actions around protocol boundaries, and save a replayable trace that can
be minimized when an invariant fails.

Current ADB inventory during this review:

- Physical: `JYPR42510121028` (Retroid Pocket 4 Pro, Android 13,
  `192.168.88.137`)
- Physical: `R3CR40Q4XPK` (Samsung Galaxy S21, Android 15,
  `192.168.88.21`)
- Emulators: `emulator-5554`, `emulator-5556`

Both physical devices are unlocked, reachable on the same `/24`, and now run
the same diagnostic APK: version code 27090, SHA-256
`db93712e0db4fb44fa3961197f82b71abc68b0e760cea2971b40168959b3a5af`.
The normal `com.dxxredux.app` package and its data were not modified.

## Existing foundation to reuse

Use these rather than creating another device-control framework:

- `android/tests/test_device_network_campaign.ps1`: explicit physical-device
  wrapper using the isolated `com.dxxredux.app.nsdtest` package
- `android/tests/run_device_network_campaign.py`: serial-targeted device
  control, fresh introspection, continuous logcat, state snapshots, fault
  injection, result manifests, cleanup, and 287 deterministic cases
- `android/tests/test_lan.ps1`: deep two-engine scenarios for saves, restores,
  rewinds, travel, briefings, host migration, doors, pickups, death, endgame,
  and join transfer boundaries
- `android/app/src/main/cpp/shared/game_introspect.cpp`: the authoritative
  native state oracle
- `android/helpers/mod_fuzz.py`: a good local precedent for deterministic seed
  generation, replay artifacts, and failure minimization

Existing cases already cover much of the proposed basic matrix:

- Host and client leave, force-stop, native kill, Home, screen-off, Wi-Fi loss,
  and process suspension at several durations
- Same-player and renamed-player returns, alternating direct and launcher join
  paths, ten-cycle client churn, and repeated host swaps
- Join cancellation, approval expiry, partial transfer cancellation, late join,
  briefing join, loading/backgrounding, and unavailable-host recovery
- Saved-session return, restore failure and resume, rewind, level transition,
  secret-travel disconnect phases, death/autosave, and nested UI survival
- Lobby identity replacement, launch cancellation, role turnover, service
  recovery, stale-advertisement checks, and real background deadlines

Do not spend a hardware campaign merely rerunning all 287 cases. Use a small
known-good subset as the preflight, then spend the device time on composed and
boundary-jittered sequences.

## Why live-device fuzzing is still valuable

The deterministic suite is strong at named regressions but comparatively weak
at operations whose cleanup is only tested by a later unrelated operation.
The previous physical campaigns found exactly that class of defect: a completed
join poisoned a later restore, a former client retained the wrong socket bind
policy when becoming host, an old window action affected a replacement game,
and a rejected autosave leaked a pause.

The central fuzzing heuristic should therefore be:

> Perform a risky operation, keep both app processes alive when possible, then
> require a different valid operation to succeed in the same processes.

A clean error dialog or a return to a menu is not enough. Recovery must end in
playable network state with fresh traffic and controls.

## State model

The generator should track a compact model derived from introspection rather
than blindly issuing UI actions.

### Launcher/session state

- idle, scanning, hosting, joined
- launch preparing, launch pending, native game attached
- lobby identity, callsign, ready state, service lease, advertised endpoint

### Native network state

- host waiting, initial join, approval pending, object transfer, committed join
- waiting for first normal player traffic, stable gameplay
- sole surviving host, migrated host, deferred/rejected join
- network status, player slots, authority, world visit, level, mission

### World operation state

- ordinary play, briefing, menu, nested menu, automap, death/respawn
- save capture, save transfer, restore barrier, rewind
- reactor countdown, score screen, normal travel, secret travel, endgame

### Android/device state

- foreground, Home/background, screen off, stopped native process
- whole-package force-stop, Wi-Fi absent, Wi-Fi reassociating, route healthy
- launcher process identity and native process identity

Each action declares preconditions, expected state changes, a timeout, cleanup,
and whether process replacement is allowed. Invalid actions are skipped and
recorded, not treated as test passes.

## Action grammar

Start with ordinary user-visible and lifecycle actions:

- Create/stop LAN lobby, scan, join by discovery, join by IP, leave, ready,
  start, cancel launch, approve or ignore Join In-Game
- Start fresh, start from save, save, restore, rewind, restart level
- Enter/exit game menu, Options, save/load picker, editable text, automap,
  briefing, and launcher
- Move/fire/drop/collect/kill/respawn so world state changes between joins
- Trigger normal exit, secret exit, reactor countdown, score screen, and level
  transition through existing debug automation hooks
- Home, wake, screen-off/on, short Wi-Fi loss, timeout-crossing Wi-Fi loss,
  native-only kill, whole-package force-stop, and graceful Leave
- Swap host roles after migration; immediately create a replacement session

Do not begin with malformed packets or security/authentication fuzzing. That is
a different campaign with different safety and correctness criteria. Do not use
root or change device security settings.

## Highest-value deterministic seeds

These should be implemented before free sequence generation because they cover
known narrow gaps and provide useful reducer fixtures.

### P0: join completion crossed with a new world operation

1. Host and client enter play
2. Client leaves or is force-stopped
3. Client starts Join In-Game
4. At each observed boundary, trigger one host action:
   - approval pending
   - object transfer active
   - transfer committed but before first normal PDATA confirmation
   - immediately after first PDATA confirmation
5. Host action variants: restore, rewind, normal level exit, secret entry/return,
   and reactor completion
6. Client retries if the first join is correctly rejected or superseded

Require both peers to agree on authority, signed level, world visit, player
roster, and restore/travel barrier state. Require bidirectional PDATA and a real
control response after settling.

This is the most important addition. Earlier planning identified the
commit-before-confirmation interval, but broad execution of its travel/secret
variants was not established.

### P0: host departure around initial launch

Sweep the host departure across observed phases rather than fixed wall-clock
delays:

- Waiting room before Start
- Launch preparation after the client receives the launch event
- Host native engine started, client not started
- Both engines in network setup
- First frame of gameplay
- 0.25, 1, 3, 8, 16, and 35 seconds into gameplay

Use graceful Leave, native kill, whole-package force-stop, Home, and Wi-Fi loss
as distinct faults. Let the surviving client migrate or fail explicitly, then
have the original host use Join In-Game. Finish with a save/restore or level
transition to expose stale authority and join state.

### P0: restore and admission collision

- Begin a co-op restore, then have one participant depart during capture,
  transfer, local load, barrier wait, and release
- While recovery is visible, attempt Join In-Game with the same pilot and then
  with a different pilot
- Return to a native menu without restarting the surviving process, host or
  join a fresh session, and perform a successful second save/restore

Require no leaked pause, stale dialog, old transfer, ghost player, or retained
busy flag. Hash the retained save before and after failed attempts.

### P0: repeated dirty-world rejoin

Run 10 to 30 cycles without restarting the host. Between returns, mutate one or
two pieces of authoritative state:

- Pickups and dropped inventory
- Open, closing, locked, and blasted doors
- Robot deaths and ownership changes
- Guidebot release/ownership/docking
- Player death, spew, respawn, and a pickup race
- Reactor state and countdown

After every return, compare compact world signatures and conservation rules,
not only player count. This targets duplication, missing rewards, stale object
ownership, and transfer growth.

### P1: migrated authority chain

- Migrate host, admit the old host, change level, save/restore, migrate again,
  admit the other peer, then abort the final game
- Alternate discovery and direct-IP return paths
- Verify each advertisement contains the current authority's address and port
- Verify the final advertisement retires without restarting the survivor

### P1: old callback versus replacement session

- End session A through timeout, aborted launch, failed join, restore failure,
  or background expiry
- During the cleanup/grace window, immediately create or join session B in the
  same launcher/native process
- Hold session B long enough for every session A timeout to fire

Require B's lobby ID, service lease, runtime registration, and native process
to survive. This directly targets async work that outlives its owner.

### P1: nested UI plus forced transition

Repeat host/client loss and forced game exit while each of these is active:

- Game menu
- Options submenu
- Save picker, load picker, and save-name editing
- Automap
- Briefing and score/endgame presentation

After recovery, verify front-window ownership, pause depth, input capture,
controller/touch response, and that a replacement game reaches actual gameplay.

### P1: identity and slot pressure

- Alternate same callsign, case-only changes, new callsigns, and the original
  callsign across more cycles than the eight native slots
- Mix returns before and after the native timeout
- Include one cancelled approval and one partial transfer before a valid return

Require exactly two connected unique players, stable host identity, bounded
slot count, correct recovered inventory owner, and no extra approval prompts.

## Seeded sequence generation

After the deterministic seeds pass, generate sequences of 8 to 30 actions.
Use a reproducible PRNG and record both the seed and the fully materialized
action list. Suggested weighting:

- 35 percent session/join/leave actions
- 25 percent world mutations and save/restore/rewind/travel actions
- 20 percent Android lifecycle or connectivity actions
- 10 percent UI nesting actions
- 10 percent role/identity/lobby replacement actions

Bias toward transitions that have not appeared in the current trace. Insert a
checkpoint oracle after every disruptive action and a stronger playable oracle
after every recovery. Reset the app only between independent seeds, never as an
unreported recovery mechanism within a seed.

Use three timing modes:

- `edge`: act immediately when the target state first appears
- `jitter`: wait a seeded 0 to 2 seconds after the observed boundary
- `deadline`: act just before, at, and just after a real protocol deadline

Avoid arbitrary sleeps where introspection exposes the desired phase.

## Oracles

Every accepted action should check the relevant subset of these invariants:

- No Java/native crash, ANR, tombstone, or unexpected process replacement
- At most one authoritative host; both peers agree on master player and world
  visit once a transition settles
- Connected player count and roster are equal on both peers; no duplicate
  callsign/client identity and no growth beyond configured capacity
- Mission and signed level agree; restore/travel/join barriers either advance
  or fail within their documented bounds
- Bidirectional PDATA counters advance after recovery
- A consumed movement probe changes actual ship orientation
- Pause ownership/depth, input permission, and front-window state return to a
  usable configuration
- Save/checkpoint hashes and formats remain valid after cancellation/failure
- Inventory, pickup rewards, keys, score/kills, doors, robots, guidebot, and
  reactor state obey scenario-specific conservation rules
- The advertised LAN endpoint and status describe the current host/session;
  retired sessions disappear
- Memory, file count, player-slot count, pending callbacks, and transfer/retry
  counters remain bounded across long churn runs

For eventual-consistency checks, use a bounded settle interval and save every
intermediate disagreement. A temporary disagreement is useful timing evidence;
an agreement observed only after a process reset is a failure.

## Observability additions to consider first

The current introspection is already broad. Add only fields needed to trigger
or distinguish narrow boundaries:

- Join attempt/generation ID and explicit phases for requested, approved,
  transferring, committed, and confirmed-by-normal-traffic
- Current transfer generation, source visit, destination visit, and owning
  player slot
- Last accepted normal PDATA time/counter per connected peer
- Native session identity and authority generation, paired with launcher lobby
  and runtime registration identity
- Count and generation of pending delayed shutdown/cleanup callbacks
- Compact deterministic world signature split into player/inventory, objects,
  walls/triggers, robots, and campaign state so a mismatch has a useful class
- Current window stack or at least front window type plus pause/input owner

Keep game-format and simulation knowledge native. The Python harness should
compare reported state, not reconstruct save formats or engine object rules.

## Evidence and replay format

Each seed directory should contain:

- Manifest: git revision, APK hash/version, device model/API/serial, IP/route,
  game/content mode, seed, generator version, and exact action list
- Continuous per-device logcat from before setup through cleanup
- Before/after snapshots for every action and higher-frequency snapshots around
  the triggering boundary
- Actual requested and measured fault durations
- Process IDs, Android exit-info, battery/power/network/window/service dumps
- Durable automation results and any save/checkpoint hashes used by the oracle
- Final classification: pass, product failure, harness failure, environment
  blocked, or inconclusive/missed trigger

On failure, publish the generated case JSON before attempting reproduction.
Never overwrite the first raw failure with a retry.

## Failure reduction

Provide `--replay <case.json>` from the first version of the fuzzer. Reduction
should proceed in this order:

1. Replay the exact trace twice without changing timing
2. Remove action blocks while preserving state preconditions
3. Replace world mutations with no-ops one family at a time
4. Shorten or widen only the delay nearest the observed failure boundary
5. Reduce content mode, level, role, and hardware only after preserving the
   failure on the original pair

A reduced sequence is supplementary evidence. Preserve the full original seed,
because cleanup bugs often need the apparently unrelated earlier actions.

## Proposed implementation

1. Refactor only the minimal reusable pieces of
   `run_device_network_campaign.py` needed by the fuzzer: `Device`, snapshot and
   evidence collection, pair liveness checks, timed faults, and cleanup
2. Add `android/tests/run_device_network_fuzz.py` with:
   - `--host`, `--client`, `--output`
   - `--seed`, `--iterations`, `--max-actions`
   - `--game d1|d2|d1d2|mixed`
   - `--reverse`, `--stop-on-failure`, and `--replay`
3. Add an explicit-only `android/tests/test_device_network_fuzz.ps1` wrapper
   that rejects identical serials and defaults to the diagnostic package
4. Add host-only generator/replay/reducer tests with fake devices before live
   execution
5. Register the top-level test in `test_suite_coverage.ps1`, assign a realistic
   master timeout, and keep it out of ordinary unattended suites
6. Add narrow native introspection only for boundaries the initial deterministic
   seeds cannot reliably observe

The fuzzer should import/reuse shared campaign code or move a small common
module out of it. It should not shell out to the fixed campaign once per action,
because that would lose continuous process/session ownership.

## Execution profiles

### Preflight, approximately 20 to 40 minutes

- D1, D2, and D1-in-D2 baseline
- One host leave/rejoin and one client force-stop/rejoin
- Saved late join followed by checkpoint restore
- One repeated host swap
- Verify APK identity, fresh introspection, traffic, controls, evidence writes,
  cleanup, and Wi-Fi restoration

### Focused boundary session, approximately 2 to 3 hours

- P0 join/world-operation crossings
- P0 host departure launch sweep
- P0 restore/admission collision
- Run each initial failure twice and reverse hardware for a successful repro

### Overnight soak

- 20 to 50 seeds per content mode, 12 to 30 actions each
- Rotate original host and physical device roles
- Cap consecutive disruptive Wi-Fi/screen actions to avoid spending most of the
  run on environment recovery
- Stop on repeated crash/ANR or device-unavailable conditions; preserve partial
  results and restore Wi-Fi/package state in `finally`

## Live-device-only ideas beyond the emulator suite

Once two physical devices are connected, emphasize differences emulators do
not represent well:

- Screen-off Wi-Fi power saving, Doze, vendor background restrictions, and
  foreground-service restart eligibility
- Wi-Fi reassociation with a changed DHCP address, access-point roam, or brief
  route availability before application sockets recover
- Phone/controller suspend-resume, Bluetooth controller disconnect/reconnect,
  audio-focus interruptions, orientation changes, and activity recreation
- Asymmetric slow hardware: fast host/slow client and reversed roles during
  transfer, loading, briefing, and first gameplay confirmation
- Thermal/load soak while save capture, texture loading, or level transition
  competes with network deadlines
- Low-memory process reclamation initiated through ordinary Android conditions,
  kept separate from explicit force-stop and native kill results

These should be layered onto the same state/action/oracle model so failures are
replayable and comparable with emulator controls.

## Recommended first run when two physical devices are available

1. Verify both are unlocked, on the same LAN, and running the same diagnostic
   APK; preserve ordinary installations and personal data
2. Run the preflight in both host directions
3. Run P0 join/world-operation crossings in D2, then D1, then D1-in-D2
4. Run the host-departure launch sweep in both host directions
5. Run 10 short seeded traces per content mode
6. Review evidence before extending the soak; fix harness ambiguity before
   spending more device time

The immediate implementation target should be replayable deterministic boundary
seeds plus the action trace format. Random generation should come only after
those primitives and oracles prove they can distinguish a product failure from
a missed timing window or Android/environment failure.

## Execution log

### 2026-10-19 physical preflight

Evidence root: `android/temp/device-lan-fuzz-20261019`

- RP4P host, S21 client: D1, D2, and D1-in-D2 baselines all passed
- S21 host, RP4P client: D1, D2, and D1-in-D2 baselines all passed
- Both directions reached two-player gameplay, advanced bidirectional traffic,
  and passed control probes

### First adversarial finding: incomplete D2 object stream deadlocks retry

The first `d2-host-leave-1` run failed after the original RP4P host left,
the S21 migrated to host, and the RP4P attempted Join In-Game. Raw evidence is
preserved at
`android/temp/device-lan-fuzz-20261019/d2-departure-rejoin-rp-host/01-d2-host-leave-1`.

Observed sequence:

1. The migrated S21 host began the returning player's object transfer at
   14:25:11.921 and sent its end marker/SYNC at about 14:25:12.8.
2. The RP4P received most of the object stream, but its join diagnostics later
   reported one deferred end marker, 16 early SYNC packets, and
   `objects_complete=false`.
3. The waiting RP4P repeatedly requested a retry. The S21 rejected every
   request from 14:25:13.686 through 14:25:25.873 as
   `welcome_player: IGNORED busy (send_objects=0 sending_extras=9)`.
4. `Network_sending_extras` cannot advance while `VerifyPlayerJoined` is set,
   and that verification cannot clear until the joining player reaches
   gameplay and sends PDATA. The incomplete object stream prevents precisely
   that confirmation, so the state is self-deadlocking.
5. At 14:25:27.211 an older reliable gameplay packet timed out. The S21 dumped
   the joining slot for packet timeout. The next join request was accepted at
   14:25:28.010 and the RP4P accepted SYNC at 14:25:28.900, but the prior dump
   dialog had already moved it out of gameplay. Protocol state showed two
   connected players while its UI remained in the removal error menu.

This is a live reproduction of the already cataloged BR-0356 incomplete object
stream/retry defect, with the additional user-visible late-success/UI split.
It is not attributable to discovery, mismatched APKs, or a dead host: the host
continued receiving requests and retransmitting SYNC throughout the stall.

An immediate unchanged replay passed in 95.5 seconds at
`android/temp/device-lan-fuzz-20261019/repro1-d2-host-leave-1`, confirming that
the trigger is datagram loss/reordering dependent rather than an unconditional
lifecycle failure. The passing control accepted the returning player's SYNC
without entering the busy-retry loop.

### Focused campaign result

The completed physical-device session produced 28 case results over 3,153.91
seconds of measured case time: 26 passed and 2 failed. Every failure preserved
its first raw evidence separately; no failing directory was reused by a retry.

Passed coverage:

- Six preflight baselines: D1, D2, and D1-in-D2 in both hardware directions
- D2 host and client process-kill/rejoin at 1, 8, and 35 seconds
- Ten-cycle D2 client churn in one host process, alternating launcher discovery
  and direct join across 0.25, 1, 3, 8, and 16-second outages
- Four D2 host migrations with the former host returning after every swap
- Saved D2 partial-transfer cancellation followed by a successful retry
- Three stopped approval-pending join attempts followed by a successful join
- Host game-menu/peer-loss and client automap/peer-loss recovery
- Native D1 host/client graceful leave and return
- D1-in-D2 host graceful leave and return
- D2 join-transfer/level-transition crossing
- D2 restore/client-loss/resume crossing
- Unchanged replays of both failing case shapes

The two failures were:

1. `d2-host-leave-1`, RP4P host departing and returning after the S21 migrated
   to host. The returner did not complete its first object stream, the host
   rejected retries as busy, and the later packet-timeout dump raced with a
   successful retry.
2. `d1d2-client-leave-1`, S21 client departing and returning to the unchanged
   RP4P host. The first transfer delivered 60 of 74 objects: the end marker was
   deferred at 14 missing objects, 16 repeated SYNCs were deferred, and every
   fresh request was rejected while `Network_sending_extras=9`. A reliable
   packet timed out after 15 seconds, reason 8 was processed by the returner,
   and the next transfer delivered all 74 objects and accepted SYNC. The old
   removal path nevertheless left the returner outside gameplay while native
   protocol state reported two connected players.

The second failure is preserved at
`android/temp/device-lan-fuzz-20261019/cross-engine-graceful-leave/01-d1d2-client-leave-1`.
Its unchanged replay passed at
`android/temp/device-lan-fuzz-20261019/repro-d1d2-client-leave-1`.

These two failures demonstrate the same defect without requiring one device,
one authority role, or one mission object set. They also establish that host
migration is not required: the second failure occurred while the original host
remained authoritative. The trigger is a lost/reordered object datagram during
an otherwise valid Join In-Game transfer.

### Recommended next implementation boundary

Fix and deterministically test BR-0356 before spending time on broad random
generation. The host must recognize a retry for its current unconfirmed joiner
and restart or resume that transfer instead of rejecting it solely because
`Network_sending_extras` is nonzero. The old attempt must be retired as one
generation so a late SYNC, end marker, dump, or reliable timeout cannot affect
the replacement attempt or its UI.

Add a diagnostic-only injection that drops one selected middle object datagram
on the first transfer. A focused two-peer regression should then require:

- the returner to keep the same authenticated identity and request a retry;
- the host to retire the unconfirmed transfer without waiting for a gameplay
  reliable-packet timeout;
- a complete replacement object stream and accepted current-generation SYNC;
- no packet-timeout removal dialog from the old transfer;
- both peers in gameplay with two connected players, advancing bidirectional
  PDATA, responsive controls, and matching world visit;
- the same behavior in paired D1 and D2 transport implementations.

Only after this deterministic loss case passes should the state-aware fuzzer
add random action composition. Otherwise ordinary Wi-Fi packet loss will keep
rediscovering the same known deadlock and obscure later state-machine defects.

## Targeted failure provocation, 2026-10-19

The detailed second trace identifies the missing first packet (clear marker
plus 14 objects), not a middle packet. The first trace is consistent with the
same loss. Successful replay does not establish where the packet disappeared.

This follow-up changes diagnostic hooks and tests only, not recovery behavior:

1. Add a shared, explicitly armed, one-shot sender drop for the first object
   packet, second object packet, end marker, or first SYNC of the next join
2. Extend the existing physical-device campaign with a no-loss rejoin control
   and those four faults, preserving both devices' logs and timed state samples
3. Observe for 40 seconds after admission, including after failure, to separate
   incomplete-stream stalls from late success underneath the removal dialog
4. Require verified injection, healthy gameplay, bidirectional traffic, and
   responsive controls for a regression pass; reproduced bugs remain FAIL
5. Build both native engines, run catalog checks, then run physical D2 cases
   and targeted D1/D1-in-D2 parity or reversed-role cases as evidence warrants
6. Record results and refine the fix boundary without implementing recovery

Normal com.dxxredux.app data stays untouched. Only the isolated nsdtest APK is
updated, without clearing its provisioned data. Each run uses a fresh evidence
directory and resets the diagnostic app processes between cases.

### Implemented diagnostic coverage

The existing device campaign now offers 15 explicit cases: control, first
object packet, second object packet, end marker, and first SYNC loss for each
of D1, D2, and D1-in-D2. The shared native hook drops one selected outgoing
join payload before socket transmission, logs its attempt, type, size and
object marker, then disarms. It does not change join recovery behavior.
Introspection exposes `join_wait.test_drops` so the runner requires proof that
the injection actually happened. The runner captures 40 seconds after the
drop, including any removal dialog and late protocol completion.

One initial end-marker run stopped before injection because process exit
preceded the launcher's returnable-activity cleanup. The runner now waits up
to 25 seconds for both launcher flags to clear, with host-only tests for both
transient and persistent stale state. That original run is retained as a
setup failure, not counted as an object-loss reproduction.

Build used: diagnostic package `com.dxxredux.app.nsdtest`, version code 27100,
HEAD `10da7fdfd0cf1198f77538373404234cc5f33bc1` plus the working-tree test hooks.
APK SHA-256:
`2eba8662d5f9789f70d9f1cddb65aad84e3f694b2e7fc9830fd73f7c5791c3ca`.
The current Gradle APK listing points to
`android/app/build/intermediates/apk/debug/app-debug.apk`; the old APK under
`outputs/apk/debug` was not used. Both devices received the same APK with
`adb install -r -t`, preserving their diagnostic data and the normal app.

Validation completed:

- Android arm64 build of both native engines and diagnostic APK: PASS
- Scoped code formatting/lint: PASS
- Campaign host-only fixture: 11 tests PASS
- Automation catalog: PASS, 95 standalone JSON tests and 426 support scripts
- Master test catalog: PASS, 364 top-level entries
- `git diff --check`: PASS

To repeat the D2 boundary matrix on the provisioned physical devices:

```powershell
.\android\tests\test_device_network_campaign.ps1 `
    -HostSerial JYPR42510121028 -ClientSerial R3CR40Q4XPK `
    -OutputDirectory android/temp/join-loss-next-run `
    -CasePatterns 'd2-join-loss-*'
```

Choose a fresh output directory. Use `d1-join-loss-*` or
`d1d2-join-loss-*` for the other content/engine combinations, and `-Reverse`
to swap the physical host. These are strict recovery regressions: the current
object-transfer defects deliberately remain FAIL, not expected-success tests.

### Targeted live results

The campaign completed 11 executions: 10 valid networking experiments and
one pre-injection launcher teardown failure. Of the valid experiments, the
no-loss and first-SYNC-loss controls passed; all eight object-transfer-loss
experiments failed. Both diagnostic apps were stopped by cleanup and both
physical devices remained ADB-connected.

| Fault | Tested combinations | Result |
| --- | --- | --- |
| No loss | D2, Retroid host | PASS, healthy traffic and controls |
| First object packet | D1, D2, D1-in-D2 with Retroid host; D2 with Samsung host | FAIL in all four: busy-rejected retries, removal, then PLAYING underneath the removal dialog |
| Second object packet | D1, D2, D1-in-D2 with Retroid host | FAIL in all three: start marker received, no subsequent transfer retry, removal |
| End marker | D2, Retroid host | FAIL: all 106 objects received, completion never verified, no subsequent transfer retry, removal |
| First SYNC | D2, Retroid host | PASS, later SYNC completed the join |

Evidence directories under `android/temp/join-loss-20261019`:

- `d2-rp-host`: first boundary matrix, including the retained setup failure
- `parity-rp-host`: D1 and D1-in-D2 first/second loss, plus D2 end-marker rerun
- `d2-samsung-host`: reversed physical roles, D2 first-packet loss

Each valid case includes `join-loss-observation.json`, raw per-device logcat,
fresh introspection snapshots, automation results, and `result.json`. Summary
results are in each group's `results.json`. Device wall clocks differ; use
within-device ordering, attempt IDs and packet hashes, not cross-device wall
clock subtraction.

The D2 first-packet reproduction intentionally dropped a 3905-byte object
payload containing the clear marker and 14 objects. The client ended with
92/106 objects, deferred 16 SYNCs, and received removal reason 8. The following
transfer used `attempt=0`, delivered 106/106, and accepted SYNC while the
removal dialog remained visible and `in_game=false`. Native D1 and D1-in-D2
showed the same failure with 60/74 objects. Reversing the hardware reproduced
the 92/106 case and the same late-success/UI split.

This establishes the recovery failure independently of accidental network
loss. It does not identify where the original first packet disappeared.
Changing only the host busy check is insufficient: the second-packet and
end-marker tests demonstrate that the client stops requesting retries once
the start marker sets `Network_rejoined`, before completion is verified.
The fix must address progress-based restart on both sides, as well as making
failure terminal before opening a dialog or disabling the attempt guard.
Recovery behavior remains unchanged in this diagnostic/test-only work.
