# Metadata viewer: interactive GuideBot route player

Planning date: 2026-09-07

Status: planning only. This document is based on source inspection. No code,
compilation, engine run, emulator, simulation generation, or runtime test is
part of this task. Other tasks are changing route code concurrently; confirm
the implementation revision before starting work.

Related design: [Automated GuideBot route videos](2026-09-07-guidebot-route-videos.md).
This plan adds interactive inspection to that recording/presentation work.
Shared automatic camera policy: [GuideBot view smoothing and target attention](2026-09-07-guidebot-view-smoothing.md),
covering Raw/Steady/Attentive modes for this player, video export, and the live
regression viewer. Preserve selected-layout touch controls for its mode selector.

Optional [Robot showcase](2026-09-07-guidebot-robot-showcase.md) adds moving ordinary
robots, harmless attacks, and 1-2 second visibility-timed explosions as a separate
recorded presentation track. Seeking restores that track; free-camera inspection
does not change the reference-view death schedule or the verified route.

## Intended experience

Add "Watch GuideBot route" beside "Preview map" in a level's metadata details.
It opens an isolated in-engine viewer of the regression route, starting in
first person, with play/pause, a seek bar, short backward/forward jumps, and
previous/next objective buttons. A list of objectives is tappable so the user
can jump directly to a useful view of an interaction.

The touch controls follow the existing automap overlay pattern: load the touch
interface the player has selected, retain only the parts appropriate to route
viewing, and add route-specific replay buttons. Preserve the selected layout's
positions, sizes, stick/gesture behavior, and customization. This is a required
interaction model, not an optional resemblance to the normal controls.

An optional "Free camera" mode lets the user leave the GuideBot's viewpoint and
fly through the level using familiar engine movement controls. The playback
world remains independent from that camera. "Follow GuideBot" returns to the
route at the current playback time.

Use the simulation's original time by default. This is an inspection tool;
the teaching pauses and retimed timestamps proposed for exported walkthroughs
can be an optional later presentation profile. Display the navigation-sandbox
description once, since the verifier removes ordinary robots and simplifies
progression combat. Robot showcase can restore harmless active ordinary robots
for presentation; it does not turn the recording into a normal combat run.

## Decisions and first-release scope

| Topic | Recommendation |
| --- | --- |
| Entry | A second action in the existing per-level metadata dialog |
| Rendering | Actual engine scene, not an embedded MP4 or static path animation |
| Initial support | D2 plus original D1 missions through the D2 engine |
| Playback source | A completed, indexed recording of a real route-confirmation run |
| Seeking | Restore a recorded scene keyframe and apply scene updates to the requested frame |
| Basic controls | Play/pause, seek bar, -5/+5 seconds, previous/next objective, tappable objective list |
| Touch interface | Filter the player's selected TouchOverlayView layout like automap, then add replay actions through the existing overlay/button machinery |
| Objective jump | Pause on an approach frame roughly 1.5 seconds before the action |
| Free camera | Optional second milestone; unlocking pauses the route by default |
| Reattach | Follow GuideBot at the current source time; do not restart the route |
| Missing recording | Prepare one on demand with visible progress and cancel, then cache it |
| Partial simulation | Allow explicitly labeled inspection of the recorded prefix |
| Robot showcase | Optional derived track with active ordinary robots, harmless attacks, and visibility-timed explosions; clean remains the default |
| Shared work | Reuse route recorder, event identities, camera math, and world markers with video export |

Do not make YouTube, FFmpeg, audio export, a map inset, chase-camera polish, or
smooth continuous reverse playback prerequisites for this viewer.

## Current code and integration points

Kotlin paths below are under android/app/src/main/java/com/dxxredux/app/.
Shared native paths are under android/app/src/main/cpp/shared/.

| Existing location | Current behavior | Planned use |
| --- | --- | --- |
| SetupSections.kt: LevelMetadataLevelDialog | Shows Preview map for a successfully scanned level | Add a route-view action and separate recording availability/status |
| SetupSections.kt: metadata result launch callbacks | Prepares requests off the UI thread, launches preview Activity, receives result/error | Reuse launch/result pattern and preserve selected level/list position |
| LevelMetadata.kt: LevelMetadataLevelRow | Contains static route status and steps | Display planned steps; do not mistake them for recorded timed events |
| LevelMetadata.kt: buildPreviewRequestJson | Prepares mission content and selected level identity | Share asset preparation with a route-specific request |
| LevelPreviewRequestStore.kt | Owns request directories, validates input paths, creates runtime-write isolation | Reuse ownership helpers and validation approach |
| LevelPreviewActivity.kt | SurfaceView, TouchOverlayView, InputMixer, loading overlay, native thread, close and result callbacks | Reuse focused lifecycle/input helpers; add playback UI in a separate Activity |
| TouchLayoutRepository.kt and TouchOverlayView.kt | Selected layout loading, geometry, mode-based visibility/input filtering, contextual action tray | Load the selected layout and apply route-view policy without changing its saved definition |
| AutomapTouchPolicy.kt | Movement-button allowlist and contextual RemainingTouchAction entries | Follow this pattern for replay action visibility and availability |
| AndroidManifest.xml | Separate :levelpreview_d1/:levelpreview_d2 and robot preview processes | Add a dedicated D2 route preview process with the same isolation principles |
| SetupActivity.kt and LevelPreviewReturnRefreshGate.kt | Preserve metadata state after a read-only preview return | Include route-view returns and preserve a paused full game |
| RouteMetadataPrecomputeCoordinator.kt | Suspends background metadata work while the metadata viewer is focused | Keep the route-view session covered by that ownership/priority policy |
| android/app/src/main/cpp/jni_main.c: android_start_preview | Native admission, preview request launch, callback, process termination | Extend narrowly for the route-preview runtime; terminate only its own process |
| android/app/src/main/cpp/android_input.c | Preview input JNI bridges | Forward commands and camera controls without driving simulation input |
| shared/android_level_preview.cpp | Asset/palette setup and automap/robot preview runtimes | Share a small loader boundary; route playback needs its own loop |
| shared/route_confirmation.cpp/.h | Canonical route controller, fixed ticks, goals, completion times and terminal state | Record a real run and its interactions |
| d2/main/render.c, gamerend.c, gauges.c | Engine view setup, scene drawing, cockpit subviews | Add the isolated recorded-scene presentation context from the video design |
| shared/android_rewind.c and state_android_shared.h | In-memory save/restore building blocks | Audit for experiments; ordinary gameplay rewind is not a route-player timeline |
| d2/main/controls.c: read_flying_controls | Existing movement axes/control interpretation | Reuse mappings and conventions in a camera controller, without player side effects |
| shared/game_introspect.cpp and preview introspection | Structured inspection and automation | Add route-player state and acknowledged commands |

Specific findings:

- The map preview loads a level, selects a start object, scans metadata, and
  opens the automap. Its event loop supplies a control timestep but does not
  run full game simulation. Calling route_confirmation_start inside that loop
  alone would not produce a progressing verification run.
- Map preview uses target.game to choose a D1 or D2 library. Route viewing of
  a D1 mission needs an explicit D2 engine mode and the original D1 assets,
  rather than reusing that library-selection rule unchanged.
- Existing route result summaries contain completion times, not enough scene
  state to restore a door, robot, or key after seeking backward. The metadata
  row parser inspected here does not itself provide a playback recording.
- The gameplay rewind ring is currently limited to 12 points at five-second
  intervals. Restoring truncates future history and can truncate a recording's
  input/RNG trace. Those semantics conflict with non-destructive scrubbing.
- Save state includes some RNG and D-tick state, but route_confirmation keeps a
  separate controller_state containing goal, action phase, waits, timestep
  remainder, carrier timer, sandbox state, and more. No route-controller
  snapshot/restore interface was found in that header.
- Multiplayer observer code changes ConsoleObject and depends on observer/net
  state. It is not a drop-in free camera for a read-only route inspection tool.
- Existing map preview closes on surface destruction and releases inputs on
  stop. Define route-player lifecycle behavior explicitly instead of assuming
  background playback or automatic native reattachment already exists.

## Shared architecture with the video feature

The video plan recommends a traced run followed by a checked rerun as an initial
export route, with recorded scene playback as an alternative. Interactive
backward seeking and an arbitrary camera make recorded scene playback much more
valuable. For the combined feature set, prioritize a shared seekable scene
recording before polishing the viewer controls.

~~~mermaid
flowchart LR
    A[Metadata level selection] --> B[Resolve matching recording]
    B --> C[Prepare route if missing]
    C --> D[Validated scene recording and events]
    B --> D
    D --> E[Native scene playback]
    E --> F[Interactive camera and controls]
    E --> G[Video camera and edit schedule]
~~~

The key separation is capture versus playback:

1. Capture runs the real confirmation controller, physics, triggers, and normal
   objective actions. Its result is the route evidence.
2. Playback reconstructs that captured scene and renders it. It does not run
   physics, AI, damage, pickup, trigger dispatch, or route replanning.
3. Viewer actions select time and camera only. A user cannot change what was
   verified by moving the camera or seeking past a switch.

These are phases in one isolated native session initially. Prepare the full
recording before enabling playback, then stop simulation permanently for that
session and reconstruct scenes from the recording. Do not maintain a live
simulation and a rewound playback world in the same engine globals.

### Alternatives and the decision gate

| Approach | Benefit | Limitation | Use |
| --- | --- | --- | --- |
| Restart and simulate forward for every seek | Small initial prototype | Increasing latency, full restore/determinism concerns, costly scrubbing | Internal proof only |
| Full simulation checkpoints plus forward replay | Can reuse save machinery | Must capture all controller/planner/runtime state and isolate rendering; ordinary rewind is incomplete | Fallback prototype if scene recording proves harder |
| Scene keyframes plus per-tick scene changes | Fast arbitrary seek and independent camera, no simulation during playback | Requires complete render-visible state capture | Recommended shared architecture |
| GuideBot poses plus static level | Very small | Doors/keys/robots are wrong across time | Insufficient for the requested feature |

Prove a door opening, key pickup, and key-carrier death can be reconstructed at
arbitrary times and viewed from both sides before committing to the format.
Do not expose a polished seek bar over an incomplete pose-only recording.

If using simulation checkpoints for a prototype, restore the latest complete
checkpoint at or before the target, then simulate forward and compare to the
reference trace. Never use negative FrameTime or reverse physics. Capture all
route-controller state, object references/remaps, RNG states and counts, D-tick
and fixed-time remainders, AI/path state, metadata/controller generations, and
pending route work. Quiesce workers at capture/restore boundaries and reject
stale publications. Do not call the gameplay rewind request API, truncate the
future timeline, or assume an ordinary save is an exact checkpoint.

## Seekable scene recording

Share the manifest, objective/event schema, fixed source ticks, stable object
identity, pre-action geometry, and source provenance with the video plan.
Add scene keyframes, scene updates, and an index. Keep the portable on-disk
representation separate from C struct layouts and process addresses.

| Data | Required state |
| --- | --- |
| Initial content | Mission/level identity, asset/mod content hashes and mount order, source engine/game mode, geometry/content references |
| Scene keyframe | Complete dynamic render-visible state at one source tick; no dependence on a prior keyframe |
| Scene update | Object create/remove/change, transforms, model/submodel animation, render type, explosion/projectile state, dynamic wall/door/cloak state, textures, lighting and effects |
| Render clock | Source GameTime/ticks and animation phases needed to draw a frozen or restored scene consistently |
| Objective state | Active goal, completed/implicit states, occurrence identities, retained target geometry, actor/target locations |
| Index | Keyframe offsets and source ranges, event-to-source-frame mapping, total recorded extent, checksums and terminal status |
| Provenance | Build/profile/seed/difficulty/radii and confirmation result, route input hash, capture origin and exact captured times |

Capture state for the entire loaded level, including objects outside the
GuideBot's current render list. A free camera can look into any accessible
room, so a recording of only what the first-person camera saw is incomplete.
Store required effect inputs/phases explicitly where the renderer otherwise
derives them from mutable time or random state. Visual capture must remain an
observer and pass the same simulation-invariance checks as the video recorder.

Use frame 0, periodic keyframes, event-adjacent points, and the terminal frame.
Start evaluating a two-second keyframe interval and a small decoded-block LRU;
tune after measuring real scene sizes. Avoid a full object array snapshot on
every frame. At 60 Hz, the longest two-second block contains 120 tick updates,
but decoding cost and object counts still need measurement.

Use the engine's own types/adapters to reconstruct walls and objects. Kotlin
never parses level geometry, decodes save structures, or decides if an objective
is satisfied. Object identities include creation/signature information so
rewinding a destroyed carrier cannot attach its marker to a reused slot.

Scene restoration must rebuild object/segment lists and other derived rendering
indices consistently, clear view caches, and preserve non-visible scene state.
Only interpolate continuous values between adjacent source samples: position
and orientation where valid. Doors changing type, key acquisition, destruction,
trigger state, and spawn/remove events occur at their recorded frame boundary.

The native playback renderer must avoid mutating restored scene state through
render helpers. Give frame-derived effects a presentation-local context and
keep simulation-facing visibility/RNG untouched. Otherwise repeatedly drawing
a paused frame or switching views could alter the next image or scene check.

## Obtaining and caching a recording

"Watch GuideBot route" resolves one of these states:

| Availability | Viewer behavior |
| --- | --- |
| Matching complete recording | Open at the remembered time, paused; first open starts at frame 0 |
| Compact simulation result only | Show Preparing route, generate the richer recording, then open |
| No simulation run yet, valid level input | Offer the same action with "Prepares a route on first use"; run the verifier on demand |
| Partial/failed run with a valid recorded prefix | Show "Watch partial route" and the terminal reason; allow inspection up to the last valid frame |
| Stale recording/assets/profile | Prepare a matching recording; do not play it against different geometry |
| Required assets/engine unavailable | Explain the missing requirement; leave Preview map available |
| Corrupt/incomplete recording | Reject unreadable blocks, keep useful diagnostics, offer preparation again |

The result must come from this capture. A Windows regression result is not proof
that a new Android capture has the same frame times. Use a compatible imported
recording to inspect that exact host run, or generate locally and label its
capture origin in details. Do not call a locally regenerated run the exact
historical regression recording. Static route status, simulation status, and
recording availability are distinct fields.

Reuse mission staging/content identity from metadata and regression tooling.
Do not assume game_data/mission_files or its simulation JSON files are packaged
or accessible on a phone. Native preparation and a recording catalog must
explicitly connect the selected installed mission to the matching source.

For generation, initialize the full confirmation game state, including authored
player start, player/GuideBot setup, timers, and mission mode. Audit against the
existing route-confirmation launch paths; the map preview's load_level setup is
not automatically equivalent to StartNewGame. Select original D1-in-D2 assets
and D2 engine behavior using explicit content_game and engine_game fields.

Preparation shows phases such as Loading level, Simulating route, and Indexing
recording, with current source seconds/objective and Cancel. Simulation duration
is unknown until it finishes; show elapsed work rather than a false percentage.
Keep the engine event queue and close requests responsive between bounded work
batches. Enforce simulation-time limits separately from wall-time resource
limits. Record a valid partial prefix only when its scene blocks and events can
be finalized coherently.

Use a cache key including effective mission assets, level/variant, route input,
capture engine/profile and recorder version. Playback compatibility is a separate
check: schema and engine render semantics plus exact assets, not byte identity
of the capture executable on a different CPU. Do not dump native pointers,
padding, or endianness-dependent structs into portable recordings.

Transient launch directories and runtime-write directories remain disposable.
Completed recordings live in a separate owned cache with atomic publication,
size limits, and least-recently-used eviction; pin the currently viewed file.
Preview-request cleanup must not delete a reusable recording. Save a small
resume record keyed to recording hash, containing source frame and camera mode;
do not write a game save or a launcher "continue game" offer.

Default to generating only the selected level. Defer whole-mission background
preparation and incremental live watching. A full first pass provides trustworthy
objective times, total duration, and arbitrary jumps without two simultaneous
engine worlds.

## Playback controls and exact semantics

Use the selected touch layout with a route-view policy, following the automap's
existing filtering and contextual-button approach. Keep Close always reachable.
Add source elapsed/total time and a seek bar with objective markers in available
space, and let the objective list collapse into a panel. Position these additions
around the retained layout controls; a fixed bottom strip must not cover a
player's customized sticks or buttons.

### Selected-layout filtering and replay buttons

Load TouchLayoutRepository.load(context) through the same path used by map
preview, then apply a transient route-view control policy. The saved layout stays
unchanged. Keep the same control geometry and eligible bindings; filtering out
combat controls should not move the remaining controls to new positions.

| Control group | Following GuideBot | Free camera |
| --- | --- | --- |
| Selected movement/look sticks, slide/bank buttons, eligible gesture axes | Hidden/inactive while the view is locked | Visible and active with the player's chosen layout and bindings |
| Settings/menu entry point and appropriate preview-close control | Retained using the existing contextual overlay behavior | Retained |
| Weapon, flare, inventory, combat diagnostics, gameplay-only radial actions | Filtered out of drawing and input | Filtered out of drawing and input |
| Route replay buttons | Play/pause, backward/forward, previous/next objective, objective list, camera toggle | Same actions, with Follow GuideBot available |
| Speed and precision actions | Existing contextual tray/panel for secondary actions | Same |

Add replay buttons through the existing TouchOverlayView action/button styling,
hit testing, pointer ownership, and contextual tray machinery. Keep frequently
used replay actions directly accessible where space permits; expose the complete
set in the contextual panel. The timeline and tappable list supplement that
interface. Do not introduce another movement-pad preset or require the player
to create a separate replay layout.

Respect layouts intended for hardware controllers or menu-only touch use. Keep
their minimal touch interface and provide access to replay actions without
inventing onscreen movement sticks. Use the selected layout's actual occupied
bounds and safe-area rules to place replay additions; on a crowded layout, use
the existing expandable panel instead of overlapping or relocating controls.

Introduce a focused RouteReplayTouchPolicy, analogous to AutomapTouchPolicy,
with binding visibility/eligibility and contextual replay action descriptors.
Reuse or narrowly generalize the existing mode filter/action-provider boundary
in TouchOverlayView. Do not set automapActive merely to borrow its appearance:
route viewing has different actions and must not invoke automap commands.

Apply the same filter to drawing, hit testing, button-mode stick directions,
double-tap bindings, extreme-stick actions, radial entries, and held-input
latches. An allowed movement stick can contain a disallowed fire gesture;
retain the movement and suppress that gesture. Release newly hidden/disallowed
inputs when switching modes. Hidden controls must never remain touch-active.

Replay buttons dispatch explicit route commands. Do not reinterpret the player's
forward/reverse movement bindings as timeline controls or reuse gameplay rewind
for backward seeking. The selected layout and UI preferences are presentation
settings, so changing them does not invalidate the underlying scene recording.

| Control | Behavior |
| --- | --- |
| Play/pause | Advance or freeze recorded source time; reaching the end pauses on the final frame |
| -5 / +5 seconds | Clamp within recorded extent; preserve play/pause intent after the seek |
| Seek bar drag | Pause while dragging, show requested time, decode bounded previews, resume prior play intent on release |
| Previous objective | Return to current objective's approach if well past it; otherwise go to the previous distinct approach |
| Next objective | Seek to the next distinct occurrence's approach and pause |
| Objective row tap | Seek to that occurrence's approach, attach first-person camera, highlight target, and pause |
| Completion marker/detail action | Optionally seek to the exact completion frame rather than its approach |
| Speed | 0.25x, 0.5x, 1x, 2x, 4x source playback; change cursor rate, never recorded physics |
| Follow GuideBot | Attach camera to current recorded actor, reset transient camera input, retain current pause/play state |
| Automatic view style | Raw, Steady, or Attentive through the route contextual actions; affects presentation only |
| Robot showcase | Toggle the derived robot track through contextual actions; retain source cursor and pause/play intent, preparing the track if needed |
| Free camera | Detach at the current viewpoint and pause playback by default |
| Close / system Back | Dismiss expanded panel first if open, otherwise close the viewer and return to metadata |

Previous-objective behavior should be deterministic: for example, return to the
current approach if more than two source seconds past it, otherwise select the
preceding approach. Coalesce identical approach times for next/previous buttons
while retaining individual rows for simultaneous events. All values here are
initial UX settings to validate later.

Display source-time labels, not export video timestamps. The player has no
automatic teaching holds by default. Frame-step backward/forward can live in a
small precision menu once basic seeking works; backward frame step uses the same
indexed restoration, not reverse simulation.

Use at least comfortable touch targets, safe-area insets, controller focus, and
readable text at narrow widths. In landscape, use a right-side objective drawer;
in portrait, use a bottom sheet above the controls. Scrolling the objective list
must not rotate the camera or move the seek bar. Manual list scrolling suspends
auto-follow until the user taps a "Current step" control or closes the panel.

### Seeking implementation

1. Convert requested source time to a clamped integer frame using the recording's
   timebase. Keep requested and displayed cursor values distinct.
2. Choose the latest keyframe at or before that frame, validate/decode it, and
   apply forward scene updates in sequence through the target boundary.
3. Rebuild derived rendering state and derive the active/completed step state
   from the indexed event prefix. Do not replay old sound or toast callbacks.
4. Publish the complete new scene/cursor together at an engine-frame boundary,
   acknowledge the request ID, and render that scene.
5. Apply the command's pause/play and camera policy only after the seek commits.

Rapid scrubs use monotonic request IDs and latest-target-wins coalescing. Bound
preview-seek frequency, initially around ten requests per second, and always
process the exact release target. Discard canceled decode results; do not briefly
show an older seek after a newer one. Keep the last valid scene visible with
"Seeking..." if work takes perceptible time. Close cancels queued/decode work.

UI callbacks enqueue native commands; they must not change Objects, Viewer, or
world arrays on the Android UI thread. Decode immutable blocks off-thread only
if needed, then commit them on the owning engine thread. Never render half a
restored scene.

For an initial milestone, backward/forward jumps and a settled scrub release
can seek exactly before continuous drag previews are optimized. Do not promise
a fixed seek latency until it has been measured on target devices.

## Objective list and world feedback

Use the actual captured event index, with static metadata steps supplying useful
labels or pending placeholders only. Each row has an occurrence ID, imperative
label, source approach/action/completion times where known, and current status.
Mark completion relative to the playback cursor, not permanently because the
overall recording eventually succeeded. Seeking backward removes later checks.

Handle the existing controller's special cases:

- Repeated/restorer activations remain separate occurrences, even though the
  regression summary deduplicates semantic objective timings
- Implicitly satisfied steps say "Already satisfied" at their observation time;
  they do not invent an on-camera action
- Key-carrier death, dropped-key spawn, and collection remain related but distinct
  events; the key is not collected merely because the robot disappeared
- A switch action can affect a distant door; list the relationship from engine
  data and highlight the target actually present in the selected scene
- Several same-frame completions form a group with individually addressable
  rows, and an exit/failure row remains inspectable after the run finishes

Choose approach times from the recorded preceding leg and target visibility,
using about 1.5 seconds of lead-in where available. If the target was not visible,
show that fact and offer the free camera; do not move the route actor or change
the source event. Objective taps reset to a known useful follow viewpoint;
ordinary timeline seeks can retain an existing free-camera pose.

Start with a simple current-target marker and readable step label. Share the
video plan's surface rings, door/wall borders, portal shading, and robot/key
outlines as they become available. Never use a marker visible through rock to
stand in for a correctly projected on-surface highlight. A separate directional
arrow may indicate an offscreen target.

## Free camera and engine controls

Maintain a camera transform and segment separately from the recorded actor and
the sandboxed player. Use the same movement axes, sensitivity, deadzones, inversion,
and pitch/yaw/roll/slide conventions as gameplay. Use the player's selected
TouchOverlayView layout and InputMixer with the mode filtering defined above;
route replay buttons and panels own their input ahead of camera movement.

The current preview initializes new_player_config rather than loading a pilot.
Carry the resolved read-only input configuration needed by the selected layout
into this session, including custom bindings and sensitivity settings. Verify
that default native initialization does not silently replace them. Keep the
camera speed setting separate from ship physics and canonical GuideBot speed.

Do not route free-camera input through normal player gameplay processing:

- Translation and rotation update only camera state
- Fire, flare, use, pickup, trigger crossing, damage, deploy GuideBot, gameplay
  rewind, saving, and multiplayer actions have no effect on the recording
- No guidebot steering, player inventory, route progress, or cached metadata is
  changed by looking or flying around
- Turning or flying while the route is paused remains possible

Use a camera/input delta based on a monotonic viewer clock, clamped after stalls,
independent from recorded GameTime and playback speed. Paused playback must not
set camera movement time to zero or require advancing the game just to move.
Playing at 4x must not make the free camera move four times faster.

Default camera collision uses a small swept camera sphere against the currently
restored mine. The camera may pass through already open portals and avoid walls,
but cannot open doors. Starting from the actor position gives a valid initial
segment. "Follow GuideBot" is always available as a recovery action.

If the user seeks backward while detached and a closed door/wall now intersects
the camera, find a nearby valid camera location in the new scene or return to
the actor with a brief explanation. Do not modify the wall state to accommodate
the camera. If the user resumes playback while detached, leave the camera in
world space and let the recorded GuideBot move independently; do not silently
reattach or orbit a moving target.

Noclip is optional later. It needs an explicit toggle, valid segment reacquisition,
and defined rendering behavior outside the mine; a stock cheat flag is not a
sufficient implementation. The first version can remain within traversable space.

If the camera is detached, draw the GuideBot model and optional route line where
visible. In first person, suppress only that model appropriately. Avoid creating
a new live object that changes object allocation or pointer-based renderer
identity. Use the explicit camera override proposed for the video renderer.

Release all held inputs on unlock/reattach, seeking, focus loss, cancel, and close.
When the objective list has controller focus, stick/D-pad input navigates the
list rather than also flying. Define keyboard/controller bindings in the viewer
context without rewriting the user's gameplay bindings.

## Android lifecycle and isolation

Add a RouteSimulationPreviewActivity in a dedicated :routepreview_d2 process,
not exported, excluded from recents, separate from launcher and :game. It loads
one engine library. Keep D1 content identity in the request while selecting the
D2 library explicitly for D1-in-D2 viewing. Validate required D2 and D1 base assets
before native startup and report missing content in the metadata dialog.

Use a request file in an owned cache directory, not a large Intent containing
every frame/event. Suggested request fields are request ID, selected metadata
target identity, content_game, engine_game, level number/file/secret flag, mounted
asset references, expected recording key or prepare-if-missing, isolated write
directory, and read-only control/presentation profile. Return result/error and
resume position, not game progress.

Reuse the existing metadata return gate so closing the viewer does not discard
the dialog, restart whole-mission analysis, change resume offers, or replace a
paused game's state. Balance the metadata-focus/precompute lease on every launch,
failure, cancellation, and close path. Clear a pending return gate if launch
fails before the Activity starts. Avoid broad changes to existing map/robot UI.

On focus loss/onStop, pause playback and release input. On surface loss, use a
simple defined first implementation: save the playback bookmark, cancel native
work, and close through the existing preview lifecycle. Reopening can restore the
bookmark from a valid cache. Robust surface recreation within one native session
can follow, but do not automatically restart simulation after a UI rotation.

Capture cancellation must remain responsive while loading and simulating. Native
errors, missing assets, failed confirmation, and unreadable recordings become
Activity results; the metadata viewer remains usable. Shut down only this preview
engine/process and its owned files, preserving any full game in the background.

Do not keep generating when the Activity is backgrounded in the initial version.
A future background job would need a separate lifecycle and resource policy.

## Native/UI interface and introspection

Keep a small command/state boundary, shared conceptually with a later desktop
viewer. Proposed commands are open, play, pause, seek_frame, seek_event,
set_speed, set_camera_mode, follow_actor, camera_input, and close.

Every asynchronous command carries request ID and recording/session identity.
Validate bounds and finite values. A state snapshot contains:

- Session/recording identity, source provenance and route terminal status
- Preparing/ready/playing/paused/seeking/ended/error phase
- Displayed frame, requested frame, source ticks, recorded end frame, playback rate
- Current objective/occurrence, completion prefix, selected row and seekability
- Follow/free camera mode, camera pose/segment and recorded actor pose/segment
- Latest acknowledged request, pending seek/cancel state, error and progress
- Scene checksum/reconstruction generation, active decoded blocks/cache bytes,
  and seek-duration diagnostics for automation

Publish compact UI state at a modest rate, for example 10-15 Hz, with immediate
command acknowledgements and important state changes. Keep high-rate camera
input and rendering native. State snapshots should be internally consistent;
do not issue separate JNI reads that mix cursor N with objective state N+1.

Expose the same authoritative state through preview introspection. Use existing
debug-only automation command conventions; add reusable scripts for seeks,
objective taps, free-camera motion, and close. Tests should assert state directly
and use rendered output for visual QA, following repository guidance.

## Implementation sequence

### Phase 0: shared recording and seeking proof

- [ ] Define the shared trace/event/scene format and ownership with the video task
- [ ] Capture one Counterstrike run with periodic scene keyframes and indexed events
- [ ] Restore before/after a door, key, and destruction in shuffled time order
- [ ] Render each restored scene from the GuideBot and a second arbitrary camera
- [ ] Prove simulation capture is unchanged with the recorder enabled
- [ ] Measure recording size, reconstruction cost, and required renderer state

Acceptance: restored world/objective state matches sequential playback at each
target frame, including visibility from a camera different from the capture view.

### Phase 1: useful metadata route viewer

- [ ] Add the second level action, typed request, D2 route Activity/process, and
  asset/recording resolution, preserving map preview
- [ ] Implement on-demand preparation, progress/cancel, and cache publication
- [ ] Show first-person playback with play/pause, -5/+5, settled scrub seek,
  previous/next objective and a tappable list
- [ ] Load the player's selected touch layout, apply automap-style mode filtering,
  and add replay actions through the existing contextual overlay machinery
- [ ] Keep source-time progress, completed state, current target, and terminal
  failure/finish frame visible and consistent
- [ ] Preserve metadata dialog and paused-game state on return

Acceptance: select a level, watch, jump backward/forward, tap an objective, and
close. The scene and step state agree, and the existing map preview still works.

### Phase 2: free-camera inspection

- [ ] Separate camera input clock and transform from recorded scene time
- [ ] Enable the retained movement/look portions of the selected touch layout in
  free-camera mode, preserving customization and suppressing gameplay gestures
- [ ] Unlock/pause, six-axis flight with camera collision, and one-action reattach
- [ ] Define input focus, held-input release, and camera recovery after time jumps
- [ ] Allow playback to resume while the camera remains detached

Acceptance: pause before a switch, fly to inspect its face and linked passage,
seek backward/forward, and reattach without changing route evidence or scene state.

### Phase 3: polish and shared video integration

- [ ] Coalesced scrub previews, decoded-block caching, and precision frame stepping
- [ ] Shared smoothing/target overlays, optional rear view/map, adaptive layout
- [ ] Compatible host-recording import and exact capture-origin labeling
- [ ] Feed the same recorded scene player into offline video camera/edit scheduling

Acceptance: both interactive viewing and exported video reconstruct the same
source scene/events, with independent presentation timelines and controls.

## Later validation plan

All validation here is planned, not executed. Add a high-level route-viewer
integration script/runner, alongside the existing random-level-preview coverage.
Use existing LevelPreviewRequestStoreTest and LevelPreviewReturnRefreshGateTest
patterns for the new request and return behavior rather than duplicating tests
for every button or trivial forwarding function.

| Scenario | Required result |
| --- | --- |
| Counterstrike L1 | First load, cached reopen, key/reactor/exit jumps, final frame remains inspectable |
| Counterstrike L2 | Switches, hidden door, fly-through, repeated/near-simultaneous steps |
| Key-carrier fixture | Rewind restores carrier, forward restores dropped key, collection at exact boundary |
| Boss/final-level fixture | Death animation and exit state survive arbitrary seeks |
| D1-in-D2 level and secret level | Correct assets/library/identity and exact requested level |
| Partial/timeout recording | Prefix seeks work, future unrecorded steps are unavailable, failure remains labeled |
| Free camera paused/playing at several speeds | Camera moves independently, route scene state is unchanged |
| Seek across closing/restored wall while detached | Camera remains valid or returns to actor; no wall mutation |
| Rapid scrub/close/focus loss | Latest target wins, no stuck inputs, no old-scene flash, responsive cancel |
| Different selected/customized touch layouts | Retained controls preserve placement, size, binding and gesture behavior; replay additions remain reachable without overlap |
| Mixed movement/fire stick gestures and mode changes | Movement remains usable when unlocked; filtered actions and stale held inputs never fire |
| Controller/menu-only touch layout | Selected minimal layout is respected; replay actions remain accessible without forced touch sticks |
| Missing/corrupt/stale recording | Actionable error or regeneration, no wrong-level playback |
| Existing paused game plus preview | No shared pilot/save/metadata changes and no wrong-process shutdown |
| Map/robot preview after route viewer | Existing previews remain functional |

For every seek fixture, compare scene checksums and objective state against a
sequentially decoded reference at the same source frame. Seek in nonmonotonic
order repeatedly and test exact event-frame boundaries, not only whole seconds.
Add an off-camera room fixture so capture cannot accidentally omit unseen state.

Assert that free-camera motion, rendering several views, and pausing/rewinding
leave the immutable recording hash and confirmation result unchanged. Also
compare reconstructed scene state before and after a long paused camera session;
unchanged files alone would not detect accidental runtime mutation.

Check the persisted touch-layout/configuration before and after viewing: route
mode must not save its filtered controls or replay buttons over the player's
layout. Exercise the same selected layouts in automap and route view, including
portrait/landscape and a heavily customized placement, to verify the shared
interaction model rather than testing only the default touch preset.

Inspect moving first-person/free-camera playback and touch/controller operation
on representative devices during implementation. Measure first-use preparation,
warm open, ordinary/event seek latency, worst-case long-level memory/disk, and
cancel latency before setting performance gates. Start with one capture job and
bounded cache resources; no performance numbers are established by this plan.

Later implementation must include relevant desktop/native and Android build
checks, scoped mixed-language quality checks, and the maintained preview tests.
None are requested or run as part of this planning-only work.

## Handoff recommendation

Start with the shared scene-state/seek proof and the basic overlay. Free camera
is a separate, useful milestone once paused scenes can be rendered without
simulation. The principal work is restoring the world accurately at any time;
the play/seek controls are a small layer on top of that capability.
