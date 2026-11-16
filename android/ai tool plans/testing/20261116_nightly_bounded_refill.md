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
