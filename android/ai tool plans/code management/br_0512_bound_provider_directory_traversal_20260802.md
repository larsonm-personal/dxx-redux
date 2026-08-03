# BR-0512 bound provider directory traversal

## Plan

- [x] Read BR-0512 and related findings in both ledgers
- [x] Trace provider directory traversal, document identity, and all callers
- [x] Implement bounded cycle-safe traversal at the shared boundary
- [x] Run focused tests, Android build, and scoped code quality
- [x] Archive BR-0512 with validation evidence

## Result

The duplicate unbounded SAF scanners were replaced by one suspendable breadth-first traversal. It validates and visits each document ID once, checks coroutine cancellation between directories and rows, caps depth, directories, rows, and retained results, and applies per-query and whole-scan deadlines. Provider queries receive a `CancellationSignal`, and all traversal failures stop before import with a recoverable launcher diagnostic. Focused traversal tests, scoped code quality, the debug JVM test, the three-ABI debug APK build, and `git diff --check` passed.
