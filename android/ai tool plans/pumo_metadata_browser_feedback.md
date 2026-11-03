# Pumo metadata browser feedback

## Intent

Make the mission browser report useful whole-package feature support and actual
soundtrack selections, without confusing runtime diagnostics or unsupported
gameplay with an inability to inspect supported metadata.

## Work

1. Remove last-run D1/D2 mod-use diagnostics from the normal package details view;
   retain engine diagnostic reports for debugging.
2. Replace the compatibility filename strip with one aggregate feature list.
   Show supported, partial/unverified and unsupported states with green/yellow/red
   labels, preserve uncertainty honestly, and keep supported analysis available.
3. Restrict the soundtrack browser to music references/playlist sources, excluding
   sound-effect assets. Preserve contextual menu/briefing/credits and level uses.
4. Correlate the original Pumo manual song list, package playlists and OGG tags;
   add a source-identified chromaprint hint catalog without changing mission audio.
5. Extend relevant model/UI tests, run scoped mixed quality, Kotlin/native checks
   where affected and an Android build. Use only an owned disposable profile if
   device validation is needed; preserve published playtest bundles and user data.

## Boundaries

This is a new feedback task, not continuation of the expired timed goal. No new
deadline is invented. Existing gameplay support claims stay bounded by evidence;
unknown inspection counts must not be invented or painted as verified support.

## Implemented

- Removed last-run D1/D2 mod-use diagnostics from package details. Engine logs remain.
- Replaced the filename strip with a package feature summary; native `supported`
  now maps to green Supported. Partial/unverified rows are yellow and unsupported
  rows red. Container placeholders cannot erase inspected level evidence.
- Kept feature inspection and movie scans available independently of classic-engine
  route/statistics support. Movie-only native requests return before level decoding.
- The browser uses explicit song references and authored music directories, excluding
  sound-effect directories unless a playlist explicitly references their audio.
  The full catalog remains available for preview and alternate MIDI metadata lookup.
- Added Pumo's 17 fingerprinted OGG slots and reviewed song titles. The manual
  page 20, embedded Vorbis tags and original descriptor corroborate the mapping:
  title/briefing/credits; level01 hub; level02..09 MV1..8; level10 hub theme again;
  level11..13 KM1..3; level14 MV secret. Artist: R.a.M. Land (Ruben "Pumo" Acevedo).
- The album generator indexes repeated identical audio with the same reviewed title
  once. This retains all 16 Pumo titles without falsely declaring the hub theme
  ambiguous. It also corrects the same preexisting duplicate-slot issue in Ulterior's
  reviewed catalog (19 unique songs); differing titles remain subject to ambiguity checks.

## Validation and limits

- XL presentation model: 8 tests passed. Music catalog: 15 tests passed, including
  sound-folder exclusion and an explicit WAV playlist exception.
- Rendered feature dialog: passed on owned DxxTraversal20261102 emulator, light/dark
  colors, aggregate view, unknown/absent states, explanations, Back and scrolling.
- Fingerprint generation: 17 files, no AcoustID network lookups; database assertion
  confirms 16 indexed songs and every distinct source title retained.
- Fingerprint source identity checks passed. Scoped mixed formatting/lint passed
  before the final movie-only guard adjustment; final rerun and original-package
  integration results are recorded in temp/pumo-browser-feedback.
- This task does not qualify additional gameplay, route simulation or physical-device
  behavior. Native runtime admission and original mission content are unchanged.
- Changes remain uncommitted on the existing worktree. No push, PR or merge performed.
  Previous playtest bundles and the preexisting stash remain intact. Debug APKs here
  are emulator validation builds, not a newly published physical-device playtest.

Final checks: original Pumo integration passed (17 soundtrack entries, one source;
archive/HOG/preview hashes preserved). Final scoped mixed quality passed. The final
x86_64 debug APK embeds the exact checked-in database (SHA-256
841ca9ac74563270299fa45606f0b5af62094221e73e2564e26230a85c2fef4d).
APK SHA-256: 4e280d4ca9390da75aaf698f9872e6c56a3fac6b76e8b48483240d8ffc6f05b6.
Base HEAD: 8d228564aab9cb99903328965b37825a5e13eded plus these uncommitted changes.
Evidence: temp/pumo-browser-feedback (build/test/format logs, screenshots, extracted
manual page and audio metadata). No physical-device validation or full-campaign
qualification was performed for this browser task.
Final Gradle invocation completed successfully (3m 5s). Final installed-APK dialog
instrumentation passed. Owned emulator-5556 was gracefully stopped after verifying
DxxTraversal20261102 identity; the remaining device list contains only the preexisting
emulator-5554 and physical device. The owned profile and prior saves remain retained.
