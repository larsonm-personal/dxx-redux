# Physical co-op LAN adversarial campaign, round 2

Status: complete, 2026-10-04 02:20 UTC; approximately 4 h 27 min active work

The user requested about four more hours, beginning with fresh theories about
failed connections, incorrect drops and stuck cooperative LAN play. Work target:
approximately 21:37 UTC October 3 through 01:37 UTC October 4. Count active
design, implementation, device execution and investigation, not unattended gaps.
The previous goal turn was progress: it completed the first campaign and left
verified fixes, reusable runners and classified evidence.

## Starting evidence and constraints

- Current HEAD: `f42cbecb`; prior report and Python harness have intentional
  uncommitted follow-up changes. Preserve those changes and concurrent work
- Read `AGENTS.md` and `.github/copilot-instructions.md`; do not edit
  `android/outstanding_bugs.md`
- Physical devices: Retroid `JYPR42510121028`, Samsung `R3CR40Q4XPK`
- Both ADB connections and Wi-Fi addresses revalidated; three emulators excluded
- Diagnostic APK SHA-256 on both devices reverified directly from installed APK:
  `e63d95e5f8987898d64e530a68e1e389255785993461dff8e06f75ae19867ece`
- Isolated package: `com.dxxredux.app.nsdtest`; preserve normal installations,
  security settings, personal data and prior campaign evidence
- Samsung initially securely locked; requested normal user unlock and continue
  independent theory/code work while waiting
- Functional gameplay, launcher and ordinary Android lifecycle actions only;
  no malformed traffic, authentication probes, native process signals or
  security-setting changes
- Evidence root: `android/temp/device-adversarial-round2-20261003-1437`
- Use fresh introspection, exact phase triggers, continuous logs, PIDs and
  post-recovery traffic/control checks; distinguish harness failures and
  missed preconditions from product defects

## Fresh theories and discriminating experiments

Prioritize sequences that cross a state-machine boundary, rather than repeating
the previous round's already-passing single departure/return cases.

| Theory                                                  | New sequence                                                                                                        | Evidence that distinguishes a bug from an allowed outcome                                                                         |
| ------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------- |
| An abandoned approval blocks another attempt            | Reach host approval, cancel or force-stop joiner, change callsign, retry immediately and after expiry               | Host prompt retires within its 12-second bound; new attempt receives a fresh prompt; one approval admits exactly one player       |
| Approval expiry poisons later admission                 | Ignore initial prompt until rejection, retry twice, finally approve                                                 | Clear failure/cancel path, no ghost slot or permanent refusal; host PID unchanged                                                 |
| Pilot identity and display name become confused         | Return with a renamed pilot, same-name peer, or case-only name change                                               | Expected explicit rejection or distinct accepted slots; original host is never displaced and a later ordinary join still works    |
| A menu stops network liveness                           | Hold game menu or automap on each role well past UDP timeout, including while peer departs/returns                  | Healthy peer is not dropped merely for UI state; after real departure there is bounded migration or exit and recoverable controls |
| A pending transfer survives world/authority replacement | Cancel partial transfer, host leaves or advances, retry against surviving/current host                              | Old destination never commits, admission does not stay busy, peers agree on host, level and visit                                 |
| Two partition survivors cannot reunite                  | Separate Wi-Fi long enough for independent solo authority, then explicitly choose one survivor and rejoin           | No duplicate connected pilots or contradictory master after deliberate reunification                                              |
| Repeated host swaps leave stale routes                  | Swap host several times, alternating discovery and normal launcher re-entry                                         | Advertised migrated port is used, both endpoints advance traffic and retain controls after every swap                             |
| Lobby identity replacement is mistaken for reconnection | Restart or recreate host at same IP while client still holds old membership, without manually clearing client first | Old lease retires; normal join selects the new identity and readiness/chat belong to that identity                                |
| Lobby lease expiry leaves capacity reserved forever     | Cross 10-second contact loss and 120-second reconnect grace, then return with same and changed pilot                | Slots become available, stale readiness is cleared, client can rejoin and launch                                                  |
| Launch preparation cannot recover from cancellation     | Start lobby launch, cancel/leave one side before native entry, recreate and launch                                  | No orphan launch event, duplicate engine or endless preparation; next session launches without data reset                         |
| One-sided launcher return retires a healthy game        | Enter launcher through normal in-game action, select return or another multiplayer session, then rejoin             | Return preserves the existing process when appropriate; replacement cleanly retires the old game                                  |
| A failed restore leaves same-process admission state    | Fail restore, return to menus, start a new session or join a survivor before cold restart                           | Old restore/admission flags do not contaminate the new session; no forced app reset required                                      |
| Gameplay interruptions collide with synchronization     | Join or leave during automap, reactor escape, scoring or secret travel                                              | Either current-world admission or bounded deferral/exit, followed by usable networking                                            |
| Device role or content hides a timing bug               | Reverse host hardware and use D1, D2 and D1-in-D2 for reduced reproductions                                         | Same failing transition identified independently of render speed or a single content path                                         |
| Prior startup ANR is repeatable under ordinary ordering | Lobby Wi-Fi return followed by app restart and engine switch; inspect window and process exits                      | A repeated app-specific failure has a causal trace; a transient OS focus failure stays separately classified                      |

The source review finds two relevant distinct timers: approval requests reject
after 8 seconds of same-pilot retries, while the main network frame clears an
abandoned prompt after 12 seconds. Gameplay UDP timeout is 15 seconds. Lobby
contact loss and reconnect grace are separate from both. Tests must observe
actual phases and elapsed contact loss, not assume a requested delay equals
the total outage including Android startup or Wi-Fi reassociation.

## Execution approach

1. Complete source/fixture review and select the highest-value new sequences
2. Extend the existing explicitly selected physical campaign runner; avoid
   introducing a second independent device-control framework
3. Use existing real UI actions and functional fixtures, adding observability
   only when evidence requires it; save reusable engine sequences in JSONC
4. Run selected cases serially, start with small reductions, vary role and
   timing after a failure, and fix only causes supported by logs/state
5. After a product change, build both Android engines, install only the isolated
   diagnostic package, and complete focused physical regressions plus adjacent
   cases that could reveal a broken invariant
6. Maintain a per-attempt classification and record unexecuted ideas honestly
7. Finish scoped mixed-language formatting, catalog validation and cleanup;
   verify Wi-Fi restored, no campaign processes left, and original data intact

## Results

No prior-round result counts as a new-round pass. Interim evidence through
22:16 UTC:

- Retroid menu/automap fixture preflight passed in a solo co-op host
- Failed initial join, Exit to Launcher, and new co-op host passed for D1,
  D2 and D1-in-D2 within one unchanged launcher process. Evidence:
  `retroid-failed-join-launcher-v4/result.json`
- Failed initial join followed by hosting through the native Multiplayer menu
  passed for D1 and D2 without replacing the native process. Evidence:
  `retroid-native-recovery-v3/results.json`
- Samsung remains securely locked. Paired native gameplay is pending normal
  unlock. Launcher networking can run behind the lock screen, so separately
  classified background-lobby tests preserve the lock before and after each
  case and do not attempt gameplay
- Replacing Retroid's lobby while Samsung retains the old membership passed:
  client joins the new identity at the same address, with unchanged client
  process and working readiness/chat. Evidence:
  `background-lobby/retroid-host-replace`
- The first rename attempt expected a disconnected lease to disappear within
  15 seconds. Actual state correctly retained a disconnected player during
  reconnect grace. Split normal Leave from stop-discovery-and-return, and added
  `lan_leave_lobby` automation dispatch to the existing UI service method
- Harness corrections: quote empty/string ADB broadcast extras (an empty D1
  mission had shifted subsequent arguments); use full launcher introspection
  for process state; resolve game-conditional fixtures with existing
  `Resolve-TestScript`; handle D1's automatic selection when only one mission
  is installed; do not depend on disabled optional network-category logs
- Observation APK SHA-256:
  `71c3faa1131ff23b0cbd22769be0137f3e92a97e3c82cd989393917087e04000`
- Scoped formatting passed. Both catalog validators passed with 356 support
  scripts and 283 top-level runner entries

### Confirmed background-service crash and first regression

`ForegroundServiceStartNotAllowedException` killed the launcher when a LAN
action tried to restart its foreground service after the app lost foreground
eligibility. Reproduced in Samsung's locked background launcher while leaving
and rejoining, while recreating a hosted lobby, and on Retroid after Home plus
a 16-second wait followed by an artificial queued join. These are functional
lobby actions; no security settings or packet contents were altered.

Evidence:

- `background-lobby-rename/retroid-host-rename`: Samsung join crash
- `background-probe-diagnostic/samsung-host-replace`: Samsung host crash
- `background-home-rejoin-before/samsung-host-home-rejoin`: Retroid Home crash
- `background-home-rejoin-after-v2/samsung-host-home-rejoin`: complete PASS,
  same launcher PID, refusal logged/deferred, service foreground after resume,
  readiness transitions and directed chat working

The shared launcher lobby service now catches only Android's specific
foreground-start refusal, retains usable lobby state, and retries service
startup on the next resume. Other IllegalStateExceptions still propagate.
Formatting, the Android build (both native engines), and existing lobby/service
lease unit tests passed. First fixed APK SHA-256:
`fb83d6c7b3d504b652dc83cce4d2eff39ae2fd556b10dfd9a231e76304c1a5fa`.

The first post-fix readiness check exposed another harness issue: cold start
used an intent without MAIN/LAUNCHER, so returning through the launcher icon
created a second Setup activity. Both handled debug broadcasts with different
in-memory pilot names. Cold starts now use the real launcher intent; the full
service-recovery regression then passed. This is classified separately from
the service crash.

### Artificial replacement-probe candidate

The replacement-lobby API sequence sometimes abandons its new manual probe
when a rejection for old membership clears that membership. Targeted logging
in `background-probe-repeat-2/retroid-host-replace` shows unchanged request
generation, discovery active, app not marked backgrounded, and only
`joined_before` changing to `joined_now=null`. The replacement announcement is
present afterward. Other runs passed.

A closer UI review showed `LanDiscoveryTab` normally displays a joined-lobby
view requiring Leave before another Join-by-IP flow. Therefore this remains an
artificial API-sequencing candidate, not a confirmed ordinary-UI blocker. No
probe cancellation or identity rules were changed.

### Continued execution

The checked-in runner now supports explicit `--lobby-only` and
`--background-lobby-serial` selection, records lock state, and excludes native
gameplay cases from that mode. Samsung remains locked; paired gameplay awaits
normal unlock. Background-lobby successes must not be reported as paired
gameplay successes. Long Wi-Fi expiry and cancellation cases are in progress.

### Execution through 22:57 UTC

- Leave/rename, pending launch cancellation, and repeated Join passed with
  Retroid hosting and with Samsung hosting (six cases), using the fixed APK.
  Evidence: `lobby-sequences-fixed` and `lobby-sequences-fixed-reverse`
- A 145-second Retroid host Wi-Fi outage passed full lobby lease expiry and
  rejoin. A 12-second Samsung client Wi-Fi outage passed after teaching the
  harness to wake the display before reconnecting; the secure keyguard remains
  intact. Samsung can defer Wi-Fi association while its display sleeps
- A 145-second Samsung outage failed the first new Join after lease expiry.
  An IPv4 address appeared before application sends stopped reporting
  ENETUNREACH; another Android association followed. This is an unresolved
  transport/environment observation, not yet a product defect. Next reduction:
  record bidirectional reachability and retry ordinary Join after the route
  settles, without restarting either app. Evidence:
  `lobby-fixed-rp-host-v2/01-lobby-client-wifi-145`
- A D2 host entered the real 20-minute background deadline at 22:58 UTC.
  Samsung is a launcher discovery observer only. The test records native state
  and process continuity before expiry, advertisement state afterward, the
  resumed native menu, and the ability to host a fresh game

### Additional boundary hypotheses

1. **Expired native host remains advertised.** The timeout callback disconnects
   the runtime bridge; LAN host cleanup currently appears in launcher resume.
   Observe announcements on the other device before bringing the host back.
   A fresh announcement after native disconnect would distinguish a live stale
   advertiser from a harmless discovery cache entry
2. **Foreground at the deadline boundary.** The deadline token is invalidated on
   resume, but the five-second shutdown grace callback has separate ownership.
   Test a normal return immediately before and after expiry, recording both
   service leases and native state. Do not accelerate the production timer
3. **Menu rehosting after timeout.** The native process can survive a disconnect.
   Rehost through its Multiplayer menu and inspect whether the service/runtime
   registration follows the new session. Compare with Exit to Launcher/rehost
4. **World changes immediately after admission.** Previous coverage interrupted
   transfer before commit. A distinct window lies between committed join and
   first normal PDATA confirmation. Use normal level exit/rewind actions near
   that boundary, checking matching visit IDs and actual packet advancement;
   do not freeze native processes or inject packets
5. **Network route returns after UI retry is exhausted.** Distinguish DHCP address
   visibility, bidirectional reachability, discovery reception, Join ACK, and
   ready/chat delivery. A failed first attempt during association is different
   from permanent lockout after connectivity is restored

These are hypotheses until an execution provides supporting evidence. Keep
artificial service-API sequences, harness failures, environmental failures,
and ordinary user-flow defects separate in the final assessment.

### Confirmed stale LAN advertisement after background expiry

The D2 real-deadline run reproduced a second product defect. The service expired
at device time 16:18:09.419, and the engine reported its completed disconnect at
16:18:09.858. Nevertheless, the launcher continued broadcasting the same lobby
as `in_game` every three seconds through 16:18:31. Samsung's post-deadline
introspection still advertised that host. Cleanup happened only after returning
to the launcher. Evidence: `dormancy-d2`, especially
`advertisement-after-deadline.json`, `native-after-deadline.json`, and both logs.

The first runner result says PASS for the checks it then enforced (no premature
drop, native disconnect, and fresh hosting afterward). Advertisement retirement
was captured but not asserted yet; evidence review found the defect. The runner
now asserts that the old host is retired before resume. Do not count that first
result as a complete deadline/discovery pass.

Fix under validation: the foreground service notifies LAN ownership when an
actually connected runtime disconnects. The lobby service retires only a
started hosted game, preserving waiting lobbies and launch preparation. This
also covers a client that adopted hosting after migration, without relying on
its original runtime host flag. Both native engines will be rebuilt, and D1/D2
will each repeat the real deadline with advertisement retirement required.

The locked Samsung discovery-only observer became idle during the first run.
Periodic introspection restarted useful observation without changing Retroid's
timer; subsequent runs sample both devices throughout. This observation mode
is explicitly artificial and remains separate from two-player gameplay.

### Validation status at 23:39 UTC

- Runtime-exit fix built both Android engines; deadline, runtime IPC, service
  lease and launch-preparation unit tests passed. Installed on both devices:
  `872b38ba878deda206c715d5d6c2de06aa6de8591b3bf72ef18a2bd8b3187320`
- `route-retry-before-runtime-fix` stopped as ENVIRONMENT_BLOCKED because the
  Samsung did not regain IPv4 within 40 seconds. The first fixed D1 deadline
  attempt also failed setup with ENETUNREACH before either game launched
- Keeping the Samsung display awake using normal wake/inert Shift key events
  has stabilized the route so far. This preserves `showing=true, secure=true`,
  changes no security or timeout settings, and is documented in
  `observer-display-maintenance.jsonl`. It is a test precondition, not a fix
- `dormancy-d1-fixed-v2` entered background at 23:28:34 UTC and remains active
- Further queued cases: native Abort Game advertisement retirement; D2 normal
  deadline with new-lobby recovery in the same launcher process; deadline while
  a game menu is open; foreground return just before the original deadline;
  24 lobby role changes across all three content modes without app restarts

### Native Abort Game variant and session ownership

The same stale-advertisement class also occurs after ordinary Abort Game while
remaining in the native main menu. Both D1 and D2 reproduced it for more than
40 seconds with an active Samsung lobby observer (`native-abort-d1` and
`native-abort-d2`). This is a normal menu action, without background expiry.

The follow-up fix uses actual network lifecycle hooks: setting native network
mode informs Android that a session started; closing network sockets informs
it that the session ended. Each engine has two guarded calls, with shared JNI
and launcher ownership logic. Native-menu rehosting reacquires its service;
role updates avoid disconnecting an existing runtime binding. Android's
foreground-start refusal is deferred until resume here as well. The Activity
stops only the game-service lease it owns.

Both engines and the focused unit tests built successfully. Installed APK:
`30ae7c36ba4a61e6de898b560d77c877d067164a58d681840c9771113b3fa1bb`.
Build warnings point to existing unused variables and array comparisons in the
UDP engines; no warning points to the added hooks.

- `dormancy-d1-fixed-v2`: PASS for native expiry and advertisement retirement
  on the preceding runtime-exit build. Samsung was converted to an active
  waiting-lobby observer before expiry because idle discovery lost Android
  network permission. Retroid stopped hosting about 10 ms after its completed
  engine disconnect
- `native-abort-d2-fixed`: PASS on the latest build: old advertisement retired,
  native-menu rehost kept the same game PID, its foreground service stayed
  active during a 90-second Home soak, return worked, and a second abort worked
- D1's matching latest-build regression and both real-deadline variants remain
  in progress. The observer now hosts its own waiting room so its ordinary LAN
  service provides stable networking; no secure lock is dismissed

Latest-build execution through 00:20 UTC:

- `native-abort-d1-fixed`: PASS, including same-process native rehost, service
  still foreground after 90 seconds behind Home, normal return, and second abort
- `retroid-native-recovery-final`: D1 and D2 PASS for failed initial join followed
  by native-menu hosting, without replacing the engine or launcher process
- `dormancy-d1-menu-final`: ongoing real deadline with the native game menu open
  and Samsung hosting its own waiting room as the active discovery observer
- Remaining planned timed runs: D2 ordinary deadline with full new-lobby
  recovery, and foreground reset just before the original deadline

### Confirmed menu-open background-disconnect crash (00:28 UTC)

`dormancy-d1-menu-final` FAIL is a product crash, not a launcher-resume failure.
The real 20-minute deadline correctly disconnected and retired the advertisement.
On foreground return, the Retroid native process died with SIGSEGV in
`strlen -> d_strdup -> nm_string -> draw_item -> newmenu_draw`. Android exit
information confirms native crash, signal 11. Samsung remained a locked launcher
observer with its own waiting-lobby foreground service.

Source tracing explains the stale menu: closing `Game_wind` invokes
`longjmp(LeaveEvents, 0)`, bypassing nested synchronous `nm_messagebox` loops.
Their windows survive while their stack-backed items no longer exist. The
normal forced-disconnect branch previously closed Game_wind directly even when
a child menu remained above it.

Shared Android fix under validation: before a forced multiplayer close, close
one child window from the top of the game window's stack, then defer game exit
to a later frame. This allows the nested caller to unwind normally. Both
engines use the guarded shared helper. It respects window close refusal and
leaves unrelated menus below the game window alone. A diagnostic records each
child close. This replaces the lower-priority deadline-reset test with full
D1 and D2 menu-open deadline regressions.

### Paused at 00:39 UTC

The goal service reported status `paused` when checked. Active test session and
observer display-maintenance helper were stopped immediately. Diagnostic apps
were force-stopped and Wi-Fi enabled on both physical devices; original apps
and data remain untouched. The requested four-hour round is not complete.

The menu-unwind fix built both engines and passed the focused unit tests
(`build-menu-unwind.log`, 56 seconds). Scoped formatting passed. Its D1 real
20-minute regression began background observation at 00:38:26 UTC but was
interrupted by the pause before expiry. This is INCOMPLETE, not a pass or a
product failure. D2's queued regression did not start. The fix remains
unverified on the device until both full menu-open regressions finish.

Resume priority: D1 and D2 menu-open background deadlines, including no native
process replacement, retired advertisement before foreground return, usable
main menu and subsequent hosting. Then repeated lobby role turnover and route
recovery if time permits. Two-player native cases still require the Samsung
to be unlocked normally by its owner; no secure lock was bypassed.

## Extrapolation from both campaigns, resumed 00:43 UTC

The preceding paused turn was progress: it isolated the native menu crash,
implemented a shared fix, built both engines and preserved the incomplete
regression honestly. The goal resumed with a request to reason from the bugs
already found. Current source, installed APK hashes and device lock state were
rechecked. The user has independently modified `outstanding_bugs.md`; leave
that file untouched.

### What the failures have in common

There are four concrete failures across the two campaigns, grouped into three
useful families rather than treating the count as important:

1. **Operation state outlives its owner.** The completed join envelope blocked
   a later restore's SYNC; an ended native game kept its LAN advertisement.
   Both first operations appeared successful. Failure emerged during a later
   operation that reused the process, slot, address or launcher
2. **An asynchronous action outlives its valid Android context.** A queued lobby
   action tried to acquire a foreground service after the Activity lost the
   right to start one. The next resume must repair the service lease without
   discarding valid membership or creating a duplicate runtime
3. **UI lifetime differs from session lifetime.** Forced game exit longjmps
   past synchronous menu callers, leaving stale windows with abandoned stack
   data. This class can affect exits other than the one timer that exposed it

The next campaign should therefore combine operations, retain processes, and
assert the cleanup boundary. A cold launch after every test masks the common
failure mechanism. Each case has a second, independent valid operation that
must succeed; a clean error dialog alone is insufficient.

### Prioritized experiments derived from those causes

| Priority / ID | Sequence and exact trigger                                                                                                                                   | Required result and evidence                                                                                                               | Current coverage / constraint                                                                                                                       |
| ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| P0 M1         | Open game menu, Home, wait real 20-minute expiry, foreground, host again                                                                                     | Child window closes before game exit; same native PID reaches usable main menu; old advert absent before resume; next hosting works        | D1 crash fixed; D1/D2 menu-exit regressions PASS (timing caveat and correction below)                                                               |
| P0 M2         | Repeat forced exit with automap, Options submenu, save/load picker and an editable text field; vary one vs two nested menus                                  | No stale window, input capture or paused clock after exit; no crash on first draw or on later menu navigation                              | Planned; paired host loss is faster than a timeout once Samsung is unlocked                                                                         |
| P0 J1         | Complete late join; at first confirmed gameplay immediately restore, travel normally, enter/return secret level, or rewind; repeat each after another rejoin | No old join attempt suppresses new-world SYNC; current visit/epoch accepted, barriers settle, actual controls and bidirectional PDATA work | Restore regression passed in prior round; immediate travel/secret variants remain unrun                                                             |
| P0 J2         | Leave during approval, object transfer, commit-before-confirmation or extras; retry same pilot, then another pilot, without host restart                     | Old reservation expires or cancels; retry gets a fresh attempt; no stale slot or busy transfer; subsequent restore works                   | Prior partial cancellation passed; phase boundary matrix and alternate identity remain unrun                                                        |
| P0 R1         | Fail co-op restore, return to menu in same process, host fresh game, admit peer, then perform successful save/restore                                        | Failed restore state does not poison admission, pause state, campaign visit or later save; both peers' recovered inventory matches         | Prior fresh-game/cold-resume tests cover part; successful second admission plus restore needs paired execution                                      |
| P1 A1         | End session A, immediately create B in the same launcher; delay old leave/timeout cleanup using ordinary Home, Wi-Fi loss and return ordering                | A disappears; B retains its own identity, service and membership; no cleanup from A retires B                                              | Native abort/rehost and 24-cycle turnover PASS; delayed cleanup boundary remains unrun                                                              |
| P1 A2         | Adopt migrated hosting, admit original host, change level, then Abort Game; reverse roles and repeat                                                         | Discovery address/port and current level match the new authority; service stays alive; migrated advert retires on exit                     | Earlier basic migration passed; migration plus lifecycle/advertisement chain remains unrun                                                          |
| P1 A3         | Home just before a join/start completes; return, leave, immediately rejoin; repeat through launcher/native entry                                             | Specific Android startup refusal is handled; one live service lease after resume; no duplicate game, lost membership or crash              | Launcher Home/rejoin fixed regression passed; native-entry boundary variant remains unrun                                                           |
| P1 T1         | Foreground at about 1190 seconds, Home again, cross the old 1200-second deadline                                                                             | Original alarm cannot disconnect current game; only the new background interval counts; same PID remains connected                         | Exposed an initial-resume regression; corrected build passed the real reset test                                                                    |
| P1 T2         | At actual expiry, return and begin a replacement session during the five-second engine-disconnect grace; keep LAN waiting-room lease alive                   | Old shutdown callback cannot disconnect replacement runtime or clear its game lease                                                        | Source-review candidate: delayed forceBackgroundShutdown has no captured session identity; no product repro yet                                     |
| P1 U1         | Host loss or restore failure while save picker/options/briefing is active, followed by a new game                                                            | Direct Game_wind close paths also unwind UI and release input/timing state                                                                 | Source-review candidate: briefing/endgame and restore-failure paths can close Game_wind outside multi_do_frame; not automatically covered by M1 fix |
| P2 L1         | 24 alternating launcher host/client roles across D1, D2 and D1-in-D2; alternate guest-leave/host-stop order                                                  | Fresh lobby IDs, empty retired membership/readiness/launch state, same app PIDs, ready and chat delivered both ways each cycle             | 24 cycles PASS; launcher-only behind Samsung lock                                                                                                   |
| P2 L2         | Retroid client loses Wi-Fi for 145 seconds, regains IPv4, then up to three normal Join attempts without restart                                              | Old lease gone, route and lobby recovery distinguished, next readiness/chat exchange works                                                 | Retroid client PASS after 145 s outage, on first Join attempt; same app PIDs                                                                        |

### Additional source evidence and why it changes priority

- `net_udp_join_wait_transport.h` retires a committed join on confirmed player
  traffic. The narrow interval after commit but before confirmation deserves
  explicit testing, especially a world transition during that interval. Do not
  weaken visit/attempt checks to make a test pass
- `MultiplayerForegroundService` uses a generation for the 20-minute alarm,
  but schedules a separate five-second shutdown callback. Foreground return
  cancels deadline callbacks; the grace callback needs its own experiment with
  a replacement session. Normal service destruction may already cancel it,
  so a source suspicion alone is not evidence of a bug
- Host migration takes a distinct Kotlin broadcast/advertisement path. The
  original client/host role, current native authority and advertised authority
  must agree after migration. A later Abort or timeout is a stronger probe than
  observing only the initial migration notification
- The menu-unwind fix covers multi_quit_game handling in multi_do_frame.
  Briefing/endgame and failed-restore paths also directly close Game_wind.
  Some already close their own waiting windows; that does not establish that
  every nested child is gone. Test those paths rather than broad speculative
  replacement of all close calls

### Test construction and stopping rules

Use observed phase transitions, not sleep duration alone. Capture lobby ID,
join attempt, visit/epoch, authoritative host, process IDs, menu/front-window
state, service foreground status and per-direction traffic before and after.
When required state is not exposed, add narrow read-only introspection first.

For boundary cases, start with before/during/after the observed transition;
then jitter ordinary actions over several repetitions. Reuse the host process
and test pilot unless a deliberate role/identity change is the variable. Keep
normal valid game traffic; no packet mutation, authentication probing, native
signals or security-setting changes. The endpoint check is playable recovery:
matching level/authority, settled barriers, advancing traffic, working controls,
and a successful later operation. Do not demand transparent reunion after a
partition when an explicit rejoin is the intended behavior.

Run deterministic reductions first. On a failure, preserve all evidence, repeat
once under the same preconditions, then reverse hardware or content before
broadening timing. Separate product failure, environment blockage, harness
failure and unexecuted design. Never count a fresh process or app reset as
same-process recovery.

### Remaining execution allocation

Resume time is 00:43 UTC with about 2 h 49 min charged before pause. Use the
remaining roughly 70 minutes for two real menu-open deadlines (about 44 min),
24-cycle launcher turnover and route retry (about 10-15 min), then evidence
review, formatting and handoff. Source-based hypotheses above form the next
paired campaign; this time-bound round cannot honestly claim their execution.
Samsung still reports secure keyguard showing, so native paired tests remain
pending normal owner unlock. Its current role is an active launcher observer,
not a second native gameplay participant.

A particularly narrow J1/J2 variant deserves the first paired slot: the current
join-retirement helper requires confirmed gameplay traffic. Once transfer is
committed, the obsolete-transfer cancellation predicate no longer applies.
Advance the world after commit but before that first confirmation, then retry
admission or restore. The experiment must determine whether the existing
transition barriers already exclude this ordering; source inspection alone
cannot establish reachability. If it is reachable, the old visit filter may
again outlive its valid operation. Add read-only commit/confirmation/visit
observability rather than forcing an impossible state or removing the filter.

### Resumed D1 menu regression: PASS, 01:05:54 UTC

`dormancy-d1-menu-unwind-resumed` completed the full real deadline on APK
`0a96dfb74fbe29f3dc84222b7f3d7cd223d6d870b5a845f2c5923304902b2185`.
The native log records child-window close before completed engine disconnect.
Foreground return reached the main menu in the same native PID, the old host
advertisement was absent before resume, and a subsequent launcher-hosted game
worked without replacing the launcher process. D2's matching run has started.

The worktree was committed externally during execution; HEAD observed at
01:00 UTC was `6f53de61`. No commits were made by this campaign agent. Test
claims remain tied to the recorded installed APK, not unrelated concurrent
publishing changes.

### Resumed D2 menu regression: PASS, 01:27:08 UTC

`dormancy-d2-menu-unwind-resumed` passed the same full real-deadline regression.
The child-window close precedes completed engine disconnect; native PID 24616
survived foreground return (D1's corresponding PID was 15343). Both engines
returned to usable native menus, retired the advertisement before foreground
return, and subsequently hosted again in the existing launcher process.
This validates the menu-crash fix for the reproduced game-menu path; the
nested-menu and other exit-path hypotheses remain separate planned coverage.

Remaining run selected at 01:27 UTC: D2 foreground return near 1190 seconds,
then re-background across the old deadline; afterwards 24 lobby role changes
and a 145-second Retroid-client Wi-Fi outage followed by ordinary Join retries.
The deadline-reset fixture now waits for the engine's background acknowledgement
and requires observed native foreground state on return. Earlier snapshots
showed a normal short acknowledgement delay after Home; treating the first
snapshot as already backgrounded would have been a harness error.

Additional advertisement variant (A4): launch preparation succeeds, but native
startup fails before any connected runtime is registered. Combine an ordinary
launch failure with Home before completion, then observe from the peer and
start a valid game afterward. Require the advertised in-game room to retire,
a clear local failure, and successful next hosting without a launcher restart.
This probes the other side of the new `wasConnected` cleanup guard; do not
assume the completed-runtime regressions cover pre-registration failure. Use
an existing functional startup failure fixture or a backed-up diagnostic-only
missing-content precondition, never damage the user's ordinary game files.
This case is planned, not reproduced or fixed.

### Regression caught by the extrapolated timer test, 01:48 UTC

`dormancy-d2-reset-final` FAIL is a confirmed regression introduced by this
round's runtime-lifecycle refactor. The service began its background deadline
at native Activity creation (device 18:27:58.881). The actual background
transition followed at 18:28:09.823. The initial onResume notification was
inside `if (gameStarted)`, so it did not cancel the prematurely started timer.
Expiry occurred at 18:47:58.891, before the requested foreground reset could
complete, despite beginning that reset at 1190 seconds after Home.

The ADB command timestamps exclude a long launch-command delay: wake and launch
completed in about two seconds. Native foreground was then observed, but the
old session had already disconnected. This is not a failed precondition or
an operating-system route problem.

Correction: always publish foreground visibility from onResume, independently
of whether the engine has started. Keep native-only resume calls under their
existing engine-start guard. Rebuild, verify first-resume cancellation in the
new log, and repeat the real deadline-reset test. Extend modestly beyond the
approximate four-hour target to validate this introduced regression. The raw
failure is preserved. The menu-close regressions remain valid for their own
cleanup assertions, but their old-build expiry timing was also measured from
this premature deadline; do not use them as proof of exact timeout duration.

### Launcher reuse and route recovery: PASS, 01:55 UTC

- `lobby-final-turnover-route/01-lobby-role-turnover`: 24 cycles PASS in 137 s.
  Roles alternated across D1, D2 and D1-in-D2 content, both launcher PIDs stayed
  unchanged, each room had a fresh identity, retired membership/launch state
  cleared, and readiness/chat worked in both directions each cycle
- `01-lobby-client-route-retry`: PASS in 172 s. Retroid client Wi-Fi was off
  for 145 s, the expired membership cleared, IPv4 returned, and the first
  ordinary Join attempt succeeded. Both PIDs survived and readiness/chat
  worked afterward. Samsung was the waiting-lobby host behind its secure lock

These launcher-only cases ran on the prior recorded APK while the independent
MainActivity visibility correction built. The new build and focused tests
passed in 1 min 28 s. Install it on both devices before the final reset test.

Final timer build SHA-256:
`1e581948eae1d5304917d9beaa18fc06afe8dedf21abb4af2eff89e9c6f47b33`.
The first corrected-build attempt stopped during observer setup because a
Samsung introspection broadcast timed out after 20 s. No corresponding new
crash or ANR was recorded; cause remains unclassified. It did not reach the
timed test and is not counted as a timer failure or pass.

`dormancy-d2-reset-fixed-v2` began timed observation at 01:58:49 UTC. Its startup
trace confirms foreground reset at device 18:58:36.625, native session start
at 18:58:37.013, actual Activity stop at 18:58:49.966, and only then the first
background deadline start at 18:58:49.970. This corrects the ordering observed
in the failed regression. Full near-expiry return validation remains running.

### Faster checks suggested by the validation regression

Expose read-only runtime deadline state (armed, background start time and
generation) through the existing diagnostics/introspection channel. A short
startup test should then assert that no background deadline remains armed while
the native Activity is foreground, even before engine initialization completes.
Home during loading should arm it, and resume should cancel it regardless of
engine readiness. Keep one real 20-minute device test for alarm delivery and
process-lifecycle behavior; use these fast ownership assertions for repeated
ordering variations. This observability improvement is planned, not added in
this round. It avoids replacing a real lifecycle test with a source-text check.

### Final corrected timer regression: PASS, 02:19:28 UTC

`dormancy-d2-reset-fixed-v2` returned at 1190 seconds with the same native
process still in network gameplay and observed native foreground state. It
then acknowledged background again and survived beyond the old deadline to
1235 seconds. The trace records foreground reset, a new background deadline,
and no expiry/disconnect during the old deadline crossing. The final return
and automation introspection also worked.

The observer retained the same lobby identity before and after this sequence,
and saw it in every post-reset sample. One earlier sample at 1032 seconds had
an empty discovery list; the next sample recovered the same identity. Keep this
as an unresolved discovery-continuity observation, not a confirmed new defect.
It did not coincide with the deadline reset and did not interrupt native state.

### Final handoff and completion audit

The resumed goal used approximately 4 hours 27 minutes of active work, excluding
the pause. The extension beyond the approximate four-hour target completed a
real-duration regression for the timer-start bug introduced during validation.

- Fresh theories, causal extrapolation from both campaigns, prioritized
  sequences, phase triggers, evidence requirements and unexecuted cases are
  documented in the plan above
- Three reproduced failure classes in this round were corrected: background
  foreground-service startup crash, stale native-game advertisement, and the
  abandoned game-menu window crash. The subsequent initial-visibility timer
  regression introduced by the refactor was also corrected and verified
- Physical evidence includes D1/D2 menu-exit regressions, D1/D2 native Abort and
  same-process rehost, failed-join recovery, both-role launcher sequences,
  24 lobby role changes, full-grace Wi-Fi recovery, and corrected timer reset
- Android builds for both engines and the focused runtime/deadline/lease/launch
  unit tests passed. Automation catalog: 94 standalone JSON, 357 support
  scripts, 185 standalone PowerShell; master catalog: 283 entries, PASS
- Samsung remained securely locked. This round used it for launcher networking
  and observation; the newly planned native paired cases were not executed.
  The prior campaign's paired gameplay evidence remains in its separate report
- Preserve the original raw failures and classification reviews, including the
  unresolved Samsung setup-introspection timeout and transient discovery gap.
  The earlier campaign's isolated startup ANR also remains unclassified
- Diagnostic apps and observer helpers stopped; both physical Wi-Fi interfaces
  are up with their LAN addresses. Ordinary installations and security settings
  were not changed. Unrelated concurrent CI/publishing work was left alone

Highest remaining paired priorities: the committed-but-not-yet-confirmed join
boundary followed by world change; failed restore followed by fresh admission
and successful restore; nested UI during other forced-exit paths; and migrated
hosting followed by rejoin, level change and retirement. These are planned
investigations, not claims that a failure has already been established.
