# Emulator Health Storage Recovery Failure

## Goal

Determine why mission metadata regeneration raised a `Count` property error in
`emu_health.ps1`, then reported a healthy emulator while app-private storage was
not ready.

## Plan

1. [Complete] Inspect the failing run log and reconstruct the recovery call
   sequence.
2. [Complete] Audit `emu_health.ps1` collection handling under PowerShell 5.1 and
   PowerShell 7.
3. [Complete] Determine whether app-private storage readiness is a downstream
   effect or an independent recovery bug.
4. [Complete] Report the root cause and recommend a scoped fix without changing
   recovery behavior during this diagnostic pass.
5. [Complete] Make the single-emulator mission batch select its primary
   emulator, fix scalar collection handling, and preserve useful storage-check
   diagnostics.
6. [Complete] Add focused regression coverage and run PowerShell compatibility
   and code-quality checks.

## Findings

- The first failure was an unscoped ADB operation during default file-set
  publication: `adb.exe: more than one device/emulator`.
- The batch does not set `ANDROID_SERIAL`, while its shared `Adb-Timeout` helper
  also does not select the primary emulator. A second or stale ADB transport can
  therefore make every ordinary ADB command ambiguous.
- `Get-OnlineEmulatorSerials` returns pipeline output. With exactly one online
  emulator it is assigned as a scalar string, and both callers in
  `emu_health.ps1` access `.Count` without wrapping the call in `@(...)`.
- The health script targets its chosen online serial for shell checks but ignores
  additional offline transports when at least one online serial exists. It can
  consequently print `HEALTHY` while unscoped batch ADB commands remain
  ambiguous.
- The final app-private-storage message is downstream and misleading. Its
  unscoped `run-as` calls could not select a device; no evidence shows damaged
  app storage.

## Implementation

- Both health-script callers now force array capture, so zero, one, and many
  online emulators behave consistently in PowerShell 5.1 and 7.
- The mission ZIP batch defaults `ANDROID_SERIAL` to the primary emulator only
  when the caller has not selected another serial. A concurrent emulator no
  longer makes the batch's ordinary ADB calls ambiguous.
- `emu_health.ps1` accepts and honors the selected serial through its initial
  check and restart waits.
- App-private storage checks retain their failed `run-as` response, and recovery
  includes that detail instead of presenting every ADB failure as damaged
  storage.
- Focused contract and helper tests pass under PowerShell 5.1 and 7. The scoped
  code-quality pass also succeeds.
