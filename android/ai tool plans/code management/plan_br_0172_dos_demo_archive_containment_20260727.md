# BR-0172 DOS demo archive containment

## Goal

Reject every DOS demo archive member whose normalized destination is not a
strict child of the uniquely owned extraction staging root before creating any
directory or file.

## Plan

- [x] Read repository instructions and the complete finding
- [x] Trace archive validation, extraction, staging ownership, and cleanup
- [x] Add fail-closed member-name, containment, collision, and link checks
- [x] Add hostile archive integration coverage and verified-package controls
- [x] Run scoped quality, focused tests, native suites, and Android builds
- [x] Record the resolution and move BR-0172 to the done ledger

## Verification

- PowerShell syntax checks passed for the bounded extraction helper, DOS demo
  extractor, and focused test.
- The focused hostile archive corpus passed, including forward and backslash
  traversal, absolute, drive, UNC, mixed separators, dot components,
  sibling-prefix escapes, Windows aliases and devices, destination collisions,
  file/child conflicts, and reparse-root rejection. Every rejection left no
  staging residue and preserved an outside sentinel.
- All four SHA-256-pinned DOS demo packages passed full bounded extraction and
  retained their expected installer.
- Repository-scoped PowerShell quality checks passed.
- All 17 native extraction tests passed.
- Both Windows host game builds passed.
- The JDK 21 debug APK build passed for arm64-v8a, armeabi-v7a, and x86_64.
- Independent P1 verification remains a campaign closure action.
