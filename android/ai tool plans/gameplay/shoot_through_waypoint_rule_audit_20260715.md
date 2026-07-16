# Shoot-Through Waypoint Rule Audit

## Goal

Determine why Obsidian is unusually susceptible to incorrect shoot-through waypoints and assess whether the route planner uses one defensible geometric rule or corpus-tuned exceptions. Do not add mission-specific handling.

## Plan

- [x] Trace the shoot-through feasibility test and the policy that accepts or rejects its waypoint.
- [x] Reconstruct the Obsidian level 2 decision from planner diagnostics and metadata.
- [x] Compare the same decision path against other affected and unaffected missions.
- [x] Identify whether the defect is geometry classification, route-cost policy, or both.
- [x] Report the smallest general rule that would be legitimate across the corpus.

## Constraints

- No mission-name, mission-shape, level-number, or corpus-protection exceptions.
- Treat corpus changes as evidence about a rule, not as a reason to preserve current output.
- This phase is diagnostic unless a concrete implementation is explicitly requested.

## Findings

- Obsidian is not unusually dependent on remote shoot-switch routes. In the checked-in corpus, all 65 missions with shoot-switch objectives have at least one remote firing waypoint. Obsidian has 19 remote waypoints among 31 shoot-switch steps, below the corpus-wide 805 among 1018.
- Obsidian level 2 exposed a false positive because its old candidate was a sampled point in segment 90 at `(70, -31.25, 246.25)`, almost 198 units from wall 114 in segment 231. Live primary fire from that posed waypoint did not activate trigger 4.
- Candidate selection proves only that the containing segment is reachable. It samples points as close as 1/16 of the distance from a segment center to a side, vertex, or edge, without proving that the player or GuideBot can occupy or navigate to the exact point.
- Wall visibility uses a zero-radius point ray. If FVI cannot produce a credible connected segment chain, the current fallback still declares the ray credible whenever it does not independently detect a locked keyed door. That is not proof that a projectile can reach the wall.
- The pending key-first/source-segment patch is a heuristic layered above that false-positive feasibility result. It inserts any directly reachable key when a complete route begins with a remote switch, prefers the source segment for the next switch, and retains a status-based fallback. It is not an optimal or semantically justified dependency rule.
- The legitimate general rule is: a remote switch transition exists only when a reachable, navigator-occupiable firing pose has a projectile-equivalent collision trace whose first blocking hit is the intended trigger wall through a valid connected segment chain. The planner should then minimize route cost over those valid transitions. Keys should appear early only when the valid dependency graph or route cost requires them.
