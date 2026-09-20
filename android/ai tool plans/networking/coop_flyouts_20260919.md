# Co-op fly-outs before results

Date: 2026-09-19
Status: Source investigation and implementation plan; engine changes not implemented

## Requested behavior

Retain available escape movies and rendered fly-outs in co-op, under the same
session option and host-control experience as synchronized briefings. Give
fly-outs a separate allowance, initially proposed as 20 seconds. The duration
survey below recommends adapting that allowance to the content. Advance immediately when everybody is
ready, without adding a fixed countdown before or after the presentation

Endgame presentations were also checked. They are already implemented; see
[the endgame implementation and validation record](coop_endgame_presentation.md)

## Confirmed flow and existing behavior

- Fly-outs precede the end-level results/score screen, not a player-selection
  screen. Normal progression is escape -> fly-out -> results -> next-level
  loading/synchronization -> incoming briefing -> gameplay
- D2 `start_endlevel_sequence` in `d2/main/endlevel.c` explicitly excludes
  multiplayer from both the built-in escape movie and rendered fallback
- D1 `start_endlevel_sequence` in `d1/main/endlevel.c` already permits the
  rendered sequence. Both engines define `SHORT_SEQUENCE`, ending after the
  five-second outside portion. The additional multiplayer early stop appears
  in the inactive non-`SHORT_SEQUENCE` branch, not the current compiled path
- Rendered fly-outs run through game frames; movies have their own event loop.
  `stop_endlevel_sequence` directly calls `PlayerFinishedLevel`. That callback
  must become a once-only handoff when synchronization owns the presentation
- D2 selects built-in escape movies by level, omits secret-level escape movies,
  and deliberately omits the ordinary escape movie on the full game's final
  level. Retain these content-selection rules
- `PlayerFinishedLevel` accounts for rescued hostages and enters progression.
  Reactor deaths can reach progression without calling it. D1's final-level
  ordering also differs from its ordinary levels. Hooks only in the successful
  escape or ordinary score-screen path would miss participants
- The D2 results screen already waits for peers to reach end-menu/dead states.
  `multi_endlevel` itself is not a blocking all-player barrier
- `coop_briefing_run` hides the mine, stops time and runs a fixed-roster
  presentation/closure/release barrier. `coop_briefing_active` also affects
  gameplay fencing, state-changing operations, game-window event dispatch and
  join handling. It cannot simply be turned on when the first player escapes
- Existing D2 normal-exit arbitration deliberately lets other players finish
  escaping. Preserve that behavior and the captured physical crossing context

## Timing and interaction design

The following describes the original fixed-20-second proposal. The duration
survey below supersedes the fixed budget and automatic host-ready shortening;
the immediate playback, escape-outcome barrier and reliable release remain

Playback starts immediately on each player's successful escape. Do not make the
first escapee watch a preparation screen until everyone reaches the exit

There are two overlapping activities: unresolved players continue the real
reactor escape, while resolved players watch their local fly-out or wait. Never
freeze the remaining players, stop their reactor countdown, declare them dead,
or treat their live gameplay as a presentation timeout

Use one host-owned 20-second fly-out deadline, starting when all participating
players have resolved their escape outcome: escaped, died in the mine, or were
authoritatively disconnected. Presentations may already be partly or completely
finished at that point. This is a maximum remaining allowance, not a delay

- If everybody has finished viewing when the last outcome resolves, release
  immediately after closure acknowledgments; do not show a 20-second countdown
- Otherwise show the remaining fly-out allowance and close remaining viewers
  when it expires, including paused movies
- Host-ready shortening uses `min(existing deadline, host-ready time + 20s)`.
  With a 20-second overall allowance this never extends the deadline or adds
  another 20 seconds. A host ready before the last escape gets no separate timer
- Enable the host's explicit override once the host has finished/skipped and
  all escape outcomes are resolved. It closes remaining presentations, then
  waits only for actual closure/release acknowledgments
- Local Skip affects only the local presentation. Missing/unsupported content
  is immediately presentation-ready. Dead players do not receive an escape movie
- While anyone is still in the mine, show that fact instead of a misleading
  countdown promising the whole group will advance
- The next-level briefing gets its existing fresh 120-second overall and
  20-second host-ready rules. Fly-out time consumes none of that budget

Example: host escapes at 0s and finishes at 8s; client escapes at 12s and finishes
at 18s. Playback begins at 0s/12s respectively, the maximum deadline is 32s,
and both proceed at 18s after closure delivery. There is no wait until 32s

This chooses the deadline origin explicitly: the last resolved escape, rather
than the first escape. Starting a shared timer at the first escape could exhaust
the allowance while teammates are legitimately still playing

## State and ownership

Extend the shared transition policy with a fly-out presentation operation and
explicit exit-presentation phases. Keep peer connection/escape states intact;
do not overload `CONNECT_PLAYING`, `CONNECT_ESCAPE_TUNNEL` or `CONNECT_END_MENU`
to represent presentation readiness

Per-player presentation state should distinguish:

| State | Meaning |
| --- | --- |
| In mine | Escape outcome unresolved; gameplay continues |
| In fly-out | Escaped and viewing a movie or rendered sequence |
| Ready | Local presentation closed normally |
| Skipped | Local Skip closed the presentation |
| Unavailable | Escaped, but no usable presentation exists |
| Died in mine | Outcome resolved; no escape presentation required |

Keep outcome resolution and presentation completion as separate facts. The
host may observe an escape packet before the viewer opens, and presentation
closure must not falsely account for hostages or resolve someone else's escape

Lifecycle:

1. Observe normal exit; lock conflicting travel/state operations without
   freezing unresolved players
2. Track local playback and authenticated per-peer outcome/progress, with
   retransmission while other players are still flying in the mine
3. Once all outcomes resolve, arm the 20-second deadline if any viewer remains
4. All ready, deadline or explicit host override requests presentation closure
5. Each peer acknowledges only after its viewer, audio and input cleanup finish
6. Host releases into results; retry the release until participants acknowledge
7. Existing results and next-level synchronization retain their own ownership

Reuse the briefing policy's monotonic clocks, authenticated host authority,
generation/revision checks, participant removal, host-loss handling and
closure/release reliability. Extract the small common presentation mechanisms
where necessary; do not copy the entire briefing runtime or use its blocking
entry point for the first escapee

Presentation ownership must be distinct from a globally frozen world. In
particular, `coop_gameplay_current_stamp` and endlevel packet validation must
continue accepting the unresolved players' current-world escape updates. The
rendered viewer also needs animation frames even when ordinary local gameplay
is blocked

## Implementation slices

1. Extend `shared/coop/coop_transition_policy.{h,c}` and its native tests with
   separate outcome/viewer readiness, fly-out phases and `COOP_FLYOUT_LIMIT_MS`
2. Add a shared fly-out adapter under `android/app/src/main/cpp/shared/coop/`,
   sharing presentation control and UI mechanisms with `coop_briefing.c`.
   Explicitly fence packets by session, world visit, level, operation and
   generation so a late exit snapshot cannot affect a new entry briefing
3. Extend both engines' authenticated packet dispatch and observer forwarding.
   Update both Android protocol versions if wire semantics change. Retain
   retransmission, stale-generation rejection and sender validation
4. Hook normal physical exits, no-media exits and reactor deaths in both
   engines. Preserve secret travel/revisits/restores and final-boss content
   selection; do not invent an escape animation for those paths
5. Enable D2 movie/rendered fallback under `GM_MULTI_COOP && CoopBriefings`.
   Retain the existing single-player `SHORT_SEQUENCE` behavior. Preserve
   single-player, competitive multiplayer and option-off behavior
6. Pump presentation networking and cancellation in both movie and rendered
   loops, including paused video. Close once and hand off to results once.
   Restore palette, cockpit, viewer, pause ownership, audio and fresh input
7. Cover every results/endgame entry path with the release gate, including
   deaths and D1 final-level ordering. Do not introduce a second results timer
8. Extend the existing overlay with fly-out wording and a context-appropriate
   `Continue now` label. Retain its separate bottom-left host button,
   generation checks and fresh down/up requirement; Skip remains local
9. Expose operation/phase, per-peer outcome/viewer state, deadline, launch
   reason and closure/release masks in introspection. Add automation controls
   for holding a rendered sequence/movie and dropping closure/release packets

No new preference is needed: the existing synchronized-briefing session option
also enables synchronized fly-outs. Keep the independent endgame experience
available regardless of this option

## Failure and membership behavior

- Apply existing peer-loss handling to stopped presentation/barrier traffic,
  while continuing to recognize live gameplay traffic from unresolved players
- A presentation's 20-second expiration is not a disconnect. Close it cleanly;
  use the existing connection/closure timeout policy if the peer stops servicing
  the protocol entirely
- Host loss before release follows the existing interrupted-transition recovery
  policy. Never reinterpret it as campaign completion
- New joins do not expand an in-progress roster. Reconnects must retain valid
  participant identity/state or follow the existing deferred-rejoin policy
- Observers see status without gating player completion. An observing host
  keeps authority and must not wait for an impossible physical escape
- Keep release state long enough for packet retries. A new briefing or save
  operation cannot overwrite the previous generation before release delivery

## Validation required before marking implemented

Extend `android/tests/test_lan.ps1` with reusable fly-out cases and paired
automation scripts, rather than relying only on source-contract assertions

- D1 rendered sequence; D2 real escape movie; D2 rendered fallback when assets
  permit; missing media on one and both peers
- Host-first and client-first exits with the other player remaining in live
  reactor escape. Assert world updates/countdown continue and no early death,
  global freeze, dropped exit packet or accidental result advancement occurs
- Both finish naturally: immediate release, with no full-allowance wait
- Local Skip on either peer, host override, paused movie, held rendered viewer,
  20-second expiry and late viewer startup at the phase boundary
- Host ready before/after the last escape; duplicate progress never extends
  the deadline; fly-out and next briefing timers are independent
- Death in the mine, all players dead, observing host, ordinary observer,
  disconnect/removal, dropped closure/release and delayed prior-level packets
- Physical D2 custom-mission exits using existing crossing-context arbitration;
  secret entry/return/advance, restore and final-boss/endgame regressions
- Verify once-only hostage accounting and results progression, cleared viewer
  and pause state, correct palette/audio and clean subsequent level/session
- Inspect Skip/Continue placement and fresh-touch behavior on real overlays
- Run relevant native policy/integration tests, Android build and paired device
  tests to completion, Windows D1/D2 builds, and scoped code quality

## Endgame audit result

`coop_endgame.c` already establishes terminal completion and releases each
participant into a local ending. D2 `DoEndGame` permits those released co-op
participants through its existing built-in and custom ending branches. D1
retains its ending briefing. D2 terminal results can be dismissed independently

The existing 2026-09-14 validation record covers D1 endings, D2 real ending
movies/final-boss completion, custom ending text/credits, missing content,
observers and host-first/client-first departure. These are recorded earlier
test results, not tests rerun during this investigation

There is no new endgame implementation needed for the requested behavior.
Preserve that terminal owner when adding fly-outs. Campaigns show their existing
single-player authored ending; this does not add a generic scrolling credits
sequence where single player has none

## Quick duration survey and revised recommendation

Survey performed 2026-09-19 using local source and owned extracted assets

### D2 movies: 20 seconds covers nominal stock playback

Read all 15 escape movies (ESA/B/C/D/F/G/H/I/J/K/L/M/O/P/Q) from each of:

- `game_data/d2 1.1 data tracks from cd image tool/OTHER-H.MVL`
- `game_data/d2 1.1 data tracks from cd image tool/OTHER-L.MVL`
- `game_data/gog installers/setup_descent_2_1.1_(16596)/extracted/OTHER-H.MVL`

The survey walked MVL entries and MVE chunks, counted display-video commands
(opcode 0x07), and multiplied by the first create-timer interval (opcode 0x02).
This follows `d2/libmve/mveplay.c`: microseconds per frame are the 32-bit timer
value times its 16-bit multiplier. Each sampled display chunk contained one
display command

| Asset set | Escape movies | Nominal duration range |
| --- | --- | --- |
| CD high resolution | 15 | 16.933-17.349 seconds |
| CD low resolution | 15 | 16.933-17.283 seconds |
| GOG high resolution | 15 | 16.933-17.349 seconds |

The longest was high-resolution ESL.MVE: 260 frames at 66,728 microseconds per
frame. These are encoded playback durations, not device wall-clock timings;
startup, decoding stalls, pauses and cleanup can increase elapsed time. A fixed
20 seconds leaves only 2.65 seconds beyond the longest nominal movie

### Rendered fly-outs: geometry matters

Both engines currently define `SHORT_SEQUENCE`. The compiled sequence is
tunnel flight/lookback -> outside for 2 seconds -> stopped-camera shot for
3 seconds -> completion. The lookback's two-second timer changes camera speed;
it is not an extra mandatory two-second shot. Camera panning and station chase
are compiled out

The ship travels at 50 world units/second; the lookback camera initially travels
at 62.5, then returns to the ship's speed. Tunnel stages advance by segment
position, not a fixed timer. A useful conservative first estimate is therefore:

`remaining tunnel path length / 50 + remaining outside shot time`

For a whole presentation, 20 seconds nominally leaves 15 seconds, or roughly
750 world units, for tunnel travel. This is an estimate, not a proven bound:
the camera offset, actual starting position, bends and discrete segment
crossings affect timing. No base/custom level geometry corpus was measured in
this quick survey, so it does not establish that every rendered fly-out fits

### Prefer a content-aware budget with fixed limits

Recommended initial policy, replacing the fixed timer above:

- Keep immediate completion as authoritative; an estimated duration never
  creates a minimum wait
- At the all-outcomes-resolved boundary, use the maximum estimated remaining
  presentation time among active viewers, plus a proposed 5-second margin
- Use `max(20 seconds, estimated remaining + 5 seconds)` as the allowance,
  with a proposed 60-second hard cap for malformed content or stalled viewers
- Movies: obtain nominal duration from a bounded header/chunk scan of the
  actual selected asset, cached by asset identity; subtract presented frames,
  not merely wall time. Do not use file size as a duration estimate
- Rendered sequences: use the engine's selected exit route and current stage,
  accounting for the remaining path and fixed outside shots. Bound traversal
  and detect cycles/invalid exits. Do not implement a second mine parser in JNI
  or Kotlin, or simulate world objects just to estimate a deadline
- Unknown/invalid estimates fall back to 20 seconds. Client estimates are
  advisory and bounded; only the host sets and advertises the shared deadline
- Freeze the deadline once armed. Progress, duplicate packets and pauses must
  not keep pushing it forward. An already paused viewer still times out
- Keep explicit host override after local completion and outcome resolution.
  Do not also auto-shorten a known longer movie to 20 seconds merely because
  the host lacks the movie or skipped it; that would defeat duration awareness

For a newly started stock D2 movie, this yields approximately 22-23 seconds of
maximum allowance, but normally releases at its approximately 17-second end.
An early escapee already halfway through its movie does not receive a new full
movie-length wait. Validate the 5-second margin and 60-second cap on devices;
they are proposed policy values, not results of this survey
