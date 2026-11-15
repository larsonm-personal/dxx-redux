# Next XL gameplay window after November 13

This plan does not start work. The user's final instruction ends monitoring once
all applicable checks except sampled readiness pass. That condition was reached;
finish the handoff and qualify its final documentation push. Read the
[report](xl_gameplay_what_was_done_20261113.md) and final CI addendum first.

## Estimate for the specific 8/8 readiness test

Budget **three 12-hour tranches**, with a provisional range of **two to four
tranches (24-48 hours)**. This is an engineering judgment, not a measured forecast.
Confidence is low until the exact four failures are diagnosed. A shared admission
dependency could shorten it; independent resource/simulation defects could exceed
the range. Do not treat two to four as a guarantee or as the estimate for all G2-G7.

The latest exact-head receipt for 81b4046f7bb61410ba85fd0da87504d294561ffd,
workflow 43533928032, pins cohort assets to
9dad21bd65ba05665ac6fe315f37fe7461e85725. Failures are BoilingPoint-XL level 7
(control2.rl2), Dinter level 1 (dinter.rl2), Sphere level 2 (sp-02.rl2), and
WShock20 level 6 (warplab.rl2). Each reports load_failed and zero frames.
The published artifact deliberately omits raw engine diagnostics, so it does not
establish four distinct root causes. The four successes are XL inactive.

The criterion is the requested original level loading and running 120 unassisted
simulation frames at 60 Hz without death or transition, using the ordinary
dxx-redux-d2-headless-route target. It does not require campaign completion,
rendering or confirmed assisted navigation. Keep all eight cases, exact assets,
deadlines and criterion unchanged. Developer metadata or rendered success does
not qualify this headless owner, and changing its flags to bypass admission does
not establish correct support.

Suggested sequence:

| Tranche        | Focus and evidence                                                                                                                                                                                                |
| -------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1              | Reproduce all four failures privately with the exact CI target; map shared and individual causes; implement and qualify the smallest real headless/resource dependency. Revise the estimate from raw diagnostics. |
| 2              | Finish source-backed loading and simulation dependencies; prove optional output cannot alter the 120-frame simulation; retest all eight unchanged cases and affected regression owners.                           |
| 3              | Resolve remaining cases and platform regressions; obtain exact-head hosted 8/8 plus all existing public/private owners, then commit handoff.                                                                      |
| 4, contingency | Independent later blockers or a second CI repair cycle. If needed beyond this, revise openly rather than weaken the gate.                                                                                         |

For the next 12 hours: use hours 0-2 for exact reproduction and ownership review,
2-3 to select a bounded cause, 3-8 for implementation and local canonical controls,
and 8-12 for CI repair, final qualification and handoff. Publish coherent tested
checkpoints early. Full atomic restore need not be completed merely to run this
two-second load/tick test; pursue G2 work below only where it is an actual
dependency or the user chooses that priority. Keep its known defect explicit.

## Remaining G2 dependency work

1. Refresh current fork HEAD, tree, stashes, stack 471 and exact candidate CI.
   Fix applicable regression failures first. Preserve unrelated work and failed
   evidence. The sampled 4/8 readiness result remains separately visible.
2. Review retained native sound work in stash
   `9783012e2a77dfdd4680812ae72b852cfaca5e17`. It is incomplete and untested.
   The native owner must reuse native bank decoding and canonical duration source
   selection, retain all target samples, validate rate/registry/overlay constraints
   before mutation, retire voices/cache before old PCM, and reject retired tokens.
   Connect it to real incoming XlPreparedResources and the existing audio owner.
   Prove prepare/discard/live PCM continuation, target independence, rate rejection,
   active reuse and stale-owner rejection. Do not publish disconnected APIs.
3. Audit retained native bitmap paging, POG retirement, palette/font/GPU ownership
   and source fingerprints. Native paging currently reads installed PIG handles;
   replacing metadata alone is insufficient. Palette publication also owns fonts
   and menu remapping. Resolve allocations and source validation before commit.
4. Complete native world preparation through existing codecs: geometry/topology,
   objects/signatures/lists, player, AI paths, native and XL runtime, epoch, clocks,
   SIM RNG and lifecycle. No guessed trailer scan, duplicate parser, live-table
   swapping, serialized rollback or observer-based repair.
5. Repair `birth-atomic` inside the canonical projectile owner and require actual
   unchanged-state and future-input controls, not merely the energy assertion.
   Keep corrupt-checksum, valid-checksum semantic corruption, truncation, wrong
   source and target-identity controls. Full G2 requires all acceptance gates.

Work sequentially without agents. Choose a bounded duration with the user and
reserve its final quarter for validation, CI repair and handoff. Publish coherent
locally tested immediate-parent layers early in larsonm-personal/dxx-redux only.
Never merge, auto-merge, enqueue or write upstream. Keep XL policy in XL_compat;
native edits must be necessary codec/state-owner work or thin hooks.

Retain Windows/Linux Debug/assertions owners, ordinary developer-OFF playback,
affected D1/D2 controls and Android owners. Compare real captures with clean
playback using detailed world/object/XL state and SIM RNG. Preserve strict FX
failure evidence: Android still adds 1,500 startup starfield draws. Do not hide
events, change authentic inputs, relax deadlines or reduce catalog membership.
Keep assists explicit, including the water-physics profile.

Final qualification must name head/merge and controller/candidate identities,
terminal public/private owners, exact artifacts, failed attempts and skip reasons.
Commit/push updated ledger/report/next plan/kickoff, then inspect newly triggered
CI. Missing camera addon remains a precise external prerequisite, not a pass.

Low-confidence remaining estimate: G2 16-32 focused hours; all remaining focused
engineering approximately 60-120 hours. Catalog-wide 90% qualification is extra
and unestimated. Do not interpret 4/8 load/tick or assisted water as playable-level
coverage. Preserve all 1,111 raw inventory IDs and the conservative 1,110 denominator.
