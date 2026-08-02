# Extraction launcher handoff recovery

## Goal

Fix extraction regression infrastructure failures where direct import and file verification succeed but SetupActivity stops responding before the automated game launch.

## Plan

- [x] Locate the failed USA Disc 2 attempt artifacts and reconstruct the launcher/process state
- [x] Trace the extraction runner's pre-launch reset and SetupActivity readiness checks
- [x] Compare the path with hardened launcher recovery used by other emulator tests
- [x] Implement state-aware recovery rather than extending the readiness timeout
- [x] Extend existing extraction or launcher recovery coverage
- [x] Run focused validation and scoped code quality
- [x] Record the root cause and rerun guidance

## Root cause

- Retained device logcat shows the extraction runner force-stopped PID 4394 at 21:14:40. No activity-start event or replacement app process followed.
- From 21:15:08 through 21:15:36, the runner repeatedly broadcast `SETUP_INTROSPECT`, but Android had no application process or registered receiver to handle it. This was not an ANR and a longer readiness timeout could not succeed.
- The runner issued an asynchronous `am start`, discarded its output, never verified that the command created a process, and entered the full readiness poll anyway. The suite-level retry restarted the emulator but repeated the same unverified force-stop/start sequence.

## Resolution

- Every extraction-test SetupActivity boundary now uses one `Start-ExtractSetupActivity` path.
- Startup uses synchronous `am start -W -S`, waits briefly for the package process, then polls SetupActivity introspection only after a process exists.
- A launch that creates no process is retried locally without rerunning extraction. A process that exists but does not answer introspection is also relaunched once.
- If both launches fail, diagnostics now include relevant activity state and focused ActivityManager, ActivityTaskManager, and setup logcat lines before returning the existing retryable infrastructure result.
- The existing workflow test asserts the synchronous start, process verification, and pre-game handoff path. It passed after scoped PowerShell formatting and linting.
- A focused end-to-end run of `Descent I and II - The Definitive Collection (USA) (Disc 2)` passed on its first complete-spec attempt, reached `Ahayweh Gate`, and finished in 3:46. The same run recovered from a separate transient ADB daemon failure while staging Track 5, providing additional infrastructure-recovery coverage.
