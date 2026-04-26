# Input Demo Phase 3 Fixture Writer Tranche

## Goal

Add the shared writer/helper surface for the first live-recorder tranche:

- sparse `rng.p0.jsonl` support
- generated `demo.json5` metadata support
- host validation for exact stable output

This tranche does not hook gameplay yet. It keeps the engine change for the next
slice thin by finishing the file-format writer work under `android/` first.

## Constraints

- Keep the work Android-first and shared under `android/app/src/main/cpp/shared`
- Use `nlohmann::json` / `nlohmann::ordered_json`
- Preserve stable key ordering and sparse git-friendly output
- Keep D1/D2 tree edits minimal
- Validate through focused host probes before wider builds

## Planned Steps

- [x] Add shared RNG stream structs and sparse JSONL helpers
- [x] Add shared metadata writer for `demo.json5`
- [x] Add focused host tests for RNG coalescing, file round-trip, and metadata output
- [x] Wire the new helper into host-test and Android build graphs
- [x] Run focused host validation and Android native validation

## Completed Notes

- Added a shared `input_demo_fixture` helper under `android/app/src/main/cpp/shared`
	that owns sparse RNG record structs, JSONL parse/write helpers, RNG-frame
	coalescing, and ordered metadata emission for `demo.json5`.
- The RNG coalescer now compresses contiguous frame-start states into `n` runs
	and only keeps `c` diagnostics when a frame actually provides a call count,
	which keeps control diffs and RNG diffs readable independently.
- Added `android/tests/test_input_demo_fixture.cpp` with exact-output checks for
	RNG coalescing, file round-trip, and stable metadata text ordering.
- Wired the new helper into the D1/D2 host-probe build path and the Android
	native D1/D2 source lists so later recorder hooks can call it without adding
	another format-only tranche.

## Validation

- `cmake --build buildd1 --target test_input_demo_fixture` passed.
- `cmake --build buildd2 --target test_input_demo_fixture` passed.
- `buildd1\maths\test_input_demo_fixture.exe` passed.
- `buildd2\maths\test_input_demo_fixture.exe` passed.
- `android\gradlew.bat :app:externalNativeBuildDebug --no-daemon` passed.
- `android\stop-stale-formatters.ps1` reported no stale formatter tasks.
- `android\run-code-quality.ps1 -Fix` passed.
- Rebuilt and reran the D1/D2 fixture probes plus the Android native build
	after formatting and after the packed-member warning fix.

## Exit Criteria

- Shared code can write schema-aligned `rng.p0.jsonl`
- Shared code can write stable ordered `demo.json5`
- Host tests verify exact sparse output for the new helper
- Android native build still passes with the new shared helper linked in
