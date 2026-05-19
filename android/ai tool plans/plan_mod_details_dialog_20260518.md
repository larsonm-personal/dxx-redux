# Mod details dialog plan

## Goal
- Add a concise mod details box from the Android mod manager list
- Show file counts, categorized payloads, patch metadata, expected base files, and detected problems
- Keep the design consistent with existing launcher UI and mod preflight/conflict behavior

## Tasks
- [x] Trace current Mods UI, manifest model, and preflight/conflict code
- [x] Identify existing file type/category metadata and scroll indicator patterns
- [x] Design a compact detail model and dialog layout
- [x] Implement a focused slice if enough context is clear
- [x] Run targeted Kotlin/build or test validation