# Owned S11/S22 mission sound banks

Windows, Linux and Android qualification passes for the profiles below. The
initial Windows HOG-bank output failure (15 observed, 25 expected) was traced to
the test playing native slot zero, `arnold_1`, while the packaged fixture replaces
`laser01`. Installed canonical and converted PCM confirmed that distinction.
The test now resolves the named replacement and asserts its voice binding; no
production playback change was needed. Windows passes 311,296 bank PCM samples
and 16,384 archive-WAV PCM samples, including the original failing boundary.

The selected mission's `mods/<hog>/<hog>.s22` (output rate at least 22050) or
`.s11`, falling back to the same basename in its HOG, replaces named ordinary
D2 samples in the XL-owned playback bank. A mod directory activates this
selection, matching source setup. Bank presence suppresses classic replacement WAVs, including malformed WAVs that
would otherwise win folder selection. Named trigger/ambient WAVs are a separate
resource class and continue to resolve normally. Redux GameSounds remain intact.

## Source behavior and explicit correction

The reference remains `arbruijn/d2x-xl` revision
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. Hash-checked SetupSounds, LoadSounds
and LoadD2Sounds run with bounded file and native-name registry adapters. Name
lookup follows the case-insensitive CHashTable::Search behavior. Reordered
headers, duplicate names (last recognized header wins), unknown names,
overlapping valid extents, trailing bytes and ignored `data_length` agree with
the source. The authored `length` determines the bytes actually consumed.
An out-of-range earlier duplicate is ignored when a valid later header replaces
it. A new actual-source case reproduced the port's original premature rejection;
validation now checks only the final selected extents. Native playback also
exercises this superseded-header case.

The source has a reproducible partial-bank defect: SetupSounds updates only the
listed samples, then LoadSounds uses every remaining base sample's old offset
against the custom file. A compact one-sample bank seeks past its end; a padded
bank replaces an omitted sample with unrelated padding. Both cases are retained
as explicit source controls. The port deliberately keeps omitted base samples,
including banks with no recognized names, instead of reproducing that defect.

The reader bounds the file and header table before consuming sample extents.
Unknown names add no native slots and their data is not read. Invalid selected
extents, unsupported versions, truncated files and foreign/shadowed resources
reject with the bank path before changing live playback. Zero-length recognized
samples have no playable source frames and reject explicitly.

Converted samples use the existing source-verified 44.1 kHz classic converter
and the shared output-rate conversion. This includes XL's unusual unsigned-byte
scaling and endpoint fades. Source storage is bounded at 128 MiB. Saved voice
identity includes the selected path, entire bank hash and rate, even when there
are no replacements; ignored-member edits cannot silently reuse old state.

## Evidence and remaining qualification

The public source owner passes 406 classic-selection controls and 62 malformed
bank controls. The original `descent2.s22` contains 183 registered samples; every
selected byte agrees with actual source loading. Original bank SHA-256:
`4f10632dd4efcbffe532c35b6763edd22817135442bbcc4171381706f3893728`.

The full Windows audio owner passes full original-byte conversion, partial
retention, both bank rates, signed scopes, atomic rejection, ordinary/object/
position consumers, finite completion, looping wrap, stock transitions, saved
identity and backend rate changes. It compares 311,296 bank PCM samples and
16,384 HOG/level WAV samples, retaining all existing named, ambient, static and
ordinary-WAV controls. Windows source, wave and inventory contracts pass.

Android's full owner passes 65 scope, 35 rejection, 75 static and 119 bank steps
on APK `7a400d5da65af37193125391e1cd90dfac77dc00a9c0bf1a5141fa358c165935`.
After the superseded-header correction, the focused 119-step bank regression
passes on final APK
`efadd79bd39bce3b773ba67a0e361eb0d6bf829012b5d05fc73bd096994bfc91`:
run `63444d0c2da140a893ea6a4017b90814`, 202.419 seconds, 18 bank controls,
15 actual-output comparisons and 361,472 samples at 48 kHz. Both runs restore
all 234 protected files unchanged. The full-owner bank run was
`712b9f5bbcf14b4f92cf3c78dac7bd10` (209.757 seconds, 355,328 samples).

The Android script belongs to the existing audio scope runner; `-BanksOnly`
selects it without removing publication or protected-state restoration. Default
runner membership includes all prior scripts. The master timeout is 900 seconds
for the expanded four-script workload; individual script bounds are unchanged.
Final Linux acceptance passes all 106 required controls, 16 original-format
witnesses and three modern source levels in 408.09 seconds of CTest wall time.
Its audio owner passes in 138.47 seconds within the unchanged 150-second bound,
including the same 311,296 bank and 16,384 archive-WAV PCM comparisons as Windows.
The source owner reports 406 selection controls on both hosts. This adds work
to the existing required owner rather than creating a duplicate CI lane.

Reproduction uses `xl_named_audio_runtime` and `xl_named_audio_source` under
CTest, plus `android/tests/test_xl_audio_scopes.ps1` with the generated fixture
directory. `-BanksOnly` runs the focused Android regression; native
`-audio-scope-test banks` runs the bank controls. Local receipts are under
`temp/xl-ambient-audio-20261030`; the full Linux acceptance receipt is
`temp/xl-private-ci-20261028/run/original-status.json`.

Actual pinned CFile::Exist/Open comparisons establish folder-bank before HOG
bank selection, and level-folder WAV before HOG WAV before mission-folder WAV
selection. The runtime implements those rules with ownership checks, actual
output and malformed-folder/foreign-shadow rejection controls.

Fixtures are derived controls. No original XL mission with a custom S11/S22
bank has yet been qualified, and this does not establish Pumo or Boiling Point
playability. DTX2 replacement files, imported-D1 banks, global addon profiles
and other independent resource requirements remain separate work.
