# FM phone prototype

In the launcher choose **Music -> MIDI -> Sound profile -> AdLib / Sound Blaster
FM**. Expand the built-in Descent 1 tracks and preview
`game07.hmp`. The preview identifies the renderer actually playing. Launch D1
with MIDI selected to hear the same profile in gameplay. Selection applies on
the next game launch and persists across restarts.

Choose **Soundfont** to return to sampled playback. The bundled/imported SF2
selection is retained independently of the renderer; imports remain limited to
64 MiB. These are different instrument sources: original FM BNKs are synthesis
parameters, while SF2 files contain samples. Importing an SF2 changes soundfont
playback and FM's fallback, not the FM instruments.

## What this prototype includes

- Pinned BSD-3-Clause ymfm YMF262 core and ymfmidi event driver; no new GPL or
  LGPL dependencies or bundled third-party FM instrument banks
- Each song's original ADLIB/AMLIB/ANLIB BNKs, resolved from its SNG and game assets
- DOS HMQ substitution when present, followed by FM track selection, retaining raw FM CC7 and
  neutral pitch; the existing GM conversion remains separate
- Shared rendering, event scheduling, repeat handling and preview seek paths
- Measured HMI voice allocation, paired stereo, volume, pan and pitch-wheel
  behavior, sustain, all-notes-off/all-sound-off, and reproducible FM reset
- Persistent renderer selection, actual-renderer preview text/introspection,
  and native bank/renderer diagnostics
- Library copyright/license notices under APK `assets/licenses/`

The renderer uses nine paired stereo voices and measured original-driver
behavior, verified against DOS register captures. This does not imply
sample-identical audio timing. Preview seeking restores musical state quickly;
it does not reconstruct the preceding envelopes sample by sample.

D2 banks and the original games' rhythm-marked patches are supported. DOS plays
those patches as ordinary paired voices, rather than enabling hardware rhythm.
Missing bank context or an unavailable FM arrangement selects soundfont playback
(including the original D2 briefing file). For HMP, fallback
also reconverts the original GM HMP arrangement, avoiding FM/GM track layering.
The title and Levels 1/2 now use their HMQ arrangements and play through FM;
their earlier fallback was caused by feeding HMP programs to HMQ banks. Custom FM
banks, named hardware presets, percussion balance and effects are follow-up work.

## Reproduce

To hear a direct SF2/FM comparison through the production converter and synth,
build `test_music_synth` as below, then run:

```powershell
./android/helpers/retain-recent-artifacts.ps1 -Artifacts temp/fm-profile-comparison
python android/tests/compare_music_profiles.py --renderer android/build/host-extract-tests/Release/test_music_synth.exe --hog PATH/TO/DESCENT.HOG --output temp/fm-profile-comparison
```

Open `temp/fm-profile-comparison/listen.html`. The script checks that the first
20 seconds of title/Levels 1/7/8 use ymfm and have different PCM from SF2.
It writes WAVs and a report with actual
renderers, hashes, peaks, clipping counts and difference RMS. These are cold-start
host renders with preview defaults, not recordings from the phone. The older
`opl-game07/listen.html` compares FM chip emulators and contains no SF2 sample.

Build a normal installable APK with ARMv7, ARM64 and x86_64 included:

```powershell
$env:JAVA_HOME='C:\local\jdk-21'
$env:Path="$env:JAVA_HOME\bin;$env:Path"
Push-Location android
./gradlew.bat :app:testDebugUnitTest --tests com.dxxredux.app.SoundfontStoreTest :app:assembleDebug --console=plain
Pop-Location
```

The normal build writes `android/app/build/outputs/apk/debug/app-debug.apk`.
Install it as an update with Android's package installer or `adb install -r`.
No game data is bundled; use the existing imported game data. A normal build
does not require the `adb -t` flag used by ABI-injected development builds.

Host checks use the same native renderer and converter as the app:

```powershell
cmake -S android/app/src/main/cpp/extract -B android/build/host-extract-tests
cmake --build android/build/host-extract-tests --config Release --target test_music_synth test_hmp_android_shared test_midi_seek_timeline hmp_midi_export
ctest --test-dir android/build/host-extract-tests -C Release --output-on-failure -R '^(music_synth_tests|hmp_android_shared_tests|hmp_playback_tests|midi_seek_timeline_tests)$'
python android/tests/test_midi_preview_sync.py
python android/tests/test_fm_playback.py --renderer android/build/host-extract-tests/Release/test_music_synth.exe --hog PATH/TO/DESCENT.HOG --output temp/fm-phone-prototype/host
```

The source comparison checks ordered event prefixes and timing against the
independent FM experiment parser. It covers every available SNG song in either
supplied HOG, exact FM restart PCM, complete FM song repeats,
unclipped FM output, explicit HMQ preview bank lookup and malformed-HMQ fallback
with PCM identical to selecting SF2 on the original HMP.

Installed-app checks require provisioned D1/D2 data and run serially:

```powershell
python android/tests/test_fm_profiles.py --adb C:/local/android-sdk/platform-tools/adb.exe --serial emulator-5554 --output temp/fm-phone-prototype/device --games
python android/tests/test_soundfont_profiles.py --adb C:/local/android-sdk/platform-tools/adb.exe --serial emulator-5554 --output temp/fm-phone-prototype/soundfont --games
```

Both runners restore the original profile manifest. Gameplay tests use the
shared music-controls test's normal pilot/settings fixtures. Before a fresh
report/package run, use `android/helpers/retain-recent-artifacts.ps1` with the
planned output directory. The local deliverable and test evidence are kept in
`temp/fm-driver-android/`; physical phone performance remains to be tested.
The current APK is `dxx-fm-fidelity.apk`. Final FM checks pass D1 41/41 and D2
48/48 gameplay steps; custom-SF2 checks pass D1 44/44 and D2 51/51. The SF2 runner
reapplies its chosen profile after the gameplay fixture resets game preferences.
