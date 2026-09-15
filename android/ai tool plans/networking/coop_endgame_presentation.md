# Co-op endgame presentation: research, implementation and validation

Date: 2026-09-14
Status: Implemented and validated

## Requested behavior

After completing a co-op campaign, every participant sees the mission's
single-player ending content and advances or skips it independently. Finishing
first, including on the host, must not interrupt another participant's ending

## Findings from the current source

### Single-player sequence

- D1: final level completion -> ending briefing -> final bonus/results screen
  -> eligible built-in-mission high-score entry -> menu
  - `d1/main/gameseq.c:1035` (`PlayerFinishedLevel`) and `:1118`
    (`AdvanceLevel`)
  - `d1/main/titles.c:1348` chooses ending music and the ending screen set
  - Registered D1 uses `endreg.txb` and three defined backgrounds:
    `end02.pcx`, `end01.pcx`, `end03.pcx`. Shareware/OEM have their own ending
    text and order-form behavior
  - There is no automatic scrolling-credits call in this D1 endgame path
- D2 built-in campaign: final boss death -> short final-boss countdown once
  the control center is destroyed -> level completion -> ending movie with
  subtitles -> final bonus/results -> eligible high-score entry -> menu
  - `d2/main/collide.c:1466`, `d2/main/endlevel.c:315`, and
    `d2/main/gameseq.c:1623` (`DoEndGame`)
  - Movie name is `end`, or `endo` for the OEM compile-time variant
  - If the movie is not played, the engine attempts the configured ending
    briefing. An empty/missing ending means there may be no fallback content
  - The ordinary escape movie is deliberately skipped on the full game's
    final level (`d2/main/endlevel.c:220`)
- D2 custom mission: configured ending briefing -> optional
  `<mission>.ctb` scrolling credits -> final bonus/results -> menu
  - D2 ending text normally selects section `Last_level + 1`; D1 missions
    running in D2 use the D1 ending-screen conventions instead
  - A missing custom credits file is silently skipped
    (`d2/main/credits.c:197`)
  - Mission ending selection and available assets should remain owned by the
    existing mission loader and presentation functions

"End credits" therefore covers several different existing experiences. Matching
single player means playing the authored ending sequence, without adding a
generic credits roll to campaigns that do not currently have one

### Current co-op behavior and constraints

- D2 explicitly excludes all multiplayer games from both presentation branches
  in `DoEndGame`, then displays multiplayer results and eligible high scores
- D1 already calls `do_end_briefing_screens` at final completion without a
  multiplayer exclusion. The inspected briefing function has no general
  multiplayer exclusion either. A D1 failure to display its ending would need
  reproduction; the D2 guard does not explain it
- The current co-op briefing coordinator is for synchronized level launch
  (`shared/coop/coop_briefing.c`). Its presentation hooks are inactive when
  that coordinator is not running, so the underlying text/movie viewers can
  also be used locally
- Despite the comments in `AdvanceLevel`, `net_udp_endlevel` is not a blocking
  all-player completion barrier. It sends/listens for status, updates the
  netgame, and returns zero (`d1/main/net_udp.c:2211` and D2 at the same line)
- D2's multiplayer score screen still pumps networking and waits for peers to
  reach end-menu/dead states (`d2/main/kmatrix.c:287`). Simply enabling the
  movie before this screen can leave a fast player waiting for a slow viewer
- D2 already supports terminal completion from a secret level whose destroyed
  base was the last normal level. `coop_travel.c` commits a terminal campaign
  record, freezes gameplay, releases participants, and invokes
  `coop_finish_secret_campaign` -> `DoEndGame`
  - `coop_travel_ending_campaign` and `coop_travel_host_disconnected` already
    prevent a host departure from undoing released secret-campaign completion
  - Existing `test_coop_secret_endgame*` scripts cover results, high scores,
    host departure, pause cleanup, and return to menu, but not ending content
- Closing `Game_wind` invokes multiplayer teardown and then `longjmp`s out
  of the event stack (`d1/main/game.c:1287`, `d2/main/game.c:1501`). Code added
  after `window_close(Game_wind)` cannot be relied on to present the ending
- Early `multi_leave_game` is also not a safe shortcut: UDP teardown changes
  `Player_num` to zero, while result/high-score code reads the player array
  through `Player_num`. Changing `Game_mode` to single player would also
  change bonus eligibility, including the single-player remaining-ships bonus

## Recommended implementation

1. Establish a shared terminal campaign state for normal final exits as well
   as the existing D2 secret-campaign route
   - Reuse the existing committed/released terminal semantics where practical
   - Confirm completion and required final player state before starting local
     presentation; retries and acknowledgements concern this transition only
   - Preserve failure behavior before completion is established. An ordinary
     disconnect or aborted campaign must not be treated as victory
   - Include observers without making their viewing pace gate the players
2. Enter a local presentation phase on each released participant
   - Keep the mine frozen and state-changing operations blocked
   - No shared page counters, launch button, briefing deadline, or all-viewers
     completion barrier
   - Host departure after committed/released completion cannot cancel movies,
     text, credits, results, or high-score entry
   - Retain enough protocol servicing for outstanding terminal delivery and
     cleanup, independently of which presentation window is active
3. Reuse each engine's single-player content selection and order
   - D1: retain its ending call and final local bonus screen
   - D2: allow co-op in the existing built-in/custom ending branches
   - Run the text/movie viewers outside `coop_briefing_run`
   - Treat endings as available regardless of the pre-level `CoopBriefings`
     option; users can skip them locally with existing controls
   - Preserve existing missing-content behavior, subtitles, music, and input
     handling. One participant's missing asset must not affect other viewers
4. Make final results independent too
   - Keep existing co-op scoring and high-score eligibility
   - Freeze the final roster/results used for display, and give D2's final
     co-op summary a terminal mode with local dismissal and no peer wait
   - Preserve each client's player identity through results/high-score entry
   - Close the game and clean up the session once that client's entire local
     sequence finishes. Do not use early teardown or single-player mode as a
     way to bypass presentation guards
5. Keep coordination in `android/app/src/main/cpp/shared/coop/`, with small
   guarded hooks in both engines. Preserve desktop builds and existing
   single-player/competitive-multiplayer behavior

The implementation should start by tracing normal final-exit delivery and the
existing secret terminal release lifecycle. That determines the smallest safe
extension; a second general-purpose briefing coordinator is unnecessary

## Validation proposed for implementation

- Capture single-player D1 and D2 endgame baselines through automation
- Two-peer D2 built-in ending: movie/subtitles, local skip, results, high score
- Two-peer D1 ending: text, music, local advance, existing bonus screen
- D2 custom mission with ending text and `.ctb`; missing-ending/credits and
  unavailable-movie variants using small controlled fixtures
- Host skips to menu while client keeps viewing, and the inverse; no disconnect
  modal, forced close, or wait for the other viewer
- Briefings option on/off, observer participation, duplicate/delayed terminal
  delivery, and host loss before versus after completion release
- Existing D2 secret-campaign endgame, host-leaves, and modal tests extended to
  expect presentation before results; verify completion and hostage accounting
  occur once and the correct local player's high score is retained
- Normal level exits still advance; aborts do not show victory; subsequent new
  games have no stale completion, pause, movie, or briefing state
- Relevant Android/Windows builds, scoped formatting, and paired LAN tests

## Implementation and validation

- Added `coop_endgame.c` as the shared terminal completion owner, with final
  result exchange, commit/release acknowledgments, frozen gameplay and local UI
- Wired both engines, movie/text/modal event loops, host departure handling,
  observer completion, and D2 local final-result dismissal
- Preserved single-player content selection and existing co-op scoring rules
- Added credits/scores window introspection and automation delivery
- Added a generated one-level custom mission with ending text and encoded credits
- The runner removes its fixture through the launcher's managed-content API;
  deleting the loose import alone left previously adopted ending assets present

Passed checks:

- Android `assembleDebug`, all three configured ABIs
- Windows D1/D2 build and both gameplay-fence test executables
- Scoped formatting and lint checks
- D1 ending/results/menu, with host-first and client-first departure
- D2 real ending movie, local skip, paused client viewing after host departure
- D2 actual final-boss death through ending/results/menu
- D2 observing host joins the ending without a local exit
- D2 authored custom text, mission credits and independent results
- D2 missing content on the fast client while the host reads its authored ending
- D1/D2 single-player ending, score entry and return-to-menu smoke tests
- D2 secret-campaign completion, including host departure during client high-score
  entry, once-only hostage accounting, retained player identity and pause cleanup

Logs are under `temp/coop-endgame-*-live.log`. Independence evidence records the
remaining viewer's active ending screen, pause state and disconnected peer.

Deliberate packet-fault injection and legacy shareware/OEM assets have not been
exercised; their handling was reviewed in the shared protocol and unchanged
content-selection branches. Single-player menus retain their existing paused
time bookkeeping; co-op tests separately verify that the terminal pause is
released when that participant closes its game.

## Reusable test entry points

- `test_lan.ps1 -Game d1 -Endgame [-EndgameClientFirst]`
- `test_lan.ps1 -Game d2 -EndgameBoss`
- `test_lan.ps1 -Game d2 -EndgameObserverHost`
- `test_lan.ps1 -Game d2 -EndgameContent custom`
- `test_lan.ps1 -Game d2 -EndgameContent missing -EndgameClientFirst`
- `test_lan.ps1 -Game d2 -InitialLevel 8 -SecretEndgameHostLeaves -AllowSecretWarps -NoCoopQol`

Built-in D2 and secret-endgame tests accept `-EndgameMovieLibrary` pointing to an
owned `intro-h.mvl` or `intro-l.mvl`. The runner extracts only `end.mve`, stages it
on both devices, preserves pre-existing files, and removes files it added.
Single-player scripts use `run_test.ps1`; D2 requires the owned `end.mve` staged
in the native data directory first.
