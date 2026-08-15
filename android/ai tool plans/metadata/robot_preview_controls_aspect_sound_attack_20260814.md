# Robot Preview Controls, Aspect, Sound, and Attack Plan

## Objective

Keep robot models correctly proportioned on phone displays, add adjacent-robot navigation and
optional recurring robot sounds, and document an engine-safe design for an attack preview.

## Work

- [x] Trace the preview surface, native viewport/projection, request lifetime, robot catalog, and sound APIs
- [x] Correct preview aspect handling for wide and tall Android surfaces in both games
- [x] Add Previous and Next controls that switch robots without returning to the launcher
- [x] Add a persistent, default-off Play sounds toggle with one random robot sound every three seconds
- [x] Extend preview introspection and automated coverage for aspect, navigation, and sound state
- [x] Research robot firing, weapon spawning, and fixed-target requirements in D1 and D2
- [x] Record the recommended optional attack-preview design and implementation boundaries
- [x] Run scoped formatting, focused tests, Android builds, and robot-preview integration validation

## Attack Preview Research

The current preview is a model-picture renderer, not a gameplay scene. It calls
`draw_model_picture_animated`, which creates a temporary camera, sizes one polygon model to the
canvas, draws it, and ends the 3D frame. It does not create a robot object, a player or target
object, a segment-backed projectile, or an object simulation loop.

The reusable base-game data is already loaded for a visual attack mode:

- `robot_info.n_guns`, `gun_points`, and `gun_submodels` identify each muzzle
- `calc_gun_point` applies the current submodel animation and object orientation to a muzzle
- D1 uses `weapon_type`; D2 can use `weapon_type2` for gun zero and `weapon_type` for other guns
- `AS_FIRE` and `AS_RECOIL` supply the authored firing motion already used by this viewer
- `Weapon_info` and the normal weapon render paths provide laser, blob, polygon, and vclip visuals
- Mission HAM/HXM loading already replaces robot, joint, model, weapon, and sound data before the
  preview opens, so the same design naturally supports mods

Calling `ai_fire_laser_at_player` directly is not suitable. It requires a registered robot object,
AI-local state, player globals, segment collision state, awareness events, firing timers, random aim,
and multiplayer/demo side effects. It also deliberately rejects charge robots. Running the full AI
frame would add pathfinding and movement behavior that a fixed presentation scene does not need.

### Recommended implementation

Add a preview-only attack renderer beside the existing model-picture renderer:

1. Keep a lightweight robot pose with the same model position, orientation, and animated joint
   angles used now.
2. Transform the selected gun point through its submodel parents, matching `calc_gun_point`, but
   without requiring an entry in the global object array.
3. Place a fixed target marker in model-picture coordinates and aim a preview projectile from the
   transformed muzzle to that point.
4. Cycle `alert -> fire -> recoil`, advancing to the next valid gun after each shot.
5. Select the real D1 or D2 weapon ID from `robot_info`, then render a short-lived temporary weapon
   with the existing weapon-specific rendering code. Keep its position and lifetime in preview-local
   state, without collision, damage, awareness, homing, or game RNG.
6. For robots with `attack_type != 0`, show a short charge toward the target instead of inventing a
   projectile. For `n_guns == 0`, companions, thieves, and other non-firing definitions, disable the
   attack toggle or describe the available behavior.
7. Add an `Attack fixed point` toggle, default off, plus introspection for enabled state, current gun,
   weapon ID, shots rendered, and charge fallback.

This is a medium-sized follow-up because the existing weapon renderer accepts gameplay `object`
structures and several weapon render types have different required fields. The clean boundary is a
small preview-only temporary-object adapter or a factored weapon-visual function. Creating a hidden
full mission and running AI would be more coupled, less deterministic, and harder to keep visually
neutral.
