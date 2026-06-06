# General Touch Region Diagnostics Plan

Created: 2026-06-05

## Request

Touch regions can be wrong on physical phones and this has recurred outside the pilot listbox. Waiting on the pilot select screen does not make the regions stabilize, so this is likely a persistent coordinate-space mismatch rather than a first-frame race.

## Plan

1. [done] Trace the Java-to-native touch path and both native menu/listbox hit-test paths.
2. [done] Add rate-limited debug logging at Java touch ingress, native normalization/remap, SDL push, newmenu hit testing, and listbox hit testing.
3. [done] Keep logs in the existing Android debug log system and limit output so exported phone logs remain usable.
4. [done] Run scoped code quality and Android build validation, then update this plan with results.

## Validation

- `android\helpers\stop-stale-formatters.ps1`: no stale formatter tasks found.
- `android\run-code-quality.ps1 -Fix -Paths ...`: passed after formatting `android_input.c`.
- `android\gradlew.bat -p android :app:assembleDebug`: passed with existing Gradle deprecation warnings.
