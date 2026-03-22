# Draft: copilot-instructions.md Additions

These are proposed additions based on lessons learned from test infrastructure work.
Review and approve before adding to `.github/copilot-instructions.md`.

---

## Testing infrastructure

### Terminal buffer pollution
- Always pipe test output to files (`Out-File`). Never trust VS Code terminal scrollback --
  it accumulates stale adb/logcat output across runs
- Always `adb logcat -c` before tests to clear stale log lines
- For long-running operations, write helper .ps1 scripts and read results from output files

### Emulator health checks
- `adb shell getprop sys.boot_completed` can take >5s under heavy game load (level loading,
  rendering). A slow response does not mean the emulator crashed
- The test runner retries health checks once (5s wait) before declaring a crash
- Check automation_log.jsonl for recent progress before concluding the emulator is dead

### Pilot file cleanup
- Use `Reset-GameState` from test_helpers.ps1 to delete .plr, .plx, descent.cfg,
  controller_config.json before each test. Do not duplicate deletion logic in test scripts

### Game data provisioning
- Place game data files (DESCENT2.HOG, DESCENT2.HAM, etc.) in
  `game_data_to_copy_to_emulator/data/`. These are copyrighted and not committed to git
- Run `push_game_data.sh` to push them to the emulator
- Tests that need game data (test_autoselect_crash, test_saf_archiver) will fail without this

### descent.cfg format
- When writing descent.cfg programmatically, always include AspectX and AspectY alongside
  ResolutionX and ResolutionY. Game defaults are AspectX=3, AspectY=4 (4:3 ratio)
- SetupActivity.writeInitialGameConfig skips writing if the file already exists, so
  pre-creating it in the test controls the game's configuration

### D1 vs D2 differences
- D1 has 48 joystick kconfig items; D2 has 56 (d1/main/kconfig.c vs d2/main/kconfig.c)
- Control names differ in capitalization: D1 "Reverse" vs D2 "reverse", D1 "Rear view"
  vs D2 "REAR VIEW", D1 "Throttle" vs D2 "throttle"
- D2 has extra controls not in D1: Afterburner, Headlight, Energy->Shield, Toggle Bomb
- The launcher has separate KC_JOY_META_D1 and KC_JOY_META_D2 metadata arrays and
  separate key_settings_joystick_d1/d2 byte arrays in controller_config.json
- Controller introspection broadcast accepts `--es game d1` or `--es game d2` to select
  the correct metadata

### json5 automation scripts
- The `when` field in json5 steps is processed by `Resolve-TestScript` in test_helpers.ps1
  -- it filters steps by game ID and removes the `when` property before pushing to device
- Prefer `select` (text-based) over `key enter` for menu navigation where possible, as
  this is more robust across D1/D2 menu differences
- Use `when` to gate game-specific steps (e.g., D2 shows "Ok" button that D1 does not)

### Matchmaking server for tests
- run_all_tests.ps1 and test_bot_client.ps1 accept `-AutoServer` to auto-build and start
  the matchmaking server. The server runs with `SKIP_GPGS_VERIFY=true` for testing
- The server binary is at `server/target/release/dxx-matchmaking.exe` (or debug/)
- Server listens on port 9000. Tests check TCP connectivity before proceeding
