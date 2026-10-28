# Modern XL lava volumes

Continue the authorized eight-hour run on xl_more_20_lava, stacked on PR #164.
Keep all PRs unmerged and verify latest-head CI. The original survey contains
13 modern levels with 279 lava-property segments; this is prioritization data,
not admission or gameplay qualification

## Scope and source decisions

Implement modern segment lava with XL-owned behavior and an explicit Redux
fallback. Preserve the shared binary, stock physics/contact behavior and the
existing legacy experimental gates. Defer multiplayer and optional fog/physics
settings. Do not navigate levels autonomously

Pinned source: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae

- CPhysSimData::Setup selects 75-percent motion for water first, otherwise
  66 percent for lava. Combined water/lava therefore retains water movement
- CSegment::Physics chooses lava damage first, using loaded texture 404 damage
  divided by two. Free-volume CheckSegmentPhysics halves that rate again;
  ApplyWallPhysics uses the contact rate. Trainee halves positive damage
- The actual CheckWallPhysics caller is player-only. Robots/projectiles retain
  fluid motion without acquiring the unreachable robot-destruction branch
- Preserve source wall/volume precedence, no-damage and invulnerability effects,
  turbulence/RNG and sound selection. Validate the actual callers, not only
  helper formulas. Keep the Redux path as the alternate
- Source non-volumetric rendering intentionally has no lava-only tinted boundary;
  combined water/lava uses water presentation. Preserve contact audio rather
  than inventing an orange surface from an unused color entry. Optional volumetric
  fog and comprehensive rendering parity remain separate requirements
- XL gets its own wall/volume/scrape path and saved relative scrape clock. Redux
  remains the alternate. Development schema 9 replaces schema 8 without migration

## Implementation and acceptance

1. Audit/pin segment, side, physics, collision and object-update consumers.
   Extract source comparisons for motion and damage/turbulence branches
2. Add bounded XL runtime handling with narrowly scoped engine dispatch hooks.
   Admit only verified modern property combinations; reject independent unknown
   properties, unsupported resources and legacy cases with existing diagnostics
3. Build a derived authored fixture with free lava, wall contact, water/lava,
   no-damage/lava and dry controls. Exercise lethal and nonlethal damage,
   invulnerability, difficulty, RNG, robot/projectile movement, boundaries,
   audible loop lifecycle, disk/rewind and stock transitions
4. Register and run an owned Android integration using controlled poses and
   actual physics. Validate state and mixer evidence, not just successful loading
5. Run focused regressions, required formatting and catalogs, audit original
   candidates without promoting them, record exact evidence, commit/push and
   open a stacked PR. Require latest-head green CI and continue within the run

## Acceptance record

The source/native controls and all 58 Android steps pass. Both catalogs and all
12 private-asset policy tests pass. Exact scope, limitations, hashes, source
comparisons and original-candidate results are recorded in
[the evidence](../../XL_compat/evidence/lava-volumes.md). Finish the final
formatted build, open the stacked PR and require latest-head green CI
