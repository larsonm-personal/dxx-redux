# D2 classic-demo serialization boundary

## Goal

Move classic-demo JSON formatting out of `d2/main/newdemo.c` without moving its private parser state or changing classic-demo parsing, mount, error, or JSON ordering semantics. Keep the engine file as the source of truth for converting parser state into compact immutable records.

## Scope and constraints

- Preserve D2 classic-demo binary compatibility and command-line behavior
- Do not change input-demo direct-command policy or recorded input demos
- Keep private `nd_*` parser globals in `d2/main/newdemo.c`
- Add shared, independently testable serialization code under the Android shared source tree
- Emit normalized JSON directly, with exact-output fixtures rather than post-processing
- Preserve unrelated route-planner, guidebot, playsave, Kconfig, network, and newmenu work

## Plan

- [x] Recover the original dump contract and identify every field derived from private parser state
- [x] Add representative exact JSON fixtures and write/error failure coverage
- [x] Define compact immutable header, frame, object, and result records
- [x] Move pure JSON escaping and formatting behind the record boundary
- [x] Replace the inherited serialization bodies with record construction and serializer calls
- [x] Run scoped formatting, focused fixtures, Windows D2 build, and diff checks
- [x] Record inherited-file churn and update the high-coupling campaign status

## Status

- H05 implementation complete

## Result

- `classic_demo_json.c/.h` owns JSON escaping, stable key ordering, and formatting for immutable header, frame, player, object, robot-damage, and result records
- `newdemo.c` still owns all private parser state, PhysFS mounting, and frame decode policy; the D2 snapshot adapter owns public engine-global and object/AI interpretation
- The exact fixture covers escaped mission text, signed values, controls, both wiggle sources, physics objects, robot AI, robot damage, and the result record
- Failure fixtures cover injected sink failure and an invalid object span; real command-line checks cover missing input and a one-byte corrupt demo without a headless UI crash
- A real v15 level-4 dump produced 5,163 frames and 51,335 objects; a real v16 level-9 dump produced 1,042 frames and 10,010 objects
- Repeated v15 dumps were byte-identical with SHA-256 `6148EBED1F20C5DE4CC4276ED4BD1FBDAEAD626F65609EFBDBCAC232E393DC22`
- Windows D2 build and `test_classic_demo_json` CTest pass
- Root owns the final combined all-ABI Android and emulator validation

## Inherited-file metric

- Before: `d2/main/newdemo.c` was `+819/-50` against `main`
- After: `d2/main/newdemo.c` is `+654/-57` against `main`
- Net reduction: 165 inherited additions and 158 total inherited changed lines
