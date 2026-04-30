# Input Demo Header, Result, and Checkpoint Compression Plan 2026 04 29

## Goal

Prepare the `.dximdemo` format for the next recording pass by making the human
facing metadata and trailer easier to read and by shrinking embedded checkpoint
saves without leaving JSON.

This tranche covers three linked changes:

- make the `result` record use descriptive JSON keys instead of sparse short keys
- add build provenance fields to the `header`
- compress `save_checkpoint` payloads before the outer base64 wrapping

## Status

- [done] Phase 1
  - `input_demo_metadata` now carries `build_number`, `git_version`, and `arch`
  - header schema validation and serialization now require the descriptive build fields
- [done] Phase 2
  - `result` JSON now uses descriptive key names throughout and dense ammo arrays
  - result schema version is now `2`
- [done] Phase 3
  - build metadata is now provided by a shared CMake helper that derives build number from
    `git rev-list --count HEAD * 10` and git version from `git rev-parse --short HEAD`
  - the recorder and host input demo tests consume the same `DXX_INPUT_DEMO_*` compile definitions
- [done] Phase 4
  - checkpoint records now include `compression`
  - writer uses `zlib` only when it makes the payload smaller, otherwise emits `none`
  - replay inflates `zlib` checkpoints before validating raw `size` and `sha256`
- [done] Phase 5
  - focused host tests were updated for the new schema
  - replay coverage now exercises the compressed checkpoint path
- [done] Validation
  - `run-windows-build.ps1 -Target both`
  - `buildd1\maths\test_input_demo_result.exe`
  - `buildd1\maths\test_input_demo_fixture.exe`
  - `buildd1\maths\test_input_demo_recorder.exe`
  - `buildd1\maths\test_input_demo_replay.exe`
  - `buildd2\maths\test_input_demo_result.exe`
  - `buildd2\maths\test_input_demo_fixture.exe`
  - `buildd2\maths\test_input_demo_recorder.exe`
  - `buildd2\maths\test_input_demo_replay.exe`
  - `android\run-code-quality.ps1 -Fix` on the touched Android C++ files and tests

## Existing Compression Plan Anchor

The existing checkpoint compression note already lives in:

- `android/ai tool plans/plan_input_demo_mid_level_start.md`

The relevant cleanup item there says the embedded `DGSS` payload is currently raw
base64 and wastes substantial space, and suggests evaluating zlib while keeping
checksum validation on the decompressed bytes.

This new plan keeps that intent, but folds it into the same implementation pass
as the header and result schema cleanup so the next demo rerecord only happens
once.

## Concrete Anchors

- `android/app/src/main/cpp/shared/input_demo_result.cpp`
  - current parser and writer still use sparse abbreviated keys such as `v`, `g`,
    `m`, `p0`, `pos`, `lv`, `e`, `sc`, `pa`, `sa`
- `android/app/src/main/cpp/shared/input_demo_fixture.h`
  - `input_demo_metadata` currently has no build provenance fields
- `android/app/src/main/cpp/shared/input_demo_fixture.cpp`
  - header parsing and writing is strict and rejects unknown keys
  - checkpoint validation currently requires `encoding == "base64"`
- `android/app/src/main/cpp/shared/input_demo_recorder.h`
  - recorder settings currently do not carry build metadata
- `android/app/src/main/cpp/shared/input_demo_recorder.cpp`
  - `input_demo_recorder_build_demo()` is the single place that fills header
    metadata before serialization
- `android/app/src/main/java/com/dxxredux/app/BuildInfo.kt`
  - Android already has `GIT_COMMIT_COUNT`, `GIT_SHORT_HASH`, `BUILD_DATE`, and
    `BUILD_TIME`
- `d1/main/vers_id.h`, `d2/main/vers_id.h`, `d1/main/vers_id.c`, `d2/main/vers_id.c`
  - native code already has `DESCENT_VERSION` and `g_descent_build_datetime`
- `d1/maths/CMakeLists.txt` and `d2/maths/CMakeLists.txt`
  - desktop input demo tests currently compile the shared fixture code without any
    explicit zlib link

## Format Decisions

- Bump the input demo schema from `1` to `2` in one pass
  - all three requested changes alter the on disk contract
  - the parser is strict and this format is still pre release, so one clean
    schema bump is simpler than teaching every parser to accept both old and new
    keys
- Keep the header fully descriptive, matching the current style
- Make the `result` record fully descriptive and non sparse
  - top level keys should become names like `version`, `game`, `mission`,
    `level`, `difficulty`, `frame_count`, `game_time64`, `player0`, `position`,
    `level_summary`
  - nested keys should also become descriptive names like `energy`, `shields`,
    `score`, `laser_level`, `primary_ammo`, `secondary_ammo`, `robots_alive`,
    `control_center_destroyed`
  - result emission should stop omitting fields just to save a few bytes, since
    the record appears once per file
- Keep checkpoint JSON valid and base64 wrapped
  - continue to base64 encode the final binary payload that lives in `data`
  - add an explicit compression field rather than overloading `format`
  - keep `size` and `sha256` defined over the decompressed raw `DGSS` bytes
- Keep build metadata plumbing out of the fixture layer itself
  - the serializer should only read values already placed into metadata
  - build discovery should happen at recorder setup time, then flow through
    `input_demo_recorder_settings`

## Planned Schema Shape

### Header

Add these fields to `input_demo_metadata` and the header JSON:

- `build_number: int`
- `git_version: string`
- `arch: string`

Assumption for now:

- the request text mentions date, but only explicitly names the three fields
  above, so this plan keeps the serialized schema scoped to those three fields
  unless the next implementation pass decides to add a separate `build_date`

### Result

Replace the sparse abbreviated object shape with a descriptive one.

Planned direction:

- descriptive key names at every level
- dense emission for stable scalar fields
- dense ammo representation if it does not introduce an awkward parser surface
  nearby; an array is preferred over sparse numbered maps if there is no hidden
  consumer that needs the old sparse object form

### Checkpoint Record

Keep the current outer shape, but add compression metadata.

Planned direction:

```json
{
  "type": "checkpoint",
  "format": "dgss",
  "encoding": "base64",
  "compression": "zlib",
  "size": 123456,
  "sha256": "...",
  "save_name": "inputdemo_start.dgss",
  "start_gt": 124125,
  "next_laser_fire_delta": 0,
  "next_missile_fire_delta": 0,
  "last_laser_fired_delta": 0,
  "auto_fire_fusion_delta": 0,
  "data": "..."
}
```

Decode rules:

- base64 decode `data`
- if `compression == "zlib"`, inflate to the raw `DGSS` bytes
- validate decompressed byte count against `size`
- validate decompressed bytes against `sha256`
- write the decompressed `DGSS` bytes to the temp save file and continue through
  the existing restore path

## Implementation Plan

## Phase 1

- Extend `input_demo_metadata` with `build_number`, `git_version`, and `arch`
- Extend `input_demo_recorder_settings` with the same fields so tests can inject
  deterministic values and the fixture layer stays pure
- Update `input_demo_metadata_parse_header_line()` and
  `input_demo_metadata_to_header_line()` to parse and emit the new keys
- Bump metadata validation from schema `1` to schema `2`

## Phase 2

- Rewrite `input_demo_result.cpp` allowlists, parser code, and writer code to use
  descriptive keys throughout
- Decide dense ammo output at the same time so the result schema only changes once
- Update result comparison error labels to match the descriptive field names
- Bump `input_demo_result.version` to `2`

## Phase 3

- Add a small shared build info helper or per platform recorder setup helper that
  produces:
  - `build_number`
  - `git_version`
  - `arch`
- Prefer filling these in D1 and D2 recorder setup code before calling
  `input_demo_recorder_start()`, instead of teaching the serializer about Android
  `BuildInfo` or host specific version globals
- Android source candidates:
  - `BuildInfo.GIT_COMMIT_COUNT`
  - `BuildInfo.GIT_SHORT_HASH`
  - compile time arch macros
- Host source candidates:
  - existing native version globals for stable version and build date context
  - new compile definitions or generated constants for build number and git hash
  - compile time arch macros

## Phase 4

- Add `compression` support to `input_demo_checkpoint`
- Compress checkpoint bytes with zlib before base64 encoding when writing
  `save_checkpoint` demos
- Inflate after base64 decode during replay restore
- Keep checksum and size validation on decompressed raw bytes
- Add decompression guardrails so malformed demos cannot request unreasonable
  output sizes during inflate

## Phase 5

- Update the focused input demo tests to the new schema and add compression
  coverage:
  - `android/tests/test_input_demo_fixture.cpp`
  - `android/tests/test_input_demo_result.cpp`
  - `android/tests/test_input_demo_recorder.cpp`
  - `android/tests/test_input_demo_replay.cpp`
- Add at least one round trip checkpoint compression test that proves raw save
  bytes survive compress -> base64 -> parse -> inflate -> compare

## Build and Link Follow Up

- Wire zlib into every target that directly compiles the checkpoint helper code
- Expect this to include at least:
  - desktop test targets in `d1/maths/CMakeLists.txt`
  - desktop test targets in `d2/maths/CMakeLists.txt`
  - Android shared targets that compile `input_demo_fixture.cpp`
- Prefer reusing an existing `ZLIB::ZLIB` style target where possible instead of
  adding a second compression dependency

## Validation Plan

- `run-windows-build.ps1 -Target both`
- `buildd1\maths\test_input_demo_fixture.exe`
- `buildd1\maths\test_input_demo_result.exe`
- `buildd1\maths\test_input_demo_recorder.exe`
- `buildd1\maths\test_input_demo_replay.exe`
- `buildd2\maths\test_input_demo_fixture.exe`
- `buildd2\maths\test_input_demo_result.exe`
- `buildd2\maths\test_input_demo_recorder.exe`
- `buildd2\maths\test_input_demo_replay.exe`
- If Android specific build info plumbing changes Kotlin or Gradle inputs, run:
  - `android\stop-stale-formatters.ps1`
  - `android\stop-stale-formatters.ps1 -Kill` if needed
  - `android\run-code-quality.ps1 --fix`

## Exit Criteria

- Header carries build provenance using the requested field names
- Result JSON is descriptive enough to read without a key legend
- `save_checkpoint` demos stay valid JSON and are materially smaller than raw
  base64 for typical DGSS payloads
- Desktop focused tests pass in both D1 and D2 before rerecording demos