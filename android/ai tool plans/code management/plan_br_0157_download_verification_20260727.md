# BR-0157 downloaded source and executable verification

## Goal

Require a repository-owned cryptographic identity check before every remotely
acquired source, archive, bootstrap, or executable is compiled or run.

## Plan

- [x] Read repository instructions, review process, and the complete finding
- [x] Inventory download manifests, cache paths, update tooling, and consumers
- [x] Centralize pinned identities and enforce verification before every use
- [x] Add online, offline-cache, mutation, and fail-closed regression coverage
- [x] Run scoped quality, focused tests, native suites, and Android builds
- [x] Record the resolution and move BR-0157 to the done ledger

## Verification

- Same-size file mutation, missing offline cache, and extracted-tree mutation
  checks passed in `android/tests/test_download_verification.ps1`
- Existing cached 7-Zip, unar, and lsar executables matched their committed
  SHA-256 values; the pinned Python and machfs trees installed and revalidated
- A stale decoder cache failed CMake configuration before compilation; clean
  online population and `FETCHCONTENT_FULLY_DISCONNECTED=ON` reuse passed
- Scoped code quality passed
- All 17 extraction CTest targets passed
- Windows D1 and D2 host builds passed
- JDK 21 debug APK builds passed for arm64-v8a, armeabi-v7a, and x86_64
