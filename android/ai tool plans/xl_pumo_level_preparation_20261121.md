# Reuse Pumo level preparation across the briefing

The matched Rookie run on build 32121 prepared Crauphel's 144 authored
textures before and after the briefing. The check took 10.89 s and the
subsequent native load spent 10.02 s in its level-file stage. The latter
stage already reused the translation of the level bytes.

1. Carry the prepared resource owner across the briefing for ordinary
   single-player travel in a published Android mission view. Match the
   mission mount generation, level bytes and signed ordinal. Invalidate on
   context recreation, mission teardown or an abandoned transition.
2. Keep the live admission checks and the existing native publication
   boundary. Run the protected Pumo route on the Android 14 emulator with
   the original archive and compare the timings at the same resolution.
   Confirm that a direct save reload can still rebuild resources.
3. Run the Windows graphical and headless builds, the relevant resource
   contracts and scoped formatting.
4. Keep the Android loading overlay visible through the native texture
   cache after a briefing, with an indeterminate bar rather than an
   invented percentage.

## Verified result, 2026-11-21

The same original Pumo Rookie route at 640 x 480 and 6:13 aspect reached
hub ordinal 1, then Crauphel ordinal 2, fired a missile in room 129,
saved, reloaded, and fired again. The protected run passed all 35 actions
and restored the original pilot profile hashes. The preparation owner was
reused exactly once for each travel load; the save reload rebuilt normally.

| Stage                | Build 32121 | Build 32122 |
| -------------------- | ----------: | ----------: |
| Hub preflight        |    14.848 s |     9.389 s |
| Hub native load      |    17.572 s |     8.787 s |
| Crauphel preflight   |    10.895 s |    13.111 s |
| Crauphel native load |    16.339 s |     7.591 s |
| Crauphel save reload |    11.940 s |    11.814 s |

The preflight timing varies in this two-run sample, so the supported
conclusion is that the repeated native load work fell by 8.8 s for hub
and 8.7 s for Crauphel. The preflight itself still takes about 9-13 s on
the emulator. The progress overlay uses an indeterminate moving bar for
this phase because it receives no finite item count. The native texture
cache also needs this overlay after the briefing; the 32122 run had
several seconds between XL preparation and the playable game while
caching 2,590 bitmaps.

Windows graphical and headless builds, seven XL resource contracts, both
automation catalog validations, the Android ordinary APK build, and the
protected original-archive emulator integration passed. No original game
asset or save was added to Git.

## Overlay follow-up, 2026-11-22

Build 32123 keeps the Android loading overlay open across the whole native
level load, including the texture cache after the briefing. The protected
original-archive route again passed all 35 actions, the two prepared
travel owners were reused, and save reload rebuilt resources. An emulator
screen capture during the hub texture cache showed the moving bar. The
capture observer added logcat polling during this run, so its elapsed
times are not used as a clean performance comparison. Both catalog
validations passed again. The pause between the level-destroyed score
screen and the next briefing still needs a physical-phone visibility
report.
