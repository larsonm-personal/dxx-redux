# Process lifetime existing-owner continuation, 2026-10-13

Diagnosis/plan only, GQ2-CHUNK-0392. Assigned process_lifetime.ps1L1-L55, whole-added/raw=frozen/baseoriginalabsent. No code/test/helper edits, execution, new root/status or inherited saving.

## Current platform and entry contracts

Preserve Windows Initialize's self-enrollment before any child launch and non-inheritable sole kill-on-close job handle. Per-worker process_lifetime_windows.ps1 creates its own job before starting the payload, preserving inherited stdout/stderr, EOF stdin and exact exit status. Host and worker job lifetimes are intentional; do not replace them with after-launch tree enumeration. Already-exited immediate worker cleanup is backed by the worker's job, not merely Stop's HasExited return.

Linux adapter preserves original ArgumentList and uses isolated Python with requestingPID, pidfd parent/child observation and subreaper adoption. Supervisor observes main child without reaping until group cleanup, retires adopted separate-session descendants and preserves normal/signalled exit semantics. The maintained Linux fixture uses the real PowerShell pool, distinct-session grandchild and abrupt ownerSIGKILL. Keep pidfd identity protection and guarded zombie reaping; generic process-name/PID cleanup or one processgroup alone cannot replace it.

Resolve-RegressionPythonCommand selects an ordinary executable by name, not version/capability. The supervisor requires Python APIs os.pidfd_open/signal.pidfd_send_signal and Linux prctl/procfs/kernel behavior. Current unsupported startup reports125 or Python launch error rather than successful work. Do not infer that name discovery proves all runtime/kernel requirements. Future admission can report capabilities before launching the payload, without changing verified extraction-runtime provenance or adding network acquisition. Validate missing runtime/API/kernel/permissions and partial startup using actual wrapper paths, not mocked name resolution alone.

The adapter leaves macOS startinfo unchanged and Initialize is Windows-only. The metadata builder selects run-linux-build.sh for non-Windows; these inspected caller ranges do not declare/prove native macOS supervisor parity. Preserve desktop engine builds and document/fail safely for unsupported supervision according to each tool's actual declared host policy. Do not claim existing Linux evidence applies to macOS.

## Existing BR0173/0543 finite cleanup acceptance

Stop tries Process.Kill(true), Windows taskkill or ordinary Kill, then unbounded WaitForExit. It does not itself check taskkill exit or expose a deadline/escalation result. Pool timeout can therefore enter retirement whose bound is not established by the task timeout. Pool finally disposes each visited state but one stop exception aborts remaining foreach states. Windows host job closes at process exit, but a callback-error invocation can remain inside a still-live host; job lifetime does not prove every pool cleanup attempt completed. Keep these under existing owned resource/process lifetime acceptance rather than duplicate roots.

Future Stop contract should make graceful/escalated retirement finite and return/throw an explicit failure with retained process-generation identity. Cleanup must attempt every acquired worker and stream even when an earlier stop/dispose/drain fails, preserve primary exception and separately report cleanup residue. Do not stop unowned process names or stale bare PIDs. Preserve start-failed InvalidOperationException distinction and caller-owned disposal.

Actual aggregate Invoke-RegressionDataStageProcess launches invoke_process_with_exit_code.ps1 directly. On Windows the helper initializes self-job ownership and preserves the native exit sidecar for WindowsPowerShell5.1. Linux Initialize is a no-op and this route does not call Set-RegressionProcessLifetimeStartInfo; the pool's Linux supervisor fixture does not cover this route. Sidecar250ms outer drain can be bypassed by continuously completed inner line tasks; aggregate tailing can likewise run indefinitely with continuous descendant output. Retain separate stream/sidecar status and lifetime policies until actual entry controls establish a shared adapter.

Extend actual pool and aggregate fixtures with normalexit/callbackerror/timeout/Ctrl-C/abrupt ownerdeath, detached separate-session descendants, continuous inherited output, denied/delayed stop, startfailure before/after acquisition, and multiple cleanup failures. Require exact exit status and logs, finite return, every owned descendant/resource retired or identified, unrelated sentinels unchanged and no false sidecar publication. Windows fixture already keeps pool owner alive after payloadexit/timeout to distinguish workerlocal cleanup. Linux fixture covers pool only. Aggregate fixture preserves exit7 and returns promptly with an idle inherited handle, then explicitly stops the background process; it does not assert that runner-owned Linux descendants were retired.

## Formatter policy and other owners

Stop-stale-formatters previews by default, excludes ancestors, matches AST invocation and checkout, and rechecks process-start ticks and fresh invocation before calling Stop. Current fixture validates preview nonmutation, matching checkout descendant cleanup and preservation of sibling checkout. Preserve BR0538 admission and BR0603 proven generation/identity policy; Stop lifetime simplification must not replace these caller-owned authorization checks. Its finally disposes the chosen handle, but stop failure prevents subsequent target retirement, leaving existing all-error acceptance.

BR0233 remains the publisher owner for sidecar/artifact checked-close/replace guarantees; native exit observation and durable publication are separate. Do not conflate ordinary helper success with engine/publication success or close that owner from source inspection.

## Simplification assessment and rating

Retain55assigned lines as the platform dispatch and shared Stop facade. Keep Windows job/C# ownership and Linux pidfd/subreaper implementation distinct. Share a finite retirement result/cleanup loop only after actual pool/aggregate/formatter entry semantics and WindowsPowerShell5.1/modern Windows/Linux child behavior are proven. Current compatibility fixture parses repository scripts and exercises JSON/path/evidence/canonical-spec helpers; it does not invoke the pool under5.1 or prove quote/runtime parity. Set-HeadlessProcessArguments and the exit-sidecar helper have different dispatch/availability contracts; no speculative quoting merge or line-removal credit.

Controlling BR0543REFERENCE47MEDIUM (23/0/4/10/10), same-ownerGQ1CH0688. Focused shared helper impact; no broad engine/platform severity uplift or score summation. AllassignedRETAIN/authored-only M0; no numerical removal estimate, newfinding/remediation/investigation/status or inherited saving. Current owner definitions and these contexts need consolidated qualification before final rating/publication. Tests/builds/runtime acceptance remain future work.


## Final diagnosis disposition

Complete current BR0173/0233/0543 and formatterBR0538/0603 definitions remain OPEN; unchanged full definitions qualified through audited0390 parent records and current exact blocks. Source, entry and platform static checkpoints do not close runtime acceptance. No distinct new root survives the actual pool/aggregate/formatter/platform comparison; no new finding, remediation or investigation. RETAIN assigned55authored lines with BR0543REFERENCE47; original path absent and inherited saving0. Preserve separate platform supervision and Win5.1 sidecar/quoting. The small shared finite-retirement and all-error cleanup proposal is concrete future work, not implemented line reduction. Consolidated consumer audit and canonical publication are still required before chunk DONE.


Frozen0392 diagnosis publication COMPLETE afterPASS4196;canonicalGQC1221/GQD1101. Future implementation/runtime owner acceptance remains OPEN. Published snapshot preserves this plan before this final closure note.
