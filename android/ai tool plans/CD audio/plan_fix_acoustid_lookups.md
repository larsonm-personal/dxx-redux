# Plan: Fix AcoustID Lookups

## Problem

AcoustID lookups return 0 matches across the project, but MusicBrainz Picard
(using the same AcoustID database) successfully identifies the same tracks.

## Root cause analysis

Verified manually:
- fpcalc fingerprint on "Cold Reality (Extended Remix)" -> AcoustID score=0.999
- Our fingerprint on the same track -> AcoustID score=0.999
- Both match recording 51a0a575-4364-4fc1-927b-b516b76cf778 (confirmed by user)

The fingerprint generation is correct. The lookup code has two issues:

1. **Pipeline never ran with AcoustID after fingerprint fix**: All pipeline runs
   after the stereo bug fix used `-SkipAcoustId`. Pre-fix runs had broken
   fingerprints/durations that legitimately produced 0 matches.

2. **Invoke-WebRequest portability**: The lookup function uses Invoke-WebRequest
   which has quirks across PowerShell versions and terminal environments.
   Switching to System.Net.WebClient is more reliable and testable.

3. **Test script coverage gap**: test_fpcalc_and_acoustid.ps1 tested only tracks
   from "D2 redbook mp3 rips" starting with track 01 "Base Return" which
   genuinely has no AcoustID entries. Should test tracks known to be in AcoustID.

## Fixes

### Phase 1: Switch Invoke-AcoustIdLookup to use System.Net.WebClient
File: `game_data/fingerprint_music_packs.ps1`

### Phase 2: Update test script
File: `android/tests/test_fpcalc_and_acoustid.ps1`
- Use System.Net.WebClient instead of Invoke-WebRequest for AcoustID calls
- Test known-good album "D2 infinite abyss redbook mp3" instead of "D2 redbook mp3 rips"

### Phase 3: Re-run pipeline with AcoustID enabled [DONE]
- `fingerprint_music_packs.ps1 -SkipExtract` (skip extraction, just lookup)
- `update_known_discs_albums.ps1 -Force` (merge with acoustid names)

Results: 248/760 tracks matched (was 0/760)

Album breakdown:
  54/80  D1 MIDI mp3 sc55
  34/34  D1 playstation mp3
  30/30  D2 macplay mp3
  26/46  D2 mp3
  26/26  D1 macplay mp3
  24/28  D2 redbook mp3 rips
  22/30  Descent Maximum (ps1) mp3
  16/16  D2 infinite abyss redbook mp3
  14/14  D2 vertigo mp3
  2/6    D2 vampyro mp3
  0/xx   All MIDI synth renders (not in AcoustID DB -- expected)

### Phase 4: Validate [DONE]
- test_fpcalc_and_acoustid.ps1: 3/3 tests pass (fpcalc comparison + AcoustID lookup)
- Code quality linters: all pass
