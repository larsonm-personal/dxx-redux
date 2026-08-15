# Robot Preview AI Attack Sandbox

## Objective

Add an optional attack mode to the D1 and D2 robot preview that models robot movement, firing cadence, gun selection, and projectile flight from the loaded game data. Reuse level-authored AI behavior when a mission supplies a placed instance of the selected robot. When base HAM browsing has no placed object, use a documented type-derived behavior profile and expose that provenance instead of implying that it is level-authored AI.

## Constraints

- Keep attack mode off by default and isolate the preview from player damage, mission state, saves, demos, and multiplayer state.
- Read robot and weapon properties from the active D1/D2 HAM/HXM data so mod replacements automatically participate.
- Preserve the existing animated model preview, previous/next navigation, sounds, and 4:3 presentation.
- Support both engines with equivalent behavior while respecting differences in their data structures.
- Avoid claiming exact level AI for base-game HAM previews, because D1 behavior is stored on placed robot objects and D2 placed objects can override the robot type default.

## Plan

- [x] Trace the existing AI, object movement, weapon creation, collision, and rendering paths in D1 and D2, and identify the smallest reusable subset that is safe inside the preview loop.
- [x] Define preview behavior provenance: prefer a matching placed robot object's behavior after a source level is loaded; otherwise derive a fallback from robot type fields such as attack type, circle distance, max speed, evade speed, pursuit, and D2 default behavior.
- [x] Add an Attack Off/On control and native lifecycle calls, with attack state reset safely when changing robots or closing the preview.
- [x] Implement preview-local robot steering using the selected difficulty's real speed, drag, circle-distance, evade, attack-type, and behavior values, including charging, orbiting, retreating, stationary/sniper, and evasive profiles where supported by the source data.
- [x] Implement firing schedules using the robot's real primary/secondary weapon choices, firing waits, rapid-fire count, gun count, gun points, and recoil/fire animation states.
- [x] Implement preview-local projectile flight using the selected weapon's actual speed, thrust, drag, lifetime, speed variation, homing flag, and visual data, with a fixed inert target and no gameplay damage.
- [x] Expose behavior provenance and key robot/weapon flight statistics in the preview UI and native introspection so the modeled result can be inspected and tested.
- [x] Add focused regression coverage for controls, D1/D2 behavior selection, mod data use, firing cadence, and projectile integration.
- [x] Build all Android ABIs and run D1 base, D2 base, and mod mission robot-preview smoke tests, including robot switching with attack mode active.

## Findings

- A direct call into `do_ai_frame` is not safe in the picture viewer. It requires a live mine graph, player object, awareness state, collision system, global AI-local arrays, and gameplay side effects.
- Mission previews now use the first matching placed robot object's behavior. Base HAM previews, and mission previews without a matching placed instance, use a labeled type fallback. D2 supplies a default type behavior; D1 does not.
- The isolated simulator ports the core relative movement rules: approach, retreat, circle/evasion acceleration, speed limiting, attack-type charging, and the same fixed-step drag calculation. Geometry-dependent path planning, door interaction, line-of-sight awareness, thief goals, companion goals, boss teleporting, and collision reactions remain outside this fixed-point target sandbox.
- Projectile launch and flight use the active HAM/HXM weapon data, including gun point, robot aim spread, speed, D2 speed variance, thrust, mass, drag, lifetime, matter/energy type, and homing steering. Hits and misses are recorded against an inert target and never enter gameplay collision or damage code.
- The overlay uses compact colored flight markers rather than creating live weapon objects. This prevents explosions, child weapons, damage, awareness events, sounds, demos, and mission state from leaking out of the viewer. Weapon firing sounds still play when the existing Play sounds toggle is enabled.

## Validation

- Scoped code quality passed for the shared native code, JNI, Kotlin activity, PowerShell smoke test, D1/D2 renderer declarations, and this plan.
- `:app:testDebugUnitTest --tests com.dxxredux.app.RobotPreviewRequestStoreTest` passed.
- `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64 with JDK 21.
- `run-windows-build.ps1` completed for D1 and D2, including the headless metadata targets.
- Host CTest passed all 33 D1 tests and all 40 D2 tests.
- Emulator D1 base robot preview passed with attack simulation, navigation, sounds, animation, rotation, and framebuffer/aspect checks.
- Emulator D2 base robot preview passed with the same checks.
- Emulator Obsidian D2 mission robot preview passed with the same checks and mod-loaded robot/weapon data.
