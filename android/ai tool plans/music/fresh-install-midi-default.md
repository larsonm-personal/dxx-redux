# Fresh install MIDI default

- Trace demo setup, launcher music selection, and native pilot defaults
- Align missing-preference defaults with MIDI while retaining explicit CD, files, and mission choices
- Remove the automation reset's explicit MIDI override so fresh-state tests exercise real defaults
- Switch the D2 download offer to the hosted PC demo and verify MIDI playback without a CD source

Findings: launch already defaults to MIDI without a pilot, but the Music page defaults to CD and native pilot reads without a music section default to CD. Retained device launch logs show built-in music. The Mac preview HOG has no MIDI tracks, explaining silence; the track controls hide when no track is playing. The PC demo HOG includes title, briefing, credits, end-level, end-game and three level tracks (HMP and HMQ). The hosted PC installer is d2demo10.zip, 4,306,833 bytes, already recognized by the importer.

The download integration exposed two existing importer issues: the original ZIP sets reserved bit 15 in local headers only, and its three SOW volumes continue files across volume boundaries. ZIP validation now ignores that one local reserved bit while retaining stream-flag checks. Known PC demo packages declare their ordered volumes; extraction appends them in one fresh staging directory and hashes complete files afterwards. Missing, additional, and duplicate volumes are rejected, and unrelated nested SOW files retain separate extraction and collision checks.

Regression coverage uses the actual default download button, an empty file set and reset preferences with no explicit music source. It checks MIDI catalog entries, nonzero synthesized audio in gameplay, and playback after relaunch. ZIP unit coverage checks stored/deflated entries with the reserved bit and rejection of mismatched stream flags.

The MIDI browser now falls back to d2demo.hog when descent2.hog is absent, exposing all eight demo tracks in the launcher.

Validation completed on emulator-5580: the 29-step download/playback regression passes, including fresh gameplay and relaunch audio. The four extracted files match the known PC demo hashes. Android native/APK builds, 37 focused JVM tests, scoped formatting and automation catalog checks pass. No changes were installed on the connected physical device.
