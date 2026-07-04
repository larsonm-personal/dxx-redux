# Coop Desync Logging Plan

- [x] Read repository instructions and confirm the game repo
- [x] Inspect Android debug log category plumbing and current coop/network/game logs
- [x] Trace coop resume lobby level loading and save handoff paths
- [x] Trace hidden door color and wall state propagation paths
- [x] Add a coop desync log category and targeted instrumentation
- [x] Run scoped formatting and the best available build or validation

Validation completed:
- `.\android\run-code-quality.ps1 -Fix -Paths ...`
- `.\gradlew.bat :app:assembleDebug` from `android\`
- `.\run-windows-build.ps1`
