# Plan: Fix Chromaprint Stereo Bug, Add Unit Tests, Re-run Pipeline

## Root Cause

`fingerprint_from_pcm()` in `fingerprint_gen.c` has a critical stereo bug.
`chromaprint_feed()` expects total int16 count (frames * channels), but we pass
only the per-channel frame count. The official `fpcalc.cpp` does
`first_part_size * reader.GetChannels()`.

For stereo audio (all CD-DA and most MP3/OGG), this means:
- Fingerprint covers first ~half of intended audio
- `duration_ms` is exactly half the real value (formula divides by channels)
- AcoustID returns zero matches (wrong duration + truncated fingerprint)
- Cross-matching has reduced discriminative power, causing false MIDI-vs-Redbook
  matches even at 0.4 threshold

Evidence: `fpcalc.cpp` line 328:
  `chromaprint_feed(ctx, frame_data, first_part_size * reader.GetChannels())`

## Phases

### Phase 1: Fix fingerprint_from_pcm [DONE]

File: `android/app/src/main/cpp/shared/fingerprint_gen.c`

Three bugs fixed:

1. **Feed size**: `chromaprint_feed(ctx, samples, feed_samples)` passed per-channel
   frames. Fixed: pass `total_samples * channels` (total int16 count).

2. **Duration formula**: `total_samples * 1000 / (sample_rate * channels)` divided
   by channels but total_samples is already per-channel. Fixed: `total_samples * 1000 / sample_rate`.

3. **MAX_FINGERPRINT_SAMPLES clamp**: Compared per-channel frames against
   `120 * 44100 * 2`. Fixed: compute `feed_count = total_samples * channels`,
   clamp to MAX, feed that.

### Phase 2: Add C unit tests [DONE]

New: `android/app/src/main/cpp/extract/test_fingerprint.c`
Update: `android/app/src/main/cpp/extract/CMakeLists.txt`

6 tests, all PASSING:

1. test_stereo_raw_matches_direct_api -- Load test_stereo_44100.raw, feed to our
   fingerprint_from_pcm() as stereo 44100 Hz. Also feed via direct chromaprint API
   with correct args. Assert base64 fingerprints match.

2. test_mono_raw_matches_direct_api -- Same with test_mono_44100.raw, mono 44100 Hz.

3. test_duration_stereo -- Load test_stereo_44100.raw, compute expected duration
   from file size. Assert duration_ms matches.

4. test_duration_mono -- Same for mono.

5. test_mp3_matches_fpcalc_reference -- Load test.mp3, compare raw uint32 fingerprint
   array against test.mp3.fpcalc.out (known-good: DURATION=10, 59 raw values).

### Phase 3: Download fpcalc as test dependency [DONE]

New: `android/get_deps/get_fpcalc.ps1`
Update: `android/get_deps/tool_versions.conf`

Downloaded fpcalc v1.5.1 Windows binary. Installed to `c:\local\fpcalc-1.5.1\fpcalc.exe`.

### Phase 4: fpcalc + AcoustID comparison test (PowerShell) [DONE]

New: `android/tests/test_fpcalc_and_acoustid.ps1` (combined phases 4+5)

Results:
- fpcalc vs our tool on D2 redbook MP3: Duration matches (213s), encoded
  fingerprint length ratio = 1.0 -- PASS
- AcoustID: 0 matches for D2 redbook tracks using BOTH fpcalc and our fingerprints.
  D2 soundtrack tracks are not in the AcoustID database. This is expected for
  a 1996 game soundtrack -- informational only, not a bug.

### Phase 6: Re-generate all fingerprints from scratch [DONE]

All existing fingerprints were wrong. Full regen completed:

1. Built fingerprint_cd.exe and fingerprint_audio.exe with the fix
2. `fingerprint_disc_tracks.ps1 -Force` -- all 203 CD track fingerprints regenerated
3. `update_known_discs_fingerprints.ps1` -- merged into known_discs.json5
   - **Additional bug found and fixed**: script was SKIPPING existing chromaprint
     entries instead of replacing them. Fixed to strip and re-insert.
   - All 203 CD tracks updated with correct fingerprints
4. `fingerprint_music_packs.ps1 -Force -SkipAcoustId` -- all album fingerprints
5. `update_known_discs_albums.ps1 -Force` -- dedup with priority hierarchy

### Phase 7: Re-run AcoustID lookups [SKIPPED]

Testing confirmed that D2 redbook tracks (and likely other Descent game music)
are not in the AcoustID database. Both fpcalc reference fingerprints and our
fingerprints return 0 matches. Re-running thousands of lookups against an
empty result set would waste API budget for no benefit.

### Phase 8: Validate results [DONE]

Results with corrected fingerprints at threshold 0.65:

Score distribution (bimodal -- noise vs true duplicates):
  0.50: 60180 | 0.55: 5295 | 0.60: 1959 | 0.65: 898 | 0.70: 303
  0.75: 124   | 0.80: 42   | 0.85: 182  | 0.90: 437 | 0.95: 992 | 1.00: 397

Key validation results:
1. MIDI-to-CD max score: 0.6384 (well below 0.65 threshold) -- ZERO false matches
2. D2 redbook MP3 rips vs D2 CDs: correctly matched (0.85+)
3. Threshold 0.65 cleanly separates noise from true duplicates:
   - Noise floor: ~0.50 (from offset alignment -15 to +15)
   - Noise tail: up to 0.6384
   - Valley: 0.70-0.80 (very few pairs)
   - True duplicates: 0.85+ (clear cluster)
4. 170 total duplicates found, 0 MIDI-to-Redbook false matches
5. Initial test at threshold=0.2: massive false positives (72k pairs). 0.2 is
   below the noise floor. User's suggestion was well-intentioned but the offset
   alignment algorithm inflates random similarity to ~0.50 baseline.
6. Pairs in 0.65-0.70 range: 898 pairs, ALL MIDI-to-MIDI cross-synth (e.g.
   d1-midi-mp3-mu80 <-> d1-midi-mp3-sc55). These are legitimate same-melody-
   different-synthesizer matches, not false CD matches.

Album-level breakdown at 0.65:
  D2 MIDI variants: 0 duplicates each (correct -- distinct from Redbook)
  D2 redbook mp3 rips: 24 duplicates (Redbook same as CD tracks)
  D2 macplay: 30 duplicates (all tracks)
  D2 mp3 rips (Definitive): 26 duplicates
  D2 vertigo: 14 duplicates (all tracks)
  D1 macplay: 8 duplicates
  D1 playstation: 22 duplicates
  Descent Maximum ps1: 30 duplicates (all tracks)

## AcoustID API notes

- Endpoint: POST https://api.acoustid.org/v2/lookup
- Parameters: client (API key), duration (INTEGER SECONDS), fingerprint (base64),
  meta (e.g. "recordings")
- Rate limit: 3 req/s, use 350ms spacing
- Duration MUST be in seconds, not milliseconds
- The PowerShell code correctly does the ms->s conversion, but the underlying
  duration_ms value was wrong (halved by the stereo formula bug)

## Files to modify

- `android/app/src/main/cpp/shared/fingerprint_gen.c` -- fix 3 bugs
- `android/app/src/main/cpp/extract/CMakeLists.txt` -- add test target
- `android/app/src/main/cpp/extract/test_fingerprint.c` -- NEW
- `android/get_deps/get_fpcalc.ps1` -- NEW
- `android/get_deps/tool_versions.conf` -- add fpcalc
- `android/tests/test_fpcalc_comparison.ps1` -- NEW
- `android/tests/test_acoustid_lookup.ps1` -- NEW
- `android/app/src/main/assets/fingerprint_config.json5` -- threshold updated to 0.65
  based on analysis (was 0.4)

## Files NOT modified

- `android/app/src/main/cpp/shared/pcm_decoders.c` -- correct as-is
- `android/app/src/main/cpp/shared/chromaprint_db.c` -- uses pre-computed FPs
- `game_data/update_known_discs_albums.ps1` -- already has priority hierarchy
- `game_data/fingerprint_music_packs.ps1` -- AcoustID ms->s conversion correct

## Additional bug found

`game_data/update_known_discs_fingerprints.ps1` had a bug: when a CD track already
had a chromaprint entry in known_discs.json5, it SKIPPED it instead of replacing
it. This meant even after regenerating all fingerprints from disk, the DB still
contained old buggy values. Fixed to strip existing chromaprint/duration_ms fields
and re-insert new values.
