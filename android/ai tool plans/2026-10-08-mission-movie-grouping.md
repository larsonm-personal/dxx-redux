# Mission movie ownership

The loose-file catalog links descriptors to HOGs and explicit dependencies, but
misses the engine's `<mission>-h.mvl` and `<mission>-l.mvl` convention. Movies
therefore become separate global content entries, including when added later.

## Plan

- Recognize exact, case-insensitive D2 movie companion names beside descriptors
- Reconcile standalone movie owners into an unambiguous matching loose mission,
  preserving the mission ID, enabled state and order; leave unrelated packages,
  ambiguous matches and conflicting bytes alone
- Publish verified movie payloads and the mission manifest before retiring the
  standalone owner, so retrying reconciliation remains safe
- Verify batch and incremental imports, launch resource scoping, disabling,
  duplicates/conflicts, and original Vertigo files with the content manager tests
- Run scoped formatting and the relevant JVM tests

## Result

Implemented in FileSetContentCatalog and FileSetContentManager. The catalog uses
the engine's exact D2 mission movie naming convention, including case-insensitive
matching. Reconciliation attaches previously separate standalone movie owners
to one matching loose mission; root imports and their `missions/` projection
refer to the same location. This also handles movies arriving before the mission.
Explicit mod/disc owners and unrelated directories are left intact.

The existing mission ID and state survive the merge. New payload bytes are
verified before publication; the mission manifest is committed before the old
movie owner is retired. Identical files allow an interrupted merge to complete,
while differing bytes and multiple candidate missions produce conflicts and
retain the separate owner.

## Validation

- Scoped code quality passed for all four changed Kotlin files
- Android Kotlin compilation and 35 JVM tests passed across
  FileSetContentCatalogTest, FileSetContentManagerTest and MissionLaunchCatalogTest
- Original retail Vertigo HOG, MN2 and both MVLs form one owner; launch resources
  contain byte-identical movie archives and remain scoped to that mission
- Incremental imports, movies imported first, preserved disabled state/order/ID,
  deletion, repeated reconciliation, interrupted retirement, conflicting bytes,
  ambiguous owners, case handling and unrelated/D1 filenames are covered
- Scoped git diff --check passed

No APK was installed or on-device UI review performed for this change. Existing
tests provide reusable coverage through `:app:testDebugUnitTest`; no native game
code changed for grouping.
