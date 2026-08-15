# Robot Preview Sound Mapping And Real Projectiles

## Objective

Correct robot preview sound selection so HAM robot sound identifiers follow the same D1/D2 translation path as gameplay, and render preview projectiles with their actual weapon bitmap, vclip, laser, or polygon model instead of stand-in particles.

## Plan

- [x] Trace robot sound playback and distinguish logical sound identifiers from translated sample indexes in preview introspection.
- [x] Add focused sound diagnostics, reproduce the first D2 robots' mappings, and correct any preview-only translation error without changing gameplay sound behavior.
- [x] Extend the model-picture renderer with a narrowly scoped in-frame callback usable by the Android preview without changing normal model-picture callers.
- [x] Build temporary, non-gameplay weapon render objects from the preview projectile state and active Weapon_info data.
- [x] Render laser, blob, weapon-vclip, and polygon-model weapon types through the original D1/D2 render functions, including vclip animation and missile orientation.
- [x] Keep a fallback only for WEAPON_RENDER_NONE or invalid mod data, and expose render type/model/vclip/sample mapping through introspection.
- [x] Extend the robot preview integration test to verify translated sounds and actual weapon-render paths for D1, D2, and mod data.
- [x] Run scoped quality checks, Android multi-ABI build, host D1/D2 build and CTest, then emulator D1 base, D2 base, and mod preview tests.

## Findings

- Preview playback already passed logical sound identifiers through `digi_play_sample`, which translates them with `digi_xlat_sound`; there was no extra or missing numeric offset in that path.
- The preview-only random pool incorrectly treated `claw_sound` as applicable to ranged robots and included D2's post-death `taunt_sound`. Early D2 robot records can contain valid but contextually unrelated samples in those fields, producing the apparent off-by-one behavior.
- The corrected pool always permits see and attack sounds, permits claw only for melee robots, and excludes taunts. Introspection now reports both each logical identifier and its translated mixer sample.
- Preview projectiles use the active HAM `Weapon_info` render type. Bitmap lasers and blobs use the weapon bitmap, vclip weapons use their animated vclip, and missiles use their polygon model and flight direction. A magenta fallback remains only for render-none or invalid mod data.
- Rendering uses a stack-local preview object and does not insert objects into the gameplay object array or run gameplay weapon logic.

## Validation

- Scoped code quality checks passed for the changed Android C++, PowerShell test, D1/D2 hooks, and this plan.
- `:app:testDebugUnitTest --tests com.dxxredux.app.RobotPreviewRequestStoreTest` passed.
- `:app:assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64.
- `run-windows-build.ps1` completed for D1 and D2.
- CTest passed 33 of 33 D1 tests and 40 of 40 D2 tests.
- Emulator D1 base robot 0 passed with weapon 22 rendered as polygon model 72 and zero fallbacks.
- Emulator D2 base robot 0 passed with weapon 22 rendered as polygon model 137 and zero fallbacks. Its logical sound 172 translated to mixer sample 130.
- Emulator D2 base robot 24 (BPER) passed with weapon 48 rendered as animated vclip 92 and zero fallbacks.
- Emulator D2 mod-backed robot preview passed navigation, attack rendering, translated sound playback, animation, aspect, and rotation checks.
