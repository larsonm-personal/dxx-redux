# GQR-0002 DXA mask filename bounds plan - 2026-08-12

## Objective

Eliminate the fixed-buffer overflow in DXA mask texture filename construction,
preserve lookup behavior, and add exact-boundary runtime coverage without
expanding inherited D1/D2 diffs.

## Plan

- [x] Confirm the live ranking, clean worktree, finding, and remediation owner
- [x] Trace the production loader, existing checked filename helpers, callers,
  build ownership, and current tests
- [x] Implement checked DXA mask-name construction in branch-added shared code
- [x] Add exact-fit, one-byte-over, empty, and ordinary-name runtime coverage
- [x] Run focused tests, scoped quality, Windows D1/D2 builds, and all Android ABIs
- [x] Audit warnings, diff scope, and final metrics; mark GQR-0002/GQF-0007 terminal

## Starting state

- HEAD: `52ca652d58ea70e62e1f28ee24e2455194be11e1`
- Worktree: clean
- Ranked impact: 56 (`MEDIUM-HIGH`), rank 8
- Finding: `GQF-0007`, P1/high correctness/bounds

## Result

- Reused `android_ogl_texture_filename` for `_mask.png` construction
- Exact 256-byte destination boundary and one-byte-over rejection are covered by
  the compiled host test; renderer contracts require rejection before lookup
- Seven renderer contracts, focused CTest, Windows D1/D2, and Android
  arm64-v8a, armeabi-v7a, and x86_64 builds passed
- Product/test numstat is `+42/-1` across three branch-added paths, with zero
  inherited D1/D2 edits
