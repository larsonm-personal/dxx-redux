# Castaway level 2 runtime validation

## Goal

Diagnose the Guide-Bot routing problems in the device log exported as
`debuglog_20260830_234435.txt`, including whether the live switch-restoration
fix works and whether the omitted blue key is actually required.

## Plan

- [x] Identify the build, metadata generation, level lifecycle, and Guide-Bot
  release point
- [x] Reconstruct every compiled selection, temporary prerequisite, legacy
  fallback, trigger activation, key pickup, and navigation failure
- [x] Compare the run with the prior log and the checked-in Castaway route
- [x] Locate each failure in the current planner, certifier, or Guide-Bot
  navigation code
- [x] Record evidence-backed findings and the next corrective work

## Constraints

- Do not treat the ordinary Guide-Bot release wall as a `Next` objective
- Do not assume that a key omitted from metadata is unnecessary
- Do not make production changes during this diagnostic pass

## Findings

- The run uses Android build 21463 at commit `254e7ea5` and starts Castaway
  level 2 (`rupture.rl2`) at log line 178. Trigger 30 releases the start area;
  this remains ordinary level/Guide-Bot lifecycle and is not a `Next` step.
- The new switch-surface restoration behavior works. While trigger 30 has left
  trigger 1's source wall open, the selector publishes a prepared temporary
  objective at segment 125. Guide-Bot successfully leads the player there,
  trigger 0 restores the source, and the selector advances to trigger 1.
- The player crosses triggers 2 and 31 on that trip. Trigger 1 fires at log
  line 1087. No key has been collected (`keys=0x0`).
- The next compiled selection is immediately route step 2, the red key in
  segment 222. The checked-in metadata likewise omits the blue and gold keys:
  `start -> trigger 1 -> red -> trigger 32 -> ...`.
- The compiled/current-state selector certifies that selection as valid, but
  the physical Guide-Bot path does not terminate at segment 222. `escort.c`
  consequently prints `Can't reach next: red key`, changes the legacy escort
  goal to `SCRAM`, and later falls back to a five-point path toward the player.
  The same failure repeats on the periodic refresh.
- This is a disagreement between semantic certification/frontier projection
  and the actual companion pathfinder, not an ordinary movement stall. The
  frontier projection returns the semantic goal itself, so the intended
  `navigating as close as possible` behavior is bypassed. The actual path has
  62 points after safety-point insertion/polishing; `Max_escort_length` is 200,
  so the log does not establish a simple hard path-length cap.
- This log proves that the compiled `trigger 1 -> red key` instruction is not
  executable by Guide-Bot from the recorded live state. It does not by itself
  identify which later wall/switch/key is the blocker. The earlier successful
  player trace remains strong evidence for the viable progression
  `trigger 26 -> 6 -> 3 -> 4 -> 13 -> 12 -> blue -> 7 -> gold`, but it does not
  prove every member is strictly mandatory or rule out a shorter player-only
  route.

## Corrective work

The physical-frontier work formerly listed here is tabled. It is downstream of
the authoritative semantic-planner defect and must not be used to work around a
false route. The replacement plan is
`castaway_level2_key_prerequisite_planner_fix_20260831.md`.
