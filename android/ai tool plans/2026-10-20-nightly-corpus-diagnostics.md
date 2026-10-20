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
- 124 nightly Python tests and 67 adapter tests passed, including real nonzero stdout/stderr execution and timeout/cancellation contracts
- Automation catalog validation passed: 95 standalone JSON, 426 support and 261 standalone PowerShell tests
- Master catalog check blocked by this executor's missing /proc child-supervision interface, exit 125
- Scoped quality runner passed with pinned Ruff 0.16.10; unrelated existing formatting churn was removed
- Targeted Lostlvls/Vela1 attempts are BLOCKED before execution: committed baseline and route oracle present, private mission archive absent
- Protected retail runs require the existing trusted cmake workflow; no trust gate or schedule was changed
