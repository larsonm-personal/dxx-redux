# GuideBot route playback: optional robot showcase

Planning date: 2026-09-07

Status: planning only. Source inspection and documentation are the only work
performed for this addition. No implementation, build, engine run, emulator,
simulation generation, or runtime validation is included. Route code is changing
in another task; recheck integration points before implementation.

Extends the [route video plan](2026-09-07-guidebot-route-videos.md),
[metadata route player](2026-09-07-metadata-guidebot-route-player.md), and
[shared camera plan](2026-09-07-guidebot-view-smoothing.md). These documents remain
the owners of recording, playback, touch controls, and automatic camera behavior.

## Intended experience

Add an optional "Robot showcase" presentation mode. Ordinary robots originally
placed in the mine remain visible, move around, and try to attack GuideBot. Their
attacks provide visual activity without damage, collisions with the route actor,
weapon push-back, explosion shake, flash whiteout, or changes to route objectives.
After roughly 1-2 seconds of useful on-screen exposure, each ordinary robot
explodes visually and is removed from the presentation.

This lets a viewer recognize which robots inhabit a room while keeping the route
easy to follow. Use the actual mission's robot instances, models, textures,
animations, and weapon definitions. Do not replace the authored population with
a generic selection or spawn every robot next to GuideBot.

Clean mode remains the default. Showcase is an explicitly labeled navigation
presentation, not a normal combat run or a robot-combat regression. Its settings
must not rewrite the selected route, regression expectations, or saved touch
layout. Export metadata should identify showcase mode and its seed/profile.

## Recommended decisions

| Topic | Initial policy |
| --- | --- |
| Source of robots | Capture an authored-instance manifest before verification removes ordinary robots |
| Simulation authority | Completed route recording owns GuideBot, objectives, walls, and progression state |
| Robot activity | Separate deterministic presentation simulation, with read-only access to the recorded mine |
| Target | Explicit GuideBot target adapter for perception, aiming, leading, and visual homing |
| Contact | No physical interaction with GuideBot or replay-owned actors; walls constrain presentation movement |
| Attacks | Visual projectiles and impact effects with no gameplay callbacks or force |
| Lifetime | Deterministic per-instance threshold between 1 and 2 seconds of useful visible exposure |
| Visibility | Main reference follow-camera view, with real occlusion and projected-size checks |
| Progression robots | Bosses and key carriers keep their recorded progression timeline |
| Seeking | Restore indexed showcase state alongside the base scene; never simulate backward |
| Camera attention | Ordinary showcase robots do not become automatic gaze targets in the first version |
| Live regression | Keep canonical live execution clean; offer showcase in recorded review |

All timings, thresholds, budgets, and UI labels here are proposed starting values,
not measured settings. Native implementation should live under android/, with
small gated engine hooks and preserved Windows/Linux/macOS builds. Start with D2
and D1 missions loaded through D2, as in the parent plans.

## Source findings and integration points

Paths are relative to the repository root. These observations identify the audit
surface; they do not establish that a safe integration already exists.

| Existing source | Relevant behavior and consequence |
| --- | --- |
| android/app/src/main/cpp/shared/route_confirmation.cpp | remove_ordinary_robots removes ordinary instances and preserves companions/progression robots; capture the roster before removal, without changing the canonical policy |
| android/app/src/main/cpp/shared/route_confirmation.cpp | The parked ConsoleObject and the moving route actor have different roles; attacking the parked player is not the requested behavior |
| d2/main/ai.c and d2/main/ai2.c | AI reads ConsoleObject, believed-player state, player flags, velocity, RNG, and other globals; changing only Believed_player_pos is insufficient |
| d2/main/physics.c and d2/main/fvi.c | Object sweeps can block movement before damage handlers run; zero damage does not imply zero collision response |
| d2/main/collide.c | Contact and projectile handlers can apply forces and other effects independently of damage |
| d2/main/fireball.c | Radial explosions can apply force/stun and death processing can release contents; ordinary death dispatch is unsuitable for visibility retirement |
| d2/main/laser.c | Normal projectiles allocate live objects and can enter smart-weapon, wall, glass, and other gameplay paths |
| d2/main/render.c | Seismic/palette/render feedback needs explicit isolation as well as damage suppression |
| android/app/src/main/cpp/shared/android_level_preview.cpp | Existing robot preview has separate animation/projectile state and behavior/weapon helpers worth reviewing; it is not full mine AI |

The current verifier intentionally simplifies the population. Do not implement
this feature by disabling remove_ordinary_robots and setting invulnerability.
That would change collision, AI/RNG activity, allocations, trigger interactions,
and potentially the very route being demonstrated.

The independently planned normal-speed/normal-escort regression in
[guidebot_second_pass_normal_speed_20260907.md](guidebot/guidebot_second_pass_normal_speed_20260907.md)
is a different concern. Showcase can consume either supported route profile, but
must not redefine either profile's execution or fidelity claims.

## Recording and simulation architecture

Use the shared seekable scene recording proposed for the metadata player:

1. At level initialization, capture a sidecar manifest of authored robots before
   destructive verification setup. Include stable instance identity, object
   signature/origin, robot-definition identity, initial segment/pose, authored AI
   behavior, contained-item declarations, and the reason for any exclusion.
   Resolve custom mission assets in the engine. Capture enough native initial
   state to initialize the selected presentation behavior reproducibly.
2. Run the existing canonical verification and produce its base scene recording
   and result. Roster capture must not advance RNG or alter object state. Compact
   checked-in regression JSON remains unchanged by showcase-specific data.
3. In a separate presentation pass, restore base scenes as read-only input and
   simulate ordinary showcase robots against the recorded mine and GuideBot
   trajectory. Produce an indexed derived track of robot poses/animation, visual
   projectiles, visibility exposure, and explosion/removal events.
4. During playback/export, compose the base scene with the derived track. Seeking
   restores both at the requested presentation cursor. The player does not run
   unrestricted GameProcessFrame on the composite scene to reconstruct a seek.

This extra activity needs its own clock, RNG, actor/effect ownership, and bounded
allocation. A separate playback process helps isolate verification, but is not
sufficient on its own: a normal gameplay tick there could still mutate restored
doors, keys, or the recorded actor. Enforce the boundary inside the player too.

Prefer explicit presentation actors and narrowly reused engine helpers over
temporarily swapping player pointers and RNG globals around normal AI updates.
Where existing rendering requires engine object handles, provide an audited
presentation-only proxy path or reserved handle domain. No base-scene object may
be overwritten, moved, or evicted to draw an extra robot or effect.

Phase 0 must establish how much stock robot AI can safely be reused. Retain real
movement, firing cadence, and distinctive behavior where practical, through
adapters with explicit state. If a behavior needs unsupported gameplay services,
give it a documented presentation variant rather than silently enabling those
services. The isolated robot-preview implementation is a useful precedent, not
evidence that full AI can be called without further work.

## Robot behavior and collision policy

The target adapter exposes the recorded GuideBot position, segment, orientation,
velocity, and visibility. Use it consistently for perception, aim prediction,
attack distances, and cosmetic homing. Do not move ConsoleObject onto GuideBot
or set global believed-player state as an implicit targeting shortcut.

Wake robots in a bounded relevant region using deterministic distance/portal
rules, with a margin sufficient to show them already moving when approached.
Wakefulness and the visibility-death timer are separate. Avoid waking the whole
mine into a single chase that empties later rooms before the viewer reaches them.
Preserve stationary/guard behavior and bound pursuit around the authored area;
mark this as showcase behavior rather than normal combat AI fidelity.

| Interaction | Presentation behavior |
| --- | --- |
| Robot versus GuideBot or replay-owned actor | No contact blocking, bump, damage, force, or awareness callback |
| Robot versus showcase robot | No physical impulse or blocking; optional deterministic visual spacing affects only showcase actors |
| Robot versus solid mine wall | Read-only geometry query keeps it inside the mine; adjust only its own movement |
| Robot versus closed door or blastable wall | Respect recorded solid state; never open, hold open, damage, or trigger it |
| Door closes across a showcase robot | Resolve/retire the decorative actor without affecting door timing; record the handling reason |
| Melee robot | Animate approach/attack; no contact result |
| Thief | Preserve recognizable movement/attack animation; no inventory inspection or theft |
| Mine-layer or homing/smart weapon | Private visual effects only; bounded child effects inherit presentation ownership |

If an ordinary robot would overlap the camera and fill the screen, retreat or
fade that presentation robot. Do not deflect GuideBot, move the camera, or add
collision avoidance to the verified controller. Segment updates and containment
checks must remain valid for portal rendering, even though actor contacts are off.

### Progression and spawned populations

Keep GuideBot and companions out of the showcase population. Bosses, key carriers,
and any additional robot proven necessary for progression stay replay-owned:
their movement, disappearance, drops, and objective highlights follow the base
recording. Do not duplicate them or make their semantic death occur on sight.
This exception keeps a key/boss objective intelligible when GuideBot reaches it.

Classify from the route recorder and actual engine state, not only robot type or
display name. Include containers and mission-specific rules in the classification
audit. An ordinary robot visually retired by showcase must never release its
contents, including contained robots or powerups.

Start with authored ordinary instances. Matcen, boss-gated, and contained robot
populations are a later explicit extension driven by recorded spawn events or
bounded presentation rules. They must not be enabled accidentally through normal
death/spawn callbacks. State the authored-only coverage in the export manifest so
the video is not advertised as a complete inventory of possible encounters.

## Harmless attacks and visual explosions

Use a dedicated visual projectile/effect path. Presentation ownership propagates
to every descendant, including smart children, trails, debris, muzzle flashes,
and impact bursts. Impact queries can place effects on walls, but must not enter
normal collide/damage/trigger handlers. Projectiles may terminate visually near
GuideBot without a hit response; keep those bursts small enough to preserve view.

Required omissions are broader than hit-point damage:

- No linear/rotational impulse, bump, weapon recoil on the route actor, radial
  force, stun, slowdown, control input change, or motion/orientation correction
- No seismic/tremor state, missile camera shake, flash whiteout, palette-hit
  feedback, rumble, or damage/homing-warning feedback caused by showcase weapons
- No shield/energy/inventory change, theft, score/kill/stat updates, loot or robot
  drops, companion messages, or changes to normal AI awareness
- No door/switch/trigger activation, wall/glass destruction, reactor/boss/key
  progression, or effect on any simulation-owned object
- No consumption of canonical RNG or canonical object/effect allocation capacity

Implement a dedicated showcase_explode operation that starts the chosen robot's
visual breakup/vclip, stops its attacks, and retires its presentation body. Do not
call ordinary lethal damage, explode_object, object_create_egg, or radial damage
dispatch to produce the effect. A zero-damage explosion helper may provide useful
render data, but its object allocation/lifetime still requires isolation.

Make explosions recognizable but brief and readable. Use existing mission art,
with bounded scale, opacity, debris, and optional local visual light. No full-screen
flash or obscuring fog. If audio is enabled in the parent player/exporter, route
showcase sounds through its presentation mixer and cap concurrent bursts; audio
is not a prerequisite for the initial silent video pipeline.

## Visibility and the 1-2 second timer

Use accumulated useful visibility, not time since spawning, waking, entering the
render list, or first becoming barely visible. Derive a repeatable 1-2 second
threshold from the showcase seed and stable robot instance identity; use a fixed
1.5 second option for diagnosis. Do not use the canonical d_rand stream.

A qualifying sample requires the robot to be in the main reference follow view,
inside the near/far clipping range, sufficiently large on screen, and genuinely
unoccluded by mine geometry and nearer scene objects. Portal/segment visibility
alone is insufficient. Account for fixed presentation overlays that obscure the
robot. Rear-view and map insets do not advance the timer, so a tiny inset glimpse
cannot remove a robot before the main view shows it.

First implement a conservative visibility query using projected robot bounds
and multiple representative depth/line-of-sight samples. Validate thin slits,
door frames, transparent surfaces, partial occlusion, and very large robots;
promote to a mask/depth coverage method if samples count unreadable glimpses.
Define coverage and projected-size thresholds in the versioned profile, relative
to viewport dimensions. Generation must not depend on asynchronous GPU query
delivery or interactive render frame rate.

| State | Transition |
| --- | --- |
| Unseen | First useful visible sample starts exposure accumulation |
| Seen | Add fixed-tick visible exposure; pause accumulation while occluded |
| Ready | Once threshold is reached, start the explosion on a qualifying visible frame |
| Exploding | Advance only the presentation effect timeline; body no longer attacks |
| Gone | Stay absent on later visits; backward seek restores the earlier recorded state |

Brief occlusion pauses rather than resets the accumulated exposure, avoiding
immortal robots that repeatedly peek around a door. A threshold crossed at a
visibility boundary must not produce an explosion entirely behind a wall; latch
Ready until the next useful view. If the route never shows it again, it need not
explode. Do not use a wall-clock timeout as a hidden substitute for visibility.

### Clocks, camera choice, and determinism

Generate the showcase against a declared reference follow-camera track and edit
schedule. For a video, use the final camera/FOV/layout and retiming profile so
the 1-2 seconds are visible playback seconds at the exported pace. Use fixed
presentation ticks and an explicit mapping to base source frames, not variable
wall time. Freeze robot motion, projectiles, explosions, and exposure during
teaching holds that are intended to freeze the scene.

For interactive review, generate a default 1x reference track. Play/pause freezes
it with the base scene; changing playback speed traverses the same stored events
faster/slower. Thus 2x playback halves the wall-clock interval, just as it halves
the rest of the recorded scene. Changing playback speed must not branch deaths.

Free-camera motion does not change the recorded reference-view exposure or death
schedule. Unlocking already pauses by default in the parent plan, allowing close
inspection; resuming advances the fixed track. Rear views, objective drawers,
temporary controls, and manual inspection do not retroactively rewrite events.
Describe this behavior in the mode help. A later "react to my camera" mode would
need explicitly branching session state and is outside the first implementation.

Changing the reference automatic camera profile, seed, or export edit schedule
invalidates the derived showcase track, not the base route proof. Prepare the
replacement track and restore the current source cursor before switching; keep
the previous scene usable/cancelable during generation. Turning showcase off and
on must not reset robot lifetimes or replay old explosion sounds.

Initially, Attentive framing still favors route objectives, never ordinary
showcase robots. This avoids a circular dependency in which looking at a robot
causes its death, which changes the camera target and changes its visibility.
Optional brief robot glances can follow later as a deterministic joint camera
and showcase scheduling pass, with objective priority and passage framing intact.

## Viewer integration

| Surface | Integration |
| --- | --- |
| Video generator | Clean/Robot showcase profile; reuse the same base recording and generate the appropriate derived track |
| Metadata route player | Contextual Robot showcase toggle/action within the selected touch interface; preserve movement controls, layout, and objective commands |
| Regression viewer | Raw/Steady/Attentive camera options remain independent; offer Robot showcase on recorded review after capture completes |
| Partial regression recording | Allow showcase for the valid recorded prefix with a clear partial-result label |

Do not inject active extra robots into the canonical live regression process.
If live showcase is wanted later, it needs an isolated buffered presentation
consumer with the same read-only boundary; shared visual transforms alone do not
provide that isolation. The initial plan promises recorded review, not a harmless
stock-AI switch in the live runner.

Keep showcase out of the objective list and YouTube chapter sequence by default.
It is ambient context, not a new route step. Progression target highlighting
retains the existing orange/red/key-color policy. An optional lightweight robot
name on first useful visibility can be added later without moving or obscuring
the objective list.

## Track data, resources, and diagnostics

Store manifest identity, per-tick actor/effect state, or equivalent seekable
keyframes/deltas with private simulation continuation state; index seen/ready/
explosion/removal events. Include animation phase, projectile descendants, effect
age, exposure counters, and RNG state where needed. Restoration must not invoke
historical one-shot sound, explosion, or UI callbacks.

The derived cache key includes the base recording hash and selected route profile,
mission asset identities, manifest schema, showcase implementation/profile version,
seed, inclusion rules, timing policy, reference camera/layout, and edit schedule.
Keep these rich artifacts separate from compact simulation regression results.

Budget actors, active AI updates, projectiles, child effects, debris, audio, and
recording size. Keep authored instances in the manifest even when dormant or
outside an activation window. Never silently omit part of the roster to satisfy
a render budget. Use deterministic effect throttling and explicit diagnostics;
if supported limits cannot represent the level, report unavailable/limited
showcase rather than corrupting the base playback. Rendering proxies must reserve
capacity for every base-scene allocation before admitting decorative effects.

Expose introspection for mode/profile/seed, source and presentation cursors,
roster/excluded/active/seen/gone counts, per-instance visibility/exposure/threshold,
target identity, effect counts and throttling, and protected-state mutation
attempts. Include reason codes for progression exclusion, visibility rejection,
unsupported behavior, and containment retirement. This allows automated checks
without relying entirely on screenshot inspection.

## Implementation sequence

### Phase 0: establish the isolation boundary

- [ ] Capture the authored manifest without changing canonical capture results
- [ ] Restore a base recording and add one moving, firing presentation robot
- [ ] Prove explicit GuideBot targeting, wall containment, private allocation/RNG,
  and visual-only impact/death paths before broad stock-AI reuse
- [ ] Identify supported behavior adapters and any presentation approximations
- [ ] Demonstrate source-scene invariance through contact, projectile impacts,
  blast effects, and a canonical allocation occurring beside showcase allocations

### Phase 1: usable recorded showcase

- [ ] Support authored ordinary instances and progression exclusions
- [ ] Implement useful visibility, deterministic exposure, and visual retirement
- [ ] Generate/index the derived track and compose it with the shared renderer
- [ ] Add Clean/Showcase export settings and selected-layout viewer controls
- [ ] Preserve objective framing, highlights, timestamps, and teaching freezes

### Phase 2: seeking and corpus coverage

- [ ] Restore identical actor/effect/exposure state after arbitrary seeks
- [ ] Verify pause, speed changes, toggles, camera-profile changes, and free camera
- [ ] Cover D2, D1-in-D2, custom robot assets, dense rooms, and effect budgets
- [ ] Add cache cancellation/invalidation, partial-recording handling, diagnostics,
  and accurate export metadata

### Later optional extensions

- [ ] Recorded matcen/contained/gated populations with explicit bounded policies
- [ ] Robot names, brief optional robot glances, and additional authored behaviors
- [ ] Buffered live presentation only after the isolation contract is proven

## Planned acceptance checks

These are implementation acceptance criteria, not tests run during this task.

| Case | Required result |
| --- | --- |
| Direct robot contact and repeated projectile hits on GuideBot | Identical recorded pose/velocity/orientation and controls; no bump or damage |
| Flash, homing, smart, and earthshaker-style attacks | Recognizable bounded visuals; no whiteout, warning feedback, radial force, or tremor |
| Attacks beside a switch, door, blastable wall, or glass | Exact base-scene wall/trigger state at every source boundary |
| Robot containing powerups or other robots | Visual retirement releases nothing and changes no counters |
| Boss/key-carrier objective and original key drop | Single replay-owned actor follows the recorded objective timeline |
| Robot behind wall, in tiny slit, distant, or visible only in rear view | No premature visible-exposure countdown |
| Partial view, occlusion, reappearance, then room revisit | Exposure pauses/resumes predictably; removal persists after explosion |
| Seek before/through/after death and shuffled repeated seeks | Same body, animation, projectile, effect, and lifetime state as linear replay |
| Manual pause, objective hold, 0.75x export, 2x interactive playback | Declared clock behavior; no hidden wall-time advancement |
| Free-camera inspection and switch back to follow | No branch in reference-view deaths or route state |
| Crowded room, smart children, simultaneous canonical object allocation | No base object eviction, RNG interference, unbounded effects, or silent roster loss |
| Custom mission robots and D1-in-D2 assets | Correct definitions/models/animations and explicit unsupported-behavior diagnostics |

Compare replay-owned scene state at every source boundary against clean playback,
including object identities, doors/walls/triggers, keys, progression robots, actor
pose, counters, RNG, and recorded result identity. Compare canonical capture with
manifest observation enabled/disabled as well. A matching final success flag alone
does not prove that showcase was harmless along the route.

Visual review should confirm recognizable robots for the intended exposure
interval, useful route visibility through crossfire/explosions, and no camera
shake or objective distraction. Enable showcase by choice only after those checks
pass; leave clean/raw inspection immediately accessible.
