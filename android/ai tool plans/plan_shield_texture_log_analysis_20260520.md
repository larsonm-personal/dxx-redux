## Goal

Analyze the Shield texture debug log for D2 level 1 startup plus brief in-level play and identify likely performance problems or misleading signals.

## Status

- [x] Read the opening section of the provided texture log
- [x] Read the tail and any summary or repeated hot-path markers
- [x] Separate real load costs from debug-log overhead
- [x] Summarize the likely issues and next instrumentation or fixes

## Notes

- The provided capture is texture-category only, so any conclusions about runtime frame performance will be partial unless the log includes the new aggregate timing lines
- Early lines are dominated by per-texture stock mask diagnostics, which may themselves be part of the performance problem if enabled on-device

## Findings

- The log has 54701 lines total
- `Stock mask check` accounts for 2804 lines
- merged-wall debug lines account for 51892 lines, broken down as:
	- `mwall_track`: 20014
	- `mwall_cover`: 19902
	- `mwall_coverbox`: 7569
	- `mwall_portal`: 4406
- The merged-wall phase spans about 49.4 seconds and covers 1069 logged frames, which implies about 21.6 logged frames per second and about 48.5 merged-wall debug lines per frame while that diagnostics path is active
- The expected Android cache summary line `cache profile: ...` does not appear in the capture even though the current source has it, so this log cannot answer whether KTX2 read, PNG read, upload, or mask work is the main startup cost

## Conclusion

- This capture is not a clean measurement of baseline Shield graphics cost
- It strongly suggests the enabled debug logging is itself a major performance problem on Shield, especially because each native debug line crosses JNI and the Kotlin sink flushes the file on every write
- The strongest actionable issue exposed by this log is high-volume merged-wall debug logging during gameplay, with stock-mask per-texture logging as the secondary startup cost