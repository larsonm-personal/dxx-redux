# Castaway CD audio and Guide-Bot log investigation

## Scope

Diagnose the supplied Android debug log for two reported Castaway Redux issues without changing game behavior:

1. CD audio is configured in the launcher but is absent from the in-game music picker
2. Guide-Bot cannot advance the opening `next` objective chain before the player opens the route manually

## Plan

- [x] Locate and extract the relevant log events and surrounding state
- [x] Trace CD audio setup, launch handoff, and in-game music-option eligibility
- [x] Trace Guide-Bot objective selection, path planning, and the repeated navigation failure
- [x] Record conclusions, evidence, and targeted follow-up recommendations

## Findings

### CD audio picker

- The supplied export contains only `GUIDE-BOT`, `PROFILING`, and `TEXTURE`
  records. It contains no `Game Logs` or launcher records, so it cannot identify
  the registered CD sources or show the existing `[music-panel]` diagnostic.
- The launcher Music page always displays the CD Audio mode chip. The in-game
  overlay instead adds CD only when all enabled sources pass registry-capacity,
  file-resolution, and URI-access checks, unless CD is already the native active
  source.
- The in-game check is stricter than `AudioSourceManager.writePlaylist()`. The
  playlist writer first drops unavailable enabled sources and validates the
  remaining usable sources, while the overlay validates the unfiltered enabled
  list and requires every source to be available. One stale enabled source can
  therefore hide CD in-game even when another source can produce a playable
  playlist.
- The next useful phone evidence is a reproduction with `Game Logs` enabled.
  The existing line reports the current native source, displayed options, and
  enabled CD source count. The active file set's `audio_sources.json` is needed
  if the enabled count is nonzero but CD remains absent.

### Castaway Guide-Bot

- The reported failure is Castaway level 2, `rupture.rl2`. The restored state
  starts Guide-Bot in segment 788 and the player in segment 2. The compiled route
  selects step 1 at segment 131 and reports it valid with no blocker.
- The physical path immediately disagrees. Its nominal 68-segment route cannot
  complete at the semantic goal, so `Escort_goal_object` becomes 13 (`SCRAM`).
  Guide-Bot then records 17 `visit_player_away_timeout` resets while remaining
  in segment 788 as the player explores.
- Trigger 30 is latched at 17:09:31.931. Guide-Bot does not leave segment 788
  until 17:10:01.000, after the player has opened the route. The semantic target
  changes from 131 to 114 and then 113 while Guide-Bot alternates between short
  come-back paths rather than advancing the objective independently.
- The player subsequently activates triggers 2, 31, 0, and finally trigger 1.
  Only after trigger 1 does the route advance to step 2, the red key in segment
  222.
- The red-key route also degrades to a nearest-progress target at segment 174.
  At 17:15:07 the physical path is only the current segment (`[414]`), followed
  by two `short_path_fallback` recoveries and `SCRAM` behavior. The player then
  activates triggers 6, 3, 4, 5, 29, 13, 12, 7, and 11 while exploring manually.
- Checked-in Castaway metadata independently shows level 2 as `route_status:
  partial`, with only Start, shoot trigger 1, and red key steps, followed by
  `route_problem: gold key unreachable`. It does not encode the switch chain
  demonstrated by the live trigger events.

## Conclusion

The level-2 Guide-Bot problem is a route-planner/physical-navigation mismatch,
not a simple movement stall. The compiled selector certifies semantic objectives
that the live pathfinder cannot reach in the current wall state. The fallback can
only guide to a nearest frontier or return to the player, and the partial route
metadata omits the trigger chain the player actually uses to open the level.

No game behavior was changed in this diagnostic tranche.

## CD audio follow-up plan

- [x] Extract the new `Game Logs` music-picker diagnostics and relevant launch state
- [x] Correlate the observed values with overlay eligibility and playlist generation
- [x] Identify the concrete failure and implement the code correction
- [x] Verify the correction with focused tests and record the final result

## CD audio follow-up findings

- The first picker evaluation at `17:58:39.440` used the panel's temporary
  constructor default (`current=cd`). The authoritative native state arrived
  39 ms later as `current=mission`, with one enabled CD source but no CD option.
- SAF disc registration copies the selected CUE into app storage because the
  runtime playlist reads that local copy. The overlay nevertheless required
  access to the original CUE content URI, which is neither required for
  playback nor persisted by the registration path. Losing that temporary URI
  grant therefore hid an otherwise usable CD source.
- The overlay also required every enabled source to be available, while
  playlist generation filters unavailable sources and uses the remainder.
- The overlay now requires the runtime local CUE, validates the actual BIN
  inputs, filters unavailable sources before capacity validation, and exposes
  CD when at least one source can be used. CD selection now stops with a clear
  message if playlist generation finds no readable source.
- The redundant constructor-default option refresh was removed so future
  `[music-panel]` diagnostics report native state rather than a transient
  synthetic CD state.

Validation:

- `MusicOverlaySourcesTest` and `AudioSourceManagerPersistenceTest`: passed
- Scoped `run-code-quality.ps1 -Fix`: passed
- `:app:assembleDebug`: passed, including CMake builds for arm64-v8a,
  armeabi-v7a, and x86_64
- `git diff --check`: passed
