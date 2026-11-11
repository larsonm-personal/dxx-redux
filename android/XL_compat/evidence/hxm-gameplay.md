# HXM gameplay definitions

Classic version-1 HXM replacements now enter the ordinary XL gameplay path after
structural and loaded-resource validation. Admission uses the actual definitions,
models, joints, textures and weapons; there is no mission-name or archive-hash
exception. Test assets remain pinned independently of production admission

## Source semantics

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae

- `gameio/loadmodeldata.cpp`: 4b191d4abf1a3a68348b468bd6db77da805c2334ba9d49d623697c294c9bfb8c
- `models/polymodel.cpp`: 5e0b43cc1b12b5cc8531bf6f3fd81ecc10703b0ea5189ad4c7ce70dec3e24f17
- `3d/hitbox.cpp`: 2062a3230e07a0a00ff880621d22e66e322330f7c98c6f6e5d28896ed8a25eed
- `objects/robot.cpp`: a33aee40ef05eb35d41538c75190df09bb744cd484146c6a3c6afb7a807818a3
- `objects/object.cpp`: e71b0a906cd686549c783563ab93365d1bbf5cf05f08c1971ef08c7116c67a64

LoadRobotReplacements reads five indexed tables and closes the file. It preserves
the base model radius while writing the computed custom radius into a separate
slot. ModelRadius/AdjustSize normally prefer the base slot; projectile creation
explicitly requests the custom slot. FixObjectSizes runs after level replacements
load. The port retains the native base radius and owns the computed custom radii
with the prepared level resources. Ordinary and reflected polygon projectiles
select the custom radius only while XL extensions are active

The standalone oracle compiles the actual GetPolyModelMinMax and CPolyModel::Size
functions against an independent adapter. It exercises the original POF path,
both sort traversal orders and nested submodel offsets. Production uses bounded
little-endian reads before native alignment/endian fixups. The comparison passes
under MSVC and GCC with the original Alarlof model. Its base radius is 316909,
serialized custom header radius 365764, and source-computed custom radius 325446

The pinned Size function uses `dx * dx + dy * dy + dz + dz`. This arithmetic is
preserved deliberately. One source defect is corrected: its `else if` extrema
updates can leave a maximum uninitialized for a decreasing or one-point vertex
cloud. Production updates minimum and maximum independently, with a separate
monotone-input control. Overflow, recursive calls, missing vertices, excessive
traversal and unsupported submodel counts fail before installation

Empty animation lists do not dereference their offsets. Zero padding after the
five tables is accepted by the existing bounded reader; nonzero unknown trailing
data remains rejected. Cyclic/out-of-range model parents, invalid gun submodels
and missing animation joints are rejected before a firing transform can run

## Original and derived controls

The original Apocalyptic Factor XL HOG is unchanged:
`f0611f1428549373526dfca8ca5a1e1883ace910f4209a938698086d189919db`

Original Alarlof member identities:

- RL2: df172e675c3754db2401ba8cbbf7e8747732666d3682918843a7e05d58d1a7e5
- HXM: a41f8c1cee39131e5e83d7712cce2d9df5b3ac5836948e9681c70632e3148e5c

A short single-level test descriptor selects those original members. It does not
rewrite the archive. Object 100, robot 3 in segment 639, starts with the custom
750 shields, model 6, eight guns, primary weapon 22 and secondary weapon 15

Native integration verifies normal StartNewGame initialization, animation/gun
transforms, the AI firing function's primary/secondary selection and cooldowns,
real projectile creation, damage and lethal combat. Disk restoration through
Counterstrike preserves earned damage and installed definitions; repeated loading
starts fresh. Rejection controls cover truncated payloads, nonzero suffix data,
missing weapons, invalid gun/joint references, parent cycles and recovery without
mutating the running world. A supported custom boss profile can be staged before
the level dependency walk; signed boss profiles remain explicitly rejected

A separately labeled derived fixture redirects the original custom model and
texture replacements to the concussion projectile model, omitting the robot and
joint overrides which depend on that model. Actual firing uses the computed
custom radius, including after disk restoration through stock. Stock firing uses
its original radius. This fixture is not evidence of an authored projectile in
the original mission

The registered Android integration passes all 42 steps on x86_64: live actor and
definition fields, controlled combat damage, memory checkpoint restoration,
disk reload, stock transition and restored combat. Protected player/configuration
files match their pre-test hashes. A fixed camera pose provides a render capture;
it does not establish full graphical equivalence or autonomous traversal

## Reproduction and remaining scope

### Final-definition boss binding

Boss registration formerly ran during geometry loading, before the normal HXM
replacement hook. This omitted newly defined bosses and retained obsolete base
boss roles. Original PMC3 exposed eleven stale registrations even though its
final definitions contain no required bosses. Registration now follows robot
replacement installation and object verification in `load_level_robots_file`.
Pinned XL revision `2576a98b5de310e3ac7c9777df2677d1627b8bae` likewise installs
replacements in `gameio/loadgame.cpp` before final robot setup and
`InitReactorForLevel` registers bosses.

The native HXM test makes separate, explicitly derived companions from the
hash-pinned original Apocalyptic Factor XL archive. One adds boss role 21 to
robot 3; the other removes the base boss role from robot 31 and uses that ID for
the controlled actor. Original mission bytes are never modified. The added-role
case failed before the fix with `Boss bindings do not match final HXM definitions`.
Afterward both cases pass normal startup, combat dispatch, death-roll objective
accounting, disk restoration through stock, and repeated loading. Existing boss
spawn, transport and segment-group controls also pass.

The original PMC3 Android scenarios now require an empty boss registry and one
remaining reactor objective, including after warm and fresh-process restoration.
APK `c63885a409055938330680fc63bf6bdabea1cbdd`, version 28790, passes 115 warm
steps (244.564 seconds) and 21 fresh-process steps (50.044 seconds). Game PIDs
25840 and 27427 differ; the player/85-actor checkpoint matches, as do protected
before/after file hashes. The rendered cold-load frame was inspected. This uses
the owned x86_64 emulator, fixed poses and controlled exits, not a physical ARM
device or earned campaign completion.

The first candidate's new assertion used an empty object instead of an explicit
equality operator. It timed out despite live state containing the correct empty
registry and one objective. The corrected script uses `{"eq": {}}`; this is a
test-format failure, not a gameplay failure. Old development saves can retain
the prior registry shape and fail validation; preserve their files and
corresponding older APK, and start a new game for the corrected state. No save
migration or full original-mission boss encounter is claimed.

- `xl_hxm_geometry_reference`: extracted source and malformed geometry controls
- `xl_hxm_native`: original actor and derived projectile lifecycle controls
- `xl_format34_native`: existing geometry/light regression, still using its
  explicitly derived HXM-free fixture to isolate that behavior
- `android/tests/test_xl_hxm_runtime.ps1`: original HOG and the descriptor emitted
  by `xl_hxm_native`; owned script `test_xl_hxm_runtime.jsonc`

The HXM path remains bounded by the loaded registries and supported mechanics.
Inert definitions, signed/D1 boss profiles, dynamic boss creation, expanded
definitions, alternate HAM/PMH/HMQ semantics and wider capacities need subsequent
implementation. The signed boss byte also carries an ends-level distinction in
the reference; treating its absolute value as an ordinary boss would be wrong

This work verifies the listed original actor and lifecycle behavior. It does not
certify Alarlof's complete encounter/progression logic or the campaign. Fourteen
other Apocalyptic Factor levels still fail their declared packed-light ranges.
The static format candidate report remains a decoding report, not a gameplay
qualification percentage

## Warp Shock 2.0 level 5 animation slots (2026-11-10)

The unchanged original `wshock20.hog` (SHA256 `2054dca034ea9aac0d13c37a5867b18fa84d9b307b36702b56bcc9ed3a61b45c`)
contains `volcano.hxm` at byte 8460823, length 13100 (SHA256 `e132d223d44349742889658f501eaa52d28256319833551973f8754d431f8d99`).
Its 27 replacement robots contain no inert model `-1` record. Robot 51 uses
model 75, two guns and submodels 2/4. Animation groups 0/1 have two joints,
group 2 is empty, and unreachable groups 6/7 contain malformed data. The
native and pinned XL consumers visit only groups 0 through `n_guns`.

XL HXM validation now clears groups above `n_guns` in a local validated copy;
the owned definition reader applies the same rule before dependency closure and
installation. Stock HXM validation remains strict. `test_d2_ham_validation`
accepts the inactive XL garbage while rejecting the same stock bytes and an
invalid active group. The original Warp 5 loaded through the normal renderer
and completed 120 unassisted simulated/rendered frames on the Intel Arc GPU.

The optional original actor witness in `xl_hxm_native` uses the existing desktop
route probe with a real OpenGL renderer. Authored object 226, robot 51, is
relinked by the normal loader from authored segment 528 to runtime segment 1040. The probe checks all active animation lists and gun transforms, uses a
disclosed player position assist to trigger production AI weapon 21, applies
combat damage, saves and restores normally through stock content, then verifies
the damaged actor in a separate process and returns to stock again. Warm and
cold frames render without GL error. This is a short controlled actor witness,
not a whole-level traversal. The headless HXM executable cannot load this
fog/skybox level because it has no active OpenGL renderer; the rendered witness
therefore runs in the normal desktop executable.
