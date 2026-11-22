# Pumo physical phone follow-up, November 21

Samsung SM-S931U, Android SDK 37, ARM64, Play Store internal build 31950
(`9d615a9a1`, `com.dxxrevival.app`). The player started a new game, left the
hub for Mineral Valley and fired a missile in **Crauphel Scilab**. The engine
exited with "XL lightning projected extent exceeds the source integer range".
The phone debug log identifies Rookie, touch input and level 2
(`mvlev1.rl2`). The last sampled viewer segment was 129, approximately one
second before the exit. Missile type, exact impact position, facing and the
outcome after a cold reload are unknown. The fallback report has no native
stack. The export-time mod list describes a later snapshot. The phone log
identifies the loaded Pumo HOG but does not fingerprint the archive at the shot.
Original report retained privately, SHA-256
`77af714c6e92c580f3c0386c50b4ddb8cbbc1cfd462704be48fb87560037e3bc`.
The separately supplied historical debug log is retained privately, SHA-256
`a9e3c66e76fd034f43761bdc26ffa71c159f19dd80bd98546bae764dab0fe20f`.

The phone recorded a 640 x 480 briefing canvas 14.76 s after the Rookie
selection. Native loads took 11.04 s for the hub and 12.23 s for Crauphel.
The last recorded touch release before the next briefing preceded its canvas
by 1.82 s. The log does not measure how much of that interval showed a bar.
Crauphel finished loading at 16:39:36.956. Touch steering continued until
the final second. The fatal error followed at 16:39:51.297. The player's
green-on-green observation was on a briefing page. The page number is unknown.

1. Exercise the camera-plane projection in the shared lightning and fireball
   renderer. Preserve ordinary viewport bounds and keep visible geometry when
   a finite vertex has no representable screen point. Repeat the missile shot
   on a rendered build.
2. Trace the reported stock D2 MIDI through the Mission zip selection with the
   original v1.1 archive. Check the briefing and first ordinal independently.
3. Time the roughly ten-second new-game resource check separately from the
   two reported two-to-three-second waits (last briefing page to flight and
   level-destroyed screen to the following briefing). Identify reusable work
   and cover any remaining wait with the Android progress display.
4. Check the authored foreground and shadow on the opening briefing pages.
   Use a page or line from the tester to identify any remaining contrast issue.

## Independent hub reproduction

On the Android 14 x86_64 emulator, the ordinary 32080 kit imported the
original v1.1 archive (SHA-256
`82c8cc4cfc740c8169f9d21a961c9ac5d2518342b3e27ac3ac438c91ca8f2ddf`).
A fresh level 1 start on Trainee had seven concussion missiles. Without
moving or aiming the ship, the test tapped Fire Secondary once. About two
seconds later the engine emitted the same fatal message as the phone. These
are the emulator's controls and difficulty. The phone shot was in Crauphel.

During the same run the mixer played `d2-original/briefing.hmp` and
`d2-original/game01.hmp`. The pilot's mission-soundtrack preference was on.
The selected mission contained `mods/pmines/music/level01.ogg`.
The build machine's original soundtrack identifies the briefing as
"Unveiling" and the hub as "Alone - Ambient Remix", both by R.a.M. Land.

Selecting Trainee to the first briefing canvas took 9.00 s on the emulator.
After dismissing the briefing, the native level load took 12.71 s, including
5.74 s preparing the level file and 5.64 s loading textures. The phone's
reported 2-3 s gaps were estimates on a different GPU and build.

## Candidate checks

On ordinary build 32120 the same shot reduced concussion ammo from seven to
six. The pilot stayed alive and the game retained control. The mixer opened
`mods/pmines/music/briefing.ogg` and `mods/pmines/music/level01.ogg`.
Switching to MIDI and back restored the mission track. The rest of the test
used a fixed position for the Kartsal entrance, traveled from 1 to 11,
reloaded the hub save and traveled to 11 again. All 45 actions passed and
the owner verified the original profile hashes after cleanup.

The first candidate took approximately 21 s from difficulty selection to the
briefing and 20.07 s for the first native level load. Its timing did not
reproduce a speedup over 32080. In build 32121 the game reads embedded track
names on demand. The level loader reuses the last translation when the level
bytes and translation options match and checks the live resources and graphics.
The pre-briefing resource check and the post-briefing level load use the
existing Android loading display when progress bars are enabled.

The original opening uses custom foreground commands. The first background
asks for pale pink (240, 220, 220). Later backgrounds ask for yellow and
blue. The old briefing parser interpreted each command as the default green.
The candidate resolves both authored colors against the current picture's
palette and retains each glyph's foreground and shadow. The first Crauphel
briefing background requests yellow. Several later pages request bright green
and retain the authored shadow in the candidate.

The local retest build is an ordinary universal debug APK, version 1.1
(32121), package `com.dxxredux.app`, SHA-256
`9e3f6050130a61f3566ac53504c5ea4c1bd4c44fea23a76095d3b6343c3a2915`.
The phone's Play internal package has a separate data directory.

Build 32121 passed all 46 hub and Kartsal actions. The opening text used the
original high-resolution hub picture's nearest palette color (56, 56, 59 on
the six-bit palette). The first briefing canvas took 13.73 s after difficulty
selection and the native hub load took 13.53 s. Private screenshots show
"Checking level resources and graphics" after difficulty selection and after
the score screen, and "Loading level" after the briefing. The cold wait
remains measurable.

## Crauphel Scilab comparison

The authored level name and the phone's load trace agree on Mineral Valley
ordinal 2. The phone samples show the viewer moving from room 105 through
the mine to room 129. No historical event records the missile slot or camera
orientation. The Android comparison uses Rookie, a 640 x 480 game surface,
the phone's 13:6 display ratio in the config and an ordinary concussion missile.
The emulator's physical display is 16:9. A fixed pose aims
at solid side 0 in room 129 from eight units away. The owner saves before
the shot, checks the ammo change and gameplay, then loads that save and
repeats. The hub entrance is also positioned by the test.

The first comparison attempt reached Crauphel on Rookie but caught a
1920 x 1080 render configuration before firing. Its profile hashes were
restored. The runner now applies the render configuration before launch
inside the protected profile and checks both dimensions before the shot.

Build 32080 passed the 640 x 480, difficulty, level, room and pre-shot
six-missile assertions. The pre-shot save completed. Fire Secondary went
down at 17:41:28.772. At 17:41:29.694 the process emitted the same fatal
message as the phone and ended. The historical phone log does not establish
that the player aimed at side 0 or that the crash came from that specific
missile type.

The following intervals are seconds from the matched Rookie, 640 x 480
emulator runs. Each cell is a single observation. Loading the new game
continued to take longer than ten seconds on both versions.

| Interval | Control 32080 | Candidate 32121 |
| --- | ---: | ---: |
| Difficulty selected to first hub briefing canvas | 17.05 | 16.51 |
| Hub native level load | 18.37 | 17.57 |
| Advancing the hub score screen to the Crauphel briefing canvas | 15.27 | 12.86 |
| Crauphel native level load | 21.08 | 16.34 |

In Crauphel, 32121 opened `mods/pmines/music/level02.ogg`. The original
audio tags name the track "Initialization" by R.a.M. Land.

**32121 passed both shots.** The first shot used one of six concussion
missiles and the pilot remained alive. Loading the pre-shot save restored
room 129 and six missiles. The second shot again left the pilot alive, with
five missiles and normal control. The final state also identified Crauphel
Scilab and the correct soundtrack. All 35 automation actions passed
(`57d28d363807409791226044262813ac`). The owner verified the original
profile hashes after both the control failure and the candidate pass.
The pre-shot candidate save is retained privately on the build machine,
SHA-256 `e66a2adf21db39cccc1c88caa6342169c0f8867bce6a76684ba3a04f8ab54666`.

The first outstanding phone action is firing a missile on the approach to
room 129 on the Samsung using build 32121, then repeating from a normal
save. The room and projection failure are established. The player's
original heading and impact point remain unknown.
