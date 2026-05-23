# Input demo hard-coded probe cleanup

## Goal
- Audit hard-coded frame, object, signature, and segment probes in D1/D2 input-demo work
- Remove stale one-off probe code where it no longer supports a maintained diagnostic path
- Move reusable probe gating into shared or non-original input-demo helper files where possible
- Keep original D1/D2 source diffs small and avoid changing game simulation behavior
- Validate with scoped quality, host builds, replay unit tests, and available affected demos

## Steps
1. Inventory hard-coded probe sites across D1 and D2 source files
2. Classify each site as stale debug debt, reusable diagnostic, or real gameplay logic
3. Move reusable debug decisions into input-demo helper APIs outside original source files
4. Remove stale debug-only sites with minimal source churn
5. Run focused validation and update this plan with results

## Status
- [x] Plan created
- [x] Probe inventory complete
- [x] Helper/API changes designed
- [x] Cleanup patch applied
- [x] Validation run

## Notes
- Keep the existing D1 homing replay parity fix from the prior audit
- Current RNG sidecars compare source line locations, so avoid unnecessary line shifts in RNG-heavy original files unless validation sidecars are regenerated or the compare result is known to be line-only

## Inventory
- D1 hard-coded frame/object/segment probes: none found in broad `d1/main` audit. Existing D1 fireball and physics hooks are generic debug gates
- D2 `input_demo_hooks.c`: one-off frame windows in AI schedule, homing, weapon focus, powerup, replay tail, robot visual, and robot lifecycle probes; object 95 and robot lifecycle object/signature targets
- D2 `input_demo_hooks.c`: stale suspect spreadfire signature tracking hard-coded frames 594..615
- D2 `object.c`: robot visual probes target robot id 38, object/signature pairs 158/4871 and 107/3818, and face-count instrumentation around polygon draw calls
- D2 `fvi.c`: homing FVI probes hard-code segment 254 side 4
- D2 `laser.c`: homing path/runtime probes hard-code frames 2790..2820 and candidate object 95
- D2 `physics.c`: stale `frame100` helper name and AI skip-add print hard-codes frames 454..460
- D2 `collide.c` and `escort.c`: thief/snipe detail probes hard-code frames 1280..1360 and snipe object 15
- D2 `newdemo.c`: classic dump stderr-only probes hard-code frames 70..110 and signatures 190/191; the JSON dump path itself is not hard-coded this way
- D2 `render.c`: render probe hard-codes frames 300..2000

## Cleanup Strategy
- Retire stale visual, object-95, segment-254-side-4, and classic stderr-only probes outright
- Keep generic replay diagnostics, but gate them through debug-enabled helper functions rather than fixed frame windows
- Move thief/snipe/contact probe activation into `input_demo_hooks.c` helper APIs so original files do not own object/frame policy
- Rename stale helper names that encode old frame-specific investigations
- Leave gameplay logic and maintained classic-demo JSON dumping behavior unchanged

## Validation
- `android\stop-stale-formatters.ps1`: no stale formatter tasks
- `android\run-code-quality.ps1 -Fix -Paths @(...)`: passed for the touched D2 files
- `.\run-windows-build.ps1 -Target both`: passed for D1 and D2. Existing D2 `loadgl.h`/GLEW macro redefinition warnings remain
- `buildd1\maths\test_input_demo_replay.exe`: PASS, exit 0
- `buildd2\maths\test_input_demo_replay.exe`: PASS, exit 0
- `d2_descent2_level9_20260512_115624.dximdemo` with `-TraceState`: PASS, exit 0
- `d2_descent2_level9_20260512_115227.dximdemo` with `-TraceState`: PASS, exit 0
- The same two demos with `-TraceRng` now fail only because RNG trace sidecars include source line metadata. The first mismatches are `ai.c` 1741 to 1740 and `ai.c` 730 to 729; call counts, RNG states, results, files, and functions match. Regenerate the sidecars after accepting the source cleanup.