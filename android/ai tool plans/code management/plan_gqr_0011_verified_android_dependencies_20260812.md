# GQR-0011 verified Android dependency acquisition plan - 2026-08-12

## Objective

Make every production Android native dependency acquisition cryptographically
verifiable, cache-safe, and reusable without a network connection. Reuse one
branch-added verification owner and leave D1/D2 original files untouched.

## Plan

- [x] Confirm the next live ranking item, finding, starting HEAD, and existing
      worktree ownership
- [x] Inventory every production native fetch, pin, cache path, extraction
      path, and current verification helper
- [x] Define one compact verified-download/cache contract with explicit hashes
      for every downloaded byte sequence
- [x] Apply the contract to the Android native build without changing desktop
      or inherited game sources
- [x] Add hostile-cache, changed-pin, disconnected-reuse, and ordinary clean
      configure/build tests
- [x] Run focused tests, scoped quality, Android ABI builds, and relevant host
      configuration/build validation
- [x] Audit dependency provenance, final diff, and worktree scope; mark
      GQR-0011/GQF-0024 terminal

## Starting state

- HEAD: `5f3a4f7685edfcfec6a775b0eaedd0f246a61512`
- Existing dirty paths: the completed `GQR-0006` product, test, plan, and ledger
  changes; preserve them without overlap
- Ranked impact: 56 (`MEDIUM-HIGH`), rank 11
- Finding: `GQF-0024`, P1/high supply-chain integrity

## Result

- Added one repository-owned CMake helper for verified downloads, cache locks,
  cache-byte revalidation, immutable archive identities, and offline reuse
- Moved all production Android native source acquisitions and the matching host
  extraction/input-demo dependencies onto reviewed manifest URL/SHA-256 pairs
- Added focused hostile-body, corrupt-cache, changed-pin, and disconnected-reuse
  tests
- Validation passed for the focused tests, scoped quality, full Windows D1/D2
  build, complete extract-host compile, and the debug APK across arm64-v8a,
  armeabi-v7a, and x86_64
- The extract CTest run was stopped after the unrelated existing
  `test_graphics_config_transaction` process remained idle for over ten minutes

## Diagnosis continuation, 2026-10-09

Original checked implementation/results above remain historical. Canonical GQR-0011 is TODO for the previously reopened desktop/native acquisition boundary. GQ2-CHUNK-0035 extends that same owner with current FluidSynth/GCEM acquisitions: reviewed URL/SHA256 and TLS remain intact, but direct FetchContent does not use the central locked cache/deadline/publication helper. Preserve checked upstream patches and include retained patched/unpacked source identity in acceptance.

- [ ] Inventory actual FluidSynth/GCEM and already open desktop JSON/texture-tool paths against the shared acquisition contract
- [ ] Delegate appropriate acquisitions to existing verifier; keep manifest pins and patch before/after predicates
- [ ] Verify ordinary accepted cache/offline reuse, changed dependency generation and consumer rebuild identity using the supported tool contract; keep deferred hostile/security/resource probes deferred
- [ ] Reconcile with GQR-0110 minimum/incremental acceptance and current exact source generations before closing the complete boundary

No implementation, configure, fetch, download, patch or test performed in diagnosis tranche. Full evidence imported as GQ2-CHUNK-0035 extraction CMake graph diagnosis 20261009 SHA256d3c0717dd371a7b6a205509231b125ef48d0faf53722fa56adb3d5cae1feaa44.
