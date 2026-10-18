# Co-op session log investigation

Analyze the S25 host and S21 client logs exported on 2026-10-17 from build 651357958

1. Identify session roles, clock offset, level transitions, restores, and disconnects
2. Reconstruct the early laser drops and pickups using recovery identities
3. Trace the initial connection failure through transport and restore state
4. Compare guidebot door behavior with replication code and available diagnostics
5. Reconstruct the final S21 plasma and quad drops, recovery bindings, and collections
6. Record findings with source log line references, confidence, and remaining evidence gaps

Scope now includes implementing and verifying all fixes requested by the active goal
Preserve unrelated working tree changes and use reproductions to resolve uncertain causes

## Implementation progress

- Completion audit found outstanding door coverage from the original validation scope: Guide-Bot flares and late join
  Extended GuidebotDoors to fire real moving flares into keyed and unkeyed portals, reject nonowner flare openings, and verify all five selected portals after client departure/rejoin
  Explicit wall-index verification reads restored wall state without resetting the door fixture on the returning process
  Both ownership runs passed in First Strike level 8 under D2: host ownership at 19:03:21 and client ownership at 19:04:53
  Both peers retained opened walls 0, 4, 6, 8 and 10 after rejoin; the client-owned case also exercises ownership departure before rejoin
  Evidence: temp/coop-door-flare-{host,client}-runtime.log; detailed client-owned logs: temp/coop-door-flare-client-owner-{host,client}.log
  Both Windows builds, four targeted native tests, all-ABI Android assembleDebug, scoped formatting, and both automation catalog checks passed (425 support scripts)
  Build/catalog evidence: temp/coop-door-flare-{native,android,catalog}.log; the refreshed app-debug.apk contains both engines for arm64-v8a, armeabi-v7a and x86_64
  No production behavior change was needed for these added cases; the original observed door identity and trigger remain unproven by the old logs
  Current-state audit found no newer phone debug logs in Downloads and no physical devices attached; original initial disconnect still needs fresh NETEXIT evidence

- Three-player pickup runtime coverage passed in both engines: race, checkpoint restore during pending requests, and level travel during pending requests
  Native D1 used level 1; D2 used First Strike level 8, including travel to level 9
  Two nonhost clients compete through the real host relay; each receives the other's logical request and permission, exactly one laser level is awarded, and the host gains nothing
  Restoring the entry checkpoint cancels both pending requests with no award or retained old drops; all three peers remain connected
  Level travel cancels both pending requests, awards no laser, and preserves abandoned live drops as recovery credit on every peer
  The pending-request fixtures hold outgoing pickup datagrams; they verify cancellation during transition, not delayed datagram delivery after transition (covered by native stale-message tests)
  Reusable command: test_lan.ps1 -Game d1 -PickupRelayDevice emulator-5558 -PickupRelayCase race|restore|travel -SkipBuild
  For D2, use -Game d2 -MissionFile descent -InitialLevel 8
  Evidence: temp/coop-three-peer-d1-runtime.log, temp/coop-three-peer-d2-race.log, temp/coop-three-peer-d1-restore-verified.log, temp/coop-three-peer-d2-restore-final.log, and temp/coop-three-peer-{d1,d2}-travel-verified.log
  Durable per-peer logs and final snapshots: temp/coop-pickup-relay-d1 for the original race, and temp/coop-pickup-relay-{d1,d2}-{race,restore,travel} for subsequent cases
  Two initial fixture failures were corrected without production changes: the reused restore assertion required unarmed delay/loss injection, and the travel script expected disabled briefings
  Scoped formatting passed; both automation catalog checks passed (362 top-level entries, 422 support scripts); evidence: temp/coop-three-peer-final-catalog.log
  All pending three-player/transition runtime coverage is now complete; the original phone disconnect still requires fresh NETEXIT evidence

- Normal Android assembleDebug passed, including armeabi-v7a, arm64-v8a and x86_64 engine libraries verified inside app-debug.apk
  Evidence: temp/coop-phone-build.log; APK: android/app/build/outputs/apk/debug/app-debug.apk
  Restored graphics/texture preferences on 5556 and 5558, restored cellular data on 5554/5556/5558, and stopped the added 5558 emulator
  Original 5554 and 5556 emulators remain online; no runtime/build/formatter processes from this investigation remain after final formatting
  The original phone disconnect remains unresolved; the new forced NETEXIT diagnostics distinguish reliable queue overflow, missing acknowledgements, and other native exits on a future reproduction

- Complete native-D1 migration plus partial-pickup runtime passed at 18:21 after restarting the investigation emulators and repairing their Wi-Fi association
  Both host swaps/rejoins preserved object parity and inventory; early SYNC/object-end were deferred correctly; bidirectional PDATA advanced after each rejoin
  The final death/respawn partial pickup reached ten homing missiles and retained three as recovery credit on both peers
  Evidence: temp/coop-migration-reboot-network-verified.log, temp/coop-migration-full-pass-{host,client}.log, and temp/coop-migration-full-pass-emulator-{5556,5558}.json
  The same APK passed without production timeout changes; the earlier very slow emulator run remains a recorded failure, not proof of the original phone disconnect cause
  Restored cellular data on 5556 and 5558 while repairing Wi-Fi; 5554 was offline after the external emulator shutdown
  Remaining protocol runtime coverage: three-player relay and travel/restore during an outstanding pickup request (native coverage already passes)

- The fresh-snapshot retry passed initial agreement, initial traffic, first migration, rejoin object parity, and inventory conservation
  It then failed sustained traffic: surviving host logged a genuine reliable timeout at 17:43:48.801 for packet 48, MULTI_COOP_RECOVERY (69)
  The returning client's saved lifecycle counters show only 11 event-loop/draw dispatches and 13 presented swaps after rejoin; its received backlog continued draining after the host timeout
  This is a different observed condition from the original phone's steady 60 FPS before exit; do not claim the original cause is reproduced
  Evidence: temp/coop-migration-fresh-snapshot-verified.log, temp/coop-migration-pdata-fail-{host,client}.log, and temp/coop-migration-pdata-fail-emulator-{5556,5558}.json
  All emulators subsequently disappeared from ADB and emulator processes were absent; restarted only investigation devices 5556 and 5558 for a fresh runtime
  Full migration plus partial-pickup completion remains pending; no production timeout change was made

- Fresh introspection had a separate timing flaw: one read 800 ms after the request could return null, and the outer retry deleted the subsequently published response
  Disabling rendering traces alone did not resolve initial snapshot agreement
  The exact runner predicates pass against both saved failure reports (roles, connected counts, and object ownership signature)
  Get-GameIntrospection -Fresh now polls the same requested response for up to five additional seconds, including incomplete JSON, without clearing/reissuing it
  Non-fresh reads retain their prior timing; game networking timeouts are unchanged
  Scoped formatting passed; temp/coop-migration-fresh-snapshot-verified.log records the full runtime retry
  Previous attempt evidence: temp/coop-migration-quiet-verified.log and temp/coop-migration-quiet-fail-{host,client}.log

- The final migration retry stopped at initial fresh-snapshot agreement, before either host swap
  Saved reports agree on both connected players and all 35 synchronized objects
  Rendering traces show the host advancing only 13 frames during the initial gameplay interval
  The runner's write_probe_debug_prefs command re-enabled graphics/texture logging after the manual preference change
  HostMigration now disables those two trace categories after enabling the network diagnostics
  Scoped formatting passed; the complete migration/partial-pickup retry is running with this correction
  Evidence: temp/coop-migration-rejoin-verified.log, temp/coop-migration-snapshot-fail-{host,client}.log and temp/coop-migration-initial-fail-emulator-{5556,5558}.json
  Restore graphics/texture logging preferences on 5556 and 5558 using SETUP_COMMAND after testing

- Both native builds and real receiver regressions pass with the stale-removal correction; Android APK and automation catalogs pass (418 support scripts)
  Runtime with the quiet fixture now passes both host swaps and both rejoins with object parity, plus bidirectional traffic after the first rejoin
  The final PDATA check stopped on a single missing introspection snapshot before observing counters; its baseline acquisition now polls, preserving the subsequent real counter-advance assertion
  Evidence: temp/coop-migration-rejoin-fixed.log and temp/coop-migration-two-rejoins-{host,client}.log
  Full runner completion including the final traffic check and post-migration partial pickup still awaits the retry

- The clip fix passed both native builds/receiver regressions and the Android APK build
  Runtime now accepts all 79 objects and completes SYNC, but exposed a separate stale-removal bug after the surviving host died during loading
  Rejoin allocates its five newer owned drops into local slots vacated by the returning player's excluded drops
  The returning player's old unowned removal packets then delete those different objects because multi_do_remobj did not verify the reverse network mapping
  Added a real-handler regression: both D1 and D2 fail before correction (temp/coop-stale-removal-native-red.log)
  Both handlers now require the resolved object to map back to the packet's owner and remote number before removal
  This prevents that specific stale-identity collision; same-owner/same-remote generation reuse is not addressed by this check
  The migration fixture now clears ordinary robots on both peers before preparing the Guide-Bot, so unrelated combat cannot invalidate its inventory conservation assumptions
  Evidence: temp/coop-vclip-fixed-migration.log, temp/coop-vclip-fixed-rejoin-{host,client}.log and temp/coop-vclip-fixed-final-emulator-{5556,5558}.json
  Native/network/Android confirmation is recorded in the later result above

- Reproduced and identified the native D1 rejoin rejection on an isolated emulator pair (5556 and 5558)
  Retail D1 loads a fixed animation-clip table but stores Num_vclips=0; the new object validator treated every powerup clip as out of range
  Received clips 18, 19, 35 and 46 were rejected, leaving zero installed objects before the later reliable timeout
  Added a real receiver regression; it failed on the valid retail clip before correction (temp/coop-vclip-regression-native-red.log)
  D1 object admission now uses the fixed table limit for the zero-count convention and rejects empty/oversized clips
  D2 retains its loaded-count convention; both engines log rejected object fields and asset limits
  Evidence: temp/coop-vclip-rejection-{host,client}.log and temp/coop-migration-object-diagnostics-retry.log
  The first isolated attempt stopped on a transient initial snapshot check despite matching saved peer states; the fixture now polls for agreement before migration
  Both automation catalog validators and the diagnostic APK build passed
  emulator-5554 is in use by a separate supplemental validation run; this investigation started DxxSdk36 on emulator-5558 instead
  Restore simulated cellular data on 5554, 5556 and 5558, and stop the added 5558 emulator after the remaining runtime checks

- Native D1 host-migration runtime reached a failed former-host rejoin at 16:19
  Every received object batch was rejected before installing objects; the later reliable packet timeout removed the waiting client
  Preserve evidence in temp/coop-migration-parse-failure-emu{1,2}_logcat.txt and temp/coop-pickup-host-migration.log
  Added object validation diagnostics to both engines to identify the rejecting bounds before changing admission rules
  This reproduction does not establish the cause of the original phone disconnect

- [x] Correct effective equipment ownership across death and respawn in both engines
- [x] Arbitrate competing recovery pickups before inventory awards
- [x] Replicate guidebot body door transitions and verify owner behavior
- [x] Bound failed rewind retries and diagnose/fix secret-level metadata rejection
- [ ] Reproduce and fix initial restored-session client exit
- [ ] Extend and run native and two-peer runtime regression coverage

The equipment change passed dedicated two-peer checks in both engines with collector roles reversed; the door change passed two-peer checks with host and client Guide-Bot ownership
The collection protocol passes competing-contact, death-policy, partial-pickup, and host-migration runtime checks; additional three-peer/transition runtime coverage and the original startup disconnect remain unfinished

Pickup protocol work in progress:

- Per-item ordered permission requests now precede local inventory awards
  Every active peer replies, with concurrent requests ordered by logical clock and player slot
  Replies carry remaining contents so a delayed ordinary collection report cannot permit a second award
- Reliable requests/replies route through the existing host relay and identify their intended recipient
- Outstanding permissions survive host mapping changes; the requester cancels unawarded attempts after death or loss of contact
  Saves wait for local requests to settle, and new peers join the permission set
- Expiry now freezes the item and waits for peer contents before converting the remainder to credit
  Physical objects survive until the host settles expiry
- Added a three-peer simulated transport test with 1,000 loss/reordering/duplicate-delivery schedules
  Added real recovery-handler checks for delayed collection reports, migration, death cancellation, and expiry during collection
  Both native builds and all twelve focused native checks passed, including these new scenarios
- The x86_64 Android build and scoped formatting/lint checks passed
  Both automation catalog validators passed (362 top-level entries, 403 support scripts)
- Two-peer D2-engine/D1-level-8 respawn pickup passed, with permission replies preceding inventory awards in captured logs
  Evidence: temp/coop-pickup-permission-lan-verified.log and temp/coop-pickup-permission-{host,client}-pass.txt
- Native D1 two-peer partial pickup passed: client reached ten homing missiles and both peers retained three as recovery credit
  Evidence: temp/coop-pickup-permission-d1-partial.log and temp/coop-pickup-permission-d1-partial-{host,client}-pass.txt
- The respawn fixture now isolates the death from the spawn point; its first attempt failed because natural respawn collection had already consumed the pack before the approach step
  The explicit collection automation hook now waits for asynchronous permission instead of assuming one collision call completes immediately
- A subsequent membership correction excludes loading players until CONNECT_PLAYING and adds a regression check
  Both native builds and all six recovery/permission/automation tests passed with this correction and brief-contact completion
  Evidence: temp/coop-pickup-contact-native.log; the x86_64 Android build also passed in temp/coop-pickup-contact-android.log
- Remaining protocol validation: simultaneous on-device contacts with delayed messages, three-peer relay, real host loss/rejoin, and travel/restore during a request
  Brief contacts now complete through the normal collision dispatcher when permission arrives, without requiring a second physical collision
  A native regression checks one award after a single contact; restore/travel freeze cancels unawarded requests
- Android protocol versions advance to D1 30070 / D2 30081 so old clients that cannot reply to pickup requests cannot join the new protocol
  The protocol-version APK build passed in temp/coop-pickup-version-android.log
- Added PickupRace to test_lan: actual death creates one laser, both peers make collision contacts with pickup datagrams held, then release the datagrams and assert one total laser-level award
  The introspection-only packet gate retains real request/reply datagrams; it does not synthesize awards or acknowledgements
- Added PickupPolicy host/client: a collector already owning plasma and quad touches a dead teammate's drops, completes both pickup attempts without gaining energy, and verifies that both drops remain live
  The teammate then respawns and recovers both items through the normal collision dispatcher
  Initial D2-engine/D1-level-8 race passed with host laser=1 and client laser=0 after both held requests were released
  The first equipment policy run did not complete either brief-contact attempt: host energy stayed 50, both drops stayed live, and the respawned client remained unequipped
  The fixture now maintains collision contacts until the real pickup handler completes and waits out the ordinary fresh-drop delay
  It records eligibility details on failure; preservation and recovery now run as separate phases so the teammate stays dead until preservation is verified
  Evidence: temp/coop-pickup-race-policy-d2.log and temp/coop-pickup-policy-fail-{host,client}.txt
- The corrected D2-engine/D1-level-8 combined race and host-collector policy scenario passed
  Exactly one laser level was awarded, the host completed both duplicate attempts with energy still 50 and both drops live, and the respawned client acquired plasma and quad
  Actual collection logs show primary flags 1->9 for plasma and equipment flags 0->0x400 for quad, with no energy increase
  Evidence: temp/coop-pickup-race-policy-d2-verified.log and temp/coop-pickup-race-policy-d2-{host,client}-pass.txt
  The corrected x86_64 APK and scoped formatting passed; both catalog checks passed with 410 support scripts
- Native D1 level-8 combined race and client-collector policy scenario also passed
  Exactly one laser level was awarded; the equipped client preserved the dead host's plasma and quad, and the host recovered both after respawn
  Evidence: temp/coop-pickup-race-policy-d1-verified.log and temp/coop-pickup-race-policy-d1-{host,client}-pass.txt
- The race/policy APK and both Windows builds passed; all six targeted pickup reward, recovery, and lock tests passed
  The catalog integration check passed on retry after the builds (362 top-level entries, 409 support scripts)
  Evidence: temp/coop-pickup-race-policy-android.log, temp/coop-pickup-race-policy-native.log, and temp/coop-pickup-catalog-retry.log

Current changes:

- Shared duplicate-reward eligibility rejects dead or not-yet-reconciled ship inventories
  Native dispatcher coverage includes plasma, death before spew, after spew, respawn, and reordered reappearance
- Rewind advances its capture deadline before submitting, even with no prior snapshot
  A new native test runs the real manager against rejected/failed/recovered checkpoint submissions and verifies retained snapshot bytes
- Level-start capture uses a monotonic five-second retry deadline after failed serialization
  Its real-manager native test verifies bounded failures, recovery, retained bytes, transition gating, and reset for a new mine
- D1 missions no longer create or adopt the D2 dormant-world archive
  This keeps the archive from retaining normal level 10 while the active mine is secret -1
- Android end-level updates continue on the score screen with the reactor intact
  The authored secret-exit test exposed a missing periodic update condition in both engines
- Shared robot-door helper restricts co-op body openings to the robot/guidebot simulation owner and broadcasts actual state transitions
  Native coverage checks host/client ownership, unowned host fallback, companion ownership independent of robot ownership, closing reversal, and single-player behavior
  Added GuidebotDoors host/client to the LAN runner: deploy a real companion, reject nonowner body contacts, and invoke the actual collision dispatcher for unkeyed, keyed, and closing doors
  Both peers must observe both sides of each portal fully open; robot AI is suspended in the fixture so flares cannot accidentally satisfy the body-contact check
  Native door tests passed again in both engines; the new runtime scenarios are awaiting their first run
  The x86_64 APK build, scoped formatting, and both automation catalog checks passed (417 support scripts)
  Evidence: temp/coop-door-android-retry.log, temp/coop-door-retry-quality.log, temp/coop-door-catalog-retry.log, and temp/coop-door-integration-catalog.log
  First direct runtime attempts failed before door assertions: slow level loading exceeded the first startup deadline, then the retry lost connectivity with client ENETUNREACH followed by mutual liveness timeouts
  The relay retry exposed a concrete transport defect: one plain IOException from a keepalive send cancels the proxy and closes its sockets
  Keepalive sends now tolerate IOException like gameplay sends, preserving the workers for native reliable retries
  Updated the existing real-loopback socket test to require traffic in both directions after an injected ENETUNREACH and no proxy shutdown
  This fixes a reproduced disconnect mechanism, but does not establish the cause of the original phone exit
  Scoped formatting, all three active real-socket proxy tests, and the x86_64 Android APK build passed
  Evidence: temp/coop-proxy-keepalive-android.log; the historical comparison test is opt-in and was skipped
  Emulator Android connectivity logs show repeated Wi-Fi disconnects/default-network switches, so simulated cellular data was disabled for the next door run
  Restore simulated cellular data on emulator-5554 and emulator-5556 after the runtime checks
  The updated APK encountered the same keepalive ENETUNREACH at 15:46:13, retained its proxy, sent a successful keepalive at 15:46:30, and resumed real game traffic at 15:46:35
  This supplies on-device recovery evidence in addition to the injected real-socket regression
  The longer-startup client-owned run deployed successfully and rejected the nonowner body collision, then recorded the owner's actual 0->1 door transition
  Its completion assertion incorrectly expected D2 OPEN for a D1 nonautomatic door, which deliberately retains OPENING after animation completion
  The fixture now uses the engine's completed-door-state rule and also requires removal from the active animation list and open flags on both faces
  The host crash dump identified the apparent stall as a fixture error: changing the companion to MT_NONE violated set_thrust_from_velocity's MT_PHYSICS assertion when a robot-position packet arrived
  Removed the movement-type mutation; SUSP_ROBOTS already bypasses AI and physics for CT_AI objects, preventing fixture flares without corrupting normal network object invariants
  The owner-reversed run using the obsolete fixture was stopped before its assertions; host/client reruns require the corrected fixture APK
  Door replication is still unverified
  Evidence: temp/coop-door-client-long-startup.log and temp/coop-door-body-failure-{host,client}.txt
  Crash evidence: temp/coop-door-host-native-crash.txt
  The fixture correction passed scoped formatting and the x86_64 APK build (temp/coop-door-physics-quality.log and temp/coop-door-physics-android.log)
  Corrected host-owned runtime scenario passed all three door cases in D1 level 8: unkeyed, keyed, and reopening while closing
  The client rejected nonowner body contacts and both peers verified completed animations/open flags on both portal faces
  Evidence: temp/coop-door-host-physics-verified.log and temp/coop-door-host-owner-{host,client}-pass.txt
  The corrected client-owned reverse-direction run also passed all three cases with matching completed state/open flags on both peers and no fixture crash
  Evidence: temp/coop-door-client-physics-verified.log and temp/coop-door-client-owner-{host,client}-pass.txt
  Evidence: temp/coop-door-network-failure-{host,client}.txt, temp/coop-door-relay-failure-{host,client}.txt, temp/coop-proxy-keepalive-quality.log
- Metadata rejection logs identify the failing stage, campaign identity, and ledger sizes
- Sparse forced network exit diagnostics cover leaving the game, window close, consistency errors, and reliable queue failure
- Recovery logs now identify dropped item types/positions and collected inventory deltas
- A startup-restore reproduction found a separate inventory bug: an unmatched client inherited the saved host's plasma, quad, and laser level despite the "spawning fresh" branch
  Both state loaders now snapshot live players before loading the legacy body, and unmatched remaps restore that player's own state with adjusted powerup clocks and object shields
  The two-peer regression failed on the original behavior and passed after correction in the D2 engine running D1 level 8

Validation so far:

- Windows D1 and D2 builds succeeded
- CTest pickup rewards, recovery, robot door, rewind capture, and level-start capture tests passed in both builds (10 tests)
- Android assembleDebug succeeded, including the later exit/recovery diagnostic additions and authored secret-trigger automation hook
- Both automation catalog validators passed
- Existing D2 secret-world LAN runner on descent level 10 is not applicable: it fails to arm D2 campaign travel in a D1 mission
- Added D1SecretSave LAN scenario and optional secret flag to trigger_levelcomplete for a targeted D1 level 10 -> -1 -> checkpoint save/restore reproduction
- `test_lan.ps1 -Game d2 -InitialLevel 10 -D1SecretSave -SkipBuild` passed on two emulators
  Both entered secret -1 in nine seconds, each wrote a different valid checkpoint, and both restored the host-selected checkpoint twice with verified inventory
  Both captured logs contain zero metadata rejection/write-failure messages
  Evidence: temp/coop-fixes-d1-secret-verified.log and temp/coop-fixes-secret-save-{host,client}-pass.txt
- The native D1 control (`-Game d1 -InitialLevel 10 -D1SecretSave`) also passed the transition and both verified checkpoint restores
  Evidence: temp/coop-fixes-native-d1-secret.log
- Scoped mixed-language formatting and lint checks passed after retrying with permission to execute the installed clang-format
- Added NewPlayerRestore scenario: save with the host alone, restart into a lobby, admit a never-saved client, restore, then verify inventory, controls, traffic, and at least sixty seconds of connectivity
- `test_lan.ps1 -Game d2 -MissionFile descent -InitialLevel 8 -NewPlayerRestore -SkipBuild` passed
  The new client kept its own starting inventory, the host recovered its saved equipment, and both peers retained traffic and controls after twelve five-second checks
  Evidence: temp/coop-fixes-new-player-verified.log and temp/coop-fixes-new-player-{host,client}-pass.txt
  This does not establish the initiating cause of the original S21 exit
  The user recalls an in-engine disconnected/failed-to-send-required-packets dialog, narrowing the next reproduction to reliable-packet failure or host dump handling
- The native D1 NewPlayerRestore control also passed, including restored inventory, sustained bidirectional traffic, and controls
  Evidence: temp/coop-fixes-native-d1-new-player.log and temp/coop-fixes-native-d1-new-player-{host,client}-pass.txt
- Both Windows builds and the x86_64 Android build succeeded with the unmatched-player restore correction
  The final scoped formatter/lint pass is recorded in temp/coop-fixes-restore-quality.log

Reproduced save-failure mechanism: StartNewLevel calls coop_campaign_note_normal_level for D1 missions in the D2 engine
That function ignores negative levels, leaving campaign_live.active_level at 10 after entering secret -1
The save writer then rejects the campaign/current-level mismatch
D1 secrets use linear progression rather than D2's revisitable dormant-world archive
The diagnostic run logged `campaign identity mismatch level=-1 campaign_level=10 generation=1`
The first automation attempt omitted the secret-exit networking preamble; the hook now invokes the authored secret trigger
That corrected trigger exposed the score-screen timeout: net_udp_do_frame only repeated end-level packets after reactor destruction or during a flyout
Secret exits can leave the reactor intact, so both score screens waited until their peers timed out
Adding NETSTAT_ENDLEVEL to the periodic-send condition fixed the real two-peer transition
The campaign correction and both capture backoffs are implemented and verified as described above

## Findings

Investigation complete against the main checkout, which matches the logged build 651357958
The IDE's temp/pr80-integrated worktree is a different revision and was not edited
The initial investigation was read-only; subsequent implementation changes and their validation limits are listed above

### Sources and timing

- H: C:/Users/first last/Downloads/debuglog_20261017_112813.txt, S25 SM-S931U, Player99, player 0, original host, 83,933 lines
- C: C:/Users/first last/Downloads/debuglog_20261017_112706.txt, S21 SM-G996U, Player84, player 1, 42,479 lines
- Both run the D2 engine with the D1 descent mission, starting at level 8, then levels 9, 10, and secret level -1
- S21 timestamps are approximately 0.3 seconds ahead of S25, including transit time: the same sync packet hash fb3d71f0 is sent at H:950 11:28:55.135 and received at C:930 11:28:55.447
- Use source timestamps with that offset in mind; the early pickup race spans several seconds and is unambiguous
- The host enabled Network, Game Logs, and Guide-Bot; the client enabled Network and Profiling
- Neither enabled Coop Desync, and the client did not enable Game Logs, limiting disconnect and door diagnosis

### 1. Early duplicated laser: confirmed

S21's death at H:4235 11:31:38.612 produces eight eggs at H:4312 11:31:40.636 / C:2847 11:31:40.928
The host creates recovery IDs 2 through 9, plus a credit row and departure marker

| Event                                         | Evidence                                                         |
| --------------------------------------------- | ---------------------------------------------------------------- |
| S21 collects recovery ID 2                    | C:3078, 11:31:57.890, player=1 used=1 remaining=0                |
| Client send fails with ENETUNREACH            | C:3080-3109, 11:31:58.237, gameplay-send, Network is unreachable |
| S25's laser rises to level 2                  | H:4719, 11:32:00.100                                             |
| S25 also collects recovery ID 2               | H:4722, 11:32:00.108, player=0 used=1 remaining=0                |
| S25 finally receives S21's collection of ID 2 | H:4765, 11:32:01.858                                             |
| S21 receives S25's collection of ID 2         | C:3119, 11:32:02.154                                             |

This is one physical recovery identity granting inventory on two peers, rather than an extra laser egg being created
ID 2 is the only recovery identity with collection events from both players in these exports
S25's later level-3 boost at H:6192 11:33:27.794 has no associated recovery collection and should not be counted as another duplicated death egg

Code cause: d2/main/collide.c:2976 permits a locally live recovery object, applies do_powerup, and then records collection
android/app/src/main/cpp/shared/coop/coop_recovery.c:497 only checks local ledger state
The collection report intersects remaining gear at coop_recovery.c:773, after inventory has already been awarded
Reliable collection delivery prevents ledger resurrection but cannot undo awards independently granted before delivery
The transient network failure enlarged this race window; these logs do not establish that it was the sole source of delay

Fix direction: arbitrate recovery pickup ownership before granting inventory, with a bounded request/grant protocol and handling for denial, retransmission, death, restore, and host migration
Add a two-peer integration case that delays collection traffic while both players touch the same laser

### 2. Initial session disconnect: sequence confirmed, initiating cause unresolved

Both peers complete initial level synchronization around 11:28:42
They then load Players/**rewind**.d2sg and synchronize again around 11:28:55
H:967 reports Player84 was not in the save and is spawned fresh

- C:1367, 11:29:14.852: local departure/drop path runs, eggs=7, life=0/0
- C:1369, 11:29:14.866: multiplayer engine protection is released for pid 7347
- C:1403, 11:29:24.218: remaining shutdown handling records a zero-egg drop for player 0
- C:1461, 11:29:28.164: the old proxy is shut down during rejoin setup, with 4,261 sends, 4,088 receives, and its last receive only 104 ms earlier
- C:1465: a new proxy uses LAN source port 52858 instead of 56121
- C:1492, 11:29:28.351: a new engine process starts, pid 7514
- H:1308, 11:29:40.267: the host times out player 1 after 15.1 seconds without recognized activity
- C:1689, 11:29:42.688: join-in-progress sync succeeds

The old proxy was exchanging traffic and was not silently dead for its entire lifetime
The host timeout is a later observation, not evidence of the initiating failure
There is no logged client timeout, crash stack, or explicit exit reason explaining why pid 7347 left its first session
Do not attribute this to LAN discovery, a proxy crash, or the initial restore without further evidence
The forwarding-loop exceptions at C:1469 follow intentional proxy shutdown and do not explain the earlier native departure
The ENETUNREACH at 11:31:58 is a separate, later incident
The user remembers an in-engine disconnected/failed-to-send-important-packets dialog
The logged revision already records every process_dump reason, but the client export contains none
Together these favor a local reliable-send queue failure over a host-issued removal, without identifying the unacknowledged packet or proving its loss mechanism
Checked a startup-stall hypothesis: deadline checks precede packet reads, but C's profiling windows immediately before departure show about 60 FPS, 23-26 received packets per second, and no long frame stall
That ordering alone does not explain this particular exit; no timeout-policy change was made on this evidence

Next diagnostics: Network, Game Logs, Coop Desync, and Launcher on both devices; log the native multiplayer-exit reason before shutdown
Exercise initial host restore with a joining player absent from the saved roster

### 3. Level-8 door mismatch: plausible code defect, exact door not identified

The logs do not record a matching door identity/state pair or flare hit that proves the observed mismatch
Existing detailed hidden-door messages use COOPLOG and were disabled on both devices
Normal doors do not have equivalent event logging in this build

Two distinct code paths matter:

- Guidebot flare: d2/main/collide.c:803 attributes a companion projectile to Escort_owner_player; d2/main/wall.c:1138 restricts handling to that local player, then wall.c:1188 opens and broadcasts MULTI_DOOR_OPEN
- Guidebot body collision: d2/main/collide.c:148 calls wall_open_door directly for several door cases, without a multiplayer broadcast; wall_open_door in wall.c:395 does not broadcast internally

The body-collision path can create a local door transition without explicitly replicating it
Peer robot positions and local key flags need not trigger the same collision on the same frame
This supports the reported symptom but does not prove that this was the path taken in the session
The straightforward flare-opening path already sends a door event
There may also be local closing-timing differences; the exports do not distinguish them

Fix direction: instrument robot/body and flare door transitions with segment, side, wall state, guidebot owner, and sender, then reproduce with separated peers
Replicate guidebot body openings from a defined authority and verify both keyed and ordinary doors
This code defect is specific to the guidebot-capable D2 path; D1 missions running inside D2 still use it

### 4. Late missing plasma/quad: plasma consumption confirmed; no proven quad disappearance

At the level-10 death around 12:22:14:

- H:64563, 12:22:12.160: Player84 is killed by a robot
- H:64651-64652, 12:22:13.307: S25 warps onto Player84 in segment 156, before the death spew
- H:64680, 12:22:14.164: S21's death generates 20 eggs
- H:64683-64686, 12:22:14.202-14.204: S25 already has plasma, gains energy to 122, and consumes recovery ID 83
- C:31686-31687, 12:22:14.522: S21 receives the terminal state for ID 83 and removes its local object
- C:31831, 12:22:26.972: S21 collects ID 82, consistent with the quad row after three laser eggs in the deterministic D1 death-drop order

The plasma was present in the spew, immediately consumed by the host, converted to energy, and removed on the client
The logs explicitly name plasma on the host; the quad-to-ID-82 mapping is an inference from creation order, not an explicit logged powerup type

Code cause of the surprising plasma conversion: coop_multi_status.c:18-35 considers the current inventories of connected players
The dying player's pre-respawn inventory still includes plasma, so all_coop_players_have_primary can return true during spew
powerup.c:521-524 then converts the host's duplicate plasma to energy
Recovery accounting intentionally treats that conversion as consumption in coop_recovery.c:525
This violates the stated policy that energy conversion is allowed only when every player already owns the equipment
The policy check runs but reads an inventory that still belongs to the dead ship
multi_do_player_explode in multi.c:2568 imports the dying ship's weapon flags to generate its drops, then leaves primary weapons and quad flags in Players[pnum]
coop_recovery_alive in coop_recovery.c:400 clears the old equipment only when the respawn/reappearance is processed
C:31683 and C:31738 show departure marker ID 100 still DEPARTED at 12:22:14.518 and ALIVE only at 12:22:16.838, after the plasma is consumed
The policy needs effective ownership after death across primary weapons, quad flags, and laser level
A connected dead player must count as missing dropped equipment; skipping that player would incorrectly relax the every-player requirement
Preserve the pre-death inventory where drop serialization requires it, and consult authoritative death/life state in duplicate reward eligibility
Add an integration case where a fully equipped host stands on or warps onto a dying teammate before respawn

At the last actual combat death in secret level -1:

- H:77145, 12:29:30.843: Player84 is killed
- H:77494 / C:35853, 12:29:32.908 / 12:29:33.147: 19 physical eggs are created and bound on the client as IDs 124 through 142
- C:35943-35961, 12:29:35.919-12:29:36.673: S21 collects 18 of those 19 identities
- C:36538-36556, 12:30:17.455-12:30:17.457: host-migration diagnostics confirm those 18 rows are taken or credit; ID 137 remains LIVE and bound to object 172
- The host records corresponding collection messages for all 18

There is no evidence here of the final batch being absent or wholesale removed before recovery
The recovery messages omit powerup type, inventory delta, and position, so the uncollected ID 137 cannot be labeled conclusively from the exported messages alone
The batch ordering and collections are consistent with primary equipment being recovered, but that is not an explicit inventory snapshot
The 12:34:53 drop follows immediate engine shutdown and lobby teardown; it is a departure drop, not evidence of another combat death

### Additional issue: repeated metadata-write failure

H:75362 through H:83559 contain 6,852 occurrences of coop_save: failed to write complete metadata
They begin at 12:29:18.221 in secret level -1 and continue through 12:30:17.049, just before S25 exits and S21 becomes host
The client log has no occurrences of this message
This is a confirmed stream of failed save/checkpoint metadata attempts, not proof of a particular disk-space or filesystem error
d2/main/state.c:2879 emits the generic message when state_android_write_save_metadata fails
That function can fail in mod metadata, co-op metadata/campaign encoding, or later Android metadata handling
The enabled logs do not identify the rejecting stage
Investigate this separately and add a logged failure reason plus retry backoff; successful gameplay does not establish successful checkpoint creation

Follow-up source finding: android_rewind.c:403 returns immediately when android_rewind_capture_snapshot fails, before advancing next_capture_game_time64 at line 404
A synchronous metadata failure therefore leaves capture due on every subsequent eligible frame
state_checkpoint.cpp:313 abandons the failed capture and returns 0, allowing this retry path
This is a concrete explanation for the retry flood, although the logs do not explicitly label each failed attempt as rewind
The ordinary co-op autosave scheduler already waits one second after failed submission in coop_save.c:1686, so applying backoff only there would miss the problematic rewind path

## Concrete fix and validation scope

- Equipment policy: centralize effective ownership for duplicate rewards in shared co-op code, accounting for dead lives and inventory still awaiting respawn reset; apply it to primary, laser, and equipment-flag checks used by both engines
  Preserve the every-player policy and the existing legitimate conversion when every player actually owns the item
  Test death before spew, after spew before reappearance, and after respawn, with host/client roles reversed; test plasma, quad, and maxed lasers plus a positive all-equipped case
- Pickup race: serialize recovery collection decisions using ordered peer permissions, epoch, item identity/revision, requesting player life, and a request identity
  Award only after every active peer grants permission, apply each request once, and reject stale replies after death or restore
  Carry remaining contents with permissions so delayed collection reports cannot authorize a second award; define cleanup for disconnect, transition, and migration
  Test overlapping pickup attempts with loss, delayed delivery, duplicate delivery, and reversed arrival order; assert total awarded inventory and remaining world contents
  Do not treat faster broadcasts or reliable transport alone as a fix
- Door replication: reproduce the direct guidebot body-collision opening using separate peer positions, instrument the transition, and broadcast from the existing guidebot simulation owner
  Test owner on host and client, keyed/unkeyed doors, flares, door reversal during closing, and late join
  Do not add unconditional networking inside wall_open_door because receive-side calls also use it
- Save flood: advance a retry deadline after rejected rewind capture, retain previous valid snapshots, and log the failing metadata stage/reason
  Honor the retry deadline even when snapshot_count is zero; the current deadline guard only applies once a snapshot exists
  Inject persistent capture failure to verify bounded retry frequency, then remove the failure and verify capture resumes
  Reproduce D1 level 10 to secret -1 to diagnose the original metadata rejection and verify a successful save can be loaded
- Initial disconnect: add sparse explicit native exit/disconnect reason logging and capture Game Logs, Network, Coop Desync, and Launcher on both peers
  Reproduce host startup restore with a client absent from the saved roster and with delayed loading; verify both peers remain connected beyond timeout windows
  Avoid a blanket timeout increase without evidence identifying the original failure

Existing coverage to extend includes android/tests/test_coop_recovery.c and android/tests/coop_pickup_reward_fixture.hpp, plus the two-peer automation runners and scripts
Protocol changes require two-peer runtime coverage; isolated ledger tests alone cannot reproduce independent inventory awards
The pickup reward fixture covers connected/disconnected phases and missing inventory but does not simulate a dead player whose pre-death flags remain populated
Extend android/tests/test_lan.ps1's existing recovery and respawn-pickup scenarios with the overlapping pickup and pre-respawn conversion cases

## Recommended implementation order

1. Protect recovery gear from duplicate-to-energy consumption during death/respawn
2. Arbitrate simultaneous recovery pickup before granting inventory
3. Replicate and instrument guidebot body door openings
4. Identify and stop the secret-level metadata failure/retry loop
5. Reproduce initial restore/join with client lifecycle and co-op logs enabled

Validation performed: parsed both full exports, compared every recovery collection identity across peers, counted metadata failures, and traced the corresponding code at the logged revision
The initial investigation required no builds; subsequent native and runtime validation is recorded in Implementation progress above
