# PO2 and Phobos-E archive compatibility plan

## Goal

Import the original `po2.7z` and `phobos-e.7z` layouts without repacking, preserve the files needed by the missions, and expose all useful readmes through the launcher metadata viewer.

## Work

- [x] Inventory both archives, including the PO2 DOCX and nested `mod` assets.
- [x] Trace archive import, mission asset publication, metadata extraction, and readme viewer behavior.
- [x] Implement minimal support for the uncovered archive layouts and document formats.
- [x] Add focused automated coverage for DOCX, multiple readmes, and nested mod files.
- [x] Run scoped formatting, unit/integration tests, and real-archive verification.

## Result

- `po2readme.docx` is decoded and shown inside the launcher.
- Multiple readmes are retained and shown as individual buttons, including `phobinfo.txt` and `phobos-e.txt`.
- A matching D2X-XL `mods/<mission>/descent.sng` is published as the engine-facing root `descent.sng` while the source archive and nested files remain unchanged.
- PO2 and Phobos-E passed focused unit tests and emulator-backed imports from their original 7z files.
