# Vertigo briefing previews and original behavior audit

## Outcome

The user confirmed importing only `d2x.hog` and `d2x.mn2`. Those files do not contain the robot movies. The appropriate repair is to import `d2x-h.mvl` (high resolution) or `d2x-l.mvl` (low resolution) from the owned Vertigo installation or disc. Both local archives are available under `game_data/extracted/VERTIGO/`, as well as the extracted Vertigo CD directories. No engine translation fix is warranted for this installation.

This investigation made no production code changes and did not modify outstanding_bugs.md. Existing unrelated working-tree changes were preserved.

## Evidence and comparison with main

- Decoded the original `game_data/extracted/VERTIGO/MISSIONS/D2X.HOG` briefing `d2x.txb`. Section `$S1` contains `$R3` for Compact Lifter and `$R9` for Fervid 99, after the story pages
- Read the directory of the original `D2X-H.MVL`: it contains ten movies, including `RB3.MVE` and `RB9.mve`. These are prerecorded movies, not previews generated from the HAM robot models
- `d2/main/titles.c` still translates `$R` followed by a character into `rb<character>.mve` and calls `InitRobotMovie`, as main does
- `d2/main/mission.c` still initializes the mission movie library after loading enhanced mission data. The D1-in-D2 wrapper returns the D2 loader's extra-robot result, preserving that gate for Vertigo's `zname` descriptor
- Main's `movie.c` also opens an external `<mission>-h.mvl` or `<mission>-l.mvl`; it does not synthesize a model preview when the movie is absent. Current code additionally supports fallback to the other resolution
- `InitRobotMovie` logs a missing movie and returns false. The surrounding text still displays, so absent optional media can look like a rendering failure

These observations explain the reported installation without attributing it to a renderer regression. Playback with the additional archive has not been verified on the user's release app; its private files were inaccessible through `run-as` because the package is not debuggable.

## What this says about regression risk

This example does not establish a pattern of breaking main's renderer. It does expose a testing weakness: importability and successful level launch are weaker conditions than preservation of the complete original experience. In particular, `FileSetContentManagerTest.realVertigoInventory` copies only HOG and MN2, and existing Vertigo game scripts skip the briefing. Those checks cannot detect missing movie dependencies, incorrect animation, or visual corruption on these pages.

The larger risk is changing resource ownership and search paths while retaining engine code that assumes a traditional installed directory. A faithful port must preserve both the bytes and the circumstances under which the engine can find and release them. Source similarity by itself cannot establish that.

## Adjacent audit leads, not reproduced failures

1. **Mission movie lifetime**: `init_extra_robot_movie` closes a prior movie library only when another enhanced mission initializes one. `free_mission` and `android_mission_asset_reset_before` do not close it. Switching to Counterstrike or D1 can therefore retain a mission movie mount. The lifecycle gap also exists on main, but immutable Android mission publications make stale mounts and later relative-path cleanup more consequential. Test Vertigo -> Counterstrike -> Vertigo -> D1, including package replacement, before choosing a fix
2. **Disc layouts with movies outside the descriptor directory**: `physfsx_android_mount_mission_directory` already exposes descriptor siblings, so simply adding that mount again would be a speculative and redundant fix. However, `stageDiscContent` moves descriptor directories under `missions/`, while other nested directories retain their paths. The published context mounts its root and the engine adds the descriptor directory; a movie left in another directory may be inaccessible by basename. Audit both extracted-disc and installed layouts with original assets
3. **Dependency visibility**: `SetupGameFiles.kt` lists the base game's optional robot movies but not Vertigo's. Successful HOG/MN2 import currently does not tell the user that briefing animation requires additional media. Any completeness indication should keep this optional: a mission without movie files remains playable

## Useful compatibility coverage

| Boundary | Required observation |
| --- | --- |
| Original asset inventory | Briefing movie requests resolve in the selected installation; missing optional media remains playable |
| Import and publication | The same requests resolve after direct import, installed-directory import, and disc extraction |
| Briefing runtime | Actual Compact Lifter and Fervid 99 frames advance and loop; verify palette and aspect ratio |
| Mission transitions | Movie archives belong to the active mission and are released before its source context is removed |
| Other presentation resources | Loose briefing text/images, ending text/movies, and music still resolve after the same transitions |

Prefer end-to-end checks through the original engine and actual published assets over assertions that only verify that a file exists in a staging directory. Keep optional-media absence separate from full-installation fidelity, and compare representative original behavior before accepting resource-management refactors.

## Validation limits

Source comparison and original HOG/MVL inspection completed. An exploratory JVM import test was attempted, but `:app:compileDebugKotlin` failed before tests because Gradle could not delete its classes directory while other build processes were active. The exploratory test was removed; no unverified test or speculative production fix is left in the tree. No unrelated build processes were stopped, and no new regression is claimed from that failed run.
