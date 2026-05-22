# Expanded CD image support plan

## Status

- [x] Confirm the launcher import entrypoints that currently block PC-supported image variants
- [x] Expand picked-file and setup-command support beyond `.bin`-only sidecars
- [x] Preserve correct track-file routing for multi-FILE CUE imports
- [x] Run focused Kotlin validation on the touched launcher/test-helper slice

## Notes

- Scope: launcher-side image import widening only, without changing the shared native extractor core
- First targets: CUE + `.img` sidecars and multi-track-file `import_cd` plumbing

## Completed

- Picked-file import now treats `.img` as a valid CUE sidecar alongside `.bin`
- The SAF disc dialog now orders selected sidecar files by the CUE `FILE` entries before parsing, extraction, and audio registration
- `import_cd` now accepts either legacy `bin_path` or multi-file `bin_paths`, and path-based disc import no longer requires `dataTrack.fileIndex == 0`
- Local multi-file audio sources now keep all referenced image paths instead of collapsing to a single file assumption
- The `add_audio_source` setup command now accepts `bin_paths` and preserves multi-image ordering from the CUE when available
- `android/tests/test_extract.ps1` direct `setup_cd` staging now supports `.img` and multi-image CUEs by staging every CUE-referenced image under its referenced relative path instead of forcing a single `source.bin`
- Launcher import helper copy now advertises `.cue` images with `.bin/.img` tracks instead of only `.cue/.bin`
- Disc import polish now reports missing CUE-referenced image files with a clearer selection hint, summarizes parsed track/image counts, and logs when extra selected image files are being ignored
- The in-app Redbook help text and disc-import dialog title now describe image sets rather than only BIN/CUE wording