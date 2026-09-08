# Shared GuideBot view smoothing and target attention

Status: planning only, 2026-09-07. No implementation, compilation, engine run,
emulator, or runtime tuning has been performed. Source is changing in concurrent
tasks; recheck the eventual integration revision.

Applies to [video export](2026-09-07-guidebot-route-videos.md), the
[metadata route player](2026-09-07-metadata-guidebot-route-player.md), and the
existing Windows regression viewer launched by watch_guidebot_simulation.ps1.
This document owns the shared camera policy; earlier camera sections are starting
designs to reconcile with it, not separate implementations to maintain.

The optional [Robot showcase](2026-09-07-guidebot-robot-showcase.md) extension adds
harmless active robots and visibility-timed explosions during recorded review.
Its initial camera policy still favors route objectives; ordinary showcase robots
do not become gaze targets. Their lifetime track is derived from a declared
reference follow camera, so free-camera inspection cannot rewrite death events.

## Recommendation

Use a shared camera that combines a stable travel heading with mild attention
toward the current visible objective. Smooth rotation and roll first. Keep the
camera on the verified actor path by default, with ordinary render interpolation
where supported. Treat substantial positional smoothing as a separate option.

The user's 6DOF observation is the key design point: looking toward a target does
not require traveling directly toward the screen center. Preserve the actor's
world-space movement while rotating the viewing frame. The same movement then
appears as a combination of forward motion, strafe, and climb/descent, like a
player maintaining a flight path while aiming elsewhere.

For example, while entering a room, GuideBot can glance up and right toward a
blue key while continuing forward through the doorway. As the route approaches
the key, increase attention briefly, then release it before flying past the key
would demand a sudden backward turn. It should acknowledge the objective without
locking it permanently to the center of the screen.

No compensating ship-control inputs are needed for this viewer behavior.
Changing GuideBot's actual orientation or flight controller would be a separate
gameplay change with different physics and regression consequences.

## Comparison of smoothing options

| Option | Strength | Main cost | Recommendation |
| --- | --- | --- | --- |
| Raw actor camera | Exposes actual heading changes, collisions, and path corrections | Uncomfortable motion remains visible | Always available for diagnosis |
| Exponential quaternion smoothing | Small, predictable baseline with few parameters | Lags during sustained turns; abrupt desired-heading changes still need care | Keep as a comparison implementation |
| Critically damped angular spring | Can ease angular velocity into turns and settle without deliberately adding bounce | Requires angular-velocity state and careful integration/limits | Preferred initial live rotation filter |
| Adaptive low-pass such as One Euro | Can suppress slow jitter while reducing lag at higher motion speed | Fast jitter or a planner flip can be mistaken for intentional fast motion | Consider for direction/velocity estimates if a measured problem remains |
| Future-and-past orientation smoothing | Can anticipate a real corner and start/end a glance gracefully | Requires recorded future motion; boundary and seek handling matter | Preferred enhancement for recorded playback/video |
| Position spline or long moving average | Can make the spatial path look much smoother | Cuts corners, changes apparent clearance, can hide actual stalls | Avoid as the default |
| Retiming motion along the recorded path | Can soften the presentation of starts/stops while preserving path geometry | Changes the video timeline and all event/audio timing | Optional walkthrough export feature |

The One Euro authors describe its speed-dependent jitter/lag tradeoff and tuning
in their [reference material](https://gery.casiez.net/1euro/). It does not solve
objective selection, visibility, or route-intent errors. Do not stack several
filters merely because each individually looks plausible.

A damped framing region is a useful design precedent: a target can move within
a comfortable screen region without the camera continually correcting it.
See the [Cinemachine rotation composer documentation](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineRotationComposer.html).
Use the idea in lightweight shared native code; this is not a proposal to add
Unity or another camera framework.

## Three user-facing modes

| Mode | Rotation | Target attention | Position |
| --- | --- | --- | --- |
| Raw | Actual actor orientation | Off | Actual actor position |
| Steady | Smoothed travel/actor heading and roll | Off | Actual path, render interpolation only |
| Attentive | Same smoothing with bounded objective framing | On | Actual path, render interpolation only |

Recommend Attentive for normal viewing in all three tools, after invariance and
comfort checks pass. Keep Raw one action away in the regression viewer, with the
current mode visible. Steady is useful for people who prefer a stable forward
view or for diagnosing whether attention itself causes an awkward camera move.

Use one profile definition and implementation across viewers. Recorded versus
live data availability selects the supported lookahead method; it must not
silently select a different target policy. Record the profile/version and
lookahead source with exports and diagnostic captures.

In the metadata player, add the mode selector to the existing route-specific
contextual touch actions, preserving the user's selected touch layout. Free
camera disables automatic smoothing/attention assistance to the extent it would
fight manual control. Follow GuideBot restores the selected automatic mode.

With Robot showcase enabled, changing the automatic reference camera profile
requires a replacement derived showcase track, while preserving the base route
recording and source cursor. Playback speed and free-camera movement traverse
the existing track without rescheduling deaths. Future optional robot glances
need a deterministic joint camera/showcase pass to avoid a visibility/death/gaze
feedback loop; they must never take priority over route objectives and passages.

## Camera calculation

1. Obtain actor pose/velocity, continuity ID, current semantic target, actual aim
   point/bounds, and route/interaction phase from read-only native state or the
   recorded scene. Never infer the target by parsing the displayed label.
2. Estimate a stable travel heading from recent displacement, with a recorded
   future window when available. Reject negligible displacement and isolated
   outliers. Keep the last stable heading when stationary; do not normalize a
   near-zero vector or average a U-turn into an undefined direction.
3. Mix travel heading with recorded actor heading as appropriate. Resolve a
   narrow upcoming portal before granting a large sideways target glance.
   Preserve an interpretable view of the passage being traversed.
4. Stabilize roll using the previous camera frame transported along the heading,
   then gently follow useful actor roll. There is no fixed world-up axis in a
   Descent mine. Do not force upright flight or erase intentional banking.
5. Select one primary attention target and compute a bounded framing correction.
   Apply its visibility/relevance envelope before the final rotation filter.
6. Construct a consistent quaternion and run the selected angular filter.
   Normalize, preserve shortest-arc sign continuity, and handle near-180-degree
   changes explicitly. Use relative rotations for angular filtering rather than
   independent pitch/yaw/bank averages or raw quaternion-component averaging.
7. Enforce angular velocity/acceleration limits after target blending. Use a
   stable spring integration method at a defined camera tick; critical damping
   alone does not guarantee that a poorly integrated or moving-target system
   cannot overshoot. Reset invalid filter state at discontinuities.
8. Render from that transform without writing it into the actor or player.

Use a sensible response time, rather than so much damping that the camera is
still facing the previous corridor after a turn. Start with roughly 0.2-0.35
seconds of angular response, around 90 degrees/second peak rotation, and around
240 degrees/second squared acceleration. These are experimental starting values,
not comfort guarantees or measured defaults. Permit easing/lower speed in an
offline presentation when a turn cannot be framed within those limits.

## Preferential target framing

Target attention should describe why GuideBot is here, while keeping the route
legible. It is a small weighted preference, not an aimbot camera.

| Phase | Camera behavior |
| --- | --- |
| Target hidden or remote | Travel-oriented view; no staring through intervening rock |
| Relevant target becomes visible near the current leg | Ease into a mild glance while preserving passage context |
| Target is comfortably framed | Allow it to drift within a screen region; avoid constant exact recentering |
| Near a switch, key, or required interaction | Increase attention if the view remains comfortable and useful |
| At very close range/passing the target | Release positional tracking before the direction flips behind the camera |
| Completed, removed, superseded, or occluded target | Fade attention toward the next useful heading, with a brief stable handoff |

Candidate policy:

- Prefer the selected semantic goal: required key or its carrier, shootable
  switch, important door, crossing region, boss, or reactor
- Use nearby physical frontiers such as the door currently blocking the route
  when they are more immediately useful than the distant final goal
- Do not select arbitrary nearby powerups, every visible key, or the next
  unverified goal just because it is colorful or close
- Use engine aim_pos/face geometry for switches and the live object bound for
  keys/robots. activation_pos can be a firing position, not the thing to look at
- Keep target identity stable through mild view/score changes. Add hysteresis
  and an initial minimum selection dwell around 0.4-0.6 seconds, except for
  invalidation, disappearance, or a real objective change
- Check line of sight in the current scene and angular eligibility separately.
  A target just outside the current viewport can invite a small glance if it is
  unobstructed. Using only the current rendered-object list would prevent that
  glance; using only distance would reveal targets through walls
- Require visibility to persist briefly before acquisition; soften brief
  occlusion flicker without continuing to track a moving hidden target through
  rock. A last-visible framing direction can decay without new hidden tracking

Start with attention influence around 20-35 percent during travel, rising toward
50-65 percent when stopped at a clear interaction. Use a view-relative angular
limit around 25-40 degrees from the stable travel heading while moving. Weight
also depends on time-to-approach, projected target size, and passage clearance;
straight-line distance alone is a poor relevance measure in a folded mine.

Aim to keep the target in a comfortable center region, rather than exactly under
the reticle. Keep sufficient room for the upcoming opening and avoid placing the
target behind the objective panel. Account for aspect ratio and overlay bounds.
Do not solve framing by animated zoom/FOV changes in the initial version.

For a key, soften tracking as the camera approaches the pickup radius, then
hold/release the last useful gaze direction. Otherwise tiny positional offsets
near contact can turn into enormous angular changes or a 180-degree snap. Use
the recorded completion/removal event to hand attention off, not an extrapolated
object slot that may already belong to something else.

For a fly-through trigger, prefer the aperture and onward direction. Tracking a
point behind the camera after crossing would obscure the route; the rear view
and completion marker can show the opening just traversed.

## Smoothing translation without disguising the route

Distinguish three different effects:

1. Tick-to-render interpolation removes stepping between simulation samples.
   It does not filter away actual movement corrections. This belongs in the
   initial smooth modes when the world can be rendered at the matching source
   time; validate camera segment/clearance around portals and discontinuities.
2. Small spatial filtering can remove fine vibration, but moves the viewpoint
   off the actor center. Make it an advanced option only after rotation is good.
3. Changing actor acceleration, cornering, or path following changes the actual
   regression run. Keep that outside this camera feature.

If spatial filtering is tried, use a short filter and cap its displacement in
terms of the loaded actor radius, initially well below one radius. Validate both
the candidate position and the sweep from the previous camera against the live
scene. Also constrain it to the local recorded path/corridor, so it cannot choose
another nearby branch. Fade the filter toward the exact actor path near walls,
door transitions, pickups, or inadequate clearance, with a safe fallback to the
actor pose. A collision-free endpoint alone is not enough.

Never hide a real navigation stall or remove an oscillating leg from the
regression viewer. Offer Raw comparison and report any applied camera offset.
Do not use a long trailing camera spring: it can be left behind a closing door
or make the user look into a wall while GuideBot has already crossed it.

For videos, a monotonic source-to-presentation time map can ease apparent speed
along the existing path. Retiming must include the whole scene, objectives,
audio, and timestamps. It cannot remove a spatial discontinuity. Do not delay
only the camera over a current-time world or pretend the live regression has
acquired a smoother velocity profile.

## Live regression viewer versus recorded playback

The current watch_guidebot_simulation.ps1 launches a visible Desktop confirmation
run. It has current state and path intent, not a completed future recording.

| Consumer | Available evidence | Initial behavior |
| --- | --- | --- |
| Live Windows regression viewer | Current/past actor state and current route hints | Causal angular spring, current visible-target attention, exact actor position |
| Metadata player with recording | Actual past/future route and restored scene | Same attention rules with precomputed lookahead and orientation track |
| Offline video export | Full recorded route plus edit schedule | Same camera policy, optional future/past refinement and retiming |

Live route hints may anticipate the next visible passage gently, but are intent,
not proof of future traversal. Bound their horizon, ignore stale hints after a
replan, and reduce influence when actual movement disagrees. Do not extrapolate
a single velocity vector through a corner to manufacture future motion.

For a recorded view, start evaluating about 0.3 seconds of history and 0.5-0.7
seconds of actual future motion. Shorten the window around reversals, visibility
boundaries, or a narrow portal. Preserve current-world visibility even though
the future target position and event are known.

A small delayed live view is an optional later way to obtain real lookahead,
but it requires buffered scene state, not just buffered poses. If introduced,
delay world, overlays, and displayed time together, label the latency in the
regression UI, and keep watchdogs tied to actual simulation progress. Do not make
that recording infrastructure a prerequisite for causal live smoothing.

Precompute recorded camera state or store enough filter/target-selection state
at camera checkpoints for deterministic warmup. Jumping to the same source
frame must give the same automatic view regardless of seek history. A paused
recorded view stays stable; free-camera input uses its separate viewer clock.

For interactive replay, derive the base automatic track in source time for
repeatable seeking. At high playback speeds, angular motion scales with speed;
do not promise real-time comfort bounds while preserving exactly that track.
An optional rate-specific track must be identified explicitly. Video exports
can instead solve/tune the final camera in presentation time after retiming.

## Regression isolation and implementation boundary

Place the shared camera policy under android/app/src/main/cpp/shared/, for
example guidebot_view_camera.h/.cpp. Inputs are immutable pose/target samples
and a presentation profile; outputs are camera transform, attention target,
weight, and diagnostics. Separate live versus recorded sample providers from
the math and attention policy.

Extend route_confirmation's read-only observation interface for target/phase
state. Do not export mutable controller internals or let camera queries trigger
planner work. Connect the desktop viewer, recorded scene player, and video
renderer to the same camera output path.

The existing Viewer assignment points at the actor. Do not temporarily overwrite
actor orientation to smooth it, or leave a presentation Viewer active during
simulation. d2/main/render.c also contains render_warn_robots_about_player_fire,
which derives information from Viewer, and update_rendered_data writes view
state. These make isolation an actual integration concern.

Use an explicit presentation transform at view setup, preserving canonical
simulation view/visibility behavior and RNG. The video plan's render-only pass
audit applies equally to the live regression viewer. If an extra canonical pass
is initially necessary for invariance, keep it until its replacement is proven.
Camera mode changes must not change actor movement, interactions, objective
completion frames, route success, or canonical RNG/state checks.

The desktop route mode currently intercepts game input; explicitly allow the
small viewer-only camera selector/Raw toggle while keeping movement/fire input
out of confirmation. On Android, use the existing selected-layout contextual
action mechanism already specified in the metadata-player plan.

## Implementation and evaluation order

- [ ] Add a read-only pose/target/phase interface and shared Raw/Steady/Attentive
  policy, with exact actor position initially
- [ ] Prototype causal quaternion damping plus target selection in the live
  regression viewer, retaining a one-action Raw comparison
- [ ] Demonstrate capture/simulation invariance with all modes and different
  render rates/focus conditions before enabling smooth mode by default
- [ ] Tune target acquisition/release on a key, switch, narrow doorway, U-turn,
  vertical shaft, carrier pickup, and fly-through aperture
- [ ] Add recorded future/past heading refinement and deterministic seek warmup
- [ ] Evaluate whether visible translation roughness remains; add interpolation
  and only then consider tightly bounded positional filtering or video retiming

Use short side-by-side comparison clips of identical source intervals. Evaluate
target visibility before action, excessive glances, heading lag, passage framing,
angular speed/acceleration, camera offset/clearance, and repeated target switches.
Expose those values through introspection rather than relying solely on visual
judgment. Moving footage and human review are still necessary for comfort.

Include a key nearly under the camera, disappearance/slot reuse, brief occlusion,
two nearby candidate targets, roll wrap, 180-degree reversal, and a real stalled
route. Test a replay seek directly into each case and a paused frame rendered for
several seconds. A smoothing feature should improve watchability without hiding
the evidence that makes the regression viewer useful.
