# Profile EQ popup

1. Store EQ overrides per OPL3 / bundled / imported SoundFont profile, default bundled to Balanced and others to Flat
2. Resolve the target profile before native activation, including SoundFont fallback and rollback
3. Replace inline controls with profile buttons and a popup showing a logarithmic frequency-response graph from the shipped preset parameters
4. Validate profile isolation, defaults, reset/export, graph response, installed popup interactions and native D1/D2 playback; build Android and run scoped formatting

Completed:

- Added `eq opl3` below the renderer choices when FM is selected and `eq [soundfont name]` below the soundfont selector when SF2 is selected
- Removed the inline EQ section; each button opens its profile popup with a 20 Hz-20 kHz logarithmic response graph and +/-12 dB grid
- Bundled SC-55 defaults to measured Balanced, other profiles to Flat. Explicit overrides persist by profile; reset clears overrides, and configuration export/import includes the profile map
- SoundFont fallback resolves the selected bank's EQ independently of the OPL3 profile. Activation resolves the target font before persistence, avoiding old-profile EQ on font switches
- Graph evaluates the shipped RBJ parameters and preamp. Independent SciPy reference checks passed for all three curves; Flat is exactly zero
- Android debug builds passed. All 21 targeted JVM tests passed. Installed-app integration passed popup selection/smoothing, profile isolation, persistence, all four presets with D1/D2 previews, seek/pause/resume, FM bypass, and native EQ playback in both games
- Visually checked popup captures and adjusted frequency labels to avoid overlap. Final UI-only integration passed after the layout polish. Captures: `temp/profile-eq-ui-final/measured-popup.png` and `opl3-popup.png`
- Scoped code-quality and diff checks passed. Device tests restored original preferences
