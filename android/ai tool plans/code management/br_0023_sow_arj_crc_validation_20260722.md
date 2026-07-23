# BR-0023 SOW ARJ integrity validation

## Goal

Reject SOW entries whose ARJ basic or extended headers, stored payloads, or
decompressed payloads fail their declared CRC, and hard-fail compressed input
exhaustion before publishing output

## Plan

- [x] Read repository instructions and the complete BR-0023 finding
- [x] Trace ARJ header layout, decoder input handling, extraction publication,
  and existing native test registration
- [x] Implement checked header and payload CRC validation plus hard compressed
  input exhaustion
- [x] Add focused valid, corrupted, and truncated ARJ regression fixtures
- [x] Run scoped code quality, the native extraction suite, and relevant build
  verification
- [x] Update the finding disposition and validation record

## Validation record

- Scoped code quality passed for the SOW decoder, integrity test, CMake file,
  and this plan
- All 12 registered native extraction suites passed
- Android debug native builds passed for arm64-v8a, armeabi-v7a, and x86_64
- Valid stored and compressed fixtures passed; basic-header, extended-header,
  stored-payload, compressed-payload, and truncation cases failed without output
- The Test Flight `descent2.sow` extracted 19 files totaling 3,172,830 bytes
