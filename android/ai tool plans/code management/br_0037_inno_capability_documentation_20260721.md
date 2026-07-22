# BR-0037 Inno Capability Documentation

## Goal

Synchronize the Inno reader's public capability and provenance documentation
with the compiled implementation, its version gates, and callback behavior

## Plan

- [x] Audit compression methods, version gates, encryption handling, and callback semantics
- [x] Update the source and public header documentation with one accurate capability statement
- [x] Add or update a lightweight documentation consistency check
- [x] Run scoped code quality and the native extraction suite
- [x] Move BR-0037 to the done ledger only after validation passes

## Current disposition

Completed on 2026-07-21. The maintained capability matrix now distinguishes
accepted, intended, tested, unsupported, and unresolved behavior. A consistency
test ties the documentation to the compiled method switches, version gate,
encryption metadata, and callback semantics. Scoped code quality, the focused
test, and all nine native extraction suites pass. BR-0037 has been moved to the
done ledger; BR-0036 and BR-0058 remain active implementation findings
