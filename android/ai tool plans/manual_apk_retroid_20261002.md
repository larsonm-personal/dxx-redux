# Manual APK Retroid validation

- Test the exact universal artifact from Actions run 37037829265 on Retroid Pocket 4 Pro JYPR42510121028
- Install the previously verified private-test-key signed copy as com.dxxredux.app.ci
- Preserve the existing Play/legacy installations and target every adb operation to the physical device
- Use ordinary release-app import and launch flows because external automation and run-as are disabled in the release APK
- Exercise both games, gameplay/menu transitions, suspend/resume and inspect crash/native-loader logs
- Capture device details, package/ABI evidence, logs and results under android/temp/

## Results

- Installed the verified signed copy of the exact hosted artifact successfully; no app rebuild was used
- Device Android 13, installed ABI arm64-v8a, version ci-9c15bcd95e77, release flags without DEBUGGABLE
- Both original app packages remained installed; the tested package is the separate CI installation
- Ordinary folder import copied all 11 pinned D1/D2 files into CI app storage and marked both games ready
- D1 First Strike / Lunar Outpost: engine load, level rendering, injected keyboard input/firing, automap, Home/background and resume, exit autosave and Load Last Save passed
- D2 Counterstrike / Ahayweh Gate: engine load, level rendering, injected keyboard input/firing and turning, automap, Home/background and resume, exit autosave and Load Last Save passed
- Both native engines reported Mali-G77 MC9; SDL audio initialized at 48 kHz stereo and loaded 143 soundfont presets
- Captured logs had no Java/native crash, ANR or missing-library signatures; process exit records corresponded to explicit game EXIT and worker teardown
- Controller was detected as Xbox Wireless Controller; physical sticks/buttons and audible sound quality were not independently tested
- Non-debug release receivers and run-as were unavailable; normal UI, screenshots and logcat provided the device evidence
- ZIP import rejected the 32,561,908-byte combined game-data archive with `ZIP source exceeds 16777216 bytes`; folder import worked around this existing app issue
- Injected alphabetic keyboard input reopened the soft keyboard in gameplay after pilot creation; background/resume hid it again
- Removed the temporary Download ZIP/folder and restored stay_on_while_plugged_in to its original value 0
- Left DXX-Redux (CI) installed with imported data and both exit saves for hands-on testing
- Evidence directory: android/temp/retroid_ci_apk_20261002
