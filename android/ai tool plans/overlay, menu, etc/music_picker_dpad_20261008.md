Music picker D-pad navigation

Goal:

- Match controller focus movement to the picker layout, including Close
- Use up/down for the vertical volume slider and track selection, with horizontal exits
- Preserve the selected track when moving between the list and volume

Plan:

- [x] Inspect the picker layout and existing controller and instrumentation paths
- [x] Replace linear focus movement with directional neighbors and volume adjustment
- [x] Extend the existing controller overlay integration checks for the requested routes
- [x] Run scoped formatting, Android build, and controller overlay checks

Navigation:

- Pause: up to Source, down to tracks, right to One track per level, left to Volume
- One track per level: up to Source, down to tracks, left to Pause, right to Volume
- Source: left to Pause, right to Close, down to One track per level
- Close: left to Source, right to Volume, down to One track per level
- Tracks: up/down select rows, up from the first row returns to One track per level, left/right enter Volume
- Volume: up/down adjust, left/right return to the originating control or track
- An expanded Source dropdown uses up/down for options; left/right close it and follow Source's neighbors
- Empty track lists are skipped; outer vertical edges stay in place

Validation:

- Scoped code quality checks passed for the panel, instrumentation checks, and this plan
- Debug app and Android test APK builds passed, including both native game targets
- Existing controller overlay runner passed on emulator-5586 with the isolated diagnostic app
- Music controller, source touch, and volume touch checks passed at 1000x600 and 600x1000
- The emulator run explicitly selected the APK's ARM64 libraries using the emulator's supported ARM64 translation
- Both automation catalog checks passed; coverage remains under the existing test_controller_overlay entry

Pause/Play follow-up:

- Native overlay snapshots encode paused and oneTrackPerLevel as integer 0/1, while the panel used JSONObject.optBoolean, which reads either integer as the false fallback
- [x] Decode the existing native flag representation and add pause command diagnostics
- [x] Extend the existing panel integration checks with repeated pause/refresh/resume cycles and rejected commands
- [x] Run scoped quality checks, Android build, and the controller overlay suite with the refresh regression
- The x86-64 emulator run confirmed JSONObject.optBoolean reads paused=1 as false, then passed three pause/refresh/resume cycles with the corrected decoder, checkbox refreshes, rejected commands, and existing controller/touch checks
