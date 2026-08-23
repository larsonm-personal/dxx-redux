# PowerShell 5.1 compatibility audit

## Goal

Determine whether the repository PowerShell scripts can support Windows PowerShell 5.1 without losing behavior, detail handling, or correctness, and make low-risk compatibility fixes where practical.

## Plan

- [x] Inventory scripts and identify PowerShell 6/7-only syntax and APIs
- [x] Classify findings into safe compatibility fixes and intentional modern-PowerShell requirements
- [x] Implement focused fixes with regression coverage for nontrivial compatibility helpers
- [x] Parse changed scripts with the Windows PowerShell 5.1 parser and run relevant tests
- [x] Run scoped code-quality checks
- [x] Leave `android/outstanding_bugs.md` unchanged because the file explicitly says not to edit it

## Constraints

- Preserve existing comments and user changes
- Do not trade away error reporting, encoding correctness, process handling, or structured data fidelity
- Prefer small compatibility shims or equivalent 5.1 APIs over broad rewrites
- Record any scripts that should remain PowerShell 7-only and the concrete reason

## Findings

- All 213 tracked and pending PowerShell files parse with the Windows PowerShell 5.1 parser
- PSScriptAnalyzer now checks PowerShell 5.1 syntax, commands, and .NET types across the whole repository rather than only `android/`
- Compatibility fixes preserve case-sensitive JSON keys, root-array shape, UTF-8 without BOM, atomic manifest replacement, strict JSON validation, process argument quoting, and relative-path behavior
- PowerShell 5.1-specific runtime fixes cover explicit compression assembly loading, missing `$IsWindows`, unavailable `ProcessStartInfo.ArgumentList`, `Join-String`, generic `Array.Fill`, and newer .NET path/hash/file APIs
- Route-corpus projection hashes are byte-stable across PowerShell 5.1 and PowerShell 7 for all 1,509 current records
- No script needs to remain PowerShell 7-only for the audited behavior

## Verification

- `android/tests/test_powershell_51_compatibility.ps1` under Windows PowerShell 5.1
- PSScriptAnalyzer PowerShell 5.1 compatibility rules: zero findings
- Scoped `android/run-code-quality.ps1 -Fix`: completed with no failed stages
- Focused Windows PowerShell 5.1 tests passed for JSONC, input-demo trace comparisons, runtime sampling, process-output capture, artifact cleanup, report parsing, fingerprint publication, mission metadata, bounded extraction, mission ZIP publication/recovery, and D2X-XL TGA layout
- The existing mission-route baseline test is currently stale under both PowerShell 7 and 5.1 because mission metadata changed independently; cross-edition current-manifest comparison is clean
- `run-windows-build.ps1 -Target both` successfully configured and compiled D1 through 224 build steps, then failed at the final headless-metadata link on the unrelated concurrent `android_newmenu_publish_interactions` C change; the PowerShell 5.1 build-wrapper path itself completed correctly
