# Ordinary XL sound replacement resources

This records the original resource-preparation tranche. The subsequent
[classic playback tranche](classic-playback.md) installs selected WAVs into the
shared bank and supersedes the historical missing-consumer gate below.
The later [mission-bank tranche](mission-sound-banks.md) adds S11/S22 decoding
and corrects the original folder-only lookup model using actual CFile functions.

Owned D2 replacement WAVs now have a production preparation path and an explicit
admission gate. A level requiring them cannot silently play the stock samples.
Playback installation, spatial consumers, shared channel priorities and saved
voices remain subsequent work. No original campaign gains qualification.

## Selection and ownership

The pinned XL `LoadHiresSound`, `SetupSounds`, `LoadSounds`, `VerifyHiresSound`
and `LoadD2Sounds` functions execute in the existing named-audio source owner.
Its 206 additional cases check sample-name registration, unknown custom names,
normal and secret folders, output-rate bank choice, wrong-rate banks, WAV
priority, incomplete global profiles and complete/mixed custom profiles.

The selected `mods/<hog>/<hog>.s22` or `.s11` bank suppresses WAV replacements.
The initial audit modeled incoming level-folder lookup before mission-folder
lookup. Actual CFile::Exist/Open comparisons in the later mission-bank tranche
show an intervening HOG lookup: folder bank, then HOG bank; otherwise level WAV,
then HOG WAV, then mission WAV. The old folder-only adapter missed that fallback.
Ordinary replacement names come from the actual loaded native sample registry;
named-sound aliases do not apply. Global high-resolution
installations remain outside the qualified owned-resource profile.

The production provider uses the selected descriptor root on desktop and active
owned package mounts on Android. Foreign shadows, nonregular files and malformed
preferred WAVs reject before any live playback changes. Each WAV is bounded to
16 MiB and the decoded stereo bank to 128 MiB. Identity covers the numeric sample,
name, selected virtual path and exact bytes, without host paths. Missing optional
replacements leave the baseline sample available for the eventual consumer.

A selected custom bank receives a specific unsupported-decoding error. A valid
selected WAV receives a specific missing-playback-consumer error after validation.
The reference falls back to bank data for an empty preferred WAV; production
rejects that malformed selected asset explicitly instead. It never substitutes
a lower-priority mission WAV.

## Original Pumo audit

The unchanged archive hash is
82c8cc4cfc740c8169f9d21a961c9ac5d2518342b3e27ac3ac438c91ca8f2ddf.
The retail D2 bank has 183 named samples. Across fourteen incoming normal levels,
1,988 selections use 196 distinct external WAVs for 142 sample names. This includes
54 level overrides. Two selected files are also referenced by ambient objects;
these counts overlap and do not measure completed runtime consumers.

An independent Python DSND reader, folder selector and standard-library WAV
decoder agree with the extracted XL source and production provider. The two
production output-rate profiles select the same `.s22` bank family; these are
source decoding checks, not resampling or actual-output qualification. All
selected source hashes, channel counts, sample rates, frame counts and decoded
little-endian PCM hashes must match exactly.

All fifteen INI members have been read separately. They select the standard
medium ship, disable the other ship choices and automap marker teleport, and
permit 3D powerups subject to the user's rendering profile.
`mvlev5.ini` additionally enables secret saves. `pmend.ini` is outside the fourteen
descriptor level names. None changes the sound profile. Applying these gameplay
options, HXM definitions and the other package resources remains a separate audit.
The teleport setting does not disable authored teleport triggers. See the
[planning checkpoint](../../ai%20tool%20plans/mission%20metadata/pumo_playability_checkpoint_20261028.md)
for configuration defaults and the source collision-profile reset hazard.

## Controls and reproduction

Native integration uses four owned normal/secret/fallback contexts, checks every
decoded stereo sample, distinguishes scope identities, and rejects malformed,
foreign and selected-bank resources without disturbing active named voices.
Removing the derived replacements restores admission and leaves playback state
unchanged. It extends `xl_named_audio_runtime`, avoiding a duplicate test process.
The preparation captures bank presence once and inventories each sound folder
once per preparation. Selected files still receive case correction, ownership
and regular-file checks. This removes repeated missing-file scans without
retaining stale filesystem results across loads or rejection recovery.

The existing Android sound-folder owner also publishes three derived rejection
missions: a valid unapplied WAV, malformed preferred WAV and selected custom bank.
The normal menus must show the resource-specific error, retain inactive XL state,
then successfully start Counterstrike. The existing actual-output, disk, rewind
and normal/secret sound-folder checks remain in that same owner.

```text
ctest --test-dir <contracts-build> -R "^xl_named_audio_(source|runtime)$" --output-on-failure
pwsh -NoProfile -File android/tests/test_xl_audio_scopes.ps1 -Serial emulator-5554 -FixtureDirectory <derived-root>
xl_named_audio_source --classic-original <descent2.s22> <package>/mods/pmines/sounds 14
xl-preview-lifecycle -classic-audio-original pmines -audio-levels 14 -audio-waves 196 -audio-selections 1988 -hogdir <retail-data> -extra-dir <package> -userdir <package>/preview-user -nomusic
python android/XL_compat/tests/classic_audio_original.py --bank <descent2.s22> --sounds <package>/mods/pmines/sounds --identity pmines --levels 14 --waves 196 --selections 1988 --reference <source-output.tsv> --receipts <package> --output <independent-audit.json>
```

The source audit's `CLASSIC_SELECTION` rows and both production JSON receipts
are required by the independent comparison. Original preparation never loads
Pumo's blocked levels. Pumo remains a local fixture outside the private CI store.

## Validation status

Windows source and native preparation/rejection controls pass. Independent
original comparisons agree on all 1,988 selections and 196 WAVs on both hosts;
the normalized audit receipts are byte-identical. The final public owners pass
43 Windows and 46 Linux tests in reported test times of 11.938 and 7.395 seconds.
Scoped mixed formatting, direct pinned PowerShell analysis and both catalogs pass.

Android passes all 65 sound-folder steps in 100.197 seconds, run
`94d33a093d7e4e849d0e553a421c6eeb`. Sixteen actual-output comparisons check
144,384 PCM samples at 48 kHz. All 27 classic rejection/recovery steps pass in
37.860 seconds, run `ec856a622705429caf512dd415af5032`. Protected application
files are restored byte-for-byte. No autonomous level navigation is used.

The final private owner passes all 103 required controls in 337.576 reported
test seconds (CTest wall time 333.39 seconds). An earlier repeated-path scan
caused boss-load and sound-scope timeouts; the per-preparation folder inventory
fix restores both controls without increasing either timeout or removing checks.
The final named-audio owner takes 38.71 seconds on the local WSL filesystem.
Receipts, including the separate INI inventory, are under
`temp/xl-audio-bank-20261029`; Android receipts are in the two script-named
subdirectories of `temp/xl-audio-scopes-android`.
