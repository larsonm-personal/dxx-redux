# Mission travel time text investigation

## Plan

- [x] Locate the affected KCXF2RMv11 metadata and identify all producers of the two travel time fields
- [x] Trace the numeric calculation and text formatting paths, including current uncommitted work
- [x] Measure the mismatch across checked-in mission metadata
- [x] Record the root cause and proposed correction without changing source during this diagnostic request

## Fix

- [ ] Make the host time formatter truncate whole minutes
- [ ] Add focused regression coverage for sub-minute and multi-minute values
- [ ] Regenerate checked-in mission metadata with the corrected formatter
- [ ] Run the focused test and scoped code-quality checks
- [ ] Record final validation

## Notes

- Reported example: `travel_time_seconds` 108 paired with `travel_time_text` `2M:48S`
- The worktree already contains related uncommitted source and generated-metadata changes, so edits must preserve them
- The C++/JNI formatter uses integer division and correctly formats 108 seconds as `1M:48S`
- The host regeneration helper casts the floating-point quotient with `[int]($Seconds / 60)`, which rounds in PowerShell; 108 / 60 becomes 1.8 and then 2
- The current worktree contains 509 mismatches among 1,274 formatted level records across 97 files; KCXF2RMv11 levels 2, 5, and 6 are affected
- Proposed source correction: explicitly floor or otherwise truncate the minute quotient in `Format-LevelTime`, then regenerate metadata and add a boundary-focused regression check
