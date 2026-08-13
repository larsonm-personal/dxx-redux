# GQR-0006 automation receiver security plan - 2026-08-12

## Objective

Remove production exposure of command, automation, introspection, and preview
debug broadcast receivers while preserving the repository's intentional ADB
automation in debuggable builds. Mark app-internal dynamic receivers as
non-exported regardless of build type.

## Plan

- [x] Confirm the live ranking, clean worktree, finding, and remediation owner
- [x] Inventory every dynamic receiver, action, sender, manifest/build contract,
  ADB helper, and existing test
- [x] Define one explicit debug-only external receiver policy and internal-only
  receiver policy
- [x] Apply the policy to Setup, game, multiplayer, and preview receiver owners
- [x] Add unit contracts for release absence, debug ADB compatibility,
  and internal non-exported registration
- [x] Run launcher tests, scoped quality, release/debug Android builds, relevant
  ADB automation, and Windows D1/D2 validation as needed
- [x] Audit merged manifests, warnings, and diff scope; mark GQR-0006/GQF-0005 terminal

## Result

- `GQR-0006` is `DONE`; `GQF-0005` and `BR-0005` are `FIXED`
- Release setup command and introspection remain available only to this app;
  host migration is always app-internal; automation-only receivers are absent
  from release registration
- Debug ADB automation remains externally reachable and was exercised on the
  emulator through a fresh setup-introspection result
- The project has no release unit-test task; release Kotlin compilation,
  manifest processing, and the pure release-policy unit case passed
- No D1/D2 or other inherited game source changed

## Starting state

- HEAD: `5f3a4f7685edfcfec6a775b0eaedd0f246a61512`
- Worktree: clean
- Ranked impact: 56 (`MEDIUM-HIGH`), rank 10
- Finding: `GQF-0005`, P1/high security
