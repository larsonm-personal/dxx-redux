# MIDI game preferences and soundfont licensing

Renderer and SF2 identity are ordinary `dxx_prefs` preferences (`midi_renderer`
and `midi_soundfont`), used by launcher preview and both game processes. The
imported soundfont catalog stays in private `soundfonts/selection.json`; its
selection fields from the earlier prototype are no longer read or written.
As with other pre-release preference changes, no migration is performed.

New preferences default to AdLib / Sound Blaster FM (`ymfm`). Both Game
Preferences presets, Original Descent and Restore Defaults, apply AdLib and
the bundled SF2 fallback. They retain imported SF2 files and catalog entries.
These are renderer defaults, not changes to MIDI/CD/external-music source mode.
D1 songs use their original FM banks/HMQ where supported; D2 and unsupported
FM material still fall back to SF2.

Selections save immediately from either the MIDI music page or Game Preferences.
Config export/import includes their values; SF2 files themselves are not copied
into a settings export. Missing imported identities safely use the bundled SF2.
Invalid renderer names and malformed font identities are rejected during import.

## Verification

- `:app:testDebugUnitTest`: asset/selection persistence, reset defaults for both
  presets, retained imports, rejected activation and settings export/import
- `android/tests/test_fm_profiles.py`: fresh defaults, both preset reset paths,
  process restart, renderer switching, HMQ previews and optional D1/D2 gameplay
- `android/tests/test_soundfont_profiles.py`: real native import/activation,
  invalid files, persisted custom SF2 and preview
- Both installed-app tests preserve the original game preferences and catalog

Completed on 2026-09-22: 1,066 JVM tests, zero failures/errors, one skipped;
ARM64, ARMv7 and x86_64 debug build; both profile runners passed on
`emulator-5554`. The FM runner included D1 and D2 gameplay (41/41 steps each)
and confirmed ymfm and SF2 fallback respectively. No physical phone was tested.
The SF2 runner verified custom import/activation, rejected invalid import,
restart persistence and switching back to bundled playback.

Reproduce from the repository root, running device tests serially:

```powershell
python android/tests/test_fm_profiles.py --serial emulator-5554 --output temp/midi-game-preferences/fm --games
python android/tests/test_soundfont_profiles.py --serial emulator-5554 --output temp/midi-game-preferences/sf2
```

Use `--adb C:/local/android-sdk/platform-tools/adb.exe` if adb is not on PATH.
Prepare output retention with `android/helpers/retain-recent-artifacts.ps1`
before creating a new run. Both tests require an installed debug build and
provisioned game data.

## Bundling soundfonts

SF2 is a format, not a blanket license. Redistribution permission must cover the
bank and its samples; a file being freely downloadable is not sufficient.

The existing bundled `gm.sf2` is TimGM6mb v20100822, SHA-256
`c5378b62028c920cb11e4803327983fee2f2cdff5dc89c708e39da417e51c854`.
Its upstream COPYING.txt explicitly identifies GPL-2. This asset predated the
FM work; it is not the license of TinySoundFont (MIT) or ymfm/ymfmidi (BSD).
No font was replaced during this preference change. The missing upstream notice
is now packaged as `assets/licenses/TimGM6mb.txt`, copied verbatim from upstream
commit `d6ad4ed72dce1fd3d67f17b74e08cd7ae7941a96`, SHA-256
`caf4761ca5e96fd034502a4dfac59f65fce61f3e5b4801b0b149c900fee29f06`.

GeneralUser GS v2.0.3 is an alternative with a custom license permitting software
use and modification. Its author also explicitly acknowledges uncertainty about
the origins of some contributed samples. It is therefore a candidate to review,
not an assertion of fully verified sample provenance or exact SC-55 emulation.
It also relies on SoundFont modulators, so TinySoundFont compatibility needs
listening and rendering tests before choosing it as a replacement.

Sources:
- https://github.com/arbruijn/TimGM6mb/blob/master/COPYING.txt
- https://github.com/mrbumpy409/GeneralUser-GS/blob/main/documentation/LICENSE.txt
- https://github.com/mrbumpy409/GeneralUser-GS/blob/main/README.md

Do not bundle extracted Roland/Creative hardware banks merely because their
files are available online. Nostalgic profile names need both verified asset
permissions and appropriate renderer behavior. The no-GPL-library requirement
remains satisfied by the FM integration; the existing GPL soundfont asset is a
separate pending selection decision under the user's broader license preference.
