# Host owner admission audit

This follow-on audit reviews all **148 existing master owners declared host** at
`e12e0bcf4c2fe6e3edd0302f75dd5c4666e9bb70`. It is source review, not an execution
report. Every owner has a concrete decision in [host_owner_audit.json](host_owner_audit.json),
including exact prerequisites, argument requirements, expected skips and fixes.

## Decisions

- runnable-with-provisioning: 29
- runnable: 71
- scoped-profile: 6
- asset-blocked: 25
- portability-fix-needed: 5
- artifact-blocked: 1
- platform-blocked: 3
- existing-owner-overlap: 2
- adapter-needed: 6

“Runnable” requires the documented environment; it does not mean admitted by the
current policy or observed passing. “Asset-blocked” names actual required files;
community archives are not automatically labelled licensed. “Platform-blocked”
includes suites whose portable helper checks run but whose complete owner requires
Windows. Profile-only runs retain the unexecuted portions as exclusions.

## Provisioning and owner binding

- Use a full checkout and a supported writable `dependency_base.txt` configuration.
- Some synthetic fixture suites require Node 20+ and the repository's code-quality
  npm dependencies, not just Python. Bounded extraction has a separate verified
  Python 3.12.8 tree requirement. Rust/Cargo, 7-Zip, JDK 21/Gradle, native libraries,
  and public downloads are needed by the respective recorded owners.
- Never add a mandatory-argument script as an empty master selection. Bind explicit
  native binaries, verified public data, isolated outputs and profile switches.
- The public native adapter profiles use the existing assertions, lazily build
  required targets and separately time setup. Public flight replay requires
  Xvfb/Mesa. Benchmark policy-only execution does not cover performance measurements.
- Existing native host CTest assertions already belong to `engine:ctest-d1/d2` and
  their Python contract adapters. Avoid double-counting a wrapper as new leaf coverage.
- Test-only portability changes must be present in the immutable target commit
  before its owner is admitted; changes to the control checkout do not patch target
  source silently.

The original [all-source reconciliation](NIGHTLY_COVERAGE_RECONCILIATION.md) is a
frozen 158-selection admission snapshot. This audit is the host follow-on backlog
resolution; new policy admissions and runtime receipts remain separate evidence.

The legacy CUE/ISO wrapper is not admitted: it removes source test fixtures after
CTest. The non-destructive full extractor adapter owns those same registrations.
