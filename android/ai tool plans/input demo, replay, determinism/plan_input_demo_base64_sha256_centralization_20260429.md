# Input Demo Base64 and SHA256 Centralization Plan 2026 04 29

## Goal

Remove the duplicated input-demo base64 and SHA256 implementations from
`input_demo_recorder.cpp` and `input_demo_replay.cpp`.

## Decision

- use a shared input-demo utility file for now
- do not pull a new external crypto dependency into the build just for this path
- reason: there is no existing cross-platform digest dependency already wired into
  the input-demo host and Android targets, so a new dependency would be more
  invasive than the deduplication requested here

## Status

- [done] Add shared utility declarations and implementation for base64 encode,
  base64 decode, and SHA256 hex
- [done] Switch recorder and replay to the shared utility
- [done] Wire the new utility source into Android and host input-demo targets
- [done] Run focused host input-demo tests
- [done] Mark this plan complete

## Implemented Files

- `android/app/src/main/cpp/shared/input_demo_codec.h`
- `android/app/src/main/cpp/shared/input_demo_codec.cpp`
- `android/app/src/main/cpp/shared/input_demo_recorder.cpp`
- `android/app/src/main/cpp/shared/input_demo_replay.cpp`
- `android/app/src/main/cpp/CMakeLists.txt`
- `d1/main/CMakeLists.txt`
- `d2/main/CMakeLists.txt`
- `d1/maths/CMakeLists.txt`
- `d2/maths/CMakeLists.txt`

## Validation

- `run-windows-build.ps1 -Target both`
- `buildd1\maths\test_input_demo_recorder.exe`
- `buildd1\maths\test_input_demo_replay.exe`
- `buildd2\maths\test_input_demo_recorder.exe`
- `buildd2\maths\test_input_demo_replay.exe`
- `android\run-code-quality.ps1 -Fix -Paths @('android/app/src/main/cpp/shared/input_demo_codec.h','android/app/src/main/cpp/shared/input_demo_codec.cpp','android/app/src/main/cpp/shared/input_demo_recorder.cpp','android/app/src/main/cpp/shared/input_demo_replay.cpp')`

## Phases

- [done] Add shared utility declarations and implementation for base64 encode,
  base64 decode, and SHA256 hex
- [done] Switch recorder and replay to the shared utility
- [done] Wire the new utility source into Android and host input-demo targets
- [done] Run focused host input-demo tests
- [done] Mark this plan complete