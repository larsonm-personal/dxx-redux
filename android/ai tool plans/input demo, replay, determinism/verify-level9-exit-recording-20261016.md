# Verify the level 9 touch-menu exit recording

1. Check the supplied recording, RNG sidecar, classic demo, and game log for
   complete finalization after the touch overlay Exit action
2. Copy the recording and companions into repository-local scratch space and
   verify the recorded asset fingerprints against local retail data
3. Run a fresh native D2 replay with strict expected-result comparison and an
   RNG trace, then investigate any mismatch using the first divergent evidence
4. Report recording completeness separately from deterministic replay results

Initial evidence:

- Build 26730 recorded 7,372 frames (123.899 seconds of simulation)
- The file has an input tail and terminal result, with per-frame state disabled
- RNG metadata declares 86,598 events and truncated=false; all events are present
- The log reports successful saves of all three companions at 07:48:11

Verification evidence:

- The supplied files were copied to `temp/level9_exit_verification_20261016`;
  SHA-256 checks confirm the copies match the unchanged originals
- Retail HOG/PIG/HAM fingerprint preflight passed
- The console backend rejected its loaded asset fingerprints at startup, so it
  did not verify the terminal result
- The normal Windows graphical backend completed all 7,372 frames and its engine
  reported `Input demo replay result matched embedded trailer`
- Independently comparing every terminal JSON field found an exact match,
  including position/orientation, ammo, health, score, clock and level totals
- The official wrapper exited 0 with `RESULT: PASS` and `RNG trace compare: PASS`
  using strict comparison and a clean native exit
- All 72,604 simulation RNG events match after ignoring diagnostic source line
  and global sequence fields; both original RNG streams have continuous call
  counts and state transitions
- Supplemental effects RNG differs between platforms and is excluded by the
  repository comparer; no recorded per-frame state was available to compare
- An initial graphical run with unnecessary detailed state output was stopped;
  the completed run verifies the terminal result and RNG without that output

Artifacts: `temp/level9_exit_verification_20261016/verification.json`,
`visual.actual_result.json`, `visual.actual_rngtrace.jsonl`, and
`visual-result-replay.log`
