# Coop level 7 ten round log review

## Goal
- Review the new coop level 7 texture diagnostics log
- Use the expanded CPU merge, GL readback, and draw-state records to identify the next cause or fix
- Update findings without changing code unless the log points to a small clear correction

## Plan
- [done] Re-read project instructions and extract key records from the new log
- [done] Compare CPU merge, GL readback, draw state, and framebuffer evidence
- [done] Identify likely cause or next implementation step
- [done] Record findings and validation status

## Findings
- The new level 7 coop log confirms the CPU legacy texmerge result is correct for the tracked face: `ref_hash=0x2820c6f1`, `live_hash=0x2820c6f1`, `mismatches=0`.
- GL readback also confirms the base, overlay, and merged texture contents match expected data, including the drawn merged texture handle.
- The remaining bad state is at draw time: the old-texmerge single-texture path draws the merged texture on unit 0 while texture units 1 and 2 remain bound to stale textures.
- The clear policy only recognized Android cached merged-wall bitmaps. Legacy `texmerge_get_cached_bitmap` results were therefore not clearing units 1 and 2.

## Fix
- Updated the shared single-texture clear predicate so any valid face draw with `tmap2 != 0` clears secondary texture units, covering both Android cached merges and legacy old texmerge bitmaps.

## Validation
- `.\android\run-code-quality.ps1 -Fix -Paths @('android\app\src\main\cpp\shared\merged_wall_debug.c')`
- `.\gradlew.bat :app:externalNativeBuildDebug`
