# GQR-0063 Inno metadata peak-memory plan

## Objective

Resolve `GQF-0076` by enforcing the 128 MiB peak-live-memory policy across
Inno metadata stored/raw buffers, decoded output, and LZMA decoder allocations
without changing inherited D1/D2 sources

## Plan

- [x] Inspect the Inno metadata decode path, allocation ownership, existing
  extraction limits, focused host test target, and canonical finding
- [x] Carry one live-allocation budget through stored input, stripped raw input,
  decoded output growth, and every LZMA SDK allocation
- [x] Add focused tests for the exact boundary, one byte over, and injected
  allocation failure using the production budget helpers
- [x] Run scoped formatting and focused host tests
- [x] Build the affected Android native targets for available ABIs where feasible
- [x] Record exact validation results and remaining blockers here

## Constraints

- Keep all product changes in branch-added Android extraction files
- Preserve existing handmade comments
- Do not edit the canonical quality ledger or the parent campaign plan
- Keep files printable ASCII without a byte-order mark

## Validation record

- Scoped `android/run-code-quality.ps1 -Fix` passed for the plan and all three
  touched source files
- MSVC `test_gog_fd` target built successfully in
  `build-br0237-extract` with no warnings in touched code
- `test_gog_fd` passed against the maintained D1 1.4a and D2 1.1 GOG
  installer fixtures, including the exact 128 MiB, one-byte-over, and decoder
  allocation-failure probes
- `inno_reader.c` compiled for both D1 and D2 targets on Android
  `arm64-v8a`, `armeabi-v7a`, and `x86_64`
- Full `:app:externalNativeBuildDebug` reached native compilation but was
  blocked by concurrent reconnect-auth edits: two callers in
  `net_udp_android.c` supplied five arguments to a six-argument challenge
  message API
- Product diff is limited to branch-added Android extraction files: 209 added
  and 34 removed lines across `inno_reader.c`, `inno_reader.h`, and
  `test_gog_fd.c`; inherited D1/D2 diff is zero
