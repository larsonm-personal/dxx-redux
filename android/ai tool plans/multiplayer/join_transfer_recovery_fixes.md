# Join transfer recovery fixes

## Objective and evidence

Fix the physical-device survey failures: loss of a first/interior/tail/end
packet must recover, repeated object packets must not mutate or recount the
same objects, and a failed attempt must never resume behind a removal dialog.
Preserve the passing reorder, delayed-end, delayed-SYNC, stale-envelope,
transition, cancellation, and established-session paths in both engines.

The baseline is the working tree containing the deterministic fault campaign
and the findings in two_device_lan_state_fuzz_20261019.md. Existing changes
are retained. Survey results describe the baseline, not validation of fixes.

## Phase 1: duplicate-safe receipt

- Track receipt by the sender's object index within the current join attempt,
  before remapping to receiver indices or allocating owned objects
- Validate the complete datagram before changing receipt state
- Ignore duplicate object bodies before counting, unlinking, allocating, or
  writing; clear and completion markers must also be idempotent
- Discard data before a clear marker; recover it through a fresh transfer
- Reset receipt state only when the attempt changes, not when a duplicate
  clear marker arrives
- Keep exact completion validation; do not accept surplus or missing objects
- Preserve ordinary initial-sync and non-Android behavior
- Validate first/interior/tail/end duplication, and delayed copies after later
  data; inspect unique counts and allocator state as well as gameplay

## Phase 2: stalled transfer recovery

- Measure progress using accepted unique objects and verified completion;
  status replies, duplicates, and repeated early SYNC must not extend progress
- Start a bounded progress deadline after admission or evidence of transfer;
  do not mistake human approval/briefing waits for stalled object transfers
- On stalled/inconsistent transfer, retire the old attempt and request a new
  generation through the existing join restart loop
- Allow the authenticated current unconfirmed joiner to replace its transfer;
  preserve busy exclusion for other identities and reject obsolete requests
- Distinguish host publication from actual client gameplay confirmation;
  sending SYNC is not proof that the receiver reconstructed its world
- Retire old reliable queues, extras, verification, and provisional slot state
  together so old timeout work cannot remove the replacement attempt
- Bound repeated failures and leave a usable launcher/menu when recovery is
  exhausted; cancellation and world changes remain responsive
- Validate loss of first, second, tail, end, and SYNC in all three content
  paths, reversed devices, and the larger level-5 transfer

## Phase 3: terminal failure and generation isolation

- Set terminal protocol state and retire the join before any modal UI pumps
  events; prevent both wrapped and legacy/unwrapped late sync from reviving it
- Route join failures through a single outer cleanup/display boundary
- Inspect dump authentication and generation lifetime; an old attempt's dump
  or queued reliable timeout must not kill a replacement attempt
- Preserve genuine host kick, host disappearance, and established game errors
- Add provocation for late objects/SYNC/dump across retry, cancellation, and
  failure, and verify no PLAYING state or active gameplay under failure UI

## Phase 4: validation and completion audit

1. Build both Android native engines and the isolated diagnostic APK, checking
   current Gradle APK metadata and hash before installing on both phones
2. Run the campaign CLI fixture and meaningful host tests for new protocol
   state logic, including malformed input and reset/generation boundaries
3. Run all fault classes for D1, D2, and D1-in-D2; run the failed loss/duplicate
   classes in reverse physical roles and the level-5 shape matrix
4. Exercise partial cancellation/retry, host migration, join across level
   transition/restore, and repeated joins in one host process
5. Require exactly the requested injection, bounded recovery, no removal UI,
   matching world visit and player identity, two connected peers, advancing
   bidirectional traffic, responsive controls, and valid object allocation
6. Require intended rejection diagnostics for obsolete/malformed traffic;
   final healthy state alone is insufficient evidence of isolation
7. Run scoped format/lint, automation catalog, master catalog, and diff checks
8. Record actual artifacts/results and update the bug record. Audit each
   requirement above against current evidence before marking the goal done

## Status

- Planning: complete after inspecting current receiver, host busy logic,
  commit boundary, retry loop, and removal handler
- Phase 1 implementation: receipt bitmap keyed by sender object index, reset
  at attempt creation; duplicate bodies skipped before count/allocation/write;
  duplicate clear/end markers ignored within the attempt
- Initial physical validation: D1 interior duplication and D2 interior, tail,
  and end duplication PASS with advancing traffic and responsive controls
- D2 interior trace: 14 duplicates skipped, exactly 106/106 objects, allocated
  and live counts both 106. End duplication: one rebuild, second marker ignored
- Evidence: android/temp/join-recovery-fixes-20261019/phase1-duplicates and
  phase1-d1-retry. The first D1 run hit an ADB read timeout during initial pair
  setup before injection; the rerun completed and passed
- Both native engines built successfully; scoped quality and diff checks pass
- Diagnostic APK: version 27110, SHA-256
  2a08150e09a094db6264e64b02834f52ba1f354723e5b8f9295649a069f64cc6
- Phase 1 broader parity, late-duplicate and malformed/reset tests remain open
- Phases 2 and 3 implementation, full phase 4 validation: pending

## Terminal failure implementation checkpoint

- Both engines now route authenticated join rejection/removal to the outer
  join loop instead of ending the guard and displaying a nested message box
- The shared failure setter records the first reason, cancels restart, and
  sets MENU while retaining the active attempt guard
- Packet dispatch rejects traffic while an active join has failed or entered
  MENU; failed joins close sockets before ending the guard and showing UI
- Existing established-game kick/removal behavior is unchanged
- Both arm64 engines and the diagnostic APK built successfully; scoped quality,
  diff checks, and all 11 campaign CLI fixture tests pass
- Tested APK candidate SHA-256:
  4b25623d2292a64df88a0dd3624cbcb1a20d4df88bf315ea5b98fafe9b9cc784
- Physical D1/D2 first-packet loss probes completed under
  android/temp/join-recovery-fixes-20261019/terminal-failure
- Both injected exactly one fault and retained removal, but neither resumed
  PLAYING under the removal dialog during the full observation window. D1
  raw logs show terminal packet rejection followed by failed-sync unwind
- Recovery remains incomplete: both probes correctly FAIL their unchanged
  strict recovery oracle; this is narrower evidence for failure finalization,
  not a claim that packet-loss recovery or all late-packet cases now pass
- Automation and master catalog validation both pass
- Remaining phase 3 audit includes object-verification failures, stale dumps
  across replacement attempts, and explicit late-packet injection after cancel

## Progress-based recovery implementation checkpoint

- First enveloped object traffic starts a five-second progress watchdog;
  accepted unique object writes and verified completion refresh it, while
  duplicate bodies, status, and repeated early SYNC do not
- Stalled or verification-failed attempts use the existing outer restart loop
  and fresh attempt token; three transfer retries are allowed before terminal
  failure. World-transition restarts do not consume this transfer retry budget
- Explicit cancellation now retires an unconfirmed published transfer as well
  as a partial transfer, including extras, verification, cursor, slot connection,
  and old outgoing reliable acknowledgements for that slot only
- Host replacement checks the validated public key, newer signed request
  counter, matching visit, and route proof before retiring the active attempt
- QUIT_JOINING now reaches cleanup after object sending has finished; its
  attempt and visit must match the pending transfer
- Android object-verification failure now restarts through the outer boundary,
  rather than opening a nested failure dialog after local retry exhaustion
- Both engines build and scoped quality passes. Diagnostic APK SHA-256:
  b2b91c8f7c86c398ed2378e393f01f3d5b85b2c545ea7f2a44744eb57285a0c3
- Physical first/interior-loss probes completed in
  android/temp/join-recovery-fixes-20261019/progress-retry
- D1 interior loss and D2 first/interior-packet loss PASS the unchanged full recovery
  oracle: one injected fault, exactly one retry, verified completion, no removal,
  matching world visit, advancing traffic and responsive controls. D1 rebuilt
  74/74 objects; D2 rebuilt 106/106. Host traces retire the old published attempt
  and begin a different attempt before the old reliable timeout deadline
- These successful runs exercised explicit QUIT retirement, not lost-QUIT
  replacement; the latter still needs its own deterministic fault test
- Remaining audit: admission evidence when every object payload is lost but
  SYNC arrives, signed cancellation/replay handling, competing admissions while
  an unconfirmed published transfer has no send flags, late dumps and old
  reliable traffic across generations, persistent loss, and full parity matrix

## Recovery-path coverage checkpoint

- SYNC now counts as initial admission evidence without extending the progress
  clock on subsequent SYNCs; this covers loss of the entire object stream
- Host busy exclusion now lasts through the unconfirmed published attempt,
  including when object/extras send flags have already cleared
- Added second_and_quit and all_objects faults for all three content paths
  (63 level-1 cases plus the existing 12 level-5 variants)
- Lost-cancellation cases require both injections and the authenticated
  transfer_replace diagnostic. Whole-stream loss requires a sampled state with
  early SYNC, zero object packets, and no retry yet. Both require exactly one
  eventual retry in addition to the unchanged gameplay/traffic/control oracle
- The host CLI fixture now has 12 passing tests, including negative tests that
  reject absent recovery-path evidence. Scoped quality, both engine builds,
  automation catalog, master catalog, and diff checks pass
- Diagnostic APK SHA-256:
  d737eb4b1ae589d5468c3aec66a07aa63f48d839b6b41444f0109d00b864772b
- Physical probes completed in
  android/temp/join-recovery-fixes-20261019/retry-paths
- First D1 lost-cancellation attempt failed during ADB KEYCODE_WAKEUP before
  setup/injection; keep as setup-only and rerun separately
- D1/D2 whole-stream loss PASS with explicit SYNC-only admission evidence;
  D2 lost-cancellation PASS with both injections and transfer_replace evidence
- Removal-message audit: current DUMP is only six bytes (type, token, reason),
  without attempt or visit. Scoping only pending-transfer dumps is insufficient:
  an established session's delayed removal can cross a later rejoin. The next
  implementation must preserve a per-player incarnation through confirmation,
  distinguish pre-admission rejection from established-player removal, and test
  real kick plus stale removal before and after replacement completion
- Cross-session audit found generate_token() uses unseeded POSIX random() on
  Android. Physical logs repeat attempt sequences across process launches
  (for example D2 1215608822 then 1019679710). Existing forged-token rejection
  tests do not prove real cross-session isolation. Use the existing secure
  reconnect RNG for Android session/attempt tokens, fail closed on entropy
  failure, and replay captured old traffic across process restart as a test
- Implemented Android secure tokens through the existing JNI SecureRandom
  bridge, reserving zero and failing closed rather than falling back to the
  repeating POSIX sequence. Non-Android token generation is unchanged
- Secure-token build and scoped quality PASS. APK SHA-256:
  eb6076166253cc4838b6412ea1721e8828ab7742a061b3cc275d6718763c938f
- Follow-up D1 lost-cancellation and D2 whole-stream-loss tests both PASS in
  android/temp/join-recovery-fixes-20261019/secure-token-retry
- D1 lost-cancellation rerun PASS on the secure-token APK: dropped QUIT for
  attempt 2355382890, authenticated replacement 3416369311, and 74/74 objects.
  Both high-bit token values survived signing/serialization and the complete
  traffic/control soak. This is functional RNG integration evidence, not a
  statistical randomness test or a completed cross-session replay test
- D2 whole-stream-loss secure-token regression PASS: attempt 1144303840
  retired, replacement 1915772650 reconstructed 106/106 objects, SYNC-only
  admission evidence present, exactly one retry, healthy traffic and controls

## Request-scoped removal implementation checkpoint

- Android DUMP packets now carry the recipient's signed request counter.
  Android protocol versions bumped to D1 30071 and D2 30082; upstream wire
  layouts/versions remain unchanged
- A new local attempt immediately retires its accepted removal range, even
  before sending a replacement request. Signing establishes a new lower bound;
  resends remain in that range. The range survives gameplay confirmation
- Established removals use stored authenticated player counters; pending
  transfers use the pending signed request. Pre-admission refusals use the
  refused request directly rather than a previous slot occupant's identity
- Added stale_dump_retry to all three content paths (66 level-1 cases).
  The case drops an interior packet, replays that attempt's removal during
  reconstruction and after gameplay confirmation, requires both rejection
  diagnostics plus healthy traffic/controls, then requires a real current kick
- Pure removal counter-window boundary checks added to the native reconnect
  authentication test; fresh x86 MSVC compile and execution PASS. Initial host
  build attempt used the wrong compiler architecture and reset its generated
  configuration; buildd1 was regenerated with x86, RelWithDebInfo, original
  dependency toolchain and existing dependency cache before the successful test
- Both Android engines build, scoped quality and 12 CLI fixtures pass
- APK SHA-256:
  38ec088e24efd829eb99fac4502924eda9fac716ad21cb81a481ca036ace9c1c
- Initial physical tests completed in
  android/temp/join-recovery-fixes-20261019/removal-lifetime
- Both engines rejected both old removals and accepted the current kick.
  However inspection of current-kick.json found network_status=PLAYING under
  the kick dialog. Initial PASS only proves the earlier narrow oracle; it does
  not prove terminal current-kick cleanup
- Expanded the oracle to reject PLAYING under the current kick dialog. Android
  established-player removal now sets MENU/GAME_OVER and closes networking
  before displaying either a kick/timeout or another valid rejection message.
  The hidden Game_wind can still exist until modal unwinding, so in_game alone
  is not treated as proof of live networking
- Terminal-removal APK SHA-256:
  6514ab89c569745007ae30636a83d48ddcd4d108fde19d9d7d3fb26b13eb1ea7
- Stricter reruns in android/temp/join-recovery-fixes-20261019/removal-terminal:
  D1 and D2 PASS. Each rejects the two stale removals, retains healthy
  gameplay before the real kick, then displays the kick without active network
  mode. CTest test_net_udp_reconnect_auth and all 12 CLI fixtures also PASS
- Remaining removal audit: migration after a reconnect must propagate the
  accepted counter to the successor; observer removal counters need explicit
  handling rather than assuming a normal player slot; admission refusals,
  initial-sync failure, cancellation replay, persistent loss, and cross-session
  captured-packet replay still need coverage. Do not claim phase 3 complete

## Persistent-loss exhaustion checkpoint

- Added a persistent end-marker drop that remains armed across all attempts;
  normal objects, status, and early SYNC keep flowing. Explicit clear_fault
  removes the injection without restarting the host
- Added end_persistent for D1, D2 and D1-in-D2 (69 level-1 cases). Its oracle
  requires four injected end losses, exactly three retries, inactive/incomplete
  join state, a retry-limit dialog, and no PLAYING state anywhere in the trace
- After failure, the case dismisses the dialog, returns to the launcher, clears
  the fault, and rejoins the same host process with healthy traffic and controls
- Added negative host fixture checks for unbounded retry counts, active or
  completed transfer state, missing failure UI, and transient PLAYING. All 13
  CLI fixtures pass. Both Android engines and scoped quality pass
- Diagnostic APK SHA-256:
  661961f3b2644a90ee638a72cfbfb54602ea12180388163f7c495d02c3a7bccc
- All three physical content-path cases PASS in
  android/temp/join-recovery-fixes-20261019/retry-exhaustion
- D1 PASS: four distinct attempt IDs each lost completion, three retries,
  inactive/incomplete terminal state and retry-limit UI, followed by successful
  rejoin to the unchanged host process. Both final snapshots have exactly two
  players/two connected peers, with one instance of each callsign
- D2 and D1-in-D2 also PASS: raw host logs show four distinct attempt IDs
  with drop counts 1 through 4 in one host process. Final observations show
  retries=3, active=false, objects_complete=false, retry-limit UI, and no
  multiplayer network state. Fresh rejoin snapshots show two connected peers
  on both devices, after the traffic/control checks

## Full fault-matrix validation checkpoint

- Started all 69 level-1 faults/controls and 12 level-5 variants against the
  persistent-loss APK above, in
  android/temp/join-recovery-fixes-20261019/full-fault-matrix
- Stop-on-failure is enabled so any first failure can be inspected before
  proceeding. A started case is not validation evidence until it completes
- Allocation audit confirms both engine receivers mark the sender receipt bit
  and increment object_count before obj_allocate succeeds. On allocation
  failure, the object body cursor also does not advance. This requires a
  targeted allocation-failure diagnostic and recovery test; normal duplicate
  passes do not establish this boundary is safe
- Both catalog validators passed, along with all 13 campaign CLI fixtures
- Implemented the allocation boundary fix in both Android receivers: sender
  receipt and count are committed only after the object write/link/map succeeds;
  allocation failure logs objects_allocation_failed and requests the bounded
  transfer restart, returning before a misaligned packet cursor can be reused
- Non-Android and inactive initial-sync behavior are unchanged. A forced
  allocation failure and subsequent successful retry still need validation
- The full physical matrix remains on the earlier installed APK. Its results
  must not be described as validating this subsequent allocation change
- Both arm64 native engines compile successfully after the allocation change
  (`:app:externalNativeBuildDebug`, 53 seconds). The APK hash remains
  661961f3b2644a90ee638a72cfbfb54602ea12180388163f7c495d02c3a7bccc;
  no replacement APK was installed during the matrix. Compiler warnings are
  the existing newlist/len and array-pointer checks, not in the changed receipt
  or allocation blocks. Scoped quality and git diff --check pass
- Next allocation probe: arm a client-side one-shot allocation failure while
  its request awaits host approval, then approve and require the exact
  objects_allocation_failed diagnostic, one bounded retry, and healthy final
  world/traffic/control state. Also test persistent failure reaches exhaustion
- Added the allocation_once client automation hook at the actual obj_allocate
  call in both receivers. It substitutes a single -1 result and logs the
  injection separately from the production allocation-failure handler
- Added three join-allocation-once campaign cases. Each starts a fresh host,
  waits for the client's pending approval, arms the client fault, then approves
  and requires one injection, one handler diagnostic, exactly one retry,
  completed/inactive join, healthy traffic and controls, and unchanged host PID
- All 14 CLI fixtures pass, including negative checks for missing/duplicate
  injection, incorrect retry count, incomplete state, and missing client state
- Scoped mixed-language quality and diff checks pass. The new physical cases
  await a new APK after the current matrix; no on-device allocation pass is
  claimed yet. Persistent allocation failure still needs its own probe
- Both arm64 native engines build with the allocation test hook (52 seconds);
  the packaged APK hash is unchanged. The frozen-APK matrix has passed D1
  control and first/interior/tail/end loss so far and continues with SYNC loss

## Successor identity replication audit

- D1 send_rejoin_sync (and its D2 counterpart) stores the returning player's
  counter, but only broadcasts ADDPLAYER when Network_player_added is set.
  Existing-slot reconnect then sends SYNC to the returning player only
- net_udp_update_netgame updates local state; it is not a broadcast. Full
  game-info updates can replicate identities, but the rejoin confirmation
  helper does not request one. Thus a successor can retain the older counter
- android_net_udp_auth_read_player replaces the stored identity from full
  game info, including its counter, without a monotonicity check. A delayed
  same-session metadata packet could undo a later update. This must be tested
  together with delivery loss, not patched with a one-shot broadcast alone
- Added identity_store and changed-identity_receive diagnostics with slot,
  old/new request counter, and same-key indication. No key material is logged
- Next proof requires three peers: keep a potential successor connected while
  the other client reconnects, inspect counter receipt, migrate the host, and
  test a genuine successor kick as well as delayed old removal rejection.
  Delivery-loss/reordering variants must prove the replicated counter does not
  go backwards. These diagnostics do not implement or prove a replication fix
- Both native engines compile with these diagnostics (1 minute 19 seconds);
  scoped quality and diff checks pass. No APK was installed during the running
  physical matrix. D1 SYNC-loss and duplicate-first cases also pass there

## Allocation fixture reachability correction

- Inspecting the D2 exhaustion/rejoin snapshots showed all 106 synchronized
  objects were unowned. Such a stream uses fixed receiver indices and never
  calls obj_allocate, so simply arming the allocation failure is insufficient
- Added a diagnostic solo-host setup that maps one existing unowned powerup
  to the host using the engine's normal local-object ownership mapper. This
  makes that object use the remote-owner allocation pass on the joining peer
- The allocation campaign saves and checks synchronized_object_owner_counts
  before launching its client; absent or wrong-owner setup is a hard failure
- All 15 CLI fixtures and scoped quality pass. The prior allocation cases have
  not run on devices, so no earlier allocation success claim is invalidated
- Full frozen-APK matrix also passed D1 interior duplication; tail duplication
  is in progress. These cases remain distinct from the new allocation probe
- Corrected fixture compiles in both arm64 engines (1 minute 36 seconds).
  D1 tail duplication subsequently PASS; duplicate-end is running. The
  original installed APK remains unchanged throughout this matrix

## Allocator accounting oracle

- Added verify_object_allocator to ordinary join-fault recovery, successful
  rejoin after exhaustion, and the allocation-failure probe. It requires
  positive integer live counts, allocated==live, nonnegative ownership counts,
  and owned+unowned==live; absent fields are failures
- Future runs save join-recovery.json before evaluating this oracle. The
  already-running matrix uses its originally loaded runner, so its completed
  snapshots were explicitly checked offline with the new helper instead
- All 16 CLI fixtures pass, including negative allocation/count/ownership and
  missing-field cases. Scoped Python quality and diff checks pass
- Offline checks PASS for all six peer states in the three post-exhaustion
  rejoin files, and all 22 final peer states from the first 11 completed matrix
  cases (D1 control, five loss cases, five duplicate cases)
- Count consistency is not proof of object identity equivalence, segment-link
  validity, or allocator free-list correctness; those remain separate audit
  requirements. The matrix continues into the D1 reordering cases
