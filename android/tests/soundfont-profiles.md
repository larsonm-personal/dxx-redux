# Soundfont selection integration

The MIDI music page now provides a global soundfont selection, import of local
SF2 files up to 64 MiB, and a bundled-font option. Imported names come from the
file picker. Assets are copied to private `files/soundfonts/<sha256>.sf2`; an
atomically replaced `selection.json` records the list and selected identity.
Import deduplicates identical contents and validates before publishing an asset.
A failed import or synth activation preserves the working selection.

`SoundfontStore` resolves the same asset for `MidiPreviewBridge` and the game
activity. Preview selection loads a replacement synth before stopping/joining
the render thread and releasing the old one. D1 and D2 resolve the persisted
choice when their game process starts. A running game keeps its loaded font;
the picker labels this as applying to the next game launch.

Both native consumers use `music_soundfont_load` in libtsf. The loader bounds
input size, validates RIFF/table sizes and references, limits preset expansion,
then checks resolved sample regions before returning the synth. SF3, ROM samples
and unsupported SF2 layouts are rejected. No additional synth library is used.
The event conversion/scheduling and existing gain settings are unchanged.

This is the first production profile family. FM playback, per-game overrides,
user-named variations and percussion/effects controls remain follow-up work in
`android/ai tool plans/music/fm-library-feasibility.md`.

## Verification

From the repository root, with the usual test environment/tool paths:

```powershell
. ./android/helpers/test_env.ps1
cmake -S android/app/src/main/cpp/extract -B android/build/host-extract-tests
cmake --build android/build/host-extract-tests --config Release --target test_music_soundfont test_midi_seek_timeline
ctest --test-dir android/build/host-extract-tests -C Release --output-on-failure -R '^(music_soundfont_tests|midi_seek_timeline_tests)$'

$env:JAVA_HOME='C:\local\jdk-21'
./android/gradlew.bat -p android :app:testDebugUnitTest --tests com.dxxredux.app.SoundfontStoreTest :app:assembleDebug '-Pandroid.injected.build.abi=x86_64'
```

Native tests load/render the real bundled SF2 and reject missing, truncated,
oversized and corrupt files. Existing MIDI seek tests cover playback state.
JVM tests cover durable selection, duplicate imports, switching back to bundled,
failed activation, invalid import and interrupted copy cleanup.

For the installed debug app on a configured emulator:

```powershell
& android/helpers/retain-recent-artifacts.ps1 -Artifacts temp/soundfont-profiles
adb -s emulator-5554 install -r -t android/app/build/intermediates/apk/debug/app-debug.apk
python android/tests/test_soundfont_profiles.py --adb C:/local/android-sdk/platform-tools/adb.exe --serial emulator-5554 --output temp/soundfont-profiles --games
```

The integration test uses the launcher automation API to import a copy of the
bundled font with a changed preset display name (distinct content hash), reject
an invalid import, restart the process, preview, and switch back and forth. It
restores the original soundfont manifest and removes its own temporary assets.
`--games` also runs the existing shared music-control test in D1 and D2 and checks
native loading of the selected asset. Those game tests use their usual pilot and
settings fixtures; run them on a test emulator. Reports/logs are in the output
directory. The same store/native paths are used by the file-picker import UI.
