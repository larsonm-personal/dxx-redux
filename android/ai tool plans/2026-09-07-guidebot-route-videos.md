# Automated GuideBot route videos

Planning date: 2026-09-07

Status: planning only. Source and documentation review completed; implementation,
compilation, engine runs, emulator work, encoding, and uploads have not been run.
All implementation checkboxes below are future work.

Related extension: [Metadata viewer route player](2026-09-07-metadata-guidebot-route-player.md)
adds interactive seeking, objective jumps, and a detached camera. For the combined
feature set, that plan recommends prioritizing a shared seekable scene recording.
The checked-rerun exporter below remains an independent bootstrap option; its
pose/event trace alone cannot support accurate arbitrary backward seeks. Agree
on the shared recorder and renderer boundary before implementing both features.

Optional presentation extension: [Robot showcase](2026-09-07-guidebot-robot-showcase.md)
adds active ordinary robots over the recorded route, with harmless attacks and
visibility-timed explosions. Clean mode remains the default; showcase uses the
shared scene recording and never changes canonical robot removal or progression.

## Intended result

Produce one navigational walkthrough per mission level from the GuideBot routes
already used for regression testing. Render the actual level in the engine,
following the verified actor in first person, with comfortable camera motion,
readable objectives, brief teaching pauses, world-space target highlights, and
a rear-view window. Export a compressed video plus exact step timestamps and
upload metadata so a later batch publisher can build a searchable YouTube library.

Use the Windows D2 renderer first, including its existing D1-in-D2 support.
Keep new reusable code under android/ and use small, gated engine hooks.
Standalone D1 GuideBot support is a separate feature and is not a prerequisite.

The presentation must describe the existing navigation sandbox accurately:
ordinary enemies are removed from verification, progression combat is simplified,
GuideBot speed is canonical for the verifier, and the reactor countdown is paused.
Optional Robot showcase restores harmless active counterparts during presentation.
Describe the series as "GuideBot navigation walkthroughs" and briefly explain this in
the opening card and description. Do not imply these are normal combat runs,
speedruns, complete secret guides, or proof of escape within the normal timer.

## Recommended design decisions

| Topic | Initial decision | Reason |
| --- | --- | --- |
| Route authority | Existing route-confirmation controller | Keep all movement, collision, actions, and success rules in one place |
| Video source | Fresh instrumented run matching the selected regression input | Compact checked-in results do not contain motion or full interaction history |
| Capture architecture | Trace pass, then checked render pass with lookahead | Future motion improves camera framing without steering the actor |
| Simulation versus video time | Separate clocks with an explicit edit schedule | Pauses, slowdowns, labels, and timestamps must agree without changing the proof |
| Default viewpoint | First person at the actor center; smooth orientation | Position smoothing can cut corners through walls |
| Output | 1920x1080, progressive 60 fps, H.264 MP4 | Matches the verifier's 60 Hz cadence and is practical for distribution |
| Audio | Silent first version; engine effects as a follow-up | Existing desktop route runner already disables sound and music |
| Teaching pace | Start with 0.75x recorded motion, configurable | Canonical verifier speed is 160 percent; do not change that setting to slow a video |
| Objective colors | Orange active, red actionable context, key-colored carriers | Accommodates both requested highlight schemes with explicit state meaning |
| Rear view | Enabled in the first complete version | Helps viewers understand the passage they just crossed |
| Map | Optional phase after the first-person version | Useful, but a Descent mine needs a stable 3D representation |
| Third person | Experimental later phase | Collision-safe camera movement is the main additional challenge |
| Robot population | Clean by default; optional derived Robot showcase track | Show actual ordinary enemies and harmless attacks without changing the route proof |
| Uploading | Separate resumable publisher consuming validated artifacts | Video rendering and channel operations have different lifecycles |

All timings, colors, and camera limits below are proposed starting values for
visual tuning, not measured final settings.

## Existing implementation and exact integration points

Paths below are relative to the repository root. This is a source survey, not
a claim that every branch has been exercised at runtime.
Another task is changing route code concurrently; recheck these integration
points against the implementation revision before starting Phase 0.

| Existing code | What it provides | Planned use or gap |
| --- | --- | --- |
| android/helpers/regenerate_all_guidebot_simulations.ps1 | Corpus discovery, mission staging, level selection, Headless/Headed/Desktop modes, D1-in-D2 data selection, result normalization | Reuse selection and staging; add a distinct video runner without rewriting regression files |
| android/helpers/guidebot_simulation_regression.ps1 | Route input hash, expected objective projection, canonical seed 1 and 60 Hz, generation 4 | Reuse identity and eligibility checks; its compact objectives contain names and rounded seconds only |
| android/helpers/watch_guidebot_simulation.ps1 | Existing Windows mission/level browser | Later add an export action using the same selected identity |
| android/app/src/main/cpp/shared/route_confirmation.h | Completion frames and fixed-point ticks, route indices, activation kinds, RNG boundaries, radius measurements | Preserve the compact result contract; add an optional recorder interface beside it |
| android/app/src/main/cpp/shared/route_confirmation.cpp | Real actor movement, action dispatch, implicit completion, replans, sandbox rules | Observe goal changes, action boundaries, physical crossings, and completion before transient state disappears |
| android/app/src/main/cpp/shared/route_confirmation_result.cpp | Detailed engine JSON with exact completion frame and microsecond-rounded seconds | Include this result unchanged alongside a richer video trace |
| android/app/src/main/cpp/shared/route_confirmation_desktop.cpp | Native launch arguments, fixed cadence, progress output, terminal result handling | Add trace/render modes and a drain/finalization stage before process exit |
| d2/main/game.c | Fixed-time preparation before GameProcessFrame; confirmation callbacks around simulation | Define an unambiguous post-simulation sampling boundary and separate render scheduling |
| d2/main/render.c | Viewer, view matrix, portal traversal, object rendering | Add an explicit presentation camera override and a view-only pass contract |
| d2/main/gamerend.c and d2/main/gauges.c | Main rendering and do_cockpit_window_view | Reuse subview conventions and compose a dedicated video HUD |
| d2/arch/ogl/ogl.c and d2/arch/ogl/gr.c | gr_flip, framebuffer readback, screenshot primitives | Capture the resolved composed framebuffer before swap; screenshots themselves are not the batch transport |
| d2/main/automap.c | Mine edges and visited segments | Reuse geometry for an isolated miniature renderer without opening the automap window |
| android/app/src/main/cpp/shared/automap_metadata_overlay.c | Labels, objective connectors, key-carrier markers | Reuse semantics and geometry helpers without calling route-refresh/adoption paths |
| android/app/src/main/cpp/shared/level_metadata_scan.h | Route activation and aim positions | Distinguish where the actor should stand from the surface/object viewers should see |
| d2/main/CMakeLists.txt | Desktop route gate and headless-route executable | Gate the optional exporter and keep encoders out of ordinary game builds |

Important source findings:

- route_confirmation_start already assigns Viewer to the GuideBot on desktop
  and Android. The simulation actor begins at the authored player start and uses
  the larger of loaded player and GuideBot radii.
- The checked-in simulation format drops exact frames per objective and rounds
  seconds. Neither it nor a line between static route waypoints can reconstruct
  the verified flight or its changing doors and objects.
- objective_source resolves wall IDs back to live segment/side geometry. Some
  existing metadata steps omit side, so requiring that JSON field would fail.
- record_objective_and_replan deliberately deduplicates repeated semantic steps.
  Restorer-trigger revisits still matter to a walkthrough and need separate events.
- record_implicitly_completed_steps can append several completions at the same
  frame. Their timestamps are observation times, not proof that several visible
  actions occurred in that instant.
- A key-carrier objective changes internally to pickup_key after the robot dies.
  Record the death, spawned key, and pickup independently while retaining their
  relationship to the original objective.
- Some switch actions call check_trigger directly; boss/reactor actions can apply
  damage directly. An explanatory "Shoot this switch" marker is appropriate;
  invented weapon fire must not be represented as recorded engine behavior.
- Rear_view reverses orientation in render_frame only when Viewer equals
  get_player_view_object(). Passing the GuideBot to the cockpit function with
  rear_view_flag=1 alone will not implement the requested view.
- Rendering writes visibility lists, lighting state, and other globals. The
  Android FOV feature has a partial visual-only pass that saves render lists;
  it is a useful precedent, not a complete isolation guarantee.
- Existing headed/headless parity coverage checks semantics and records timing
  differences. It does not establish identical frame timing across platforms.

## Pipeline and determinism contract

| Approach | Tradeoff | Decision |
| --- | --- | --- |
| Record the current live window | Quick preview, but limited future awareness and weak control of timing/frame loss | Useful only as an early visual probe |
| Trace then rerun with isolated presentation | Full future/past smoothing and normal engine scene fidelity; requires exact same-build repeat checks | Recommended first implementation |
| Record complete render state then play it back | No second simulation; supports arbitrary camera edits and seeking, but requires a complete scene recording format | Explicit fallback if render isolation fails |

~~~mermaid
flowchart LR
    A[Mission assets and regression identity] --> B[Verify and trace]
    B --> C[Camera and presentation schedule]
    C --> D[Repeat simulation and check trace]
    D --> E[Engine views and overlays]
    E --> F[Frame encoder]
    C --> G[Step index and chapters]
    F --> H[Validated video package]
    G --> H
    H --> I[Upload queue]
~~~

### Pass A: verify and trace

1. Resolve mission metadata path, target_index, original game, engine mode,
   level number/file, secret flag, source assets, and route input hash using the
   same rules as regression generation. Preserve D1-in-D2 versus fan-conversion
   identity and negative secret-level numbering.
2. Start an isolated desktop export process with fixed seed, difficulty, speed,
   radius policy, settings, and immutable asset staging. Lock the executable
   and effective data content hashes for both passes.
3. Run the normal confirmation controller. Initially retain a fixed canonical
   render schedule with the unsmoothed actor camera, even when its image is
   discarded. This preserves any render-derived behavior until isolation is proven.
4. Record initial state at simulation frame 0, then actor pose, selected target,
   events, and relevant state checks at a documented boundary each simulation tick.
5. Require terminal confirmation and a matching expected objective projection
   before producing a normal walkthrough. Failed runs can produce explicitly
   labeled diagnostic videos, outside the normal publication queue.

### Schedule generation

Build the entire orientation track and presentation schedule from the trace.
Use actual future actor motion, not speculative planner points. This permits
anticipating turns and visible objectives while keeping camera choices separate
from the route. The same schedule drives the HUD, freezes, encoding timestamps,
subtitles, chapters, and map progress.

### Pass B: render a checked repeat

Reload the same assets and initial state in the same desktop executable and
configuration. Advance the same canonical simulation, retaining the same base
render cadence used in pass A. Add isolated presentation views to capture the
smoothed first-person view and rear window.

At each recorded boundary compare actor pose, relevant progression state, and
event sequence to pass A. Compare full objective frames and terminal state as
well. Fail the video with render_replay_mismatch on divergence; report the first
different frame and field. Do not move the actor to a recorded pose, overwrite
RNG state, grant an objective, or skip a blocked portion to make the video agree.

Exact repeat checks apply within the same capture platform/build. A canonical
headless corpus result establishes matching route input and semantic completion;
its times must not be used as timestamps for a different desktop run. Record
both provenance and the actual captured run's timings.

Audit extra rendering against:

- Actor/object state, wall and trigger state, player keys, controller state,
  RNG streams and call counts, AI-facing visibility information, and simulation
  frame/time counters
- Render-only state that affects repeat images: light smoothing, palette flash,
  effect animation, viewport/canvas, render lists, and cockpit caches
- Asynchronous route work and OS focus/pacing: encoder backpressure and a hidden
  window must not select different outcomes through wall-time-dependent behavior

A presentation view must neither publish simulation visibility nor consume the
simulation or canonical effects RNG. Give purely visual variation a local,
frame-derived presentation seed if needed. Freeze animations according to source
time. Restore transient graphics state after each view; do not restore engine
RNG as a workaround for accidental simulation calls.

Decision gate: prove the checked-repeat approach on the representative fixtures
before building batch polish. If isolating legacy rendering is impractical,
use a recorded render-state stream: initial scene plus per-tick transforms,
object lifecycle, walls, textures, lights, and effects, played through the engine
without AI or physics. That is a larger, explicit format project. Actor poses
alone, ordinary input demos, and assumed classic-demo compatibility are not an
adequate fallback. Do not quietly reduce the checks to final success alone.

## Recording schema and event semantics

Create a versioned video-specific trace, separate from checked-in simulation
JSON. Keep metadata normalized and human-readable; use a compact binary frame
stream if measurement warrants it. Reuse an existing compression dependency if
available. Specify endianness, units, schema version, counts, and bounds.

| Record | Required contents |
| --- | --- |
| Manifest | Engine executable/content hashes, source revision and dirty-source identity, platform, mission identity, effective asset/mod hashes and mount order, route input hash, regression generation/result hash, seed, difficulty, speed, radii, fixed Hz |
| Pose sample | Integer simulation frame, accumulated fixed-point ticks, actor index/signature, segment, position, orientation, velocity, route generation/current objective identity |
| Target sample/change | Stable semantic source, activation kind, resolved segment/side/wall/trigger, aim position, activation position, live object index/signature/type/radius, key color, linked affected doors |
| Geometry snapshot | Side vertices and triangulation, surface normal/UV basis, portal child and paired side, relevant object bounds; captured before mutation/removal |
| Event | Monotonic event sequence, source frame/ticks, pre_action/post_action/observed phase, semantic step key, occurrence number, event kind, source/affected targets, completion evidence |
| Repeat check | Fixed-field digest of relevant simulation state plus selected explicit fields for useful mismatch reporting |
| Terminal record | Confirmation result, frame/sample/event counts, stream checksum, first problem if any |

Generate one run-local objective ID when the controller selects a semantic goal;
include route generation and semantic geometry/object identifiers, and separately
preserve the authored route index for regression projection. A mutable live index
or object slot alone is not stable. Object references need signature plus slot
and level identity to survive slot reuse safely.

Record at least these events:

- Goal selected, goal revised, frontier changed, traversal/approach started
- Switch activation, hidden-door opening, blastable-wall destruction, keyed door
  passage, and incidental door interactions useful for navigation
- Fly-through/pass-through side crossing, including source and destination side
- Key carrier identified/destroyed, key spawned, key actually collected
- Reactor damaged/destroyed, boss death started, exit unlocked, exit entered
- Semantic objective completed, observed already satisfied, repeated activation,
  route failed, and route confirmed

Place recorder calls before objective deduplication and before replanning replaces
the old goal. Snapshot pre-action target geometry before check_trigger, damage,
pickup, or removal. Keep event observation read-only: do not call planner adoption,
simulation helpers, or mutating introspection just to fill a record.

Keep the verifier's current maximum of 96 semantic results explicit. The physical
event stream is separate and may be longer; bound it independently and fail with
a clear capacity error instead of truncating. Several events may share a frame;
event sequence establishes their order.

## Presentation clock, pauses, and timestamps

Maintain three distinct quantities:

- Simulation frame/ticks: authoritative engine time, never changed for presentation
- Video frame/PTS: integer output frame at a rational output frame rate
- Wall time: progress, worker deadlines, and throughput only

Represent the edit as ordered segments containing source frame interval,
presentation frame interval, playback rate, and optional hold source frame.
Use half-open intervals and a rational accumulator when translating durations
to frame counts. Export both source frame and fixed ticks because the 60 Hz
fixed-point timestep alternates integer tick lengths.

A source instant can occur in several video frames during a hold. Each event
therefore stores separate first-visible, action, completion, and recommended
seek video frames. Seek to the approach/focus shot, typically 1-2 seconds before
the action, rather than landing after the door opened or the key disappeared.

Proposed important-step sequence:

1. About 1.0 second before an interaction, show the instruction and strengthen its
   target highlight. Choose a visible approach frame from the current route leg.
2. Hold that source image for 0.6-0.8 seconds when it improves target recognition.
   Keep the camera settled and the instruction readable.
3. Resume through the real recorded action. Do not advance completion early.
4. At confirmed completion, hold 0.8-1.0 seconds and show a checkmark plus the
   satisfied objective. Let the completion emphasis fade over 3.0 video seconds,
   including the hold, then scroll the row into history.
5. Resume with a short eased rate transition if it improves readability. All
   easing changes the source-to-video schedule, not engine FrameTime.

The initial hold implementation caches the world image and projected marker
geometry and animates only the 2D composition. It does not repeatedly call the
normal game frame or render callbacks. Later camera motion during a frozen shot
requires a proven view-only renderer or recorded scene state.

For slow presentation, simulate all original ticks. Render interpolation of
position is optional and collision/segment checked; the initial implementation
can repeat world frames and interpolate only the HUD. For fast presentation,
skip output images according to the schedule, never simulation ticks. Preserve
at least one visible image of every important short-lived action.

Cluster same-frame or closely spaced related events into one pause/card with
several completion rows. Retain individual event timestamps in the sidecar.
Do not accumulate a long queue of stale completion cards over later objectives.
Repeated required switch activations receive a new occurrence label such as
"Shoot the switch again". Already-satisfied steps use that wording and do not
pretend an unobserved action was performed on camera.

Illustrative timing only: a completion at source 20.0 seconds appears at video
27.667 seconds when motion has been played at 0.75x and a previous 1.0-second
hold has been inserted. All later timestamps must include that hold. This is why
copying existing regression seconds into a YouTube description is insufficient.

## First-person camera

Shared camera policy: [GuideBot view smoothing and target attention](2026-09-07-guidebot-view-smoothing.md).
Use its Raw/Steady/Attentive modes and shared implementation across this exporter,
the metadata player, and the live regression viewer. The recorded-track outline
below is supplemented by that plan's live/recorded timing and target-framing rules.

### Position and orientation ownership

Keep the camera at the recorded actor center initially. Change the render view
matrix without writing to the actor's orientation or to a fake object inserted
in Objects. Retain the real actor identity for self-model exclusion and any
renderer references that expect an object from the object array. A stack copy
used as Viewer needs an audit for pointer arithmetic and retained pointers;
prefer an explicit camera transform override at view setup.

### Future-and-past smoothing algorithm

1. Split the trajectory at discontinuities, level transitions, resets, and
   untraversable jumps. Never smooth across a discontinuity.
2. Estimate travel direction from distance-weighted samples over roughly 0.35
   seconds of history and 0.65 seconds of future source motion. Arc-length
   weighting prevents stationary waits dominating the direction estimate.
3. Blend this direction with the authored/recorded forward direction. Increase
   travel influence while moving, hold the prior stable heading near zero speed,
   and treat reversals separately so opposite directions do not average to zero.
4. Parallel-transport the prior up vector along the trajectory and blend gently
   toward recorded roll where useful. Descent has no universal gravity-up axis;
   forcing world-up creates flips in vertical tunnels.
5. Blend a bounded framing preference toward the relevant visible target before
   the final angular filter. Use target-selection hysteresis and a comfortable
   framing region; release attention before close passage would cause a spin.
6. Construct a normalized target quaternion, make neighboring quaternion signs
   consistent, and apply the recorded future/past smoothing refinement. Avoid
   averaging Euler angles, especially across angle wrap or near vertical directions.
7. Apply bounded angular velocity and acceleration in presentation time after
   retiming and attention blending. Start evaluation around 90 degrees/second
   and 240 degrees/second squared; recheck limits at schedule boundaries.

Camera smoothing is an editorial calculation; it must never adjust steering,
route speed, flare aim, collisions, target selection, or success tests.

Avoid aiming at activation_pos: for a switch this can be a firing location in
space. Prefer aim_pos and resolved surface geometry. For robots/keys, use the
recorded live position and bound. Limit focus to targets actually visible in
the source world state, respecting transparent textures and grates.

Handle vertical shafts, banking, U-turns, stationary firing, near-zero movement,
and rearward targets explicitly. At a hard discontinuity, use a labeled cut or
reject the capture; do not create an apparent path through rock. If a key carrier
dies before it can be shown from this route, flag that step for review rather
than changing its lifetime. Later annotated cutaways need their own source-time
and camera provenance and must be visibly distinct from the continuous route.

## Objective highlighting

Render world markers with the current view projection and correct visibility.
Keep them bright and thick after compression, but leave the target surface or
robot readable. Use a dark outer stroke and a narrower color stroke, starting
around 6-8 output pixels at 1080p, scaled with output resolution. Prefer triangle
ribbons/rings with controlled pixel thickness over driver-dependent wide lines.

| Target | Geometry and behavior |
| --- | --- |
| Shootable switch | Thick ring lying on the actual switch face around aim_pos or the verified switch patch; clip to the face and offset slightly to prevent z-fighting |
| Hidden/key/important door | Outline the opening perimeter from live segment/side vertices, including paired-side resolution; keep a subtle frame outline as the panel opens |
| Blastable wall | Face perimeter plus light hatching and "Blast this wall" label; preserve pre-destruction geometry for the completion fade |
| Fly-through/pass-through region | Border on the actual crossing polygon, light translucent fill, and directional arrow; do not shade an entire segment and imply any entry satisfies the trigger |
| Boss/reactor | Outer sphere/silhouette rings and low-opacity shading, sized from the live object; label the required action |
| Key carrier | Key-colored sphere/rings and key icon; link the carrier to the dropped key event |
| Loose key | Smaller key-colored halo; completion only on actual collection |
| Affected distant door | Mark only when seen in the current view or map; explain the relationship in text without drawing a solid marker through intervening walls |

Use orange for the current instruction, red for other nearby actionable targets,
and blue/yellow/red with a key icon for key-bearing objects. Completion gets a
checkmark and fading emphasis. Icons, text, and stroke pattern distinguish
states independently of hue. A red carrier must remain distinguishable from
generic red action markers.

Switch textures may cover only part of a wall, and sides can be triangulated or
nonplanar. Resolve the hit/aim triangle and UV transform in engine code. If the
switch patch cannot be established, highlight the verified face with an explicit
face-level marker; do not invent a precise circular target at an arbitrary center.

Perform portal/frustum clipping, near-plane clipping, and depth testing against
the rendered scene. Test transparency at the same texture/geometry semantics as
the renderer. Do not use an always-visible screen-space ring as a substitute.
An offscreen direction indicator may be shown as an arrow with a label, clearly
separate from an on-surface marker.

The completion fade must survive removal of the target object or wall surface:
use the captured pre-action bound/face, and label it as the completed location.
Never attach a stale marker to a new object that reused the same object slot.

## Scrolling steps and video layout

Default 16:9 layout:

- Small top-left mission, level number/name, and first-person viewpoint label
- Compact top-right step panel, about 28 percent of width, with one previous
  completion, the current instruction, and the next two steps
- Rear-view window at bottom-left, about 24 percent of frame width, labeled REAR
- Optional map at bottom-right, about 22 percent of width
- Small elapsed video time and step number outside the aiming/center corridor

Reserve a 5 percent safe margin and check readability at 720p and phone viewing
size. Wrap labels to two lines, measure text widths, and keep font/graphics
settings in a versioned presentation profile. Avoid stock shield/ammo numbers
from the sandboxed player at the level start, debugging counters, and unrelated
GuideBot messages. Display the information needed to follow the route.

Use imperative labels while pending: "Shoot the switch", "Open the hidden door",
"Fly through the marked opening", "Collect the blue key". Completed labels can
be brief: "Blue key collected". Retain internal trigger/wall IDs in diagnostics;
show ordinal numbers or a location description only where viewers benefit.
Explain linked effects only when known from engine state, for example "Unlocks
the door to the next passage"; do not invent room names.

The panel's current step follows actual event state, not a timer. Animate row
insertion/scroll over about 0.25 seconds and completion emphasis over 3 seconds.
Keep the same step numbering in the panel, exact step index, and captions.

## Rear-view implementation

Reuse the cockpit subview's canvas/framing conventions, but render an explicit
camera at the same source position rotated 180 degrees about the smoothed
camera's local up axis. Do not rely on the player-only Rear_view special case.
The rear view is a backward-facing camera, not a horizontally mirrored image.

Both views must use exactly the same source time, wall state, object state, and
presentation hold. Give the rear view its own projection and label placement;
draw the current target there if it is visible behind the actor. Restore the
main view's canvas, viewport, depth state, and render lists afterward.

Keep ordinary gameplay cockpit behavior untouched. Prefer a small reusable
viewport helper and an explicit export render context to broad changes in
get_player_view_object or persistent cockpit preferences.

## Optional miniature map

Start with a stable oblique 3D wireframe of the local mine, not a rotating
top-down map. Keep orientation stable per level or leg and use slow bounded
recentering when the actor approaches the inset edge. Stacked passages need
depth/height cues; automatic full-mine fit is often unreadable.

Show the actor arrow, traveled trace, a short upcoming trace, and current target.
Use solid bright history, dotted future, and muted surrounding edges. The future
trace comes from this verified run, including its replans, not straight chords
between static objectives. When simplifying it for display, preserve segment
crossings and corners so it cannot depict travel through walls.

Read visited state or a presentation-owned visited set. Do not set
Automap_active, open a modal window, mutate Automap_visited for map appearance,
or call automap route adoption/update just to draw an inset. Reuse extracted
edge/label helpers where practical. Optional level overview cards and a mission
route index can follow once the inset is useful on small screens.

## Optional third-person following camera

Implement only after first-person capture and view isolation are reliable.
Render the GuideBot model and start with a camera boom about 3-5 actor radii
behind it, with a small local-up offset and the same trajectory-based orientation.

Sweep a camera sphere through the live mine from the actor toward the proposed
camera, including doors and blastable walls. Shorten the boom immediately when
blocked and extend it gradually after clearance returns. Test the transition
between consecutive camera positions too; a clear boom endpoint does not prove
the moving camera stayed outside walls. Validate the camera segment every frame.

Avoid smoothing position across corners, forced world-up, and large shoulder
offsets in narrow tunnels. Fall back smoothly to first person when clearance is
insufficient. Retest depth clipping and actor-model visibility in each mode.
The camera must never move the actor, open a door for clearance, or choose a
different route. A usable prototype is plausible; consistently pleasant framing
through arbitrary six-degree-of-freedom mines is the high-risk polish work.

## Capture, compression, and audio

Use a fixed-resolution render target, real graphics context, and bounded frame
queue. A headless route executable using dummy rendering cannot produce these
images. Initially use the desktop renderer offscreen/hidden where supported,
with an explicit preview option; unattended export must not steal keyboard focus.

Capture after the main view, rear view, world markers, map, HUD, palette effects,
and any MSAA resolve are complete, before buffer swap. Verify that the selected
readback buffer is the composed target. Handle pack alignment, row stride,
bottom-up OpenGL orientation, pixel format, and resize prevention explicitly.
Avoid the ordinary screenshot function's naming, filesystem, and UI side effects.

Feed raw frames through a binary pipe to an external FFmpeg process. Do not use
a text PowerShell pipeline for pixels. Drain encoder stderr asynchronously,
bound buffering to a few frames, and let encoder backpressure delay wall time
without dropping frames or changing simulation time. Use explicit argument arrays.

Initial encoding profile: H.264 through libx264, preset medium, CRF 18-20 for
evaluation, yuv420p, progressive 60 fps, MP4 with faststart. CRF is a project
quality choice, not a YouTube bitrate requirement. Pin the chosen FFmpeg build
and binary digest when implementation begins; preflight encoder availability.
FFmpeg documents explicit rawvideo size/pixel format/frame rate and the faststart
container flag in its [format documentation](https://ffmpeg.org/ffmpeg-formats.html),
and libx264 controls in its [codec documentation](https://ffmpeg.org/ffmpeg-codecs.html#libx264_002c-libx264rgb).

YouTube recommends MP4/H.264, progressive output, 4:2:0 chroma, the recorded frame
rate, and BT.709 SDR; its 1080p high-frame-rate reference bitrate is 12 Mbps.
Use a tested RGB-to-YUV range/color conversion, not color tags alone. These are
upload settings, not live-stream settings. See [YouTube upload encoding guidance](https://support.google.com/youtube/answer/1722171?hl=en).

At 1080p60 RGBA, raw output is about 498 MB/second, or 299 GB per ten minutes;
a four-frame queue is about 33 MB. At an illustrative 12 Mbps, ten minutes of
compressed video is about 900 MB excluding audio and overhead. Stream frames
instead of retaining a raw corpus; CRF output size will vary by level.

Write route.partial.mp4 and finalize to route.mp4 only after successful encoder
exit, expected frame count/duration, and a complete decode check. Retain useful
trace/log artifacts on failure. Track render and encoder heartbeat separately
from simulation timeout, since teaching pauses increase output duration.

Audio can follow as an engine mixer tap producing source-timed PCM. Use the
presentation schedule for retiming and silence/short fades during holds; do not
repeat a frozen weapon sound. Derive exact sample counts from video duration
with a rational accumulator. At 48 kHz and 60 fps there are 800 samples per
video frame. Spatial audio should reference the captured camera, not the parked
player. Keep music optional per publication profile; audio selection must be
explicit and included in artifact identity.

## Artifact package and batch behavior

Suggested output root: android/temp/guidebot_route_videos/<run-id>/.
For new timestamped artifact generations, use retain-recent-artifacts.ps1 as
required by repository guidance. Keep a long-lived publication catalog outside
scratch retention so cleanup cannot erase the upload/resume ledger.

Each level package contains:

| Artifact | Purpose |
| --- | --- |
| manifest.json | Provenance, settings, source/result hashes, status, counts, checksums |
| confirmation.json | Exact detailed result from the captured engine run |
| trace/ | Poses, target records, events, geometry, repeat checks |
| presentation.json | Camera/profile identity and source-to-video schedule |
| route.mp4 | Final compressed composed video |
| steps.json | Every event/step with source time, video action/completion time, and seek time |
| steps.vtt | Optional readable objective cues on the final video timeline |
| chapters.txt | Platform-compatible grouped chapters |
| description.txt and upload.json | Reviewable title, description, tags, mission identity, and publication settings |
| thumbnail.png | Representative in-engine frame with mission/level title |
| validation.json and logs/ | Repeat check, encoding, chapter, and visual coverage results |

Identify a simulation capture by effective asset content, engine binary/build,
simulation configuration, and route-input hash. Identify a presentation by that
capture plus camera/overlay/timeline/audio profile versions. Identify an encode
by presentation plus encoder version/settings. Do not use a human mission name
or sanitized filename as the only unique key.

Use separate stage statuses such as selected, tracing, trace_validated,
rendering, encoding, validated, upload_pending, uploaded, processing, published,
failed. Preserve the failing stage and first actionable error. Never overwrite
checked-in mission or simulation baselines as a side effect of video creation.

Reuse successful traces when inputs match. The checked-repeat design still
requires simulation/engine rendering for a camera change; changing captions or
encoding need not imply a new canonical regression. Begin with level-level
resume. Mid-level resume needs full simulation checkpoints or a render-state
stream and must not be claimed from pose samples alone.

Default to one GPU render worker, bounded encoder concurrency, and isolated
settings/work directories. Parallelize CPU trace jobs only after checking
contention and deterministic route-work ordering. Reserve sufficient disk space
before each stage, support cancellation of only the job's process tree, and
retain validated videos on later upload failures. Reuse established process
lifetime helpers where they fit.

Proposed future entry points, not commands currently implemented:

- android/helpers/generate_guidebot_route_videos.ps1 with MissionJson, target
  selection, Level, LevelFileFilter, OutputRoot, Profile, NoBuild, DryRun,
  Resume, and MaxRenderWorkers parameters
- A JSON job manifest consumed by the native exporter, avoiding a large family
  of duplicated command-line settings
- A separate publisher accepting validated package paths, channel profile,
  privacy/schedule settings, and resume mode

Make discovery and metadata planning usable without a renderer or encoder.
Dry-run must enumerate exact levels, eligibility, missing assets, stale results,
planned outputs, and estimated video duration without launching the engine.
Estimate duration from trace time/rates plus holds; compact corpus times only
provide a rough estimate before tracing.

## YouTube steps, chapters, and publishing

Keep the exact step index independent from YouTube chapters. Current manual
chapter rules require the first timestamp at 00:00, at least three ascending
timestamps, and chapters at least ten seconds long; chapter access can depend
on channel features. See [YouTube chapter guidance](https://support.google.com/youtube/answer/9884579?hl=en).

Build chapters from approach/seek times on the final video timeline. Merge
nearby steps into meaningful groups, such as "Blue key and hidden passage".
Check intervals after rounding to whole seconds, including the final chapter
through video end. Do not add artificial ten-second pauses to every switch just
to meet chapter rules. If a short level cannot support three valid chapters,
export the exact step list without promising chapter markers.

Keep the description's chapter block valid. Put dense per-step timing in
steps.json/VTT and a generated local index; once a video ID exists, generate
direct time-offset links for every step. An eventual hosted catalog may expose
that index, but is not required for initial rendering. Avoid a second dense
timestamp block that could interfere with chapter parsing.

Generate titles such as "Descent II - Counterstrike - Level 02: [Level name] -
GuideBot route", shortening optional suffixes as needed. Preserve mission
variant and D1-in-D2 identity. Include navigation-sandbox explanation, grouped
chapters, known route limitations, and available mission/author credits.
YouTube's video resource specifies a 100-character title limit and a 5000-byte
description limit; validate UTF-8 byte length and prohibited characters when
assembling the actual payload. See [video resource fields](https://developers.google.com/youtube/v3/docs/videos).

The future publisher should:

1. Consume only validated packages with a terminal successful route, matching
   inputs, and resolved important-step visibility checks. Keep diagnostics in
   a separate opt-in collection.
2. Use channel OAuth and the YouTube upload API, not browser automation. Save
   resumable upload session state and offsets outside Git and redact credentials
   and session URLs from ordinary logs. Resume after interrupted transfers;
   follow the [resumable upload protocol](https://developers.google.com/youtube/v3/guides/using_resumable_upload_protocol).
3. Persist a durable mapping from artifact hash to video ID before post-upload
   tasks. If completion is ambiguous, reconcile the session/account state instead
   of blindly uploading a duplicate. Upload APIs are not assumed to provide an
   application-level idempotency key.
4. Retry transient failures with bounded backoff, pause on quota/auth failures,
   poll processing status, then apply thumbnail, captions, and mission playlist
   membership using the required scopes. Store the final URL and processing state.
5. Support a configured private/unlisted/public policy and scheduling. Use a
   private pilot batch for initial visual review; once the user configures an
   automatic publication policy, apply it without per-video approval prompts.
6. Record supersession when a route or visual profile changes. Update text-only
   metadata in place when appropriate; replacement media produces a new video
   identity. Do not automatically delete earlier published videos.

As checked on 2026-09-07, the API documentation describes private-only uploads
for affected unverified API projects and an audit to lift that restriction.
Upload quotas have also changed from older unit-cost examples; read the active
project limits rather than hardcoding a videos-per-day estimate. See
[videos.insert](https://developers.google.com/youtube/v3/docs/videos/insert) and
[current quota guidance](https://developers.google.com/youtube/v3/getting-started).

No uploader connection, credentials, channel modifications, or publication is
part of this planning task. Channel identity, privacy/schedule policy, and audio
selection become configuration decisions when the publisher is implemented.

## Proposed code ownership

Keep related small pieces together initially instead of creating a framework.
Suggested boundaries, with final filenames chosen during implementation:

- shared/route_video_trace.h/.cpp: observer records, native target resolution,
  pose/event stream, checksums, and repeat diagnostics
- shared/route_video_presentation.h/.cpp: camera track, edit schedule, step
  presentation state, highlight descriptors, and exact timestamp conversion
- shared/route_video_render.h/.cpp: export context, world annotations, views,
  HUD composition, and framebuffer readback
- shared/route_video_export.h/.cpp: desktop job lifecycle, encoder transport,
  cancellation, progress, and finalization
- helpers/generate_guidebot_route_videos.ps1: corpus selection/staging and batch
  artifacts, reusing regression helpers without duplicating game-format logic
- tools/guidebot_video/: host packaging/publishing only where a small script is
  clearer than native code; no level parsing outside the engine

Use DXX_GUIDEBOT_ROUTE_VIDEO or an equivalent explicit build feature, with
desktop-only process/graphics dependencies separated from reusable trace and
presentation code. Existing Android verification should still build with the
recorder disabled. Consider corresponding D1 rendering hooks only when actually
needed; D1-in-D2 already uses the D2 hooks. Preserve Windows/Linux/macOS builds
and avoid adding a codec library dependency to the shipped game.

## Implementation sequence and acceptance gates

### Phase 0: prove capture boundaries and isolation

- [ ] Pin frame numbering, before/after-action event phase, and terminal capture
  behavior; capture frame 0 and the final state before window exit
- [ ] Add the opt-in trace observer and goal/target snapshot with no presentation
- [ ] Demonstrate two same-build trace runs with identical required samples/events
- [ ] Render raw first-person and explicit rear cameras without altering required
  simulation fields; audit RNG, visibility lists, lighting, and async route work
- [ ] Decide checked-repeat versus render-state recording using those results

Acceptance: exact same-build repeat checks and semantic agreement with the chosen
regression inputs on a small representative set. A concrete mismatch is a design
finding to resolve, not a reason to weaken verification.

### Phase 1: smallest useful video exporter

- [ ] Isolated desktop job, fixed framebuffer, timestamped frame stream, FFmpeg
  transport, cancellation, and reliable final-frame drain
- [ ] First-person camera, static rear inset, elapsed time/current step, exact
  steps.json, and final MP4 validation
- [ ] Single-level and selected-level batch commands with NoBuild and DryRun
- [ ] Distinct failed-route, replay-mismatch, graphics, disk, and encoder errors

Acceptance: repeatable compressed level videos with synchronized rear view and
correct event timestamps, no dropped output frames, no gameplay mutations, and
no shared settings/baseline changes.

### Phase 2: teachable walkthrough presentation

- [ ] Future/past quaternion camera smoothing and collision-safe focus behavior
- [ ] Separate presentation clock, important-step holds, 3-second completion
  emphasis, clustered events, and scrolling step panel
- [ ] Switch rings, door/wall outlines, portal shading, boss/carrier/key markers
- [ ] Pre-action target preservation, implicit/repeated-step wording, and linked
  action/effect labels
- [ ] Representative event contact sheets and important-target visibility checks

Acceptance: viewers can identify the passage/target before each required action,
follow the route continuously, and read completion after it occurs. Unshowable
important targets get explicit review findings, not fabricated shots.

### Phase 3: corpus packaging and publication-ready metadata

- [ ] Hash-based caching, level resume, render-resource limits, stage progress,
  durable publication catalog, and retention integration
- [ ] Chapter grouping, exact time-offset index, captions, thumbnails, title and
  description validation, and mission/variant ordering
- [ ] Representative local review set followed by a larger successful-level
  packaging batch; private pilot uploads belong to Phase 4

Acceptance: interrupted batches resume without replacing baselines, stale assets
cannot reuse videos, and every published package has a traceable confirmed source.

### Phase 4: publisher

- [ ] OAuth setup, resumable uploads, duplicate reconciliation, processing polls,
  quota-aware scheduling, playlist/caption/thumbnail operations
- [ ] Channel privacy and schedule policy, retry/resume, and supersession records

Acceptance: an interrupted pilot upload resumes or reconciles to one video ID;
the configured queue can progress without per-video manual work.

### Phase 5: optional viewing modes and audio

- [ ] Stable local 3D map inset and optional level-overview card
- [ ] Source-timed engine effects audio with synchronized holds and camera listener
- [ ] Collision-safe chase view, first-person fallback, and optional marked cutaways

Acceptance: additional views/audio preserve simulation repeat checks and remain
clear at small playback sizes. The first-person export is useful independently.

## Validation plan for the later implementation

No checks below have been executed for this planning task.

Prefer a high-level export integration runner over tests that mirror individual
drawing functions. Reuse existing mission fixtures where possible:

| Fixture/selection | Coverage |
| --- | --- |
| Counterstrike level 1 | Key, reactor, exit, fast terminal sequence, complete package |
| Counterstrike level 2 | Several switches, hidden door, close completions, fly-through, repeated/restorer behavior |
| Counterstrike level 10 | Key carrier, object removal, key spawn and pickup relationship |
| Counterstrike levels 20 and 24 | Boss death, exit unlock, final-level lifecycle |
| FirstStrike selected passing level through D2 | Original D1 assets and identity, distinct from fan conversion |
| FirstStrike long-path fixture | Long traversal and camera comfort; select a currently passing instance |
| Castaway restored-switch route fixtures | Revisited interactions and source/affected-door relations |
| Obsidian switch/grate and blastable-wall fixtures | Occlusion, difficult surface visibility, wall destruction |
| Vertigo selected passing door route | Mission data variation and keyed-door framing |
| TEW selected passing level | Larger custom assets/geometry and label readability |
| A secret level and a deliberate failed/timeout run | Identity and partial-diagnostic behavior |

Fixture names indicate intended coverage, not a claim of current success. Resolve
the active corpus result and fail/skip with a reason if the selected fixture is
not eligible; do not silently change routes or certify broken cases for a demo.

Meaningful acceptance checks:

- Repeat invariance with smoothing/rear/map/holds toggled, plus slow encoder
  backpressure and window focus changes; first differing field on mismatch
- Timeline case with several holds, fractional speed, simultaneous events,
  repeated triggers, early target deletion, and final-frame completion
- All important event source frames map to valid video frames; exact steps,
  captions, and visible checkmarks agree within one output frame. Whole-second
  chapter timestamps round the chosen approach seek earlier by less than one
  second, with grouping intervals revalidated after rounding
- Camera continuity through roll wrap, U-turns, vertical shafts, near-zero speed,
  tight portals, and discontinuities; no interpolated camera position outside
  the traversed mine or through a closed surface
- Rear view actually faces backward; an asymmetric corridor/target fixture
  detects a forward duplicate or mirrored composition
- Markers project to the right side/triangle/object, stay occluded correctly,
  and remain tied to the original target after destruction or slot reuse
- Decode the complete output; validate frame count, duration, frame rate,
  resolution, pixel format, metadata, and audio sample count when enabled
- Inspect full moving pilot videos and before/action/after contact sheets;
  static screenshots alone cannot establish camera comfort or fade timing
- Missing assets, stale hashes, encoder disappearance, write failure, cancellation,
  partial output, upload interruption, and ambiguous upload completion preserve
  useful errors and do not report success or duplicate publication
- Build the relevant desktop targets and preserve ordinary D1/D2 and Android
  configurations in the later implementation; run the scoped code-quality
  command and the meaningful export integration checks then

Do not demand byte-identical compressed MP4s across different encoders/GPUs.
Require deterministic source events/timelines and same-build simulation repeat
checks; use tolerant image/visibility assertions plus visual review for rendering.

## First implementation handoff

Begin with Phase 0 on Counterstrike levels 1 and 2 and a key-carrier fixture.
The first concrete review artifact should be a short 1080p60 video showing one
switch, one door/portal transition, and one key, together with its source event
trace and synchronized step index. Establish this before scaling to the corpus.

The main uncertainty is isolation of legacy rendering from repeat simulation.
Once that is proven, step presentation, compression, and batch metadata are
bounded engineering work. Map and chase-camera polish should not delay the
initial body of useful first-person navigation videos.
