# Nightly corpus diagnostics

Base: 84387ccdfb5fcd761f39bd0b4f6702ad92fa8150
Branch: fix/nightly-corpus-diagnostics-20261020

- Inspect scheduled run 39401018352 and fixed corpus owners
- Preserve structured failure causes, command outcomes and asset availability at the private receipt boundary; retain local raw logs without publishing assets or arbitrary test output
- Correct supported staging/admission causes and add nonzero, exception, timeout and capability regression coverage
- Run focused checks, publish a topic PR and inspect terminal CI

Evidence: Linux job 118555674132 writes owner logs but uploads only results.json and retail-verification.json. The private receipt projection replaces every reason with phase/status. Raw artifact transport currently returns HTTP 403 in this executor; connector metadata and job logs work.

## Results

- Preserve allowlisted causes, bounded metadata mismatch paths, command outcome/exit/duration, stream byte counts and fixture availability through collection and retail projection
- Persist command progress before launch and after completion/timeout; interrupted owners retain diagnostics without gaining coverage
- Nested variants remain explicitly unavailable in planning; the runner guard remains intact
- 128 nightly Python tests and 68 adapter tests passed, including real nonzero stdout/stderr execution and timeout/cancellation contracts
- Automation catalog validation passed: 95 standalone JSON, 426 support and 261 standalone PowerShell tests
- Master catalog check blocked by this executor's missing /proc child-supervision interface, exit 125
- Scoped quality runner passed with pinned Ruff 0.16.10; unrelated existing formatting churn was removed
- Initial targeted attempts were blocked before execution; later targeted reproduction reached both owners
- Protected retail runs require the existing trusted cmake workflow; no trust gate or schedule was changed

## Inner worker qualification

Enable the existing native/Kotlin worker diagnostic directory for each unique
owner attempt. Project only shutdown exit/duration/completion flags, stream byte
counts and recognized assertion/sanitizer/timeout/EOF categories. Raw logs and
exception details stay local. Interrupted collection reports missing shutdown
proof without granting coverage. Unknown inner messages remain unclassified.

A real synthetic Python child launched through the PowerShell metadata helper
wrote a private assertion marker and exited 7. The projection retained exit 7,
complete stderr (47 bytes) and the assertion category without exposing the marker.

## Targeted reproduction

- Lostlvls completed with worker exit 0 and failed strict comparison only on 24
  data_warnings entries. PR #100 intentionally stopped reporting nonnegative
  empty joint offsets as normalized; the baseline still contained the old warning
- Vela1 initially exited 139. A native debugger traced the crash to the debug
  connectivity message box in load_level, then gr_get_string_size without a
  font canvas. Apply the existing no-render guard on hosts as well as Android
  in both games; connectivity validation and the interactive dialog remain
- With the dialog fix, Vela1 completes with worker exit 0 and differs only on
  the same obsolete warning in its two levels
- Correct only those 26 warning clauses. Full generated JSON equality was
  verified after that exact substitution; no other metadata or oracle changed
- Add data_warnings to the safe schema field vocabulary, retaining no warning
  values, and persist attempted_tests immediately after successful launch
- Final strict targeted reruns passed: Lostlvls 42.817 seconds, Vela1 6.107
  seconds; both worker commands exited 0. Both D1 and D2 metadata workers rebuilt
- Repeated Python qualification: 128 nightly tests and 68 adapter tests passed
