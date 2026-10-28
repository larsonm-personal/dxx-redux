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
