# Authored segment resource damage

Implementation base: cmake `02bb982a375d32c5731565b26d9b501cfd627a0c`.
Pinned XL reference: `2576a98b5de310e3ac7c9777df2677d1627b8bae`.

Modern 21/40, 23/40, 25/40 and 27/40 default singleplayer profiles retain both
signed raw rates in inspection and admit only the segment_damage requirement.
Independent geometry, ownership, properties, cameras, effects and multiplayer
requirements remain enforced. This is component support, not mission playability.

## Behavior and ownership

The owned adapter runs once in object_move_one before fuel/repair and movement.
It reads the active immutable source and affects only the local OBJ_PLAYER.
Raw FrameTime times integer authored rate gives fixed damage; shield damage is
capped at shields plus one raw quantum, energy damage at current energy. Updates
run shield then energy, and nonzero deltas clamp to raw -1 through 200 fixed units.
Zero deltas preserve even over-cap values. Negative rates replenish. Negative
shields enter ordinary native death and skip centers, while exactly zero survives.
Repeated dead OBJ_PLAYER ticks still update resources; ghost objects do not.

There is no combat/fluid/wall percentage helper, slow-motion clock, sound, RNG,
palette flash, new asset, accumulator or save schema field. Native death owns its
camera/control state and object-shield reset. Wide arithmetic remains wide through
capping/subtraction/clamping; cases outside source-defined signed ranges are an
explicit deterministic extension, not claimed source parity.

The checked accessor uses existing source ownership. Stock transition clears the
source, rejected pre-commit replacement retains the still-active source, and
normal disk restore reconstructs rates. No duplicate rate table can go stale.
A distinct nonzero-damage identity section hashes its marker, segment count and
all ordered raw pairs for every admitted modern version. Existing all-zero
identities remain unchanged. No compatibility reader or migration is added.

## Qualification

The public source oracle extracts actual ShieldDamage, EnergyDamage,
HandleSegmentFunction and CShipEnergy::Set plus the existing exact inline Update.
The existing mandatory modern reader oracle executes actual ReadExtras.
Consumed source files retain exact CRLF SHA-256 pins. The matrix covers signed
rates, fractional frames, zero/negative/fractional/over-cap resources, both
resources, actor exclusions, center order/maxima and death boundaries. Undefined
reference extremes are tested separately using wide arithmetic. The native
modern-layout owner uses a separate --runtime-damage fixture option, preserving
the existing Android fixture bytes and all 113 private FAST cohort members.
It requires DAMAGE_REFERENCE as well as REFERENCE; missing source comparison
fails. The new public oracle increases only the public selection from 49 to 50.

## Executed evidence (Linux)

- Public owner: 55/55 source, format, GPU and SDL_mixer contracts pass. The new
  oracle passes 115248 exact source-defined caller/leaf/Set/Update comparisons,
  with separate wide extreme controls. ASan/UBSan passes for that oracle only;
  the engine was not sanitizer-instrumented.
- Native modern owner: all four modern versions pass normal admission, source
  loading, real starting-room portal entry/exit, 8400 object_move_one samples,
  protection/difficulty/centers, zero-delta/negative/cap/death behavior, invalid
  indices, imported-D1 and multiplayer bypass and wrong rate/sign/location/to-zero save identity
  rejection. Four malformed source-identity shapes per version reject atomically.
  A 70-frame 60 Hz sequence replays the same lethal boundary after disk restore
  through stock; repeated dead-player updates and the native death-to-ghost
  transition preserve normal camera/death ownership. All 8400 samples match the extracted source caller. Missing source
  oracles fail immediately, rather than skipping successfully.
- Eight selected native owners pass: modern layouts, player resources, repair,
  legacy repair, modern properties, water actors, lava volumes and the existing
  source lifecycle owner. Stock loader and route-checkpoint isolation pass.
- D2 upstream compatibility and classic-trigger demo pass (2/2). D1's normal
  headless metadata target compiles, with no D1 source changes.
- Private-assets policy tests pass (33/33). FAST remains 113 tests; only the public
  selection adds a test. Default Android fixture output matches the baseline
  byte-for-byte, including rtlayout.hog SHA-256
  `38caedf8321ad8de4eb3fd452dd9efc6210cf19620b75884a90de4ff283afed8`.
- Scoped mixed-language quality and diff checks pass after local pinned tool setup.
  Automation catalog validation passes: 99 standalone JSON tests, 546 support
  scripts, 315 standalone PowerShell tests. Catalog execution fails at the known
  Linux child supervisor prerequisite: unreadable /proc/self/task/<pid>/children,
  exit 125. That check was neither bypassed nor reported passed.

An initial native repair harness failure exposed its artificial live/negative
shield sample. It now asserts normal source death and finishes that sample with
native death cleanup before the next independent case; the gameplay dead-player
semantics were retained. An initial disk replay used post-restore FrameTime;
replay now explicitly supplies the same fixed durations without resetting earned
resources. A temporary unoptimized diagnostic harness timed out the existing
170-second native owner deadline. The final optimized owner passes within the
unchanged deadline. POG initially timed out during competing compilation and
passed its focused rerun and the complete public owner without timeout changes.
Existing unrelated const-string compiler warnings remain.

## Unchanged original Dinter witness

Private asset checkout: `9dad21bd65ba05665ac6fe315f37fe7461e85725`.
All original inputs are retained and unchanged:

| Input         | SHA-256                                                          |
| ------------- | ---------------------------------------------------------------- |
| dinter-1.5.7z | 445c17f06d94f72c02fe4bed68a58a241415b8fdfa8155b4a35330ce09c3dd75 |
| dinter.mn2    | 89ea5005ce971a3c7462dbfe727bbfa9b17b8766432e5f418e93c50c76ec72b1 |
| dinter.hog    | b4dc0423e781de5b667093313e92ffdb582c5b75c45ec119f680b2613b31fb13 |
| dinter.rl2    | 32c101c6c8a2177be8bc5ca74dcdb44dd1f27a2f637612e632ff30396d99f69b |

The baseline inspector reproduces 44 blockers and the implementation reproduces 43. Only segment_damage record 2215, raw 3/5, disappears from runtime blockers;
its signed pair remains in inspection. The actual pinned reader agrees on the
unchanged original: 6744 segments, 40464 sides, 569599 reader comparisons and
19257 vertex-brightness maxima. Planning did not execute these assets; these
results are implementation-time executions.

Remaining exact requirements:

- Object-trigger type 16: records 20, 37, 44, 49, 56, 70, 76, 88, 93
- Stationary uncontrolled particle shape: objects 1483, 1488, 1492, 1508, 1518, 1520
- Unqualified bubble profile: objects 1486, 1493, 1495, 1496, 1504
- Related effect object type 20: objects 1483, 1488, 1492, 1508, 1518, 1520
- Lightning requiring one stationary target owner: objects 1521, 1522, 1523, 1525,
  1526, 1527, 1529, 1530, 1538, 1543, 1545, 1546, 1547, 1549, 1553, 1557, 1558

The normal unassisted load/120-frame entry rejects Dinter before tick 1. The
unchanged frozen eight-case cohort completes at 4/8 (50 percent), with four
load failures. Dinter, Boiling Point, Sphere and Warp Shock still fail in this
independent cmake-based branch. No membership, denominator, timeout, readiness
threshold, renderer requirement or original asset was changed.

## Remaining platform and coverage limits

Windows and Android execution are unrun; no emulator/device was available and no
Android automation was added. Android in-memory rewind is unrun. Static tracing
shows disk and rewind use the same xl_wall_trigger_read/write identity validator
in d2/main/state.c; rates have no additional mutable or serialized state. No
full-engine sanitizer, rendered original-Dinter qualification, Entropy/team,
optional ship scaling/recharge, live multiplayer, camera/effect, slow-motion or
campaign-completion claim is made. Nonlethal native RNG/palette invariance and
source no-hit-sound behavior are checked; platform audio output from authored
segment damage is not separately instrumented.

## Final local artifacts and entry points

The final optimized eight-owner run passes in 59.38 seconds; the extended modern
owner takes 29.50 seconds under its unchanged 170-second deadline. Normal and
developer engine builds retain the repository RelWithDebInfo flags. The temporary
unoptimized diagnostic build is excluded from qualification.

| Executable                | SHA-256                                                          |
| ------------------------- | ---------------------------------------------------------------- |
| Normal D2 headless route  | 3a81289212114c7b2cdc6e552270925666dc080d4458f70e3a6e9d404d460155 |
| Native XL lifecycle owner | cf48bb9dbb9ce67dce55469ef8a405f7ec4e26da8957e0772b99bcd15a29a55b |

Executed entry points (private paths remain local):

```text
python3 android/XL_compat/tests/public_contracts.py --work temp/xl-segment-public-full --reference-root temp/xl-reference --gpu --audio
ctest --test-dir temp/xl-segment-public/build --output-on-failure -j2 -R '^(xl_modern_runtime_layouts|xl_repair_centers|xl_player_resources|xl_legacy_repair|xl_water_actors|xl_lava_volumes|xl_modern_properties|xl_preview_lifecycle)$'
ctest --test-dir temp/xl-segment-stock --output-on-failure -R '^(test_upstream_compat|test_classic_trigger_demo)$'
python3 -m unittest discover -s android/tests -p test_xl_private_assets.py
pwsh -NoProfile -File android/tests/test_validate_automation_catalog.ps1
pwsh -NoProfile -File android/tests/test_run_all_tests_catalog.ps1
```

The separate source-oracle build supplies
`-fsanitize=address,undefined -fno-omit-frame-pointer` and runs only
`xl_segment_damage_reference`. Native private outputs include the 8400 actual
samples, extracted-source comparison receipt, malformed-state rejections and
per-version load/identity/death checks. Complete original inspections, assets,
raw engine output and readiness receipts remain in the private workspace.
