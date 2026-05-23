# Input Demo Codec Dependency Swap Plan 2026 04 29

## Goal

Replace the hand-written SHA256 and base64 code in `input_demo_codec.cpp`
with pinned external dependencies fetched by CMake.

## Decision

- use `okdshin/PicoSHA2` for SHA256
- use `renenyffenegger/cpp-base64` for base64
- pin both to exact git commits in CMake download logic
- keep the existing `input_demo_codec.h/.cpp` API so recorder and replay stay
  unchanged outside the shared codec layer

## Phases

- [ ] Add shared CMake dependency helper for the pinned codec dependencies
- [ ] Wire Android and host input-demo targets to the helper targets
- [ ] Replace `input_demo_codec.cpp` internals with PicoSHA2 and cpp-base64
- [ ] Run focused build and recorder/replay host validation
- [ ] Mark this plan complete