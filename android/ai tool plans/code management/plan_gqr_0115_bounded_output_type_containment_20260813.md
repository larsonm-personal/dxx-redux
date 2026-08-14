# GQR-0115 bounded output type and containment plan

## Scope

- Remediate `GQF-0128` in branch-added bounded-extraction scripts and tests
- Reject links, reparse points, special files, multiple-link files, and unsupported output types before publication
- Verify each output's physical containment and byte accounting without following attacker-controlled links
- Preserve the completed runtime-admission and process-tree supervision work from `GQR-0112` and `GQR-0114`
- Make no changes under `d1/` or `d2/`

## Work plan

- [x] Freeze scope and record a durable implementation plan
- [x] Inspect the extractor publication boundary and current cross-platform filesystem checks
- [x] Add a fail-closed output-tree validator with physical containment and accounting
- [x] Extend POSIX and Windows-supported hostile filesystem fixtures plus ordinary-package coverage
- [x] Run focused Python and PowerShell suites
- [x] Run scoped code quality and record exact results and platform limitations

## Acceptance criteria

- Only regular single-link files and ordinary directories can reach publication
- Symlinks, hard links, junctions or reparse points, FIFOs, sockets, devices, sparse files, and alternate data streams are rejected where platform APIs expose them
- Validation walks without following links and verifies physical containment of every admitted object
- Output quotas use measured logical and allocated bytes and fail closed when an exact required measurement is unavailable
- Validation failure cleans only the attempt-owned staging tree and preserves unrelated sentinels
- Tests cover supported hostile output types, containment escape sentinels, accounting bypasses, ordinary packages, and cleanup ownership

## Constraints

- Keep changes in branch-added scripts and Python tests
- Use printable ASCII without a byte-order mark
- Preserve existing comments and concurrent work
- Do not edit the canonical quality ledger or campaign plan

## Completed implementation

- POSIX validation opens the output root with no-follow semantics and walks child names relative to retained directory descriptors
- Every admitted POSIX file is reopened without following links and must retain the same device and inode, be regular, have one link, and remain on the output filesystem
- Windows validation opens each path with `FILE_FLAG_OPEN_REPARSE_POINT`, compares final physical paths and volume identities to the retained root, and rejects reparse points, repeated identities, multiple links, non-disk handles, sparse files, and alternate streams
- The per-file quota uses logical length while the total quota counts the greater of logical and allocated length for each file
- Sparse outputs are rejected during the strict terminal pass so an ordinary extractor may use temporary allocation patterns while it is still writing
- The owned process tree is terminated before the strict pass, and a successful child is changed to failure when its final output is unsafe
- Existing callers retain attempt-owned staging cleanup and publication only after a zero bounded-extractor result; the validator adds no broad or name-based deletion

## Completed validation

- Windows Python suite: 22 tests completed successfully with four platform/capability skips
  - Exercised hard links, an unprivileged junction escape, alternate streams, a sparse file, strict disappearance races, ordinary nested files, terminal rejection after child success, and outside-sentinel preservation
  - Skipped POSIX-only allocated-byte, device, FIFO/socket cases and a symbolic-link fixture that lacked `SeCreateSymbolicLinkPrivilege`; the junction covered Windows reparse containment
- WSL POSIX Python suite: 22 tests completed successfully with three platform/capability skips
  - Exercised symbolic-link escape, hard links, FIFO, Unix socket, sparse output, allocated-byte accounting, strict disappearance races, ordinary nested files, terminal rejection after child success, and outside-sentinel preservation
  - Skipped Windows-only stream and junction cases and character-device creation denied by the unprivileged WSL session
- `test_bounded_python_runtime.ps1`: eight runtime admission and wrapper cases passed
- `test_bounded_extraction.ps1`: nineteen synthetic ZIP policy cases and four verified installer packages passed
- Scoped code quality, Python compilation, printable ASCII, no-BOM checks, and `git diff --check` passed

## Diff metrics

- Increment over the recorded `GQR-0114` worktree snapshot:
  - `android/helpers/run_bounded_extractor.py`: 294 insertions, 20 deletions
  - `android/tests/test_run_bounded_extractor.py`: 189 insertions, 12 deletions
- Inherited `d1/` and `d2/` changes: zero
