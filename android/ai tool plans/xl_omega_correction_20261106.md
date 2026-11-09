# Omega correction and isolation

Correct #398 as an unmerged follow-up. No pickup admission, merge, stack rewrite
or asset changes. Pin reference revision 2576a98b5de310e3ac7c9777df2677d1627b8bae.

The real engine confirmed frame-time charge/deadline with fixed-20-Hz blob damage.
Scale active-XL damage by the same frame time, preserving classic/network damage.
Qualify actual native target collision and charge at 20/30/60/120 Hz with a genuine
render context. Compare classic controls against the previous common engine paths.
Keep the existing blob lifetime/visual behavior explicitly outside visual-parity claims.

Require DXX_XL_DEVELOPER_PROBE for all mutation-heavy diagnostic dispatch/includes.
Verify ordinary and developer compiler configurations independently.

Input demos do not encode the render chronology needed by XL Omega. Reject
capture/replay when an active XL player owns Omega, including newly acquired Omega,
before accepting a replay result or recording frame. Apply the same admission to
headless, rendered and manually paused replay paths. Classic/network handling stays
unchanged. Do not synthesize hidden render ticks or claim equivalence.

Exercise gate negatives, save/restore and source identity; retain existing XL schema
32 and classic native save layout. Report the intentional old-XL-schema incompatibility.
Run scoped quality, classic differential controls and applicable existing regressions.

## Evidence and boundaries

- Actual robot collisions at 20/30/60/120 Hz: XL consumes 65536 charge and
  applies 14680064 fixed-point shield damage at each rate (strength 458752).
  The old #398 laser translation unit fails the same assertion.
- The complete pre-slow-motion `laser.c` from
  `02bb982a375d32c5731565b26d9b501cfd627a0c` was compiled into the same
  instrumented engine. Its eight classic/network-mode rows match the corrected
  implementation exactly. This is a translation-unit differential, not a claim
  that every pre-chain engine path or real network session was compared.
- Developer and ordinary compiler controls inspect both diagnostic flags; neither
  is present in the ordinary route-confirmation object. No production renderer
  behavior is added by the diagnostic harness.
- Replay controls generate a real input recording, load it into the initialized
  fixture, then exercise the shared headless step and real draw/key dispatch.
  The fixture temporarily detaches the global game window so the normal abort
  callback does not terminate the entire test process. All three paths unload
  without a result, simulation-time advance or weapon-clock change. This tests
  acquisition/pre-frame rejection, not a legacy checkpoint startup end-to-end.
- Existing ten-event rendered clock controls pass for XL and classic, including
  actual disk save/restore. The baseline classic translation unit passes too.
  This patch changes no save bytes/schema; schema 32 still rejects older XL
  schemas. This is not comprehensive historical save compatibility coverage.
- Pinned `weapons/omega.cpp:161` scales damage by frame duration; `:240` consumes
  that duration as charge. Existing Redux blob lifetime and visual differences
  remain unaddressed. No complete slow-motion or pickup-support claim is made.

Reproduction: use `android/tests/xl_omega_damage.py` under a genuine GL context,
with explicit engine/data/extra/reference-root/work paths, mission, level and
`--xl` only for XL. Its receipt is assisted and never updates readiness. Actual
qualification used the derived wetbots fixture and stock d2 level 1; it did not
alter or rerun the frozen original readiness cohort. The multiplayer rows use
engine mode flags and native collision, not a multi-peer network session.

Final local checks: desktop build passed; eight XL/multiplayer-mode and eight
classic/multiplayer-mode actual target cases passed; existing XL/classic ten-event
render/save controls passed; 36 private-assets Python tests passed; upstream
compatibility and classic trigger/demo CTests passed (25.33 seconds total); scoped
quality and automation catalog validation passed. Catalog execution stopped at the
4 GiB free-space reserve (3.70 GiB available), so it remains incomplete. No deadline
or reserve was relaxed. Windows/Android builds, true multi-peer networking, the
standalone headless executable and comprehensive historical-save loading remain
outside this local evidence. Hosted CI is required after publication.
