# Wall camera and smoke trigger qualification

The remaining Sphere 5 and Warp Shock 3 failures include wall CAMERA type 16.
Boiling Point 5 also requires smoke speed/density types 21 and 22. Preserve the
existing camera-object binding path; these wall actions are a different owner.

Source audit: main/trigger.cpp at qualified revision
11038eb53dd86f3bcdc027dddd3f93c60f02536e has SHA256
74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852.
Smoke types 20-24 have explicit empty switch cases. CAMERA reaches default
Int3(), then common one-shot and alternate bookkeeping. Crucially, the actual
include/error.h (SHA256
c9f66fd0211aca53e9ad9d0933315af26578a9c7401feac7b288095e4391b777)
defines Int3() as ((void)0) under DBG and empty otherwise. It is not a debug
trap in this pinned source. Qualify both preprocessor branches explicitly rather
than inferring behavior from the diagnostic name.

Implementation requirements:

- Extract and exercise actual source dispatch/bookkeeping with hash-pinned
  constants and Int3 definitions. Do not invent camera rendering or smoke
  mutation absent from this source action.
- Admit only qualified types and retain every trigger record and authored index.
  Update inventory/admission and runtime restore validation consistently.
- Route these actions through the existing actor gates, cooldown, delay,
  scheduling and post-action flags, but bypass native classic action dispatch.
- Preserve camera-object bindings and test their independence from wall actions.
- Extend the existing native scheduler/flag owners with immediate, delayed,
  disabled, one-shot, alternate, permanent, autoplay and chained activation,
  player/non-player gates, removed attached actors, save/restore continuation,
  world state and RNG controls. Test malformed records before state publication.
- Rebuild Debug/reference and ordinary developer-OFF targets, exercise original
  Sphere 5/Warp Shock 3, and re-inventory Boiling Point 5 without expecting its
  independent equipment blockers to disappear. Run the frozen 64-case survey
  and affected Android owner before claiming cross-platform qualification.

## Native integration discovery at 6 PM PST

The trigger-dispatch audit alone is insufficient for wall-camera admission.
The first native fixture run failed because its derived mission title exceeded
the native 25-character limit; shortening that test title allowed loading to
reach the real second dependency. The renderer rejected the wall binding with
`Wall camera viewer placement remains unqualified`. Both failed receipts remain
under the private `camera-trigger-01` evidence directory.

Pinned `render/cameras.cpp` has SHA256
607e48f8ee93ff3edacfb44ab824c2ee67bf7e8c38c7aff06459b61957ef4ac1.
`CCameraManager::Create` creates monitor bindings for wall CAMERA records at
level setup, independently of their inert activation dispatch. `CCamera::Setup`
uses source-side normal zero converted through ToAnglesVec and matrix Create,
the source side center, and the source segment. A same-source/target face sets
the mirror flag. These wall viewers are private camera objects, not attachments
to the live object array. The existing native renderer only supports attached
object viewers and deliberately rejects wall bindings.

The premature admission and supported-status changes have been withdrawn.
Original camera admission remains blocked. Dispatch/reference work is retained
as unfinished work; its passing source controls do not qualify the level.

Before reopening admission:

1. Qualify wall-viewer position and orientation against actual extracted Setup,
   normal-to-angle and matrix routines, including non-axis-aligned and split
   sides. Reuse existing source-matched geometry helpers where exact.
2. Give each wall monitor a retained private viewer with validated source face,
   without allocating a live object or consuming simulation RNG. Preserve
   attached-camera lifetime and dead-owner freeze behavior.
3. Exercise wall/object mixed binding order, monitor selection, mirror UVs,
   reload/stock transitions and save/restore reconstruction. Keep malformed
   geometry and capacity failures transactional.
4. Run real host and Android monitor capture checks as well as native trigger
   tests and original Sphere 5/Warp Shock 3. Headless load success alone cannot
   qualify this renderer dependency.

The two original levels are still counted among the eight unresolved failures.

## Wall viewer implementation and first source comparison

The working renderer now retains a private zero-initialized viewer per wall
monitor, using the compiled source-side center and first normal. Object-attached
monitors keep their controller and dead-owner freeze path. Wall viewers use the
existing source-matched angle/matrix helpers and do not allocate gameplay objects.
Admission remains blocked while integration and rendering checks are unfinished.

The native Debug probe generated 558 viewer rows across all six sides of the
loaded derived Neptune level. The independent reference executable extracts
actual CCamera::Setup, ToAnglesVec, angle tables and matrix routines. All 558
position/segment/orientation comparisons passed. The source center is supplied
from the previously qualified geometry compiler; this comparison is not an
independent requalification of that compiler. Invalid source faces were rejected,
and native object allocation and SIM RNG remained unchanged. A one-unit matrix
mutation was rejected by the oracle. Private receipts: camera-trigger-01,
native-flags-03/camera-wall.txt, wall-reference-01.log and wall-negative.log.

The full native wrapper still fails at the intentionally retained admission
guard. Do not report this partial receipt as a passing trigger integration test.
Both Debug native and reference builds succeeded after fixing include-order and
extraction-placement errors; initial failed build logs are retained.

## Integration candidate at 6:15 PM PST

Wall type 16 admission is enabled in the unpublished working candidate for full
integration validation after implementing the missing viewer path. This is not
an accepted or published compatibility claim; rendering and Android gates remain.
The native trigger owner now passes all 256 flags, actor rejection, malformed
links, immediate and chained delayed actions, disk restore, permanent rearming,
autoplay and removed attached-actor continuation. Its CMake runner now consumes
the native viewer receipt with the effect reference executable when configured.
The combined wrapper passed in native-flags-05.

The complete developer-OFF survey camera-64-01 finished in 278.328 seconds:
58/64, six load failures, zero simulation failures, original 8/8, no budget
exhaustion. Original Sphere 5 and Warp Shock 3 both loaded and ticked. The
candidate is parent 32e081bac70529d70148b4a2e87e8f6f6f8b90f6 plus the recorded
working patch and untracked files, not that parent alone. Binary/source hashes
are retained in camera-trigger-01/measurement-source-hashes.json. Renderer probe
edits made after this build are not represented by that binary.

The prior boss repair 8419d73908058421651344a6addb5b2a85dc65e3 now has successful
private Linux, result aggregation and final publication in run 43997829644.
Its expanded measurement was complete at 56/64 in 240.307 seconds. Restacked
build-margin candidate 32e081bac70529d70148b4a2e87e8f6f6f8b90f6 measured complete
56/64 in 233.458 seconds in run 43997888855; its Linux owner was still running
at the last inspection. Neither is CI evidence for the unpublished camera patch.
