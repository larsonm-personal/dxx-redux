# Vertigo level 6 blue-door stall

## Goal

Diagnose and fix the deterministic GuideBot simulation stall directly in front of the blue door after acquiring the blue key in Vertigo level 6, without regressing confirmed routes.

## Phases

- [x] Identify the canonical Vertigo mission artifact and reproduce level 6 twice
- [x] Trace the stalled path edge, wall state, key ownership, and door interaction behavior
- [x] Implement the narrowest shared correction
- [x] Add a focused deterministic integration regression
- [x] Validate level 6 repeatedly, compare affected campaign routes, build D2, run CTest, and run scoped quality checks

## Findings

- The official campaign is target 0 of `CD - Descent II - The Vertigo Series (USA).json`, with level file `d2xlvl06.rl2`
- Both runs complete the blue key at frame 1,052 and then stall while approaching the gold key
- The planner correctly identifies segment 70 as the physical frontier in front of a blue-keyed door, with the blue key present in player flags
- The actor stops in adjacent segment 378 only about 0.03 world units short of relinking into segment 70
- Frontier flare targeting required an exact segment match, so no flare was aimed at the keyed door despite the actor being effectively at the frontier
- Keyed physical frontiers now accept an actor in the directly adjacent segment when its collision shell is within 1/16 world unit of the target point
- Flare targeting and its close-range recovery inspect that keyed frontier segment, so wall 74 opens and the actor collects the gold and red keys
- Restricting the exception to keyed frontiers is necessary: applying it to every physical frontier regressed Counterstrike level 20's ordinary exit approach
- The subsequent live rescan reports `route target unreachable` with no pending goal, despite the canonical route containing the reactor and exit
- A fresh player-sized canonical scan also stops after the red key, while the checked-in metadata still contains the older reactor and exit tail; this later planner disagreement is separate from the blue-door interaction stall
- Focused repeat tests pass for Vertigo 6 and retain confirmed Counterstrike 6, 20, and 23 behavior
- A repeat-two run of all 24 official Vertigo/tutorial levels had no nondeterministic pairs and no status regressions outside the newly more precise Vertigo 6 result
- The final D2 Windows build, all 45 D2 CTests, and scoped code-quality checks pass
