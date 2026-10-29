# Canonical 25/27 runtime layouts

Continue the twelve-hour run ending 2026-10-29 04:54:16 UTC. This branch starts
from fd7b8e711084cd4a6abb7a2168a32e271459f669, PR #186, which contains complete
25/27 inspection and the Pumo inventory. Monitor #185 and #186 CI while working.
Do not merge PRs or modify the user's root cmake checkout.

## Implement

1. Audit all exact 23/40 conditions in translation, admission, lighting,
   resource precedence, repair and owned state. Extend only source-equivalent
   behavior to the validated modern profiles. Preserve noncanonical-geometry
   blockers and independent gameplay/resource checks.
2. Translate canonical explicit sides without losing source topology or UVLs.
   Strip the extra equipment/fog header bytes correctly and rebase every section.
   Trace wide producer/value consumption and normalization instead of silently
   truncating live values. Keep authored source fields available to XL ownership.
3. Compare vertex brightness to the extracted side reader, including explicit
   triangular/degenerate controls, without granting those shapes runtime support.
   Retain fog settings and corner ownership in new-layout save identities.
   Require actual authored fog application before admitting affected volumes.
4. Create labeled 25/27 fixtures from unchanged supported 23/40 geometry. Verify
   normal loading, geometry/textures/UVLs, objects/events, repair/source/resource
   behavior, disk/rewind, mission transitions, malformed-state rejection and stock
   isolation. Use controlled engine events and poses, not autonomous navigation.
5. Exercise the normal Android runtime and useful rejection/recovery, run scoped
   source/native/platform checks, publish the dependency increment and continue
   with Pumo's remaining fog/effect/event/resource requirements.

The original Pumo campaign remains independently blocked. Its canonical geometry
lets this work precede general non-hexahedral geometry; no mission-name checks or
optimistic whole-campaign support flags are permitted.

## Initialization evidence correction

The full source trace exposed an earlier generator-oracle omission: it began at
AssignProducer after assuming serialized producer indices survived initialization.
LoadExtSegmentsCompiled first calls CreateGenerator in geometry order; those
functions renumber robot/equipment makers. Reactors do not enter the source
producer station list. The expanded extracted-source comparison fails before the
production correction (`temp/runtime-producer-before.txt`). Correct initialization,
equipment startup target selection, boss dependency closure and save identity as
part of this branch. Preserve raw inventory fields, compare the full setup path,
and replace the old assignment claims in evidence instead of retaining false parity.

## Validation checkpoint

Eleven focused native/source controls pass, including modern robot/equipment
production and affected boss behavior. Five layout/inventory/source checks pass
under MSVC and Linux GCC with strict warnings and undefined-behavior checking.
The expanded source comparison covers 60 layout profiles and all original Pumo
vertices. Both normal Android build targets pass. The modern runtime runner
passes 87/87 steps, with two detached captures, six rewind restores and both
new formats restored through stock; protected device files match exactly.

The first Android attempts exposed two test omissions: the multi-level start
dialog and explicit worker drain while paused. The corrected reusable script
passes without weakening state comparisons. The same APK also passes all 53 robot
generator and 61 equipment steps with corrected mapping expectations, actual
spawns/pickups, six/nine rewinds and exact protected-file recovery. Local
implementation and validation are complete; publish this increment and monitor CI.
Five new native runtime tests are
required by the original-asset runner; its 12 policy checks pass. Both catalogs
pass with 419 top-level entries and 475 support scripts. PRs #185/#186 are green.

Final scoped formatting passes. PowerShell files were also formatted/linted
directly because the helper excludes an ancestor directory named temp in this
isolated worktree. Final catalog reruns pass; original-inventory regeneration
preserves bytes and modification time. The root cmake checkout remains clean at
e2cc8219a2bc2ffe36bce54986b67bde2eda0bf8.

Published as 23830d44e0931596375b0ae001aa69d4bb8c9370 in PR #187, based on
xl_more_40_layouts. All seven workflows passed on 2026-10-28. The PR remains
open and unmerged; the next fog increment is isolated on xl_more_42_fog.
