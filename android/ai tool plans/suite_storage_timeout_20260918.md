# Suite storage and timeout failures

- [x] Give the mission metadata integration owner a budget covering both child scenarios
- [x] Release its large staged mission fixtures and generated publications on success or failure
- [x] Run the metadata owner followed by the merged-wall probe, then scoped lint

The report shows a 120-second outer timeout for the metadata owner and ENOSPC during launch asset preparation in the later merged-wall probe

## Fix and validation

- Set the suite owner budget to 900 seconds for its 360-second in-game scenario, 300-second preview, staging, and cleanup. Confirmed the suite selected this budget with an exact test filter
- Release the 408,845,824-byte Enemy Within archive, test mod manifest, generated mission publications, and generated mount references in `finally` before preview. Clearing mount references is necessary because preview metadata workers read them before a new game launch
- Final metadata owner run passed Enemy Within guidance (27 steps), Maximum S5 preview, and preview cleanup
- Immediately following it, merged-wall probe passed all 41 steps
- Emulator had about 1.2 GB free after fixture cleanup; the staged Enemy Within archive was absent
- APK build, scoped code quality checks, and `git diff --check` passed
- Logs: `temp/suite_storage_20260918/metadata_fixed.log` and `temp/suite_storage_20260918/wall_probe.log`
- Full suite not run
