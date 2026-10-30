# Classic WAV replacement playback

This tranche's custom-bank rejection is historical. The subsequent
[mission-bank work](mission-sound-banks.md) adds S11/S22 decoding and corrects
HOG fallback selection; that evidence records current qualification status.

The shared XL bank now installs the owned classic replacements selected for the
incoming signed level number. Ordinary logical sounds, unlinked physical samples,
object owners and position owners use that bank. The native sample registry and
`GameSounds` storage remain unchanged, so returning to Redux clears the XL bank
without replacing stock data. No `d1/` or `d2/` edits are needed for this tranche.

Preparation still rejects malformed preferred files, foreign shadows and selected
custom S11/S22 banks. The complete immutable bank identity includes every selected
sample name, path and source hash. Preparation does not mutate live playback;
installation and saved-state restoration use the existing shared-channel and
owner validation. Diagnostics expose `classic_replacements` by sample name and
owned virtual path.

## Source contract and deliberate correction

Resource selection retains the 206 actual-source controls and the independent
original Pumo comparison in [classic bank evidence](classic-audio-bank.md).
The pinned `CAudioChannel::Start` classic branch now also executes with independent
byte-flow adapters: 24 cases distinguish stock conversion, the high-resolution
quick-WAV path, custom data indices and speed handling.

The source's mixed-bank path (`!gameOpts->sound.bHires[0] && pSound->bHires`)
passes the channel's empty `m_info.sample` buffer to `Mix_LoadWAV_RW`, without
binding the selected sound's bytes first. A fresh channel therefore supplies no
WAV data. The extracted branch reproduces that defect. A high-resolution profile
instead binds the selected bytes before `Mix_QuickLoad_WAV` and bypasses stock
resampling and speed handling.

Production intentionally supplies the validated selected WAV and converts it to
the actual stereo output rate. It does not preserve empty-buffer playback or
play differently formatted raw WAV data as though it already matched the device.
The source adapter proves startup routing and byte identity, not SDL decoding or
output equivalence. Native and Android actual-output controls supply separate
evidence for the implemented playback policy.

## Current qualification

Windows and Linux first-pass loaded tests cover four normal/secret/fallback
contexts, repeated filenames, ordinary engine dispatch, object and position
owners, mixed named audio, exact saved-state restoration through a stock level,
and backend reopening at 48 kHz. The strengthened Windows run also verifies
finite completion and permanent loops across the WAV boundary; it checks 790,528
replacement PCM samples. Independent labeled WAV expectations are shared with
the Android test, rather than reading expected values back from the installed bank.
Changing valid WAV bytes at the same path also rejects the old saved bank without
mutating live state; restoring the original bytes recovers its identity and PCM.

Android passes both existing owners: 65 sound-scope steps and 35 classic-only,
malformed-WAV, custom-bank and Redux recovery steps. The scope run checks 25
actual outputs (13 replacement cases), including disk and rewind restoration;
the classic-only mission adds one output/replacement case. Together they compare
237,568 samples at 48 kHz. All 234 protected files restore byte-for-byte.

The first Android run correctly failed because the
test selected physical sample zero (`arnold_1`) while the published replacement
was `laser01` (sample 14 in the retail bank). The revised diagnostic resolves the
authored sample name through the loaded registry and verifies its installed path.
This was a test routing error; no playback assertion was weakened.

The existing Android scope owner now covers replacements without adding another
build job. It retains named output, real publisher/normal/secret transitions,
disk and rewind restoration, stock recovery, malformed WAV and custom-bank
rejections. A separate existing derived mission with no SND actions checks that
classic-only replacements are installed as well.

The complete Linux audio owner passes in 93.86 seconds inside its unchanged
110-second timeout, including all five existing ambient runtime profiles and
source oracles. Both automation catalogs pass with unchanged membership.

The full native lane passes all 106 required tests in 379.02 seconds (382.120
reported test seconds), with 16 format witnesses and three modern source levels.
Its final audio member passes in 88.57 seconds. The Windows full audio integration
also passes. No required member, source oracle or timeout was removed or increased.
CI qualification remains pending publication.

## Original resources and retained evidence

The original Pumo component audit now prepares the complete bank and converts
every selected replacement to device-rate stereo. An independent Python bank
reader, WAV decoder and timestamp interpolator verifies all 1,988 selections,
196 distinct WAVs and both 44.1/48 kHz output profiles. All 196 original WAVs are
44.1 kHz stereo 16-bit inputs; this original audit does not widen that input
format witness. Windows and Linux source-resource and converted-playback
receipts are byte-identical. The unchanged original HOG remains
`6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043`.

These are complete-bank and conversion checks, not actual original-level audio
callbacks or a campaign playthrough. Pumo retains independent gameplay and
graphics blockers. Static explosions require their animation/rendering,
lighting and collision behavior as well as ambient audio and remain separate.

Retained artifacts use `classic-*` under `temp/xl-ambient-audio-20261030`:
the native status/JUnit receipt, full Windows detail, original source TSV,
per-host original resource/playback receipts and independent audits, Android
results/state and before/after protected-file hashes, catalogs and formatting.
Android runs `46dff7e860ec4ddda08b05d11ed6fce2` (153.714 seconds) and
`0496a892636640178bad4f5bbd5a6836` (57.204 seconds) cover the two existing scripts.
The verified APK SHA-256 is
`de9416aee9213b7b99638d14275a42e3942407f57b4bccf0bf1f2ed656b1b9bf`.
The final Windows runtime SHA-256 is
`6a5de0ddaf1015d74b1bfe1acc501d3fe27f4bde4af5d3247fed56f0688231e3`.
