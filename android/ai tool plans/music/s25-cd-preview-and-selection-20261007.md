# S25 CD preview and music selection

- [x] Inspect S25 registry, playlist and native playback diagnostics; reproduce CD selection failure
- [x] Fix track preview layout and the confirmed CD selection cause, with targeted diagnostics
- [x] Run scoped formatting, relevant tests and Android CMake/APK build
- [x] Install without clearing phone data and verify preview playback and D2 CD selection

Findings and fixes:

- The unweighted disc label consumed the preview row width before the weighted track title was measured. Stack the title and source label in one weighted column, provide a minimum touch height, and explain that tapping a track opens playback controls
- The source dropdown is drawn over the one-track-per-level control but handled taps after that control. Give the open dropdown priority for the whole gesture, including dismissing it without activating covered controls
- Instrumented touch logging reproduced the first CD option being intercepted on the original S25 test installation

Validation:

- Scoped mixed-language code quality and git diff --check passed
- Android arm64-v8a CMake builds for D1 and D2 and debug app/instrumentation APK builds passed
- MusicOverlaySourcesTest (9 tests) and CdAudioSourceVisibilityTest (8 tests) passed
- Existing controller overlay integration passed on RFCY703C48J, including new portrait/landscape source selection and outside-dismissal coverage and existing volume gestures
- Both automation catalog checks passed; the new assertions extend the registered controller overlay runner
- Reproduced the broken preview on the S25 using the same Definitive Collection Disc 2 image, then inspected the corrected layout and tapped Track 1 > Play. Preview advanced to 15296 ms of 44000 ms
- Launched D2 Counterstrike level 1 with MIDI, opened the music overlay and tapped CD. Introspection showed music type 2, 8 audio tracks, playing state, 48384 delivered mixer frames and zero source I/O errors

Device deployment uses the existing com.dxxredux.app.nsdtest installation. The Play-signed com.dxxrevival.app cannot be replaced with the debug signing key and was left intact. Disc test files and the updated APK are available in the isolated test app; evidence and logs are under temp/s25-music

Source Info follow-up:

- Added a bounded scroll container and the existing directional scroll indicators to the CD source details popup, keeping its title and Close button outside the scrolling content
- Original S25 accessibility dump had no scrollable node and clipped the Disc ID to 29 pixels high
- Updated S25 test app exposes a scrollable body, a down indicator at the top and an up indicator at the bottom. A real swipe reaches the final BIN path and fully displays the Disc ID at 60 pixels high
- Scoped code quality passed; device evidence and build log are under temp/s25-source-info
