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

This is a plan and source audit only; no new trigger admission is enabled yet.
