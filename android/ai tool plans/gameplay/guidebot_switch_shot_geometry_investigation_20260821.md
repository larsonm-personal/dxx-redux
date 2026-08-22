# Guidebot switch shot geometry investigation

## Goal

Determine why Obsidian level 7 objective 3 is represented as a shoot-through-door trick shot and why Guidebot appears to fly to the start of that shot.

## Plan

- [x] Correlate the supplied debug log with objective, shot geometry, and Guidebot route decisions
- [x] Trace the metadata and runtime code that selects switch firing points and path endpoints
- [x] Compare the current behavior with prior switch-shot and nearest-guidance work and identify the regression or missing rule
- [x] Recommend the smallest code and regression-test change without modifying gameplay code during this diagnostic pass

## Implementation

- [x] Rank firing poses by travel distance plus shot distance
- [x] Suppress automap connectors for local and short switch shots while retaining longer remote-shot connectors
- [x] Add focused route-planner, automap, and Guidebot regression coverage
- [x] Regenerate focused Obsidian metadata and verify a legitimate remote-shot control
- [x] Run scoped code quality, native tests, Android integration tests, and relevant builds

## Findings

- Fresh host analysis reproduces level 7 objective 3 as shoot-switch trigger 7, wall 77, with a remote firing waypoint in segment 308. The switch aim point is `(222.5, 30, 265)`.
- The supplied trace shows Guidebot deliberately targeting segment 308 until the switch is activated. It then changes its route target to segment 379, the next fly-through objective. This is consistent with the semantic route metadata, not a separate Guidebot endpoint reversal.
- The planner minimizes travel distance to any valid firing pose. A remote pose can therefore beat the switch-side pose even when the latter is reachable through an ordinary openable door.
- Prior Obsidian work added a switch-side preference only when the player already holds a key and the approach crosses that keyed door. Level 7 exposes the same missing preference for a conventional approach through an ordinary door.
- Firing poses now use travel distance plus shot length for candidate ranking. The route path continues to store travel distance alone, so Guidebot movement semantics are unchanged.
- Fresh host and Android analysis move level 7 trigger 7 from segment 308 to segment 303. Its activation point is `(188.75, 30, 300)`, 48.6 units from the switch at `(222.5, 30, 265)`, instead of the prior shot of approximately 146.8 units.
- The automap now omits activation-to-aim connectors for local switch faces and remote shots no longer than 64 units. It retains connectors for longer remote shots. The level 7 regression verifies both classifications: trigger 5 retains its connector and trigger 7 has none.
- Guidebot continues to target the activation position exactly. The level 7 regression verifies that the path endpoint is segment 303 and that Guidebot leaves the level start toward it. This confirms the observed return to the start of the old shot was caused by the old geometry, not an independent target reversal.
- The existing keyed-door exception remains intact: Obsidian level 2 trigger 5 still selects segment 215. The level 6 switch/grate integration now selects the closer segment 257 instead of 270, and its full movement and stall-recovery sequence passes.
- Verification passed for Windows D1 and D2 builds, Android debug APK assembly, D1 and D2 native route snapshot tests, focused Obsidian host metadata generation, the new level 7 integration, and the existing level 6 Guidebot/grate integration.
