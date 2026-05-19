# Plain Texture DXA Base Version Preflight 20260518

## Goal
Make plain texture DXA manifests state the required base data version clearly, add machine-readable compatibility checks for launcher preflight, and improve engine-side patch failure messages.

## Tasks
- [x] Identify current baseline file hashes and how to describe them
- [x] Add required-version notes and compatibility requirements to generated manifests
- [x] Add verifier checks for compatibility metadata
- [x] Add a launcher-side preflight design and helper implementation if the existing launcher structure supports it cleanly
- [x] Improve D2 engine patch errors for failed JSON Patch application
- [x] Regenerate DXAs and run validation

## Direction
- Keep archive paths and source lookup names generic
- Keep detailed file-format logic in native/converter-side code where possible
- Use SHA-256 requirements so the launcher can give precise mismatch errors before starting the game
- Treat multi-version patch support as a future semantic remapping problem, not a blind byte patch problem

## Results
- D1 manifests now note the required D1 retail v1.4a/v1.5 `DESCENT.PIG` and `DESCENT.HOG` hashes, including GOG or Infinite Abyss installs that preserve those files
- D2 manifests now note the required D2 retail v1.2 `DESCENT2.HAM` and six PIG hashes, including GOG or Steam installs that preserve those files
- `compatibility.requiredBaseFiles[]` gives the launcher filename, SHA-256, version label, size, and reason for every required base file
- The verifier now rejects generated DXAs that lack the required-base note or compatibility hash metadata
- `ModManager` checks enabled DXAs against the active file set before writing `.active_mod_paths`
- Normal launcher starts show a detailed selectable dialog with the required-version note and expected/found SHA-256 values when preflight fails
- Automation, command, demo replay, and multiplayer launch paths use the same preflight and log failures
- D2 HAM patch application now validates RFC 6902 `test` operations before applying changes and logs the failing semantic path plus expected/actual JSON on mismatch

## Validation
- `game_data\mods\xfing\convert-xfing-minimal-dxa.ps1 -Game both`
- `game_data\mods\xfing\verify-xfing-minimal-dxa.ps1`
- `android\run-code-quality.ps1 -Fix -Paths ...`
- `run-windows-build.ps1`
- `ctest --test-dir buildd1 --output-on-failure` and `ctest --test-dir buildd2 --output-on-failure`; no tests registered, exit code 0
- `android\gradlew.bat ':app:compileDebugKotlin' ':app:buildCMakeDebug[arm64-v8a]-2'`