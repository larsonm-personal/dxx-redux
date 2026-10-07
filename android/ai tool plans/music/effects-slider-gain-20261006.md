# Effects slider gain investigation

The user reports CD music balances with effects only at music 1/8, with both
too quiet, after source loudness calibration

- [x] Trace both music and effects gain paths in D1/D2
- [x] Log actual effects channel gain and distance attenuation before changing playback
- [x] Remove duplicate Android effects attenuation if runtime evidence confirms it
- [x] Build both Android engines, exercise playback and verify scoped code quality

The music comparison measured music sources only. Effects startup previously
multiplies per-sound volume by digi_volume for Mix_SetDistance, while
Mix_Volume also applies digi_volume. At effects 2/8, a full-level sound gets
32/128 channel gain times 32/255 distance gain, about 0.0314 total before pan,
instead of the intended 0.25. Later positional updates use per-sound volume
alone, so startup and updates also disagree

Preserve desktop behavior and existing music calibration. Keep the effects
default at 2 until the actual mixer defect is addressed and listening reassesses
the balance. Do not edit the user's outstanding_bugs.md

Runtime evidence on emulator-5582 confirmed channel_volume=32 and distance=223
for sound_volume=65536 before the fix. Afterward the same full-level sound has
distance=0, with channel_volume still 32. A half-level sound changes distance
from 239 to 127. The added checks in the existing registered sound trace runner
failed on the diagnostic-only build and passed on the corrected build (23/23
gameplay steps). Both Android x86_64 engine libraries rebuilt; scoped code
quality and both automation catalog checks passed

Evidence: temp/effects-gain-logcat-before.txt, temp/effects-gain-logcat-after.txt,
temp/effects-gain-build-after.txt, temp/effects-gain-test-after.txt

Music source gains and saved slider values are unchanged. At the default effects
2, ordinary full-level starts gain about 18 dB; music 1 to 8 is also about 18 dB,
so effects 2 / music 8 is the next listening starting point. This verifies mixer
gain routing, not perceived balance on the user's speakers or final-mix clipping
