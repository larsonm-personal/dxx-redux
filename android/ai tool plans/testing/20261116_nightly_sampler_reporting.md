# Nightly sampler reporting and ETC2 build repair

Base: remote cmake 71e6efb90419b8e8a2f4ceed116e5ab3871c80d2

## Scope

- Partition selected sample owners into full PASS, partial PASS, FAIL, TIMEOUT, runtime exclusion, blocked prerequisite and pending receipt groups
- Preserve target/controller/fixture-scoped exclusion invalidation and cross-revision traversal credit
- Compile ETC2 layout initialization in both C and C++ with strict warnings
- Record configured standalone build failures as FAIL without granting test-attempt or traversal credit; keep configuration/prerequisite failures separate
- Validate existing sampler/host tests, native compile/CTest, catalogs and scoped formatting

No changes to Android preflight, NOT_RUN collection, retail sanitization, baselines or coverage rules. No merge or cmake push. Publish an isolated ready-for-review PR after local validation

## Bounded refill follow-up

Refill is deferred because selected tests currently determine provisioning, shard batch IDs, recovery collection and publication admission. Appending pending IDs inside execute would either reject valid receipts or weaken the trusted boundary. Increasing the estimate budget alone would not implement refill

Implement a versioned, immutable reservation plan in the trusted planner:

1. Assign each remaining owner to exactly one lane/shard queue in deterministic hash order, with separate public and private host queues and the existing Android three-player/fixture incompatibility rules. Never steal work from another shard
2. Include all reserved owner definitions, queue order, core IDs, exact target/catalog/controller/fixture identities and the existing absolute deadline in the plan digest. Preflight/provision against the entire reservation, before any candidate execution, with credentials removed as today
3. Run core first, then admit one next queued owner at a time while the original deadline minus the 120-second receipt reserve permits. Persist an admission record before launch. Run each admitted owner at most once; a failed shared prerequisite ends that queue rather than retrying setup for every owner. Keep owner timeouts within remaining wall time
4. Bind each child receipt to the reservation digest, shard, queue index and exact owner definition. Trusted reconciliation must independently derive child plans and reject unreserved IDs, duplicate assignments, forged/reordered admissions and wrong target/fixture identities. Never trust a candidate-written expanded plan
5. Recovery collects only admitted work and preserves partial/test/setup outcomes. Unadmitted reservations remain pending, with no traversal or coverage credit. Advance only existing terminal test-attempt receipts; next night resumes the next undone owner, retaining current wrap and exclusion invalidation semantics
6. Test fake-clock underestimates, exhausted deadlines, crash before/after admission, duplicate and forged receipts, shared setup failure, public/private isolation, mutually exclusive Android queues, exact-SHA coverage versus traversal and next-night continuity. Exercise workflow provisioning and recovery together before deployment

A running job cannot cross the public/private boundary to use spare time. A short public queue may still finish early. The target is bounded useful sampling, not a promise that every runner stays busy or that a one-hour run completes the inventory

## Local validation

- Planner/workflow suite: 132 tests pass, 3 PowerShell-dependent skips in the initial PATH; rerun all 18 SamplingNightlyTests with PowerShell enabled passes without skips, including actual producer/collector/report and hash ordering
- Host adapter suite: 75 tests pass, 1 existing skip
- GCC 14.2 C++14 strict warning build of existing ETC2 limits test passes; standalone CMake Release target test_etc2_limits and CTest pass 1/1
- GCC C99 with -Wall -Wextra -Werror -pedantic compiles and executes a mip-layout check using the same header
- Scoped mixed-language formatter passes
- Automation catalog passes: 99 standalone JSON, 549 support scripts, 315 standalone PowerShell
- Master catalog test attempted; blocked by this runtime's unavailable /proc/self/task/PID/children interface (Linux child supervisor exit 125). This is not a pass and no supervisor guard was bypassed

The overnight workflow and refill are not locally exercised. Normal PR CI cannot deploy the trusted nightly controller; review/deployment on cmake remains separate
