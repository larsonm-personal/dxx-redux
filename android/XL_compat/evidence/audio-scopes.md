# Owned mission and level sound folders

Named WAV lookup now supports the selected mission HOG, its incoming normal or
secret level sound folder, and its mission sound folder, in that order. Numeric
and prefix aliases resolve before lookup. Global addon installations and ambient
spatial playback remain separate requirements.

## Runtime ownership

The incoming signed mission ordinal passes through normal loading and entry
preflight. It does not use the outgoing `Current_level_num` or infer a folder
from the level's basename. Direct filename-only diagnostics may infer a unique
descriptor match; repeated filenames require an explicit ordinal. The normal
loader, secret entry and disk restore already know that ordinal.

Folders use `mods/<selected-hog>/sounds/levelNN`, `slevelNN` and the mission
`sounds` directory. Desktop resources must come from the root captured with the
selected descriptor. Android published packages use their active owned mounts,
including resources outside the descriptor's archive. Foreign shadows reject
before reading; a malformed preferred resource never falls back to a different
sound. The global addon fallback is explicitly reported as unqualified.

Preparation completes before replacing live voices. Identity includes the
selected virtual path and exact content, without host paths. A failed preflight
retains current playback; a saved voice from another level's sound scope cannot
be restored into it. Redux keeps its existing audio path.

## Evidence

The pinned XL `LoadAddonSound`, `FreeAddonSounds` and `LevelFolder` functions
supply 384 additional source comparisons across aliases, presence combinations,
normal/secret ordinals and alias-cache release. Six invalid mission identities
are rejected. Existing event, repetition and output contracts remain required.

The derived fixture repeats `scope23.rl2` at normal levels one and two and at
secret level minus one; level three uses 27/40. Its HOG and mission folders have
deliberately malformed lower-priority duplicates. Distinct constant stereo
waveforms establish which selected resource actually reaches output.

The native integration exercises four contexts, normal and secret repeats,
mission fallback, actual PCM, malformed/missing/foreign resources, stable
identity, atomic failures, disk-through-stock restoration and resource cleanup.
It is part of the existing `xl_named_audio_runtime` owner, not a duplicate job.

The normal Android launcher publishes a staged derived ZIP, and the game selects
its owned context. All 65 steps pass in 97.080 seconds,
run `d20455cf0dfa472cb4a3871d4bd61f5d`. Sixteen actual-output comparisons verify
138,752 samples at 48 kHz, alongside rewind, disk, level and stock transitions.
The selected context belongs to `mod/xl-audio-scopes.zip`. Protected application
files are restored byte-for-byte. No autonomous level navigation is attempted.

## Pumo resources

The unchanged distribution archive has SHA-256
82c8cc4cfc740c8169f9d21a961c9ac5d2518342b3e27ac3ac438c91ca8f2ddf.
Its external sound tree contains 261 WAV files, 127,766,720 bytes. An independent
selection and PCM audit resolves all 644 ambient objects across fourteen levels
to 41 mission-folder files, with no missing dependency. All 41 are 44.1 kHz
stereo 16-bit PCM. Production resource preparation agrees with the independent
paths, source hashes and decoded samples.

This establishes resource availability and decoding. The ambient objects remain
blocked until spatial output, ownership, enable/disable and persistence are
implemented. The other 220 external sound files still require consumer and
replacement-scope qualification; presence and successful decoding alone do not
qualify their application. A filename audit finds 196 files matching 142 entries
in the 183-entry retail D2 sound bank, including level overrides and two files
also referenced by ambient objects. Another 26 files are outside those bank and
authored-ambient selections. These overlapping counts describe dependencies,
not completed runtime consumers. HXM, INI, other gameplay requirements and human
campaign acceptance remain open.

## Reproduction

```text
ctest --test-dir <contracts-build> -R "^xl_named_audio_(source|runtime)$" --output-on-failure
pwsh -NoProfile -File android/tests/test_xl_audio_scopes.ps1 -Serial emulator-5554 -FixtureDirectory <derived-root>
xl-preview-lifecycle -named-audio-original pmines -audio-levels 14 -audio-actions 60 -audio-waves 22 -audio-ambient 644 -audio-ambient-waves 41 -hogdir <retail-data> -extra-dir <isolated-full-package> -userdir <isolated-full-package>/preview-user -nomusic
```

The derived fixture comes from `xl_audio_fixture` and the unchanged private
Neptune geometry. The Android owner pins its HOG and descriptor hashes and
requires the provisioned retail D2 test profile. The optional original audit
writes per-resource receipts at both output-rate profiles and does not admit
Pumo's levels. Pumo remains absent from the private CI asset store.

Local receipts are under `temp/xl-audio-scopes-20261029` and
`temp/xl-audio-scopes-android`. Final local runs pass all 43 Windows public,
46 Linux public and 103 Linux private required tests. The private owner also
checks sixteen original-format outcomes and three complete modern source
comparisons. Reported test time is 10.656, 6.148 and 319.041 seconds respectively;
build and preparation time is additional. Native scoped output checks 131,072
samples on each host. Both automation catalogs, scoped formatting and direct
PowerShell analysis pass. The original distribution archive remains unchanged.
