# KCXF2RMv11 Level 2 Exit Path Investigation

## Goal
Determine why the guidebot gets stuck finding the exit in KCX-F2 level 2 and identify the true intended success path.

## Checklist
- [x] Read repository instructions.
- [x] Inspect the KCXF2RMv11 mission archive contents.
- [x] Locate the level 2 data file and mission metadata.
- [x] Analyze exits, locks, walls, triggers, keys, and route connectivity.
- [x] Summarize the likely success path and any guidebot trap.

## Findings
- Level 2 is `kcxf2_n2.rl2`, "Aquabed Borehole".
- The level has no reactor object and no external `child == -2` exit side.
- The only exit is trigger 16, attached to wall 135 at segment 896 side 4.
- Segment 896 is reachable only after a chain of `TT_OPEN_WALL` triggers opens closed-wall barriers.
- The only real key is the blue key, object 29 in segment 152.
- The one initially locked non-key door is wall 71 at segment 833 side 5; it is not linked to any trigger in the level data and does not appear to be the intended exit lock.
- The guidebot likely gets confused because it targets the exit trigger segment 896, but the route requires earlier open-wall triggers that the exit pathfinder does not model as an ordered objective chain.

## Trigger Chain
- Get the blue key in segment 152.
- Use an open-wall switch at segment 727 or 728: triggers 4 and 5 both open walls 76 and 77 between segments 703 and 704.
- Use trigger 7 at segment 854 to open walls 63, 64, 65, 66, 82, and 83.
- Use trigger 8 at segment 851 to open walls 67, 68, 78, 79, 80, and 81.
- Use trigger 13 at segment 104 to open the closed walls around the blue-key chamber.
- Use triggers 19, 18, and 17 at segments 182, 177, and 179 to open the final passage, including wall 123 between segments 143 and 9.
- Continue through segments near 9, 855, 860, 869, 874, 879, 888, 0, and 895 to segment 896, then hit the exit trigger wall.
