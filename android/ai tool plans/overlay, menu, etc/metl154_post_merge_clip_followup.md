## Metl154 Post-Merge Clip Follow-up

### Goal
- analyze the newest highres-pack metl154 logs after the narrow merge-clip experiment
- identify the next root-cause candidate for the unchanged clipping-swapping behavior
- implement the next narrow fix or diagnostic step in D1 and D2, then validate it

### Plan
1. Review the newest metl154 debug log capture and compare it against the current merge-clip hypothesis.
2. Inspect the relevant D1 and D2 render paths implicated by the new evidence.
3. Implement the next narrow change in both D1 and D2.
4. Run Android validation and update the metl154 phase notes with the outcome.

### Status
- [x] New log reviewed
- [x] Root cause refined
- [x] D1 and D2 patched
- [x] Validation and notes updated

### Result
- New log `android\temp_game_logs\debuglog_20260414_102547.txt` shows the
	narrow merge-clip helper is affecting real metl154 faces. Some recurring
	sequences now stop after `[metl154face]` with no later draw-time logs,
	which means they are being culled before the OGL draw body
- The visible issue still persists on the surviving plain metl154 draws, so
	clipping and fan triangulation are not the whole story by themselves
- The same log also shows the highres upload path using
	`ETC2 upload: metl154 512x512 fmt=0x9278`, which weakens the theory that the
	highres pack is losing alpha by landing on an RGB-only compressed format
- The next narrow experiment is now implemented in both `d1/arch/ogl/ogl.c`
	and `d2/arch/ogl/ogl.c`: plain Android metl154 overlay draws log
	`[metl154wrap]`, read the live `GL_TEXTURE_WRAP_S/T` state, and force both
	axes back to `GL_REPEAT` if the actual state does not match the expected
	repeat wrap
- Validation passed with `android\run-code-quality.ps1 -Fix` and
	`android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
- Runtime retest is still blocked in this session because `adb devices`
	returned no attached target. The next required data point is a fresh device
	log from this build, especially whether `[metl154wrap]` ever reports
	`forced=1` on the still-bad faces