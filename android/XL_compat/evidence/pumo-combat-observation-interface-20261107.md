# Developer combat observation interface

The native Pumo player plan may specify `track_objects` as up to 16 distinct, live robot or reactor indices at entry. Each binding records level, load generation, slot, signature, type and ID. Loading a checkpoint or transitioning levels advances the generation and invalidates earlier bindings, even when an object slot and signature recur. There is no automatic rebinding after `quick_load`.

`combat_observation.tracked_objects` reports `exists` and an `actor_state` object only while the original binding matches. `actor_state` contains raw shields, death flag, segment, fixed position and velocity (null for non-physics movement). For missing or recycled actors, `actor_state` is null; `current_slot` is a separate identity-only observation, or null when the slot is outside the active range. The existing nearby `level_scene.robots` list is unchanged. Fixed-view aim trace entries include this target state, and aiming stops if the captured binding no longer matches.

`combat_observation.player_object` and `player_signature` identify the current player object (signature null if unavailable). `combat_observation.owned_weapons` lists live player-owned weapons by object/signature, weapon ID, parent identity, creation time and lifetime. It reports the total matching count, a 64-object output limit and whether output was truncated. A before/after presence proves a live emitted weapon, but absence does not prove no shot was fired. `primary_fire` exposes raw Fusion charge, a pending flag and a timer delta that is null when no Fusion auto-fire timer is queued. `simulation_rng` exposes read-only state availability, the state when available and the call count.

This is an observation-only interface, not a gameplay change. Its bounded
original Coldrom Fusion qualification and remaining limits are recorded in
`original-coldrom-fusion-observation-20261107.md`.
