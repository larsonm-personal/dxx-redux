# Pumo S21 briefing progress, November 23

## Reproduction

The connected Samsung SM-G996U (Android 15) ran the isolated diagnostic
package `com.dxxredux.app.nsdtest` with the original Pumo v1.1 archive.
The protected 32160 run passed the 35-step original hub-to-Crauphel route,
including a missile shot in Crauphel room 129 before and after quickload.
Its screen recording shows a black frame with no progress indicator between
the level 1 score screen and the level 2 briefing. The raw log places the end
of the `mvlev1.rl2` preflight at 01:09:48.329 and the first briefing canvas
at 01:09:49.785, a 1.456-second interval. The hub interval was 1.477 seconds.
The private recording is retained in ignored `temp/` with SHA-256
`f3e35ab9c64d85eddb46a7b9a015b2da89421cebb66d5f334d31a172850383b1`.

## Change and verification

For an Android single-player XL add-on with a prepared transition,
`ShowLevelIntro` now starts an indeterminate progress indicator before intro
asset preparation. The first successfully prepared briefing page dismisses
it; the intro return and preflight cleanup also dismiss it if briefing
construction exits early. The hook does not own another active progress
indicator and does not span built-in intro movies or D1 emulation.

The 32180 diagnostic APK, SHA-256
`ba923c75fd3647ff63746aa7cd7e3cf8c3c8b64b55ca8e87aeb4b25dc11edf97`,
was built from this branch and installed only into the isolated package.
The same original archive, Rookie, 640 x 480, hub-to-Crauphel route passed
35/35 steps on the S21. The shot and quickload still passed, and the runner
restored every protected profile hash. The private route receipt is
`temp/xl-pumo-s21-20261122/briefing-phone-route/owner-result.json` with
warm run ID `300ee619f50d4cbd9dede5845cadadf3`. Its private screen
recording, SHA-256
`d0c0f17cab21c1b5644e6ab8fd98529add21edb595cd6ab0943ad07c67cfe161`,
shows the indicator over the black interval after the score screen.
Preflight ended at 01:24:56.360 and the first briefing canvas appeared at
01:24:57.832, a 1.472-second interval. This is a visibility fix; it does
not claim to shorten the interval. The other reported interval from the
last briefing page to flight remains a separate phone observation.

Windows D2 and all three Android ABIs built, and scoped code quality passed.
The diagnostic APK and both recordings remain ignored. Automated steering
on this S21 does not substitute for the human report on the SM-S931U.
