# GuideBot impassable grate routing

## Goal

Prevent GuideBot objective routing from treating shoot-through but non-pass-through grates as traversable shortcuts, while preserving valid firing-position routing to targets that must be shot through a grate.

## Plan

1. [done] Correlate the Castaway level 1 and Obsidian level 10 failures with the exported debug log, mission geometry, and current GuideBot path policy
2. [done] Correct movement reachability so non-passable grates cannot win objective path selection or closest-point fallback
3. [done] Add focused regression coverage for alternate routes around shoot-through grates and firing-position behavior
4. [done] Run scoped formatting, tests, and the Windows CMake build, then record results here

## Constraints

- Keep legitimate line-of-fire routing through transparent grates working
- Preserve desktop behavior and minimize changes in upstream D2 source
- Do not modify unrelated worktree edits

## Results

- The supplied export was found at `C:\Users\first last\Downloads\debuglog_20260823_135021.txt`; the requested `debuglog` subdirectory did not exist
- Castaway reached the red key, selected cached route target segment 537, failed to reach it, and repeatedly fell back to SCRAM paths. Obsidian selected cached route target segment 172, reached that one-segment endpoint, and repeatedly recreated the same path while stationary
- The live certifier rejected those endpoints during reachability, but then published the first unreachable prepared step as a valid prepared fallback. That prevented the current-state full planner from searching for another route
- Unreachable prepared targets now fail certification so the existing full-planner path runs. A focused regression proves that no valid certificate is published for an unreachable prepared target
- Route search and live certification no longer infer movement through a solid illusion wall. Actual engine flyability remains the source of truth, so shoot-through visibility remains independent from movement passability
- Route-cache generation 10 invalidates prepared generation 9 paths on upgrade
- Focused Obsidian and Castaway host metadata analysis completed successfully. Their static canonical route descriptions did not change, as expected; the corrected behavior is in live current-state replanning
- Scoped code quality checks passed
- All 43 D2 native tests passed
- D1 and D2 Windows builds passed
- Focused Android route-cache unit tests passed. Kotlin daemon startup failed, then Gradle's built-in non-daemon fallback completed the test task successfully
