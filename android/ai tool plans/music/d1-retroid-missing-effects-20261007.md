# D1 missing effects on Retroid Pocket 4 Pro

## Implementation: initial MacPlay disc

- [x] Inspect actual installer sound resources and establish sample mapping
- [x] Extract/convert sounds through the native MacPlay CD import path
- [x] Preserve generated sound assets in launcher import and D1 loading
- [x] Add parser and real-disc integration coverage with non-silent effects checks
- [x] Build, format, and verify on the Retroid, including PC D1/D2 controls

Initial supported media: game_data/CD images/Descent - Mac macplay

- [x] Build and install ARM64 debug APK on the connected Retroid
- [x] Reproduce absent D1 effects at slider 8 and inspect playback diagnostics
- [x] Identify whether normalization is responsible using matched device/build tests
- [x] Preserve evidence and restore the original release APK

Preserve the installed release app's data and unrelated working tree changes

## Confirmed diagnosis

The original GitHub installation uses D1 Mac (MacPlay) assets. Its imported set
contains only descent.hog and descent.pig; the PIG is 3,975,533 bytes with SHA-256
9eb232c9da830309b2d3a1de75713f6155c62215cd867f4570536180af31f299.
There is no Sounds directory containing the separate Mac sound samples.

D1 piggy.c recognizes this as MacPig and skips the PC sound headers in the PIG.
piggy_read_sounds then asks ds_load for Sounds/SND0000.raw onward. The first
missing file stops loading. Unlike the Mac demo path, full Mac data does not
produce a missing-sounds warning. The HFS extractor exports data forks only;
there is no resource-fork sound conversion in that path.

On Retroid JYPR42510121028 with the same ARM64 debug build and effects slider 8:

| Data                     | Captured PCM samples | Peak absolute PCM | Started effects |
| ------------------------ | -------------------: | ----------------: | --------------: |
| PC D1 control            |               312832 |              3727 |               3 |
| Original MacPlay HOG/PIG |               312320 |                 0 |               0 |

Music was muted for both laser/explosion probes. Both runs reported a working
48 kHz stereo mixer, effects channel gain 20, foreground unpaused gameplay,
and no audio enqueue failures. The PC trace confirmed successful sample reads,
matching conversion hashes, and distance zero for full-volume effects.
This isolates the reported silence to missing Mac sound assets, before mixing,
not the recent volume normalization.

Evidence is under temp/d1-retroid-audio/: d1-store-audio.wav,
mac-store-audio.wav, corresponding introspection JSON, original debug logs,
probe.jsonc and probe-mac.jsonc. The original APK and settings/saves/logs were
backed up there. The signed debug APK was installed over the original package
only to read its data, without launching or resetting that package. The isolated
com.dxxredux.app.nsdtest installation was used for playback experiments.

At the initial diagnosis, no engine or importer code was changed. The required fix was
Mac sound extraction/conversion and import coverage, including an actual
non-silent effects assertion. The existing Mac CD regression only verifies the
two HOG/PIG files and reaching level 1, so it cannot detect absent effects.

## Implemented initial disc support

The MacPlay installer stores the 98 format-2 standard mono 11025 Hz PCM snd
resources in the Descent application's resource fork, IDs 10000..10097, with
names SND0000.AIF through SND0097.AIF. The native installer extractor now saves
that fork as descent.rsrc, retaining the existing CRC, size limits and atomic
output behavior. A shared bounded parser validates every sample before import
succeeds or the engine registers any samples. Aggregate PCM allocations are
bounded by the resource data size, including for overlapping malformed entries.

The launcher recognizes descent.rsrc as base game data and retains it during
production disc import. The Android D1 Mac retail path loads the bank when no
legacy Sounds/SND0000.raw override is present (case-insensitive, matching ds_load). PC D1, Mac demo RAW loading and
D2 playback are unchanged. Older installations require reimporting the disc;
there is no way to reconstruct these samples from HOG/PIG alone.

Validation on Retroid JYPR42510121028:

- Full launcher CUE/BIN import and 45-step D1 gameplay test passed
- Final reusable-runner repeat passed: effects peak 3309 PCM, music peak zero
- Imported bank: 1328290 bytes, SHA-256
  d680bac0ddf87d8f6585597026e8c2a5426614125d1d191488212fb2cf752e59
- Effects peak 3475 PCM with music peak exactly zero at effects slider 8
- PC D1 and D2 controls passed, effects peaks 3966 and 3707 PCM respectively
- Original GitHub package updated with the signed debug fix and reimported
  through the production import_cd command without resetting its state
- Original package independently captured effects peak 2570 PCM, music zero;
  restored both sliders to 8; all five backed-up pilot/save files were unchanged
- Native STi2 tests: 12 passed, zero skipped, including actual MacPlay/D2 media,
  bank identity, truncation, corrupt map, unsupported encoding and invalid rate
- Kotlin extension parity and file-set content tests passed
- Automation catalogs passed: 297 top-level entries and 373 support scripts

Reusable device runner: android/tests/test_macplay_audio.ps1. It requires the
locally owned initial MacPlay CUE/BIN and a current debug installation. On a
physical device it uses the isolated diagnostic package and a test-owned set.
Artifacts and build/test logs remain under temp/d1-retroid-audio. The original
release APK remains backed up there; the fixed debug build is currently installed.
