# Joining during briefings and level transitions

Status: implemented with focused validation complete, 2026-09-30

## Implementation progress

- Consolidated the nine new join support scripts into `test_join_phase.jsonc`,
  selected by the existing LAN runner through the shared parameter resolver.
  These are in-session actions, not standalone tests; they share the running
  host/client and do not add app setup/teardown. Prefer the `transfer` scenario
  for routine transition coverage: it includes escape, flyout, scores and the
  destination briefing in one session. Keep `escape` and fresh `scores` entry
  variants available for focused diagnosis rather than running all three for
  every change
- Shared host phase/deadline replies and client countdown/activity overlays are
  implemented. Briefing waits use the actual native pages/videos and inherit
  the host deadline without joining the current readiness roster
- Incomplete transfers carry attempt/world-visit tags. A phase change unwinds
  sync, abandons old objects and reloads the authoritative destination through
  the same Join action. New player identity is published with successful SYNC
- Both engines pass the Android x86_64 build and Windows builds. Reconnect-auth,
  transition-policy and gameplay-fence native tests pass. Scoped mixed-language
  formatting/lint passed (`temp/join-code-quality.log`)
- D1 and D2 `-JoinTransitionCase transfer` passed: receive a partial mine, destroy
  the reactor, follow escape/flyout/scores, show level 2's actual briefing and
  reach two-player level 2 gameplay. The completed-level roster stays unchanged
- D2 `-BriefingCase first_join -BriefingJoinDelaySeconds 85 -BriefingJoinAction skip`
  passed: actual briefing, inherited decreasing timer, unchanged host roster,
  Skip without cancel, host-driven closure and subsequent gameplay
- D2 testing found a stale status reply after slower local loading could consume
  the one presentation attempt. Opening now waits for a fresh reply. Loading
  also gives the next query a full response allowance without extending the
  briefing deadline. Both engines' compact overlays were visually checked
- Evidence: `temp/join-formatted-build.log`, `temp/join-windows-build.log`,
  `temp/join-transfer-d1.log`, `temp/join-transfer-d2.log`,
  `temp/join-late-skip-d2.log` and `temp/join-briefing-d1.png` / `-d2.png`
- Reusable runner options: `-JoinTransitionCase transfer|escape|scores`,
  `-BriefingJoinDelaySeconds` and `-BriefingJoinAction skip|cancel|host_loss`
- The repository-wide automation catalog check reported nine existing failures:
  eight older flyout/score-catchup scripts use dynamically constructed names
  the checker cannot follow; `test_manual_ip_engine` lacks a suite family.
  None of the new join support scripts were reported (`temp/join-catalog.log`)
- Additional passes: D1 Cancel, D2 host loss (finite failure after about 30 seconds),
  a fresh D2 arrival during score review, D1 content in D2, and D1 reconnect
  during the original briefing. Evidence: `temp/join-cancel-d1.log`,
  `temp/join-loss-d2.log`, `temp/join-scores-d2.log`,
  `temp/join-imported-d1.log` and `temp/join-rejoin-d1.log`
- Score arrival also exercised a next briefing that completed naturally before
  the reader could remain on it. The join proceeds into synchronization rather
  than reopening or extending that finished briefing. The runner accepts either
  a still-active native briefing or a host-confirmed completed briefing
- Final Android build: `temp/join-verified-build.log`. Final scoped checks after
  fixture corrections passed: `temp/join-final-quality.log`. Both peers need
  the updated Android protocol (D1 30069, D2 30080); desktop versions are unchanged
- The broader matrix below remains follow-up regression coverage, especially
  combinations of save/restore, secrets, paused media, multiple newcomers and
  packet-loss timing. Those combinations were not all rerun for this change

## Recommendation

Show the actual briefing on a joining client's device, using only the host's
remaining briefing allowance. The user confirms that deferred joining works,
but explicitly requires briefing content as well as countdown/progress and
explanation text. A status-only briefing wait is not the completed feature

The client must show the current phase, why it is waiting, and a countdown or
progress indicator. If a level ends partway through synchronization, the host
should simply tell the client to move to the current flyout, score-review or
briefing phase. Discard the incomplete old-level transfer and synchronize the
eventual playable destination afresh

Implement three milestones:

1. Explain existing waits with live phase, countdown and progress feedback
2. Show the actual briefing while joining, following the existing countdown
3. Carry the same Join attempt through level endings using a simple host
   phase notification and automatic fresh synchronization afterward

Prefer a joining-reader presentation mode that does not enroll the newcomer
in the current barrier. Showing local briefing content need not wait for
world synchronization or change who can hold up the team's launch. Preserve
the existing approval policy, roster and launch deadlines; do not introduce
a general admission service, capacity reservations or resumable old-world
transfers merely to display the briefing

For flyout and score review, an incompletely synchronized newcomer can show
the phase's waiting/status presentation. It need not construct a flyout from
partial mine data or enter a score screen that assumes participation in that
level. Actual briefing pages/videos are required; actual exit cinematics or
result tables can be added independently

Assumed initial scope: Android LAN co-op, native D1, native D2 and D1 content in
D2, with small mirrored engine hooks and shared implementation under Android.
Preserve desktop behavior/protocol. Competitive modes and matchmaking need
explicit follow-up coverage; do not accidentally advertise support for them

## Original behavior before this implementation

The initial source review and earlier checked-in validation records identified
these barriers. The implementation progress above records subsequent changes

| Area | Evidence | Consequence |
| --- | --- | --- |
| Launcher probe | `app/src/main/cpp/jni_engine_query.cpp`, `decode`, accepts only `NETSTAT_STARTING` and `NETSTAT_PLAYING`; `EngineQuery.kt` reports other states as unavailable | Manual IP and QR can fail before native admission starts |
| Native join | Both `net_udp_can_join_netgame` implementations reject other statuses; `net_udp_do_join_game` rejects `NETSTAT_ENDLEVEL` | The native path also needs phase-aware admission |
| Advertised status | Both `net_udp_send_game_info` implementations report ENDLEVEL during reactor destruction/flyout, and near a timed match's end | Status alone does not identify why a session is temporarily unavailable |
| Briefing/travel/restore guard | Both `net_udp_defer_join` implementations return while briefing, travel or save transfer is active | Requests are silently retried, with no pending-admission record or phase response |
| Host approval | The guard also runs before `net_udp_do_refuse_stuff`; Android auto-host enables `RefusePlayers` | Approval intentionally waits until the transition ends, avoiding an expiring F6 prompt during presentation |
| Request dispatch | `net_udp_process_packet` distinguishes STARTING, WAITING and PLAYING; ENDLEVEL has no admission branch | Simply removing the front-end rejection still would not admit a newcomer |
| Level synchronization | Android `net_udp_process_request` in WAITING only marks an authenticated existing player ready | A new arrival is not equivalent to an existing participant completing a load |
| World transfer | `net_udp_welcome_player`, `net_udp_send_objects` and rejoin sync completion reject a destroyed reactor or active exit | A join that starts during play can fail midway when the level ends |
| Transfer capacity | `Network_send_objects`, `Network_sending_extras` and `UDP_sync_player` serialize joins | Multiple newcomers need queueing rather than concurrent transfers |
| Client destination | `net_udp_do_join_game` calls `StartNewLevel(Netgame.levelnum)` before waiting for sync | A client can load a source level that the host subsequently leaves |
| Briefing membership | `coop_briefing_run` captures participants once; `coop_transition_policy` supports removal, not addition | New members cannot be inserted safely by changing only a roster bit |
| Briefing bootstrap | `coop_briefing_receive` requires a non-observer's first snapshot to be PREPARE and include its slot | A new client cannot adopt a reading-phase snapshot today |
| Rejoin suppression | Both `StartNewLevelSub` paths call `coop_briefing_disarm_for_rejoin` | Ordinary late joins intentionally skip presentations |
| Score handling | `kmatrix` checks connected players' escape/end-menu outcomes | Prematurely adding a newcomer can change completion conditions |
| Existing score catch-up | `shared/net/net_udp_score_catchup.h` repairs authenticated existing peers' adjacent-level completion traffic | Useful precedent for retained snapshots, not a protocol for admitting new peers |

Relevant source anchors in D2 are `net_udp.c:2445` (deferral), `:4521`
(request dispatch), `:7023` (join entry), `:9132` (approval),
`gameseq.c:2124` (rejoin suppression) and `kmatrix.c:375` (score polling).
The corresponding D1 hooks were also inspected

The prior [briefing plan](../networking/coop_briefings_and_launch.md) explicitly
chooses a fixed roster. Its `first_join` and `rejoin` validation records mean
"wait during reading, then join the settled mine with zero presentations."
`tests/test_lan.ps1::Invoke-BriefingRejoinScenario` asserts that behavior.
It covers first-time readers and reconnect after participant removal, not
reconnection before removal or every closing/loading boundary

Networking is already pumped in briefing/movie waits, ordinary score screens
and travel barriers. Much of the plumbing can be reused. Long synchronous
loading calls still need measurement and a bounded control-only pump where
necessary; a draw-loop hook alone does not prove continuous service

The 30-second timeout in native auto-join is for obtaining game information.
It is not evidence of a 30-second limit on the subsequent briefing wait.
Increasing that constant would not solve these admission problems

## Proposed behavior by phase

| Host/session phase | New arrival behavior |
| --- | --- |
| Launcher lobby / native player selection | Preserve existing lobby admission and readiness |
| Ordinary gameplay | Existing approval and object synchronization, protected against a concurrent transition |
| Briefing preparation | Keep retrying the join; show preparing/loading and a progress indicator |
| Briefing reading or video | Show the actual local briefing from its beginning with the host's remaining allowance and countdown overlay; do not wait for gameplay admission to show it |
| Host ready, shortened countdown | Same rule; joining never restarts the 120-second limit or adds another 20 seconds |
| Closing presentation / loading / final release acknowledgements | Remain pending; follow the committed destination without reopening presentation or modifying that barrier |
| Reactor countdown, including everyone dying in the mine | Keep the join attempt alive; show escape phase and reactor countdown; wait for the next safe world |
| First normal exit while teammates still escape | Wait for their outcomes, even if the local host is already watching a flyout or scores |
| D1 rendered flyout / D1-in-D2 flyout / D2 exit movie | Follow departure/score status, show the actual next briefing if still active when prepared, then join the playable destination |
| Score review | Show that the team is reviewing results; optionally display a read-only score snapshot; do not join the completed level's roster or award its bonuses |
| Host and existing client on different sides of score dismissal | Track authoritative destination and session phase; do not infer that every peer shares the host's local screen |
| Secret entry, return, revisit, or destroyed-secret fallback | Wait through travel; follow the actual committed signed level and world visit, never calculate destination as current level plus one |
| Save/load, cold resume, rewind, restart, recovery | Wait until transaction completion, then synchronize the resulting world and preserve existing gear-recovery rules |
| Ordinary pause, automap, menus, modal dialogs, paused movie | Continue serving admission/status where networking is alive; distinguish a quiet simulation from a lost host |
| Campaign-ending movie, credits, terminal results | Report mission complete; do not promise a next mine. Read-only results are an optional extension |
| Host lost, session closed, incompatible build/assets, denied/full | Explain the actual failure and leave cleanly; no endless transition wait |

Waiting newcomers, including joining readers, must not count as escaped,
dead, score-ready, load-ready or briefing-ready. A real existing participant
reconnecting before removal needs separate reconciliation rather
than being silently treated as a new pending player

## Milestone 1: explain the existing wait

### Minimal native status response

Add a shared join-wait status snapshot with small hooks in both UDP engines.
Reply to a valid joining client's request even when normal admission is
currently deferred. Reuse its retry cadence and existing network pumps;
additional status updates can be sent while an active transfer is tracked.
This needs a narrow pre-admission reply path because ordinary player MDATA
assumes an established slot

The snapshot should carry:

- Session/attempt identity and increasing status revision
- Current session phase and the reason the join is waiting
- Current world visit and authoritative destination when known
- Remaining phase allowance and its duration, when a real timer exists
- Briefing generation and mission/level presentation identity for local playback
- Readiness counts or completed/total work, only when actually available
- Approval state, so a status reply cannot be mistaken for acceptance

Keep phase classification and timer values native. Kotlin renders the
snapshot; it does not reconstruct transition rules from `NETSTAT_*` or infer
that all peers share the host's local screen. Examples: the host can be in a
flyout while others still escape, or loading while a client reviews scores

A retry gets the latest complete status rather than requiring every phase
notification to arrive. Reordered or duplicate snapshots cannot return the
client to an old phase or restart a timer. Check sender, session and attempt;
preserve the existing reconnect proof before changing a player's identity or
address. Do not route arbitrary gameplay packets to unadmitted clients

### Required client presentation

Use one consistent join-wait view, driven by a native/JNI snapshot, for waits.
While showing the actual briefing, use a compact overlay with the same phase,
countdown and join-status information. Keep the native briefing controls usable
and distinguish Skip briefing from Cancel join/Back

| Phase | Example explanation | Indicator |
| --- | --- | --- |
| Briefing | Briefing - joining the team; content appears behind the overlay | `Up to 1:14 remaining`, countdown bar, optional actual ready-player count |
| Host has finished briefing | The host is ready. Waiting for the other players | Remaining shortened allowance and countdown bar |
| Reactor countdown | The mine is being evacuated. You will join the next level | Reactor timer, clearly labeled as time until explosion, not time until joining |
| Flyout | Players are leaving the mine | Remaining presentation allowance if known; otherwise activity bar and available outcome counts |
| Score review | The team is reviewing level 3 results | Actual continuation countdown if one is active; otherwise activity bar |
| Loading | Preparing level 4 | Existing measured loading progress if exposed; otherwise activity bar |
| Synchronization | Receiving level data | Measured transfer progress if a reliable total exists; otherwise activity bar and received count |
| Awaiting approval | Waiting for the host to accept your join request | Activity bar |
| Another join is synchronizing | Waiting for another player to finish connecting | Activity bar |

Use determinate bars only for real deadlines or measurable work. Never invent
an overall join percentage or a finish time for score dismissal/loading.
Object transfer's current object index is not necessarily a meaningful total;
a count plus activity bar is sufficient unless a trustworthy total is added

Count down locally using monotonic time between host updates; apply newer
host snapshots as corrections. A host-ready update can shorten the countdown
but retries must not reset it. At zero show that the team is preparing to
start; do not claim the client has entered gameplay until synchronization
completes. The countdown describes the allowance, which can finish early

Keep the reason and the phase distinct: a client can be waiting for approval
while the team reads the briefing. Do not label the client as accepted merely
because the host is responding. Preserve the current deferral of approval
until the mine settles; moving approval into presentations is not required

Valid host status replies establish liveness during a long wait. If updates
stop, show reconnecting/lost contact according to the existing timeout policy
instead of continuing a fabricated countdown indefinitely. Measure servicing
of slow synchronous loads before adding targeted control-pump hooks. No new
network thread or broad timeout increases are planned

## Milestone 2: actual briefing on the joining client

This is required, not optional. Examples:

- Host has 74 seconds left: the newcomer starts the briefing with at most
  those 74 seconds, less any time spent preparing its local content
- Host has finished and 8 seconds remain: the newcomer gets at most 8 seconds
- Host chooses Launch now or the original participants all finish: close the
  newcomer's presentation too, even if it is still on the first page
- Closure has already begun by the time local content is ready: show loading
  or synchronization progress, without reopening an expired briefing

Start at the newcomer's first page/video, not the host's current page or movie
timestamp. Display the existing countdown throughout. A later host-ready
shortening applies immediately; joining, local Skip, retries, reconnects and
presentation preparation cannot start a fresh 120- or 20-second allowance

Implement an explicit joining-reader mode around the existing native briefing
renderers. It receives the current briefing generation/identity and live phase
updates through the join-status path. It does not enter `coop_briefing_run` as
an ordinary participant, emit readiness/closure acknowledgements, or add a
player bit to the host's barrier. Keep admission and local viewing separate:
viewing locally available content is not acceptance into a player slot

The renderer needs validated mission assets and a safe local presentation
context. Audit the existing entry hooks for palette, robot model, movie and
level-data dependencies; prepare only what playback requires, without waiting
for the host's live object transfer. Both current `StartNewLevelSub` sync and
`coop_briefing_run`'s game-window assumptions need deliberate handling. Prefer
small shared presentation hooks over a second parser/renderer or reopening
the whole mine-start path. This is the main additional implementation risk

While preparing content, show progress and the host's ticking countdown.
Recheck the latest generation/phase before opening each presentation. If the
host advances or the timer expires, request normal presentation cancellation
and unwind at an engine-safe boundary, including paused movies. Then switch
to the current phase/progress view and continue joining automatically

Local Skip closes the newcomer's briefing only and shows the remaining wait.
Missing media uses the existing unavailable-content behavior. No-content
levels and save resumes retain their existing presentation suppression.
After this joining-reader presentation, ordinary world admission must not
replay the same briefing; keep the existing rejoin suppression for that path

Keep generation, phase and local skipped/completed state through UI recreation
so a recreated overlay does not reopen the sequence. If contact is lost, show
the connection state rather than letting an obsolete briefing run indefinitely.
A real reconnecting barrier participant retains its existing lifecycle; only
new or removed clients use the nonparticipating joining-reader mode

This approach lets the newcomer see the briefing without holding up the team.
Full dynamic barrier membership is unnecessary for the requested behavior

## Milestone 3: simple host-directed phase transition

### Notification, discard, follow, retry

When the reactor/exit or another world replacement invalidates a join:

1. The host stops sending that client's old-level objects/extras and reports
   the current phase, such as escape, flyout, score review or briefing
2. The client records the notification, then leaves the incomplete sync at a
   safe engine boundary. It discards partial world data and switches to the
   matching phase view without showing an error requiring a retry; for an
   active briefing, prepare/show its actual content as described above
3. Both sides clear the abandoned transfer's temporary state. The client keeps
   its join target and identity; preserve any approval already granted to this
   same attempt, without reusing it for another identity or session
4. The client continues asking for the latest phase. It can jump directly to a
   newer phase if intermediate notifications were lost or the host moved ahead
5. When ordinary joining is safe, refresh authoritative game information and
   run the existing level-load/join sequence against that destination from
   scratch. Keep showing phase/loading/transfer feedback throughout

There is no attempt to finish, repair or resume the obsolete world transfer.
There is also no requirement to replay every skipped presentation. If the
current phase is a briefing, show it with the time still available while
ordinary world admission remains deferred

One small correctness boundary remains necessary: late old-transfer packets
must not contaminate the fresh join. Reuse session/world-visit identity where
it already covers a packet; add a transfer attempt generation only where
needed for objects/extras/SYNC and retries. Draining a socket alone cannot
exclude delayed UDP packets. A duplicate transition notification is harmless

Use one shared cancel/reset path for the active join transfer. Audit provisional
slot/ADDPLAYER/verification state so abandoning a join does not leave a ghost
player holding up scores or a barrier. Once a player is fully committed, use
ordinary participant transition handling rather than treating it as an
incomplete join. Do not call `StartNewLevel` recursively from a packet handler

### Join entry points and other transitions

Let launcher query decoding and native admission distinguish a live transition
from an unavailable session. Manual IP, QR, discovered games and auto-join must
reach the same phase-aware wait. While the host is in ENDLEVEL or WAITING,
reply with status without marking a newcomer as an existing ready player

Follow the host's actual destination, including negative secret levels,
restarts and loads of the same numbered level. Keep the existing world-visit
fences, asset validation, reconnect identity and saved-gear recovery. Save,
travel and briefing transactions retain their current participant sets

Multiple waiting clients can retry normally and receive status; preserve the
existing one-client world transfer. A general queue with reserved player slots
is not required. Capacity is still checked by ordinary admission, and the UI
must not promise a slot that has not been granted

Campaign completion is terminal: show that the mission has finished rather
than promising a next mine. Genuine denial, full capacity, host loss or asset
mismatch still need specific failure messages and a clean exit

## Optional later work

- Full dynamic briefing membership, allowing a newcomer to count toward the
  current team's readiness; actual briefing display is already required above
- A read-only completed-level score table
- Actual late-entry flyout rendering or movie playback
- Approval controls during presentations, capacity reservations, or a richer
  pending-player roster

These are separate from the required briefing display and waiting feedback.
The fixed briefing roster and ordinary world-admission rejoin suppression
stay intact; joining-reader playback is an explicit separate mode

## Work packages and rough effort

The original estimate included dynamic briefing enrollment and a larger
admission service. Those remain unnecessary. Actual local briefing playback
is now required and adds a presentation-lifecycle work package

| Package | Scope | Rough effort |
| --- | --- | --- |
| Visible waiting | Shared phase/timer responses, JNI/status view, countdown/progress behavior, existing briefing-join regression | 2-4 days |
| Joining-reader briefing | Safe local content preparation, native playback/overlay, existing deadline, cancellation and rejoin suppression | 2-4 days |
| Transition continuation | Query/dispatch gates, host phase notification, shared transfer discard/reset, automatic fresh join | 3-5 days |
| Integration hardening | Arrival/transfer boundary faults, slow loading, D1/D2/imported D1 and UI lifecycle | 2-3 days |

These are provisional engineering-effort ranges including focused validation,
not a delivery commitment. Visible waiting is an incremental milestone, not
completion of join-during-briefing. Confirm presentation initialization and
partial-transfer reset behavior before firming up the estimate

Likely file families: both `net_udp.c/.h`, Android protocol definitions in
`multi.h`, a shared join-wait helper, native query/JNI/introspection and the
Android wait UI. Add small level-entry/teardown hooks only as necessary.
Add joining-reader lifecycle hooks to shared briefing code and both native
presentation entry paths. Keep travel/barrier membership policy unchanged.
Keep new shared behavior under Android and preserve
Windows/Linux/macOS behavior and desktop packet layouts

Bump the Android protocol versions for wire changes. Both peers need matching
builds. No pre-release Android compatibility migration is proposed

## Validation and acceptance

Extend `tests/test_lan.ps1` and introspection for visible wait phase/reason,
remaining time, progress type/value, last host response, transfer attempt,
reset count, joining-reader generation and playback state. Preserve existing
admission/reconnect invariants, but explicitly update the old first-join test's
zero-presentation expectation for the new required behavior

Full regression matrix (focused passes are recorded above):

- Full two-minute briefing wait; host-ready shortening, early all-ready finish,
  Launch now, missing/no briefing content, and closure/load/release waits
- Client sees decreasing countdown and live explanation while host generation,
  participant set and deadlines remain unchanged
- Newcomer actually renders briefing pages/robots/videos with 74 seconds or
  8 seconds remaining; content preparation consumes that time, not a new timer
- Host-ready shortening, all-ready, Launch now and expiry close the joining
  presentation, including paused video; local Skip never launches the host
- Phase/generation changes while content loads or between pages; no obsolete
  briefing opens, no repeat after sync, and palette/audio/input restore cleanly
- Reactor destruction during objects, extras, SYNC and provisional roster
  publication: discard once, show phase, automatically join the correct world
- Arrival during flyout or scores; host/client score dismissal in either order
- Lost, duplicate and reordered phase notifications; the latest status repairs
  the display without replaying old screens or resetting timers
- Delayed old object/SYNC packets after fresh synchronization begins
- Native D1, native D2 and D1-in-D2, including three peers to establish that a
  newcomer does not delay the existing team's exit/score/briefing progression
- Secret entry/return, restart or restored same-number level, saved inventory,
  reconnect before/after removal and campaign completion
- Slow loading, paused movie, background/resume, multiple waiting clients,
  denial/full, host/client loss, cancellation and a new session at the same IP
- Discovery, manual IP, QR and native auto-join; existing asset/version checks
- Visual inspection of timer/bar labels, unknown-progress activity bars,
  phase changes, approval wording, Cancel/Back and controller navigation

Acceptance: no unexplained wait while the host remains reachable. A single
Join action follows changing phases and reaches the correct playable world,
with no manual retry for an ordinary transition. The client clearly explains
why it is waiting, shows a truthful countdown/progress indicator, and displays
the actual briefing when it can be prepared before that briefing closes.
The newcomer receives only the existing remaining allowance and never
changes existing players' scores, inventory, barriers or deadlines. A discarded
transfer cannot leave a ghost player or apply stale data

For implementation, run scoped code quality, both Windows builds/upstream
compatibility checks, Android builds, focused protocol/reset tests and serial
multi-device integration. Implementation and validation results are recorded
in the progress section above
