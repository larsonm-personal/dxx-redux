# Owned S11/S22 mission sound banks

Publication status: draft for CI, not qualified for merge. The latest Windows
HOG-bank output check fails at sample 0 (15 observed, 25 expected). Preparation
reports the selected bank and expected first canonical sample; the installed
playback path still needs diagnosis. This failure remains enabled in the native
integration owner. Expanded HOG Android coverage and final Linux acceptance
have not yet passed on this revision.

The selected mission's `mods/<hog>/<hog>.s22` (output rate at least 22050) or
`.s11` replaces named ordinary D2 samples in the XL-owned playback bank. Its
presence suppresses classic replacement WAVs, including malformed WAVs that
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

The public source owner passes 405 classic-selection controls and 62 malformed
bank controls. The original `descent2.s22` contains 183 registered samples; every
selected byte agrees with actual source loading. Original bank SHA-256:
`4f10632dd4efcbffe532c35b6763edd22817135442bbcc4171381706f3893728`.

The initial Windows integration passes full original-byte conversion, partial
retention, both bank rates, signed scopes, atomic rejection, ordinary/object/
position consumers, stock transitions, saved identity and 90,112 actual PCM
samples. A subsequent Android checkpoint passed all four audio scripts (65, 35,
75 and 64 steps), including ten bank output checks and 269,312 samples at 48 kHz,
with 234 protected files restored. That checkpoint predates the HOG fallback
changes and does not qualify the current revision. Final Linux acceptance and
the expanded 119-step bank Android run are pending. The Android script is owned by the existing audio
scope runner; `-BanksOnly` selects it without removing publication or protected
state restoration. Default runner membership includes all prior scripts.

Actual pinned CFile::Exist/Open comparisons establish folder-bank before HOG
bank selection, and level-folder WAV before HOG WAV before mission-folder WAV
selection. The runtime now implements those rules with ownership checks; the
failing HOG playback check above prevents treating selection alone as success.

Fixtures are derived controls. No original XL mission with a custom S11/S22
bank has yet been qualified, and this does not establish Pumo or Boiling Point
playability. DTX2 replacement files, imported-D1 banks, global addon profiles
and other independent resource requirements remain separate work.
