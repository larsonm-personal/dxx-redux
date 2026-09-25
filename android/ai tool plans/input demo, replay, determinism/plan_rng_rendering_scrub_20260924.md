# D1/D2 rendering RNG scrub

## Objective

Keep rendering and presentation randomness outside the simulation RNG sequence used by input demos, without moving collision, damage, physics, or allocation-sensitive gameplay choices to FX

## Plan

- [x] Inventory both engines' RNG calls, including helper calls and direct libc RNG
- [x] Trace remaining candidates and compare with previous audit/revert findings
- [x] Move any confirmed presentation-only calls and document retained SIM cases: no additional safe moves found in the current tree
- [x] Validate the source inventory; no engine changes, so builds and replay runs are not needed for this documentation-only audit

## Constraints

- Preserve existing workspace edits, including flyout/death-camera RNG work
- Do not compensate for replay drift or regenerate recorded baselines

## Result

No additional rendering-only SIM RNG calls found after the existing flyout and death-camera changes. No engine code or recorded demos changed by this audit

The inventory covered `d_rand`, `d_srand`, their stream/annotation wrappers, `make_random_vector`, `pick_random_point_in_seg`, direct libc `rand`/`srand`, and xmodel random helpers across D1 and D2, including D1-in-D2 implementations. There are no call-site overrides of `DXX_RNG_DEFAULT_STREAM` in the checked source

## Already FX-owned

- `endlevel.c`: flyout explosion placement/cadence, sound choice, and stars in both engines
- `object.c`: death-camera direction and attached fireball/vclip placement, size, crackle, and post-death cadence in both engines
- `cntrlcen.c`, `ai.c`/`ai2.c`, and D1-in-D2 equivalents: dead-reactor and dying-boss decorative fireballs
- `game.c`: palette decay and fusion sound cadence; D2 also has palette-save gating and ambient water/lava sounds
- D2 `lighting.c`: omega light flicker
- AI sound timers, D2 buddy hint wording/delay and seismic sound timers, music selection, and demo autoplay selection

## Retained SIM calls

| Candidate | Reason to keep simulation ownership |
| --- | --- |
| `collide.c`: `check_collision_delayfunc_exec` | Shared gate also allocates player/robot contact explosions; moving it was previously reverted because allocation and later object-processing order changed |
| `fireball.c`: debris velocity, spin, lifetime | Both engines dispatch weapon/debris collisions to `collide_weapon_and_debris`; these are physical objects that can intercept weapons |
| `fireball.c`: exploding-wall positions | The same randomized positions feed periodic `object_create_badass_explosion` calls with nonzero damage, radius, and force |
| D2 `laser.c`: omega blob perturbation and lifetime | Real `OBJ_WEAPON` objects carry damage; positions affect segment lookup and collision, and lifetime controls persistence |
| D2 `laser.c`: unlocked omega direction | Perturbed direction feeds the firing ray and goal position, so this changes aim |
| `game.c`, `cntrlcen.c`, D2 `weapon.c`: recoil/countdown/seismic shaking | Writes live player, companion, or guided-missile physics; the audio-only timers are already FX |
| `object.c`: network spawn preview | `gameseq.c` uses `previewed_spawn_point` as the actual respawn selection; this is not just camera placement |
| AI visibility helpers near chatter timers | SIM random vectors update the AI's estimated position of a cloaked player; only chatter cadence belongs on FX |
| Reactor extra shots, AI/pathing/awareness, matcens, thief behavior | Changes live attacks, routing, spawning, or inventory |
| Drops, pickup/flare lifetimes, shot spread/speed, smart-child targets | Changes object existence, trajectories, collision timing, or pickup availability |
| Network spawn/drop/session setup | Gameplay or protocol state rather than rendering; wall-clock seeding remains a separate multiplayer determinism concern |

## Other RNG paths and limits

- Direct libc RNG in `net_udp.c` handles protocol tokens and multiplayer cosmetic colors. It does not consume the default engine LCG state
- The `xmodel/xmaths.h` random helpers have no callers elsewhere in either engine; editor random geometry perturbation is not a runtime rendering path
- The optional `NO_WATCOM_RAND` implementation shares libc RNG between SIM and FX, so it does not provide stream isolation. No checked-in build definition enabling it was found. Changing that alternate RNG backend is separate from this call-site scrub
- This source audit does not prove all engine determinism or certify the existing demo corpus. In particular, moving RNG to FX alone does not isolate shared object allocation. The collision-delay revert remains relevant
- Left `android/outstanding_bugs.md` unchanged because it explicitly says not to edit it
