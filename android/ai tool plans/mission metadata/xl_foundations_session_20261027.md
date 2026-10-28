# XL single-player foundations session

Target: thirteen hours under the active overnight goal, beginning
2026-10-28 02:18 UTC. Continue toward complete
single-player support in dependency order. Feature counts and mission admission
percentages are not completion criteria. Keep the single binary with separate XL
behavior and Redux alternatives where their semantics differ

The preceding stack is merged. Start from origin/cmake e2cc8219a, whose tree
matches the validated xl_more_28_fluid_alignment endpoint. Use the existing
isolated worktree for build caches; leave the user's root checkout alone

## Sequence

1. Complete classic HXM gameplay definitions and ownership. Verify source readers,
   actual installed robot/model/joint/bitmap behavior, collision dimensions,
   weapons/gun transforms, initialization, repeated load and restoration. Remove
   the blanket HXM gameplay rejection as semantics become implemented; do not
   replace it with mission-name or hash exceptions
2. Complete supporting format and capacity requirements revealed by original
   assets. Expand legitimate data and runtime headroom rather than retaining
   Redux limits as permanent XL constraints. Keep malformed inputs bounded
3. Continue remaining gameplay mechanisms in dependency order: event dispatch
   and actions, physical environments and movement, then custom combat, bosses
   and generation. Earlier original assets remain regression inputs
4. Strengthen campaign transitions and cross-feature persistence. Gameplay-facing
   visibility, lighting, transparency and audio are part of fidelity; purely
   cosmetic rendering and multiplayer remain later work

## First batch: HXM

The pinned source is arbruijn/d2x-xl
2576a98b5de310e3ac7c9777df2677d1627b8bae. Start with LoadRobotReplacements,
ReadRobotInfos, CPolyModel::Read/Size and CObject::ModelRadius/Verify. Investigate
the whole normal startup path before inferring live behavior from individual
functions. Compare source semantics through extracted code where practical

The original Alarlof HXM provides one replacement robot, ten joints, one model
and six bitmap references. Its original archive SHA-256 is
f0611f1428549373526dfca8ca5a1e1883ace910f4209a938698086d189919db.
The HXM SHA-256 is
a41f8c1cee39131e5e83d7712cce2d9df5b3ac5836948e9681c70632e3148e5c.
Object 100 in segment 639 actually uses robot 3; serialized model/health differ
from its custom definition. Check normal actor startup and combat, not only
definition-table equality. Base/custom model radii and signed boss flags are
known semantic differences needing explicit treatment

Startup source inspection confirms that level object verification precedes the
level HXM load. FixObjectSizes then refreshes radii without changing model IDs.
ModelRadius defaults to the retained base radius; projectile creation explicitly
requests the custom radius. Preserve that distinction and verify the actual
authored robot plus custom projectile models. Do not infer live robot health or
model IDs from the serialized level alone

The prior read-only survey matched HXM companions to 84 levels in 41 archives.
Its retail-baseline structural results are useful leads, not mission
qualification. Fourteen other Apocalyptic Factor XL levels retain independent
light-index problems. Resolve legitimate format cases from source evidence;
do not silently rewrite original mission bytes to make a test pass

## Acceptance and delivery

- Every substantial batch has source semantics, actual production activation,
  dependency/failure controls, lifecycle/save/restore and stock isolation
- Use controlled poses, crossings, authored actions and combat. Do not attempt
  autonomous 3D navigation
- Register Android integrations and owned scripts, run required catalogs,
  complete scoped formatting, and inspect terminal test results
- Preserve original assets and protected emulator state. Distinguish derived
  fixtures from unchanged-original evidence in reports and compatibility status
- Commit and push reviewable branches and open stacked PRs as in the preceding
  session. Do not merge. Finish with green latest-head CI for every PR
- Reassess the next dependency after each batch. Keep remaining requirements
  visible; do not declare full XL support from a successful load or test count
