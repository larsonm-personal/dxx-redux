# November 17 XL recovery outcome

This explicitly invoked tranche began November 16 at 9:20 PM PST and targets
completion by November 17 at 10:00 AM PST. It started from merged cmake
b90b11d118837dcaa75b48b12f0af99cf1fee8ca with 58/64 expanded readiness and original
8/8. The runtime work is published through #569; exact final validation is in
xl_recovery_execution_20261117.json and the handoff PR.

## Result and scope

The final runtime #569 CI measurement is **61/64**, original **8/8**, with three
load failures and no simulation failures. Probes took 243.4 seconds and the
expanded job took 8 minutes 43 seconds. This is the unchanged short load/tick
criterion, not campaign playability. Trusted run 44047189499 binds the result to
runtime head 879386551dc71d104662e06594f2bb1a17c2bda7; the execution ledger records
its artifacts and regression-owner outcomes separately.

The final runtime private Linux owner passed all 113 required tests and navigation
in 811.809 seconds. Public save/replay, ASan and Android checks were still running
when the committed ledger snapshot was taken. Their terminal outcomes and the
documentation-only final-push checks are recorded in the handoff PR body; pending
entries in this snapshot are not passes.

Three original cases recovered:

| Original case               | Cause addressed                                                                          | Qualification beyond load/tick                                                                                                                  |
| --------------------------- | ---------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| Boiling Point 1, Waterworks | Original type-10 RLE TGA image could not decode                                          | Independent pixels, malformed images, owned scopes and real GPU controls                                                                        |
| Boiling Point 4, Subzero    | Actual record boundaries, sentinel powerup initialization and invalid animation fallback | Source reader/initializer comparisons, original pickup and disk continuation, retained effect save validation, rendered probe                   |
| Warp Shock 4, Nautilus      | Unused negative animation fields on closed walls and illusions treated as dependencies   | Debug/optimized original capture/replay, full object/world/SIM state, illusion/cloak transitions, cold partial-cloak restore and rendered probe |

Legacy smoke actions 20-24 now follow the pinned empty action bodies while
retaining flags, scheduling, object-owner retirement and save behavior. Source
Debug/Release and native Debug/optimized lifecycle controls qualify that change.
It removes two Boiling Point 5 blockers but does not claim another passing case.

## Published checkpoints

All PRs are non-draft immediate-parent changes in larsonm-personal/dxx-redux,
linked to historical stack 471. No merge, auto-merge, enqueue or upstream push.

- #562: source-owned sentinel record boundaries; initial CI exposed a stale
  modern-owner rejection assertion.
- #563: corrected full modern-source assertions; private CI passed at 58/64.
- #564: bounded RLE image decoder and ownership controls; complete CI 59/64.
- #566: original Subzero initialization and effect-aware native/Android save
  validation; first Linux compile failed on an ambiguous JSON conversion.
- #567: explicit JSON conversion repair; private 113-test owner passed. Expanded
  measurement was incomplete, and its error status remains historical evidence.
- #568: wall dependency correction and original lifecycle; complete CI 61/64,
  private 113-test owner and navigation passed.
- #569: source-defined legacy smoke lifecycle and strengthened existing owner.
  Final checks are recorded separately from earlier green checkpoints.

## Remaining deficit and next work

| Case                           | Remaining shared work                                                                                 |
| ------------------------------ | ----------------------------------------------------------------------------------------------------- |
| Boiling Point 2, Control       | Generated-item definitions, selection, lifecycle and pickup-mode behavior for IDs 7/8/9/24/26/27      |
| Boiling Point 3, Canyon        | Per-side owned door-strip phase, rendering, collision and saved continuation                          |
| Boiling Point 5, Boiling Point | Equipment lifecycle above plus flag IDs 46/47; legacy smoke trigger blockers are separately addressed |

The next-window plan gives source findings, design boundaries and acceptance
gates. These are two substantial shared workstreams, with possible downstream
dependencies; there is no unconditional finite 64/64 estimate.

## Validation limits and preserved evidence

Each checkpoint report records source, original input, binary and local receipts.
New receipts are under E:/dxx-redux-builds/xl-recovery-20261117. GitHub artifact
archives were checked against their published SHA256 digests. Frozen64, original8,
authentic private Vertigo/XL resources, seed, 120 unassisted frames, watchdog/start
guard and the ten-minute CI budget remain unchanged.

Final Windows ordinary developer-OFF run on runtime head
879386551dc71d104662e06594f2bb1a17c2bda7 was incomplete: 57 passed, three failed and
four untested in 318.719 seconds of its 360-second probe budget. Original 8/8 did
complete. The four untested cases were lsrbase3.mn2 level 1, ewithin.mn2 level
4 and secret 2, and ironblad.msn level 2. This receipt is not combined with CI into
a claimed complete local score. Earlier incomplete/failed receipts also remain.

The trusted private controller still uses its reviewed runner. Supplemental
original Subzero lifecycle is locally exercised but not yet adopted by that
controller; the original Warp lifecycle is optional/provisioned. Existing required
native controls and ordinary frozen-cohort cases have separate CI ownership.
Android libraries were rebuilt and real native save cases passed, but no new APK
was installed and no original Subzero/Warp Android campaign qualification is
claimed. Debug Subzero's earlier diagnostic timeout remains a limitation, separate
from its passing optimized lifecycle and source contracts.

G2 rejected-save energy 91-to-100, Android startup FX's extra 1500 starfield draws,
guarded general Android XL timeline replay and the prior-APK Android equipment
failure remain open. No clock/RNG compensation or relaxed assertion was used.
User AGENTS.md/.codex work, the four original stashes, authentic assets and app
state are preserved. Low-space cleanup used the prescribed preview/apply path;
it found no eligible deletions. Builds and receipts remained on E:.

See xl_recovery_next_20261117.md for the next plan and
xl_recovery_kickoff_20261117.md for an explicitly invoked replacement kickoff.
