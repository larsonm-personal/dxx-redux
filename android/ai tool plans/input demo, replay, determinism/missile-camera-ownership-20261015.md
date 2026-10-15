# Missile camera simulation ownership

Baseline: e775ca994dd98bf370b837f60340cbccfe4e9fed

Preserve the deliberate May 12 implementation in 94f6d295d7: controlled guided,
current valid selected missile, then oldest valid local missile. Preserve wake
geometry, cadence, firing suppression and native D1 actor exclusion. Rendering
must not mutate identities consumed by this resolver.

- [x] Reproduce disabled-view selection changing real robot awareness
- [x] Make presentation selection and guided retirement nonmutating
- [x] Bind selected identity signatures at creation/release and pair resets
- [x] Preserve static transitions using presentation ownership
- [x] Add real-function regression cases and retain failing baseline control
- [x] Run bounded existing-object validation and independent review

No save/replay format change, recorded wake events, fallback removal, or new
simulation calls from rendering. Existing guided restore reconstruction is out
of scope. Full builds and PowerShell quality tooling are unavailable within the
current disk/tool constraints; distinguish focused evidence from full validation.

## Implemented boundary

The visual selector no longer binds or retires the ordinary selected identity.
Extra-view rendering no longer clears controlled guided ownership. Existing
weapon-box ownership drives static transitions, including no retrigger after
static finishes. Ordinary creation and valid guided release bind the signature
at assignment; stale guided release returns without camera adoption or network/
demo publication. The existing level reset clears pointer and signature together.
The simulation resolver checks its existing weapon validity predicate before
reading/binding the signature. Its guided/current/oldest choice, fallback scan,
geometry and tick cadence are unchanged.

This fixes the remaining shared-pointer edge in the intentional May 12 change;
it does not implement the old BR-0368 proposal to gate waking on the visual option
or remove fallback. BR-0368's save reconstruction concern remains separate.

## Focused evidence

- Production objects compiled for gamerend.c, laser.c, gameseq.c and object.c
  without new warnings and linked against existing D2 test objects/libraries
- 60 real-function cases pass across D2/native-D1 actor profiles, view enabled/
  disabled, zero/one/repeated queries, active/released guided selection, deletion,
  slot reuse and stale guided identity
- Real AI firing remains suppressed for camera-only awareness; SIM/FX counters
  are unchanged throughout wake/query checks
- Real ordinary firing binds its new signature before rendering or wake update
- Actual extra-view dispatch preserves gameplay identity, enters static once,
  preserves its timer and does not restart it after completion
- Stale guided release preserves the previous ordinary selection
- Existing public missile selector/geometry helper passes updated nonmutation
  expectations
- Negative control: the same durable regression linked with baseline production
  objects fails guided-release signature binding (exit 1). The earlier independent
  witness also establishes disabled visual selection changing actual awareness
- Independent source review found no blocking defect

The bounded executable reuses e339e575a test scaffolding/libraries with the exact
changed production sources and new fixture; it is not a fresh full e775ca994 build.
Unchanged old fixture code emits existing string-literal-to-char* warnings. GPU
renderer/discovery parity, public-level replays, full aggregate targets and the
PowerShell formatting/catalog entry points have not been rerun in this tranche.
No replay oracle was refreshed. No production push or merge is authorized here.
