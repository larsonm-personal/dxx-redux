# GQR-0034 ISO physical output containment plan - 2026-08-12

## Objective

Prevent ISO extraction from following intermediate or final filesystem links,
junctions, reparse points, or replacement races outside its owned output root.
Keep the implementation in branch-added Android extraction code and leave D1/D2
original files untouched.

## Plan

- [x] Confirm the next ranked remediation, finding, starting HEAD, and worktree
  ownership
- [x] Trace every ISO output path, directory creation, final open, platform
  abstraction, and transaction boundary
- [x] Define the smallest handle-relative physical-containment contract for
  POSIX and Windows
- [x] Implement containment without changing valid ISO naming or extraction
  behavior
- [x] Add outside-sentinel tests for intermediate directory links/reparse
  points and final file links/aliases
- [x] Run focused tests, scoped quality, extraction-host build/tests, Android ABI
  builds, and relevant Windows validation
- [x] Audit final diff and worktree scope; mark GQR-0034/GQF-0047 terminal

## Result

- Added one branch-owned `.c/.h` physical output owner with handle-relative
  directory traversal and exact opened-file cleanup
- POSIX uses `openat`, `mkdirat`, `O_NOFOLLOW`, held directory descriptors,
  regular-file verification, and single-link final-file admission
- Windows uses held directory handles, root-relative `NtCreateFile`, explicit
  reparse rejection, regular-file identity checks, and handle-based deletion
- Real ISO extraction fixtures preserve outside sentinels across Windows
  directory-junction and final-link aliases and Android POSIX symlinks
- Focused CUE/ISO tests pass 76/76 on Windows and on emulator-5554; the full
  extract-host build and 40 non-hung suites pass, and all Android ABIs build
- The unrelated existing `graphics_config_transaction_tests` hang remains
  excluded from the full extraction CTest run

## Starting state

- HEAD: `35df421938dafaf5bce41a6843479f25b95ac234`
- Ranked impact: 56 (`MEDIUM-HIGH`), rank 12
- Finding: `GQF-0047`, P1/high physical path containment and link-race safety
- Existing dirty path: unrelated performance study plan; preserve without edits
