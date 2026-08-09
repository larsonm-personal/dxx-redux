# Track selector shared render geometry

## Goal

Make the music track-selector buttons use identical sizing and placement in the
touch editor preview and the live game overlay.

## Plan

- [x] Trace all editor and runtime scale inputs for the track selector
- [x] Centralize the complete render geometry and apply it in both paths
- [x] Add regression coverage that compares preview and runtime inputs
- [x] Run scoped formatting, unit tests, Android build, and emulator smoke check
- [x] Record the completed behavior and any remaining limitations

## Result

The editor and runtime now calculate track-selector button, spacing, arrow, and
label geometry from the same full overlay surface dimensions. The editor hit
area uses that same geometry. The editor also previews the runtime ring and
arrow glyphs. No remaining sizing limitation is known for this control.
