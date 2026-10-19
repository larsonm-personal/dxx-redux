# Unconditional public LAN smoke

- Run the existing public shareware launcher lobby, join, ready, start and both-in-game assertions on every pull request
- Reuse the existing immutable target plan, Android dual-emulator executor and strict receipt reconciliation
- Omit that owner only from extended PR selections to avoid duplication; preserve conditional deeper coverage and scheduled/manual selections
- Keep read-only permissions, no secret inheritance and the existing fork approval boundary
- Validate planner selection, failed/missing/skipped receipts, workflow syntax and public CI; do not merge into cmake

## Android rerun prerequisite timeout

- Attempt 2 timed out in the D2 auto-level owner after 600 seconds while building native targets; D1 death was not run
- Build only the selected native game executables before emulator boot and the test budget, using the existing Linux runner and revision stamps
- Preserve owner budgets, gameplay assertions, unconditional LAN selection and strict reconciliation
- Validate Linux runner argument/stamp behavior, workflow selection, actual native build and hosted CI
- Local verification: 81 CLI/planner tests passed; selected D2 desktop target built successfully and the existing replay freshness guard accepted it without rebuilding; Actionlint, Bash syntax, shellcheck and shfmt passed
- Automation catalog validation passed; catalog-runner integration is blocked locally because the Linux child supervisor cannot read `/proc/self/task/<pid>/children`; hosted CI remains required

## Nested fixture freshness correction

- Artifact 12147828938 confirms Android recording passed, then generated `android/tests/build-shareware-fixture/CMakeFiles/Makefile.cmake` invalidated the prebuilt game and triggered an all-target rebuild inside the unchanged 600-second owner budget
- Exclude CMake-generated `CMakeFiles` contents and generated install/test scripts from source freshness scanning; keep real source timestamps and revision validation
- Regression coverage includes the exact nested fixture path, generated compiler probes, and real C/CMake source changes
- Verification: guard regression tests, scoped quality checks and automation catalog validation passed; real D2 game-only build passed and remained fresh after advancing the actual nested generated Makefile timestamp; catalog-runner test retains the documented local `/proc` supervisor limitation
