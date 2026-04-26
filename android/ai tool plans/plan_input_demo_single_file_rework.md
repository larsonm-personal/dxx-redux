# Input Demo Single File Rework Plan

## Goal

Replace the current multi-file input-demo fixture layout with one canonical
demo file that contains metadata, interleaved per-frame input and RNG data, and
trailer result data

## Requirements

- New demos are single `.dximdemo` files, not directories or archive wrappers
- Per-frame input and RNG data are interleaved in frame order
- Header data lives at the beginning of the file
- Trailer data lives at the end of the file
- Existing code, tests, scripts, and plan docs move to the single-file shape
- Keep D1 and D2 source edits narrow and mirrored

## Initial Phases

- [x] Inventory current multi-file assumptions in code, tests, and plans
- [x] Define the single-file JSON schema and update schema docs
- [x] Update shared fixture writer, reader, recorder, and replay loader
- [x] Update host/unit/smoke tests and scripts
- [x] Update device-recording and regression plans
- [x] Run targeted validation and record results

## Chosen File Shape

Use `.dximdemo` as the canonical demo file extension. The file is newline-delimited
JSON, parsed with `nlohmann::json`. It is not a zip file and it is not a
directory wrapper around older fixture files.

Record order is strict:

1. First non-empty line: `{"type":"header",...}`
2. One `{"type":"frame",...}` line for every frame from `0` to
     `frame_count - 1`
3. Last non-empty line: `{"type":"result",...}`

Frame records carry input and RNG together so correlation never depends on
matching separate files. The writer may omit unchanged `ft` values after frame 0,
and input uses the existing sparse held-state and pulse key objects, but every
frame line must include its own RNG state object.

Example:

```jsonl
{"type":"header","version":1,"game":"d2","mission":"d2","level":1,"difficulty":2,"start_mode":"new_level","rng_mode":"lcg_state","frame_count":3}
{"type":"frame","f":0,"ft":3276,"input":{"s":{"f":44}},"rng":{"s":100}}
{"type":"frame","f":1,"input":{"p":{"f1":1}},"rng":{"s":100}}
{"type":"frame","f":2,"input":{"s":{"f":0}},"rng":{"s":102,"c":3}}
{"type":"result","result":{"v":1,"g":"d2","m":"d2","l":1,"d":2,"fr":3}}
```

## Inventory Notes

- Shared metadata, recorder, and replay code still had separate metadata,
    input, RNG, and result file assumptions at the start of this tranche
- Unit tests and the Windows smoke runner create the four-file fixture shape
- D1 and D2 CLI help and local variable names referred to the old metadata path
- Device-recording plans describe `.dximdemo` as a zip or directory, which is
    now superseded by this native single-file format

## Completed Notes

- Shared fixture code now reads and writes one strict newline-delimited JSON
    `.dximdemo` file with header, frame, and result records
- The recorder flush path now writes one demo file instead of separate metadata,
    input, RNG, and result files
- Replay now loads the same demo file, keeps the embedded result trailer as the
    expected baseline, and writes `<demo-file>.actual.json` for diagnostics
- D1 and D2 command-line help, replay startup, replay result comparison, and
    recording flush paths use demo-file terminology
- Host probes and the Windows smoke runner now create and replay `.dximdemo`
    files directly
- Plan docs now treat `.dximdemo` as the artifact itself, not a zip or directory
    wrapper

## Final Validation Results

- `android\stop-stale-formatters.ps1`: no stale formatter tasks found
- `android\run-code-quality.ps1 -Fix`: passed
- D1 and D2 focused host probes passed after the final formatting pass:
  `test_input_demo_fixture`, `test_input_demo_recorder`,
  `test_input_demo_replay`, `test_input_demo_result`, and
  `test_input_demo_rng_mode`
- `run-windows-build.ps1 -Target d1`: passed after the final formatting pass
- `run-windows-build.ps1 -Target d2`: passed after the final formatting pass
- From `android`, `gradlew.bat :app:externalNativeBuildDebug --no-daemon`
    with `JAVA_HOME=c:\local\jdk-21`: passed
- `android\tests\test_input_demo_runtime_smoke.ps1 -Game both`: D1 and D2
    passed after the final formatting pass and matched the embedded trailer