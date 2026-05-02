# Plan: Player Drag Localization 20260501 141150

## Scope

Narrow the first hidden player-motion divergence in:

- `android/temp_game_logs/d2_descent2_level2_20260501_141150.dximdemo`

Focus on the first matching-by-`gt` drift step at `gt=1523057`, where recorder and replay positions still match but replay velocity has already diverged.

## Local Hypothesis

The hidden split is not introduced by control/thrust construction. Recorder and replay entry state at `gt=1523057` match, and after-move thrust still matches. The cheapest discriminating check is to include that step in the existing replay drag probe window and compare replay `pre_drag` and `post_drag` against the recorder's `after_move` velocity for the same `gt`.

## Plan

1. Retarget the player drag/detail probe window in `d2/main/physics.c` to include frame `545`
2. Rebuild the D2 host target
3. Rerun the `141150` replay bundle with sandbox retention
4. Compare the new frame-`545` drag logs against the recorder-side `gt=1523057` motion log
5. Summarize whether the first hidden drift starts before drag or inside drag integration

## Findings

- Completed steps 1 through 4
- Replay still fails on the same final position mismatch and RNG compare still passes
- Frame `545` replay `entry` matches the recorder at `gt=1523057`
- Replay `pre_drag` differs from `entry`, but that delta is explained by the expected `PF_WIGGLE` path in `read_flying_controls()` and does not, by itself, prove a divergence
- Replay `post_drag` and `pre_fvi` are both logged before any later object collisions
- The same move step includes a player-weapon collision and damage event:
	- `weapon_obj=162`, `weapon_id=42`, `weapon_sig=4175`
	- collision point `(-14432486,-2900449,-5110660)`
- Replay `after_move` includes the result of that player-weapon bump, so recorder-vs-replay `after_move` is not a pure drag comparison on frame `545`
- A focused bump probe in `d2/main/collide.c` shows the replay hit response path is:
	- `post_drag vel=(1358211,-270459,247683)`
	- bump force `float_force=(547553,-143462,31867)`
	- player post-bump vel `(1323989,-261493,245692)`
- The same bump probe also shows `vm_vec_scale2()` and an all-fixed `fixmuldiv` alternative produce the same force on the replay host for this hit:
	- `float_force == fix_force`
	- `delta=(0,0,0)`

## Conclusion

The original drag-localization hypothesis is falsified. The first hidden player velocity split that later becomes the visible frame-`546` position drift is carried by a move step that includes a player-weapon bump after the player's own drag/pre-FVI phase. The replay host's `vm_vec_scale2()` float-based force scaling is not the differentiator for this specific hit, because it matches a fixed-point alternative exactly. The remaining likely source is the weapon side of the collision path: weapon state, collision geometry, or another pre-bump input to the player-hit response that is not yet logged symmetrically in the existing recorder bundle.

## Next Step

Add symmetric recorder/replay lifetime and impact logging for the weapon that later hits the player, so the next demo can compare the hit projectile's state and the exact player-hit inputs instead of inferring them from player after-move velocity.