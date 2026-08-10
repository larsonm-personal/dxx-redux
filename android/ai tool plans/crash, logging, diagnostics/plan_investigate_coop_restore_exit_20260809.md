# Investigate coop restore exit after build 19940

## Scope

- Determine whether the 2026-08-09 coop restore ended through a crash, an intentional quit, or Android process termination
- Trace the final log activity through the restore, network diagnostics windows, profiler, and crash-reporting paths
- Compare build `6f729df7` with the preceding working build where repository history permits
- Recommend or implement only the smallest justified diagnostic or corrective change

## Plan

- [x] Establish the terminal event and timing from the supplied logs
- [x] Inspect relevant source and recent changes
- [x] Verify crash-report generation and identify coverage gaps
- [x] Record conclusions and any follow-up work

## Findings

- The cooperative restore completed successfully at `21:16:10.939`; the log has no save rejection, disconnect, fatal error, or normal quit marker
- The slowdown recorder then captured the restore frame as a 907 ms non-wait stall and the supplied log ends before another engine frame completes
- The installed APK was build `6f729df7`, built at 19:40. Current commit `02e04d23`, committed at 21:10 but absent from that APK, fixes BR-0355 in D2 save restore
- In the affected code, a rejected serialized object topology partially changed live segment heads before fallback relinking. The fallback could create a cyclic object list, after which the next render, AI, or collision traversal could run forever even though restore had already reported success
- The network stats and events overlays are not on the native restore path. Their JNI polling copies fixed-size arrays and their code did not change in build `6f729df7`; the log contains no evidence that either overlay caused the termination
- xCrash is configured for Java, native-signal, and ANR reports, but an object-list loop has no crashing signal. If Android or the user kills the unresponsive `:game` process before xCrash's ANR detector records it, no crash report is produced. The app does not currently persist Android `ApplicationExitInfo`, so SIGKILL/self-exit/low-memory attribution is lost on the next launch
- The current BR-0355 fix stages topology in local storage and clears all segment heads before fallback. Its focused 14-test validation suite passes locally

## Status

Investigation complete. Rebuild and retest from `02e04d23` or later. If the issue repeats on that build, collect the full debug-log export plus Android exit history/logcat because this log alone contains no terminal marker.
