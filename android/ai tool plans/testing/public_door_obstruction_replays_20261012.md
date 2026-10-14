# Compact public door obstruction replays

## Goal

Record automatic door lifecycle behavior from ordinary Descent 1 shareware and Descent 2 demo starts, using only normal controls and preserving the existing nine recordings and oracles

## Plan

1. Develop compact control-only routes under real SDL/OpenGL Xvfb, inspecting screenshots and authored wall pairs
2. Prove original closed walls, opening, both collision flags, player obstruction beyond the automatic wait, clearance, natural closing, reopening and passage; preserve the D1 frozen-closing and D2 waiting/reversal differences
3. Add read-only geometric observations tied to full object/world state, strict semantic admission and deliberate bad-control rejection
4. Independently capture twice from published source, admit immutable frame/diagnostic/terminal/SIM and door observations within 100000 bytes per pair
5. Validate both true headless and headed playback and all previous cases, publish a focused stacked draft and follow exact-head CI to terminal

## Constraints

No warps, injected poses, inventory, health, world state, embedded saves, modified AI or replay-specific simulation fixes. Small waypoint/aim/hold capture helpers may supply ordinary controls only

## Capture and development evidence

- Source-only commit `9c6852ab607666e2167f9ccff25010a9a4826516` was published before capture, then explicitly configured and built for both native engines
- Two independent clean captures are byte-identical in the full recording, RNG trace, geometric observations, capture report and compact replay/oracle pair
- D1 level-three walls 72/73: 244 frames and 47145 combined bytes; original automatic wait, player obstruction with frozen closing time, clearance, full close at frame 201, ordinary reopen at 214 and traversal at 243
- D2 level-one walls 76/77: 362 frames and 72160 combined bytes; waiting while blocked, clearance, automatic closing-obstruction reversal at 221, full close at 324, ordinary reopen at 336 and traversal at 361
- Real SDL/OpenGL Xvfb screenshots were inspected during route development and exact-source rendered captures match the clean frame/diagnostic/terminal/SIM/geometric oracles
- Both development scenes pass twice through true native headless and headed backends; removing only clearance controls preserves real approach/obstruction and completes playback, then fails the dedicated clearance gate in both games and backends
- D1 can create an impact fireball after the fully closed boundary; the gate requires clear geometry immediately before close and a cleared player afterward, without mislabeling a newly born impact effect as a pre-close obstruction
- All original nine replay/oracle pairs and manifest entries are preserved; imported D1 qualification remains explicitly incomplete beyond its existing flight scope
- All 75 Python admission/semantic tests and automation catalog validation pass locally. The master catalog's Linux child supervisor cannot read `/proc/self/task/.../children` locally; the same catalog passes in the preceding segment's exact-head CI. Scoped C++/Python formatting passes; the local Prettier executable is unavailable
- Nine-case CI measured roughly 33 minutes headless and over 49 minutes headed; the expanded eleven-case workflow keeps bounded 60/85-minute replay step budgets with job headroom
- Final exact-source native runs and exact-head CI remain required before claiming the draft complete

## Imported scope correction after exact-head CI

The first headed CI attempt at `4d747f99` passed all 22 native runs but automatically added the new level-three D1 scene to imported diagnostics. D2 import produced nondefault `wall.controlling_trigger=5`, and the unchanged D2-only-state gate correctly rejected that evidence. Run `38316349764`, artifact `11778690791`, retains the original failed comparison

Keep the prior imported scope explicit with a schema-versioned manifest inventory for every D1 case, retaining required flight and the existing level-one diagnostic cases. Declare level-three lifecycle excluded with its evidence and reason, expose the exclusion in reports, and reject missing future declarations. Do not change engine behavior, native qualification, strict imported checks or any replay/oracle bytes
