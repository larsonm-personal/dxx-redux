# Modern XL repair centers

Continue the authorized run on xl_more_22_repair, stacked on PR #166. Keep
all PRs unmerged and require latest-head green CI

Pinned source: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae

## Scope and source decisions

- Modern function 2 repairs the local player at the source default 11 shield
  units per second, capped at InitialShield (100 for the default ship). Redux's
  existing 25-unit path remains the alternate. Legacy producers-disabled mode,
  custom shield scaling, entropy/team ownership and multiplayer remain outside
  the admitted profile
- Preserve the strict quarter-second sound interval and backward-clock reset.
  Own the XL cue clock separately, reset on mission change and save elapsed time
  in a directly replaced development trailer schema
- Preserve the source blue automap cue before normal wall overrides. Optional
  decorative repair lightning/sparks are outside this gameplay chunk
- Admit modern function 2 only after actual player frames, boundaries, cues and
  source restoration are verified. Do not promote original missions from counts

## Acceptance

1. Extract CSegment::Repair and the real player caller, pin defaults and compare
   amounts, caps, clock boundaries, backward time and cue decisions
2. Add XL-owned repair state and dispatch, preserve stock behavior, reject malformed
   saved clocks atomically and test dry/fuel/repair and fluid/property combinations
3. Run actual native player frames, physical entry/exit, low/full/overfull/dead
   health, sound timing, disk through stock and rewind controls
4. Register/run Android healing, live mixer, automap, boundary and lifecycle
   controls using controlled poses and real input; restore protected file hashes
5. Audit the 28 surveyed modern repair levels (145 centers), run focused regression
   and private-policy/catalog checks, format/lint and publish a stacked PR

## Completed local checks

The 5,760 repair samples and 42 actual player frames match extracted source
functions. All 15 focused regressions and all 70 Android steps pass; protected
device file hashes match. Both catalogs and all 12 private-policy checks pass.
All 28 candidates were reinspected with stable repeat bytes/mtime and no gameplay
promotion. See [repair evidence](../../XL_compat/evidence/repair-centers.md)

Latest-head PR CI remains the final acceptance step. Keep the stack unmerged
