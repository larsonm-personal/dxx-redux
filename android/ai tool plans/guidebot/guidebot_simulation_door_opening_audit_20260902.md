# GuideBot simulation door-opening audit

## Plan

- [x] Enumerate every simulation path that can open or damage a door
- [x] Separate real flare projectile behavior from direct wall interaction fallbacks
- [x] Identify the conditions that can produce an apparently spontaneous opening
- [x] Report the exact behavior and the smallest honest correction, if needed

## Findings

- `fire_path_flare` creates a real `FLARE_ID` projectile, aimed only after an engine visibility trace confirms the wall
- `shoot_path_door` independently calls `wall_hit_process` on the next path door every frame, so it can open before the flare arrives
- `shoot_frontier_door` directly calls `wall_hit_process` for surrounding frontier doors when the actor reaches a route frontier
- Hidden-door and blastable-wall objective fallbacks also call `wall_hit_process` directly at close range
- The smallest honest correction is to remove the proactive `shoot_path_door` call and retain direct interaction only as a timed, close-range recovery after a real flare had a chance to arrive
