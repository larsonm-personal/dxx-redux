# Floating point determinism survey

## Scope
Surveyed build flags and floating point use in the d1 and d2 game, math, render, and data-load paths with emphasis on input-demo replay determinism across Windows, macOS, Android ARM, and Android x86_64

## Reference synthesis
- Exact replay with floating point is easiest when every participant uses the same binary, compiler, instruction set, rounding mode, and floating point environment
- Cross compiler and cross architecture replay needs stricter rules: avoid value-changing compiler optimizations, avoid fused multiply-add contraction unless every target deliberately uses the same operation, avoid x87 extended intermediates, avoid approximate reciprocal or reciprocal square root instructions, and avoid libm for simulation decisions
- Basic IEEE arithmetic can be made reproducible in practice when expression order, rounding mode, precision, and contraction are controlled. Transcendentals, decimal parsing, and compiler reassociation are the usual leaks
- This codebase has a major advantage: most gameplay state is already 16.16 fixed point with integer sin, cos, asin, acos, and integer square roots. The hardening goal should be to keep simulation values in that world and treat float as render, UI, or loader-only unless explicitly audited

## Current build posture
- No explicit deterministic floating point flags were found in the d1, d2, Android, or shared CMake roots
- No `fast-math`, `ffast-math`, `Ofast`, `fp:fast`, fenv setup, or `_controlfp` setup was found in the checked build files
- Android currently enables interprocedural optimization for both game shared libraries in `android/app/src/main/cpp/CMakeLists.txt:630` and `android/app/src/main/cpp/CMakeLists.txt:749`. This is worth disabling for deterministic replay builds or pairing with strict FP flags
- Android builds `armeabi-v7a`, `arm64-v8a`, and `x86_64`. The ARM targets make FMA contraction control important, especially on arm64 where fused multiply-add is normal hardware

## Active simulation and replay-sensitive sites

| Priority | Location | What it does | Hardening thought |
|---|---|---|---|
| High | `d1/maths/vecmat.c:145`, `d2/maths/vecmat.c:145` | `vm_vec_scale2` converts fixed point vectors to float, scales, then converts back | Replace with an integer implementation based on `fixmuldiv`, or a checked 64-bit helper if overflow was the reason for the old float path. This function is used by FVI, collision impulse, and D2 Phoenix rebound speed, so it is the top code target |
| High | `d1/main/fvi.c:68`, `d2/main/fvi.c:70` | Plane-line intersection scales a movement delta through `vm_vec_scale2` | Hardens automatically when `vm_vec_scale2` is fixed. This can affect wall hit points and segment transitions |
| High | `d1/main/collide.c:267`, `d2/main/collide.c:304` | Object collision impulse scales relative velocity through `vm_vec_scale2` | Hardens automatically when `vm_vec_scale2` is fixed. Add replay/hash coverage for ship-wall and ship-object collision |
| Medium | `d2/main/physics.c:801` | Rebalanced Phoenix bounce reduces speed by calling `vm_vec_scale2(&velocity, 8, 10)` | Hardens automatically when `vm_vec_scale2` is fixed. Include D2 multiplayer/rebalanced weapon coverage if practical |
| High | `d1/main/kconfig.h:31`, `d2/main/kconfig.h:31` | Key hold timers are stored as float inside `Controls` | Convert these timers to `fix`. The values are bounded by `F1_0`, and the initial sensitivity step can be computed as `(F1_0 * KeyboardSens[i]) / 16 + 1` |
| High | `d1/main/kconfig.c:1471-1712`, `d2/main/kconfig.c:1566-1808` | Keyboard sensitivity ramps multiply fixed frame times by float timer ratios | Replace expressions like `speed_factor*FrameTime*(timer/F1_0)` with `fixmul(speed_factor * FrameTime, timer)`. For pitch half-speed, use `fixmul(speed_factor * FrameTime / 2, timer)` |
| Medium | `d1/main/aipath.c:853-855`, `d1/main/aipath.c:1170-1172`, `d2/main/aipath.c:1567-1569`, `d2/main/aipath.c:1843-1845` | AI and player path smoothing use a float divisor based on `FrameTime` | Preserve the current truncation order with fixed math, for example `fixmuldiv(norm_vec_to_goal.x / 2, FrameTime, F1_0 / 30)`. This affects robot and guidebot path velocity |
| Medium | `d1/main/object.c:125`, `d1/main/object.c:1952`, `d1/main/object.c:2353`, `d2/main/object.c:167`, `d2/main/object.c:2264`, `d2/main/object.c:2725` | Homing missile update cadence stores `idealHomerFPS` as float and computes fixed frame time with float division | Make `idealHomerFPS` an integer and compute `idealHomerFrameTime = F1_0 / update_rate`. The current values are integer update rates, so this should be behavior-preserving except for removing float conversion |
| High | `d1/maths/fixc.c:105-127`, `d2/maths/fixc.c:105-127` | `fix_atan2` normalizes with double `sqrt` before using fixed asin/acos tables | Rewrite using integer magnitude: accumulate `sin*sin + cos*cos` in 64-bit or `quadint`, take `quad_sqrt`, then call `fix_asin(fixdiv(sin, mag))` or `fix_acos(fixdiv(cos, mag))`. This keeps angle extraction inside the game's fixed math tables |
| Medium | `d1/maths/vecmat.c:842-899`, `d2/maths/vecmat.c:804-861` | Matrix and quaternion conversion uses `fl2f(1.0)`, `fl2f(2.0)`, `fl2f(0.25)`, and `* .5` | Replace constants with integer fixed constants (`F1_0`, `F2_0`, `F1_0 / 4`) and replace `* .5` with `/ 2`. These functions are used by compressed object position paths in `gameseg.c` |
| Medium | `d1/main/gameseg.c:1093-1136`, `d2/main/gameseg.c:1175-1218` | Compressed object positions serialize and restore orientation through the quaternion helpers | Hardens automatically when quaternion conversion is fixed. Important if checkpoints, multiplayer, or demo systems use compressed position state |
| Medium | `d1/main/bmread.c`, `d2/main/bmread.c` | Text asset parsing uses `atof` and `fl2f` for gameplay constants such as player ship mass, drag, thrust, weapon strength, weapon mass, weapon drag, weapon lifetime, powerup size, texture damage, and vclip timing | Add a deterministic decimal-to-fix parser and use it for all fixed-point table fields. Decimal parsing and float-to-fix conversion are not where we want libc variation. This is startup-only but it seeds simulation constants |
| Low | `d1/main/cntrlcen.c:130`, `d1/main/cntrlcen.c:172-173`, `d2/main/cntrlcen.c:148`, `d2/main/cntrlcen.c:202-203` | Gameplay-adjacent constants use `fl2f(12.75)` and `fl2f(0.65)` | Replace with integer fixed constants or a `FIX_FROM_RATIO` style macro. This is cheap hardening |
| Low | `d1/main/gameseq.c:531`, `d2/main/gameseq.c:619` | `flash_dist=fl2f(.9)` | Replace with a fixed integer constant. It is probably presentation-adjacent, but the cost is low |
| Low | `d1/main/fireball.c:54`, `d1/main/powerup.c:71`, `d2/main/fireball.c:433`, `d2/main/powerup.c:293` and nearby `fl2f` constants | Explosion and pickup presentation constants use decimal float constants | Audit whether each affects collision, lifetime, or only visual size. Convert active gameplay constants first, then presentation constants opportunistically |

## Libm and render-only sites

These are less likely to affect input-demo state, but they should stay out of replay hashes and simulation decisions

- `d1/main/render.c:847-849`, `d2/main/render.c:962-964`: `sinf` warps rendered hostage positions. This is render-only; if a future replay hash includes rendered geometry, replace with `fix_sincos` or a deterministic table
- `d1/arch/ogl/ogl.c`, `d2/arch/ogl/ogl.c`: `tan`, `sinf`, and `cosf` are used for projection and circle vertices. Render-only
- `d1/arch/sdl/gr.c:331`, `d2/arch/sdl/gr.c:332`: `pow(..., 1.0)` in gamma setup is mathematically redundant. It can be simplified but is not simulation state
- `d1/texmap/scanline.c`, `d2/texmap/scanline.c`: `fabs` is used in software texture mapping. Pixel output only unless screenshot comparison is used
- `d1/editor/*`, `d2/editor/*`: `sqrt` calls are editor geometry helpers, not runtime replay simulation
- `d1/xmodel/*`, `d2/xmodel/*`: many float vectors and `sqrt` calls live in model/texture tooling and OpenGL render support. They are outside core simulation unless their outputs are fed into gameplay state later
- HUD, automap, menu, movie, title, score, and observer-card float uses are presentation paths. Keep them out of deterministic state hashes

## Dead, commented, or currently inactive sites

- `d1/main/physics.c:222-305` and `d1/main/physics.c:510-572` contain `pow` in `GEOMETRIC_DRAG` branches, but `STRATEGY` is currently a local constant set to `ORIGINAL`. If that strategy is ever enabled, it should be rewritten in fixed point or implemented with a deterministic table
- `d2/main/physics.c:845-859` has a double `sqrt` collision diagnostic inside a block comment. No runtime impact
- `d1/maths/vecmat.c:441-462`, `d2/maths/vecmat.c:403-424` have an old double cross-product implementation under `#if 0`; the active implementation is fixed point via `fixmulaccum`

## Compiler and runtime hardening proposal

1. Add a deterministic FP build option, probably `DXX_DETERMINISTIC_FP`, enabled for Android debug/internal replay builds first
2. For Clang and GCC targets that compile gameplay or math code, add explicit flags such as `-fno-fast-math`, `-ffp-contract=off`, and, after compiler checks, `-ffp-model=strict` or the closest supported equivalent. Consider `-fexcess-precision=standard` where supported
3. For MSVC targets, add `/fp:strict` for deterministic builds. For clang-cl, use checked support for `/fp:strict` or `-ffp-model=strict`, plus contraction off
4. For 32-bit x86 builds, either force SSE2 scalar FP or treat cross-machine deterministic replay as unsupported until x87 precision is explicitly controlled. If supporting it, set and check x87 precision and round-to-nearest at startup and around external library calls
5. At game startup and before deterministic replay ticks, set and assert round-to-nearest with `fesetround(FE_TONEAREST)` and `fegetround()`. On Windows also use `_controlfp_s` for the x87 control word when applicable
6. Assert or log `FLT_EVAL_METHOD` in deterministic builds. The preferred value is 0. If a platform reports extended evaluation, rely on the code cleanups above rather than trusting compiler flags alone
7. Disable interprocedural optimization for dxx-redux-d1 and dxx-redux-d2 in deterministic replay builds, or at least disable it until the active float sites above are removed
8. Keep simulation code free of libm calls. Use the game's fixed tables for sin, cos, asin, acos and integer helpers for square root, scaling, and parsing

## Verification plan

- Add a small host test for deterministic fixed helpers: `vm_vec_scale2`, `fix_atan2`, quaternion round-trip, and decimal-to-fix parsing across representative edge cases
- Add an input-demo replay hash test that records state at fixed frame numbers and compares across Windows host, Android arm64, and Android x86_64
- Include collision, wall grazing, guidebot pathing, keyboard ramp input, homing missile guidance, and one data-driven weapon constant in the replay coverage
- Run both debug and release or internal builds, since compiler optimization changes are part of the risk model

## Unit test extraction notes

The safest migration shape is to extract each replay-sensitive floating point expression into a tiny named helper before changing its implementation. The first extraction should preserve the old float or double math exactly and get locked by host tests. The second change replaces the helper body with fixed or integer math and updates only the expected results that are intentionally different by a documented one-LSB or truncation-policy amount.

Suggested structure:

- Put production helper code in the engine math area, not under `android/`. A reasonable shape is `d1/maths/math_fp.c` and `d2/maths/math_fp.c`, declared from the existing `d1/include/maths.h` and `d2/include/maths.h`, or a small shared `math_fp` implementation included by both game math libraries if the build can do that without widening the upstream diff
- Keep these helpers as pure math functions with no Android, SDL, PHYSFS, renderer, or launcher dependencies. The goal is cross-platform simulation determinism, so the code should build anywhere the core math library builds
- Add a focused host test such as `test_math_fp.c` using the existing simple `expect_*` style from the input-demo tests. It can initially live in the current host-test harness if that is cheapest, but the production code itself should live with the game math library
- Wire that test into `d1/maths/CMakeLists.txt` and `d2/maths/CMakeLists.txt` under the existing `if(NOT ANDROID)` block, linked to `${DXX_TARGET_PREFIX}maths`, so both games exercise the same helper behavior without adding duplicate target names to the Android graph
- Keep test vectors as integer inputs and integer expected outputs. Do not compare floats directly; for the first pass, call the extracted legacy helper and snapshot the exact fixed-point result it already produces on the current host build
- Include representative normal cases plus sign, zero, large value, and boundary cases. Determinism bugs tend to hide at rounding boundaries, not in pleasant midrange inputs
- Where the new fixed-point implementation deliberately changes one or two edge values, keep the old expected value in a nearby comment or fixture row with a reason, so future replay desync work can distinguish intentional drift from accidental drift

Per-location notes:

| Location group | Extraction helper to test before change | Test vectors to capture | Expected post-change tolerance |
|---|---|---|---|
| `vm_vec_scale2` and callers in FVI, collision, and D2 Phoenix rebound | Extract the component math as something like `fp_legacy_vec_scale_component(fix value, fix n, fix d)` and have `vm_vec_scale2` call it for x/y/z | Values around zero, mixed signs, `n < d`, `n > d`, `n == d`, exact Phoenix case `8/10`, FVI-style fractions, and large components that explain why the old code avoided simple `fixmul` | Prefer exact integer match for common cases. If overflow-avoidance requires saturating or wider 64-bit behavior, document every changed boundary row |
| Keyboard sensitivity timers in `kconfig.h` and `kconfig.c` | Extract timer seed and contribution helpers, for example `fp_legacy_key_timer_seed(int sensitivity)` and `fp_legacy_key_ramp_delta(fix speed_factor, fix frame_time, fix key_down_time, int half_speed)` | Sensitivity values `0`, `1`, `8`, `16`, repeated frame updates until clamp at `F1_0`, positive and negative thrust or turn direction, and half-speed pitch cases | Aim for exact outputs. The struct type change from `float` to `fix` should not change the final `Controls.*_time` results except possibly at clamp boundaries, which should be called out explicitly |
| AI and player path smoothing in `aipath.c` | Extract the per-axis nudge expression as `fp_legacy_path_velocity_nudge(fix norm_component, fix frame_time)` | `FrameTime` values for 15, 30, 60, and uneven FPS; components `-F1_0`, `-F1_0/2`, `0`, `F1_0/2`, `F1_0`; and small components near truncation thresholds | Exact match should be attempted by preserving division order. If the new fixed expression chooses a clearer truncation policy, the expected rows should change only at one-LSB threshold cases |
| Homing missile update cadence in `object.c` | Extract `fp_legacy_homer_frame_time(int update_rate)` from the float `idealHomerFPS` division | Update rates `1`, `10`, `25`, `30`, `60`, and any config-controlled values used by the launcher or multiplayer options | Exact match is likely for integer rates that divide cleanly enough after truncation. Any change should be measured as fixed-point ticks and checked in a short homing replay hash |
| `fix_atan2` in `fixc.c` | Extract the normalization part as `fp_legacy_atan2_normalize(fix cos, fix sin, fix *norm_cos, fix *norm_sin)` or wrap the full old `fix_atan2` behind a test-only name before replacing it | Cardinal directions, diagonals, near-axis values, negative quadrants, non-normalized pairs, max safe fixed values, and zero vector | The new integer magnitude path should match most rows exactly. Non-normalized large vectors may move by a small angle-table step; keep those rows separate and documented |
| Quaternion conversion in `vecmat.c` and compressed object positions in `gameseg.c` | Extract fixed constants and half-scaling into helper-level tests around `vms_quaternion_from_matrix` and `vms_matrix_from_quaternion` | Identity matrix, 90-degree axis rotations, small off-axis orientation matrices, and round-trip matrix to quaternion to matrix cases using existing packed `vms_quaternion` storage | Replacing `fl2f(1.0)` and `* .5` with fixed constants should be exact or very close. Because `vms_quaternion` stores signed shorts, assert packed outputs and round-trip orientation components |
| `bmread.c` decimal parsing | Add a new deterministic parser helper such as `fp_parse_fix_decimal(const char *text, fix *out)` and initially compare it against a legacy wrapper `fp_legacy_atof_fl2f(const char *text)` | Table-like strings: integers, one decimal, many decimals, leading sign, leading/trailing spaces, values near `0`, `1`, weapon strengths, ship thrust/mass values, and malformed strings if callers need rejection behavior | During the first implementation, exact match old `atof + fl2f` for accepted table strings. Later, if locale-independent parsing rejects strings that `atof` accepted, document those as loader validation changes |
| `cntrlcen.c`, `gameseq.c`, `fireball.c`, `powerup.c` decimal constants | Do not extract whole callers. Add a small constant conversion test table for each decimal literal being replaced, using named fixed constants or ratio macros | `12.75`, `0.65`, `0.9`, and each active explosion or pickup constant that affects simulation state | Expected output should be exact. If a decimal literal was only visual, test the constant helper once and leave deeper validation to render or smoke tests |
| Render-only and dead/commented libm sites | No unit extraction needed unless a future replay hash includes render output or the inactive physics branch is enabled | If activated, test the specific enabled helper before wiring it into simulation | Keep out of the deterministic simulation test suite until it affects replay state |

Before changing implementation, run the new host tests against both D1 and D2 builds and keep the captured expected rows in version control. After the fixed-point rewrite, rerun the same tests first, then replay hash tests. This gives a crisp answer to whether a replay difference came from an intentional math policy change or from a wider behavioral regression

## Recommended tranche plan

This should be multiple tranches. The first priority is to lock down existing behavior with unit tests before changing any floating point logic. The risky part is not writing fixed-point replacements; it is losing track of what the current code actually does, then being unable to separate intentional policy changes from regressions. Each tranche should end with updated notes, a host build, the new or changed unit tests, and at least one replay or integration check when the changed logic can affect simulation.

### Tranche 0: Existing-behavior lockdown

1. Add `math_fp` to the engine math library in both games, preferably as `d1/maths/math_fp.c` and `d2/maths/math_fp.c` with declarations in `maths.h`
2. Move only extracted legacy float/double expressions into `math_fp`; do not change behavior yet under any circumstance in this tranche
3. Add `test_math_fp.c` with integer input/output tables for `vm_vec_scale2` component scaling, key-ramp math, AI path nudge math, homing frame time, `fix_atan2`, quaternion constants, and decimal parsing
4. Build and run the tests for D1 and D2, then freeze the captured expected rows as the existing-behavior baseline that later tranches must preserve unless a change is explicitly documented
5. Add or identify the replay state-hash smoke test that later tranches will reuse, but treat the unit-test baseline as the primary lock on current behavior
6. there will be, already, a few passing input-based demo files to be used for regression testing. these will be passing even though we haven't cleaned up floating point use. these are key test artifacts because they already demonstrate wide overlap between ARM and x86 implementations, given that the demos were created on ARM and run on x86. run these and continue to run these as we go

### Tranche 1: Compiler guardrails without simulation rewrites

1. Add a deterministic FP CMake option, for example `DXX_DETERMINISTIC_FP`, scoped to the game/math targets
2. Add checked compiler flags for no fast math, no FMA contraction, and strict or precise FP mode where supported
3. Set or assert round-to-nearest at startup in deterministic builds, and log or assert `FLT_EVAL_METHOD`
4. Disable Android IPO for deterministic replay builds only
5. Rerun the locked existing-behavior unit tests first, then run host builds plus a replay smoke test to make sure flags alone do not change the captured baseline unexpectedly

### Tranche 2: High-risk per-frame simulation math

1. Replace the `vm_vec_scale2` helper body with fixed or 64-bit integer math, then rerun the locked baseline tests before touching callers
2. Convert keyboard sensitivity timers and ramp contribution helpers from float to fixed math in both D1 and D2, updating expected rows only where the change is intentional and documented
3. Convert AI and player path smoothing helpers to fixed math in both D1 and D2, again preserving the locked baseline where possible
4. Run replay coverage that includes wall grazing, object collision, keyboard-ramp input, and guidebot or robot path following

### Tranche 3: Angle, quaternion, and timing cleanup

1. Rewrite `fix_atan2` normalization to use integer magnitude and the existing fixed trig tables
2. Replace quaternion decimal constants and `* .5` operations with fixed constants and integer division
3. Convert homing missile cadence from float FPS math to integer update-rate math
4. Run the existing-behavior unit tests first, then replay coverage for homing missiles, compressed object position paths if available, and any checkpoint or multiplayer path that serializes quaternions

### Tranche 4: Data-load determinism

1. Add deterministic decimal-to-fix parsing in `math_fp` or another engine math parser helper, not in Kotlin and not in Android-only code
2. Convert `bmread.c` fixed-point table fields from `atof + fl2f` to the parser in both D1 and D2
3. Convert simple decimal constants in `cntrlcen.c`, `gameseq.c`, `fireball.c`, and `powerup.c` to named fixed constants or ratio macros
4. Expand the locked unit-test table with representative table strings and constants, then run a level-load replay or state-hash check that exercises player ship and weapon constants

### Tranche 5: Cross-platform replay gate

1. Run the same locked unit-test suite and replay state-hash fixtures across Windows host, macOS if available, Android arm64, and Android x86_64
2. Add at least one fixture for collision/FVI, keyboard-ramp movement, guidebot or robot pathing, homing missile guidance, and data-loaded weapon behavior
3. Document any intentional baseline changes from earlier tranches and keep old rows nearby in the tests or plan notes so changed results are always explained against the original locked behavior
4. Leave render-only and dead-code libm sites out unless they become part of replay state or screenshot-based assertions
