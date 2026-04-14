## Metl154 Runtime Diagnostic

### Goal
- add a throttled on-device runtime diagnostic for the remaining `metl154` one-direction transparency issue
- keep logging volume low enough for normal gameplay captures

### Plan
1. Add a once-per-second texture debug log when the runtime draws `metl154` through the merge path.
2. Include the fields most likely to explain the one-direction clue: overlay orientation, shader path, flags, mask presence, and raw UV range.
3. Keep the diagnostic Android-only and no-op for other textures.
4. Validate with Android build/test tooling and update notes.

### Status
- [x] Diagnostic added
- [x] Logging throttled
- [x] Validation run
- [x] Notes updated

### Notes
- the metl154 diagnostic now matches the bitmap name case-insensitively so uppercase in-game names still trigger the log