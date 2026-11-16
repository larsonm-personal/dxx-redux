# Bounded nightly refill

Parent: PR #539, f86a071746b874b4d3aa3f684a7bda6635ea20c7

## Implementation

The old sampler stopped after its estimated batch even when wall time remained. Extend its trusted planner with an immutable reservation of next-undone owners. Preserve the initial hash-ordered allocation, then distribute remaining host owners among compatible queues by estimated load, retaining each queue's order. Android queues stop before the existing three-player/private-fixture incompatibility boundary

The workflow already provisions from plan.tests. Reserving the full allowed queue there makes private checkout, corpus/demo staging and Android prerequisites account for every possible refill before candidate execution. Public and private host queues remain separate, with no work stealing. Provisioning and the private sanitizer are unchanged. The only workflow edit selects a bounded counts-only job display and continues uploading the complete summary artifact

The reservation digest binds target, catalog, immutable retail identity, controller file hashes and workflow SHA, ordered queues, core IDs and the existing absolute 3000-second window. Its estimated inventory may exceed 45 minutes; this is a reservation, not an execution-time allowance. The reservation is capped at 4096 policy-approved owners and the existing 8 MiB artifact limit; the current reviewed corpus reserves 2745 owners in about 1 MiB. Every actual child plan retains the original 1024-owner maximum and ordinary per-lane estimated budget. Materialize the immutable corpus policy once per controller process rather than re-enumerating it for each recovered child

Execute core first, then one next owner at a time while the original absolute deadline minus 120 seconds permits. A monotonic elapsed-time guard also prevents clock rollback from extending the allowance. Persist admission before launching. Never rerun an existing output directory; recover its evidence instead. Shared build output stays at results/build, already excluded from public artifacts. Owner processes still use existing timeouts, bounded above by the remaining global allowance

Collect only a contiguous prefix of admitted children, derive each child independently from the trusted reservation, and validate its exact batch/target/lane/retail provenance. Parent reconciliation rejects changed/reordered/duplicate/unreserved receipts. Nested result rows have already passed existing public/private sanitization before their trusted child digest is attached. The private upload still includes only the aggregate results.json and retail verification

A setup failure or missing/incomplete execution receipt stops that queue rather than hot-looping another owner through shared setup. Actual terminal test failures allow further owners to run but remain failures. Scoped capability exclusions may continue the queue without coverage credit. Unadmitted owners and setup-only failures remain undone for next night. Existing cross-revision traversal, failure history, partial coverage and exclusion invalidation rules remain unchanged

## Additional reporting correction

PR #539 changed standalone build failures to FAIL, but inspection found the collector still converted all pre-test failures to BLOCKED. Preserve only the explicitly nonretryable build-failure case through collection. It remains attempted_tests=false, coverage_complete=false and cannot advance traversal. The regression checks collection and reconciliation together

## Validation plan

- Deterministic reservation and successive refills versus estimated duration; next-night continuation
- Multi-shard uniqueness and public/private isolation; Android incompatible-queue boundary
- Absolute/monotonic budget cutoffs; timeout recovery; no same-run retry after setup failure
- Exact target, controller, shard, child identity, order, duplicate and admission forgery rejection
- Real subprocess execution with a tiny target-side journal producer, followed by actual collection/reconciliation
- Existing planner/host suites, catalogs, scoped formatter, exact-head PR CI

## Limits

Queues can finish early if exhausted, blocked by setup, or separated by a private/resource boundary. Refill cannot guarantee all runners stay busy or the whole inventory completes in one night. One-owner master refills may repeat legitimate per-owner runner setup, but share controller provisioning and host build output. Core is ordered within each lane, as before; this is not a cross-lane barrier

This PR must remain stacked on #539. No merge, history rewrite, cmake push, baseline relaxation or Android preflight/sanitizer change is authorized here

## Local results

- Final full planner/workflow suite before the display-only addition: 146 tests passed without skips
- Final sampler/refill suite after the display addition: 33 tests passed, including the real subprocess producer, all 2745 reserved real-catalog owners and derived child plans, and bounded display preservation
- Host adapter suite: 75 tests passed without skips; retail boundary suite: 19 tests passed
- Automation catalog: 99 standalone JSON, 549 support and 315 standalone PowerShell entries, passed
- Master catalog attempted and blocked by this runtime's missing /proc/self/task/PID/children interface (supervisor exit 125); no bypass and not a pass
- Scoped mixed-language formatting and diff whitespace checks passed

Final full-suite and exact-head hosted checks are recorded in the PR. No actual one-hour hosted run or private-asset execution is claimed by these local tests. Review and deployment of this controller on cmake remain separate

## Review follow-up: collection scaling

Independent review found that live execution collected and reconciled every prior child after every completion. Replace this with one live collection/reconciliation per child, durable per-child receipts, and one independent full re-read at completion or handled interruption. Workflow recovery still independently reconstructs the full admitted prefix after a hard interruption. The aggregate is written once rather than rewritten after every child

The final re-read compares each child's sanitized receipt with a digest retained in controller memory when that child completed. Changed or disappeared completed evidence prevents aggregate publication; a typed integrity-error marker prevents a subsequent recovery step from silently clearing an observed mismatch. Recovery still validates admission order, exact attempt/child/reservation identity, target and retail provenance. The aggregate validator now slices its already-verified ordered rows once instead of scanning all rows for each child

Count-based regressions cover 25, 100 and 243 admissions with a fixed reservation: exactly 2N collection calls and 2N reconciliation calls (live plus full recovery), three queue derivations and one aggregate write. Additional tests modify an earlier completed receipt during a later child and interrupt execution before the next receipt exists

A local no-payload benchmark uses the actual 3139-owner materialized policy and prefixes of a real 243-owner private shard. It writes synthetic child evidence and exercises actual controller collection/reconciliation; it does not execute tests, stage private assets or establish coverage. Each timing below is one warm local measurement, not a hosted performance guarantee

| Controller | Children | Live execution plus final recovery (seconds) | Collection calls | Reconciliation calls | Separate fresh recovery (seconds) |
| ---------- | -------: | -------------------------------------------: | ---------------: | -------------------: | --------------------------------: |
| Before     |       25 |                                        4.028 |              350 |                  350 |                             0.293 |
| After      |       25 |                                        0.587 |               50 |                   50 |                             0.306 |
| After      |      100 |                                        2.549 |              200 |                  200 |                             1.152 |
| After      |      243 |                                        5.399 |              486 |                  486 |                             2.364 |

The old 243-child execution was not timed; its prefix algorithm performs 29889 collection and reconciliation calls each, versus 486 each after this change. Benchmark script and measurements are retained under temp/refill-scaling-20261116. The committed count-based tests enforce scaling without timing-dependent assertions

Follow-up validation: all 150 planner/workflow tests pass after scoped formatting, including 18 refill tests. Automation catalog validation passes; the master catalog remains blocked by the same unsupported local /proc child-supervisor interface (exit 125). No baseline, timeout or coverage rule changed

### Live admission-loss review follow-up

An observed loss of the current child's admission after launch could previously
raise before its receipt entered the live digest map. Final recovery then accepted
the shorter prefix. The live collection/reconciliation boundary now durably
latches a fixed `live-child-evidence-invalid` reason and child batch identity before
unwinding. Final collection checks the latch before publishing any aggregate.
Errors during final independent collection also latch `final-evidence-invalid`
unless a more specific integrity marker already exists. Exception text is never
written into these markers.

Child collection now verifies the on-disk plan against the independently derived
child and rejects present but malformed or mismatched attempt identities. Missing
attempt evidence remains NOT_RUN: it can legitimately follow admission and a hard
kill before executor startup. An unadmitted tail remains acceptable to standalone
recovery when no live integrity loss was observed. No inferred crash-tail failure
is latched by standalone recovery.

Regression coverage corrupts/deletes the CURRENT admission or plan after an earlier
PASS, corrupts present attempt identities, and corrupts an earlier child's attempt
during a later subprocess. It requires a durable typed marker, no prior-only
aggregate publication, and rejection by subsequent recovery. A separate recovery
fixture retains an admitted child with no attempt and a truly unadmitted plan-only
tail, with exactly one terminal traversal credit.

Validation: all 153 planner/workflow tests passed without skips (186.572 seconds),
including the existing 25/100/243-child operation-count checks. Scoped formatting
and the automation catalog passed (99 JSON, 549 support, 315 PowerShell). The
master catalog reached the same unsupported `/proc/self/task/PID/children`
supervisor restriction (exit 125); it remains blocked, not passed.
