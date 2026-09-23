# MIDI game preferences

The user accepts game08's bell balance. Leave authored events and gain unchanged.
Continue profile integration by storing renderer and soundfont identity with
ordinary dxx_prefs game preferences, shared by preview and both game launches.

1. Keep the global imported-asset catalog separate from preference selections
2. Default to AdLib (ymfm) and bundled SF2 fallback; apply the same defaults to
   both Original Descent and Restore Defaults without deleting imported assets
3. Show MIDI controls in Game Preferences as well as the MIDI music page, with
   changes and resets reflected immediately; include selections in config export
4. Validate selection values and missing imported-font references safely
5. Test persistence, rejected activation, both preset defaults, export/import,
   preview switching, D1 FM and D2 fallback; build an installable debug APK

Licensing: soundfonts have independent asset licenses. The existing pinned
TimGM6mb is GPL-2 (upstream COPYING.txt); no replacement is selected in this
increment. GeneralUser GS grants software inclusion under its custom license
but expressly acknowledges uncertain provenance of some samples. Document
these findings instead of treating a downloadable SF2 as redistributable or
claiming that a permissive renderer licenses its instrument assets.

Sources:
- https://github.com/arbruijn/TimGM6mb/blob/master/COPYING.txt
- https://github.com/mrbumpy409/GeneralUser-GS/blob/main/documentation/LICENSE.txt

## Results (2026-09-22)

Implemented the five steps above. JVM suite: 1,066 tests, zero failures/errors,
one skipped. Gradle assembled ARM64, ARMv7 and x86_64 successfully. The emulator
FM profile test passed fresh defaults, persisted choices, both preset resets,
retained imports and renderer switching. D1 and D2 each passed all 41 gameplay
automation steps, with native renderer confirmation (D1 ymfm, D2 SF2 fallback).
The SF2 profile test also passed valid/invalid imports, persisted custom-font
selection, renderer confirmation and safe preview switching.

Installable APK: `temp/midi-game-preferences/dxx-midi-preferences.apk`.
SHA-256: `6a196160774e36c86e663596f2dc5143e59e769fa57cf6948911b0a13f9e0818`.
APK signature and bundled license notice verified. No physical phone was
available for validation; emulator tests use the production code.
