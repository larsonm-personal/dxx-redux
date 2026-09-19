# Regression data stage failures

- [x] Fix mission soundtrack archive accounting for the levelpack collection
- [x] Diagnose and fix D1 import launch SIGFPE
- [x] Fix extraction-test storage cleanup and verify failed CD cases
- [x] Run focused regression checks and scoped formatting

Preserve existing uncommitted code and generated regression data changes

## Findings and fixes

- `levelpack.7z` contains 918 files, but soundtrack inspection also counts every nested HOG member. Increase only the aggregate traversal budget from 4096 to 65536; retain per-extraction file/byte/time limits. Its real audio track now fingerprints successfully
- Android mission switching closes D1 game data, but the legacy table reader retained its `Installed` flag and parser counters. Add an Android reset before reinitialization, including the multiplayer ship texture sentinel
- Extraction sanitization cleared imported sets but left about 1.3 GiB of generated mission publications. Clear those publications and mount references while the app is stopped. Unexpected staging errors now also remove partial import scratch
- The device also contained other downloaded test sources; those were left intact
- After fixing storage, the European D2 disc exposed identical `README.TXT`/`readme.txt` resources. Collapse case variants only when fingerprints and ownership sets match; preserve exact descriptor spelling and continue rejecting differing content or ownership

## Validation

- APK build passed
- Real levelpack soundtrack extraction/fingerprinting passed without external AcoustID lookup
- Archive budget, CD runner, and extraction workflow tests passed
- Scoped formatting/lint passed
- Mac disc import and game launch passed
- All three legacy D1 discs passed full extraction and reached Lunar Outpost; the US disc needed one existing infrastructure retry for an ADB read timeout
- Mission launch catalog/publication tests passed (4 tests), including identical case variants and rejection of different contents or ownership
- Both large D2 discs passed extraction and reached Ahayweh Gate with the final launcher build
- All six previously failing CD cases passed. The complete multi-stage regeneration and full test suite were not repeated
- Logs: `temp/regeneration_fix_20260919/`
