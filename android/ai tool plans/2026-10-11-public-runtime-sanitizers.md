# Public-only instrumented runtime coverage

1. Preserve the existing retail All suite; add an explicit Public profile using pinned public fixtures only
2. Reuse full CTests and public replay, secret-area, worker-recovery and malformed-header tests with explicit instrumented executables; retain assertions and diagnostics
3. Parameterize config-bound mission selection so all three existing variants run against d2demo without retail data
4. Add a Linux CI job, runner contract coverage, scoped quality checks and serial ASan validation after the emulator resource window closes

## Validation

Public-profile runner contract checks pass, including stage failure continuation and captured sanitizer findings. Linux CLI checks pass (15/15), public fixture validation passes, automation catalog validation and direct host catalog pass. Scoped formatter/lint passes. Full catalog subprocess test hits this executor's known missing /proc/self/task/<pid>/children support.

Actual sanitizer execution is blocked locally: even a standalone `int main() { return 0; }` GCC ASan executable fails with `LeakSanitizer does not work under ptrace`. No sanitizer options are disabled to bypass the environment. The dedicated GitHub job runs actual Debug ASan builds and all public workloads in a normal Ubuntu runner; its result is required before calling this coverage green. No sanitizer suppressions or baseline updates are permitted. Public graphical replay kills the idle menu after complete state/RNG output; do not claim graceful shutdown/leak coverage there. ASan process log files preserve findings even when worker helpers discard stderr.
