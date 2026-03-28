# Plan: Unified Launcher+Game JSON5 Scripting

## TL;DR

Move pre-game launcher actions from PS1 scripts into JSON5 game scripts using a unified
linear step array. New `enter_launcher` / `enter_game` mode-switch elements partition the
script into segments processed by Kotlin (SetupActivity) or C (game engine). Scripts can
pass back and forth between runtimes any number of times. PS1 scripts shrink to minimal
wrappers (push files, send broadcast, watch result).

## Architecture

### Mode switching

A single JSON5 array contains ALL steps. Two special actions switch the processing runtime:

- `{"action": "enter_launcher"}` -- Kotlin (SetupActivity) processes subsequent steps
- `{"action": "enter_game", "game": "d2"}` -- launches the game process; C engine processes
  subsequent steps starting at the next index

The script can alternate between modes any number of times.

### Lifecycle flow

1. PS1 pushes JSON5 to device, sends `SETUP_AUTOMATE` broadcast to SetupActivity
2. SetupActivity parses the full script, begins executing from step 0 (must be `enter_launcher`)
3. Launcher processes steps: `log`, `wait_ms`, `wait_for`, `assert`, `setup_command`
4. On `enter_game`: launches game via intent with extras
   `automation_script=<path>` + `automation_start_step=<N>`
5. C engine loads script, skips to step N, executes game steps
6. On `enter_launcher` (in C engine): writes `automation_result.json` with
   `{"result":"LAUNCHER_CONTINUE","next_step":<M>}`, then triggers graceful game exit
7. Game process dies. SetupActivity.onResume() fires
8. SetupActivity reads result, sees `LAUNCHER_CONTINUE`, resumes executor from step M
9. If end-of-script reached (either runtime): writes `{"result":"PASS"}`
10. PS1's `Watch-AutomationResult` sees PASS/FAIL, reports result

### Step types by runtime

**Both runtimes** (same JSON format, different data source):
- `log` -- `{"action": "log", "message": "..."}`
- `wait_ms` -- `{"action": "wait_ms", "ms": 5000}`
- `wait_for` -- `{"action": "wait_for", "field": "...", "value": "...", "timeout_ms": ...}`
  (launcher polls setup_introspect; game polls game_introspect)
- `assert` -- `{"action": "assert", "expect": {...}}`
  (same polling difference)

**Launcher-only** (new):
- `setup_command` -- `{"action": "setup_command", "command": "import_gog",
  "args": {"path": "...", "include_audio": true}}`
- `reset_state` -- `{"action": "reset_state"}`
  (deletes .plr/.plx/descent.cfg/controller_config.json)
- `write_config` -- `{"action": "write_config", "file": "descent.cfg",
  "content": "ResolutionX=640\nResolutionY=480\n"}`

**Game-only** (existing, unchanged):
- `key`, `select`, `skip_briefing`, `send_axis`, `send_button`, `assert_overlay`, `introspect`

**Mode switches:**
- `enter_launcher` -- processed by both runtimes (C engine yields; Kotlin continues)
- `enter_game` -- processed by launcher (launches game with start_step)

### _info block

- Scripts with `enter_launcher` as first real step are "launcher scripts"
- `_standalone: true` means run_test.ps1 can handle them (auto-detects launcher path)
- `_standalone: false` means a custom PS1 is still needed (for large file pushes, etc.)
- `_deps`, `games`, `vars`, `when` unchanged

### Result file protocol

`automation_result.json` gets a new result value:
```json
{"result":"LAUNCHER_CONTINUE","next_step":22,"steps_completed":15,"total_steps":40,"elapsed_ms":12345}
```

Existing `PASS` and `FAIL` values unchanged.

## Implementation phases

### Phase 1: C engine changes (game_automate.cpp / .h)                     [x]

1. Add `STEP_ENTER_LAUNCHER` to `enum step_type`
2. Add `STEP_ENTER_GAME` (skip/no-op in game engine -- launcher-only)
3. Update `parse_script()` to recognize `"enter_launcher"` and `"enter_game"` actions
4. In `game_automate_tick()`, handle `STEP_ENTER_LAUNCHER`:
   - Write result `"LAUNCHER_CONTINUE"` with `"next_step": g_current_step + 1`
   - Set `GameArg.SysQuit = 1` to trigger graceful game exit
5. Add `g_start_step` global; `game_automate_set_start_step(int)` setter
6. In `game_automate_tick()` script-load path: set `g_current_step = g_start_step`
7. Skip launcher-only steps (setup_command, reset_state, write_config) -- treat as no-op
   in C engine (log + advance)
8. Expose `game_automate_set_start_step` in game_automate.h

### Phase 2: JNI bridge + MainActivity                                     [x]

9. jni_main.c: add `nativeSetAutomationStartStep(int)` JNI function
10. MainActivity.kt: declare `external fun nativeSetAutomationStartStep(step: Int)`
11. MainActivity.kt: in `onStart()` or game-started handler, read intent extras
    `automation_script` + `automation_start_step`. If present, call
    `nativeSetAutomationStartStep()` then `nativeLoadAutomationScript()`
12. Keep existing AUTOMATE broadcast receiver working (backward compat)

### Phase 3: Kotlin launcher script executor                               [x]

13. Create `LauncherScriptExecutor.kt`:
    - JSON5 preprocessor: strip `//` comments + trailing commas, then `org.json`
    - `suspend fun execute(context, scriptPath, startStep)`: sequential step loop
    - Step handlers: log, wait_ms, wait_for, assert, setup_command, reset_state,
      write_config, enter_game, enter_launcher (no-op)
    - `wait_for` / `assert`: trigger setup introspect, read setup_introspect.json,
      check field values
    - `enter_game`: build intent with extras, launch game activity, suspend executor
    - On PASS / FAIL: write automation_result.json
14. SetupActivity.kt:
    - Register `SETUP_AUTOMATE` broadcast receiver (debug builds only)
    - On receive: call `LauncherScriptExecutor.execute(scriptPath, 0)` in coroutine
    - In `onResume()`: if game just exited, read automation_result.json. If
      `LAUNCHER_CONTINUE`, resume executor from `next_step`. If `PASS`/`FAIL`, done

### Phase 4: PS1 runner updates                                            [x]

15. test_helpers.ps1: Watch-AutomationResult -- handle `LAUNCHER_CONTINUE` as still-running
16. test_helpers.ps1: add `Start-LauncherScript` helper (push script, SETUP_AUTOMATE, watch)
17. run_test.ps1: auto-detect launcher scripts (first real step is `enter_launcher`).
    Use `SETUP_AUTOMATE` instead of game launch + AUTOMATE

### Phase 5: Migrate test_gog_installer_redbook                           [x]

18. Rewrite test_gog_installer_redbook.json5 as unified launcher+game script
19. Simplify test_gog_installer_redbook.ps1 to: push GOG exe + invoke

### Phase 6: Migrate remaining high-value tests                            [x]

20. test_resolution -> unified JSON5 (launcher->game->launcher->game)
21. test_autoselect_crash -> unified JSON5 (launcher->game->launcher->game)
22. test_controller_compare -> unified JSON5 (launcher->game->launcher) -- may defer

### Phase 7: Verification                                                  [x]

23. Run test_gog_installer_redbook end-to-end
24. Run test_resolution end-to-end
25. Run standalone tests for regression check
26. Run code quality: `android\run-code-quality.ps1 --fix`

## Test migration assessment

### High value -- move most logic into JSON5
| Test | What moves to JSON5 | What stays in PS1 |
|------|---------------------|-------------------|
| test_gog_installer_redbook | clear, import, audio verify, MIDI/CD preview, in-game | GOG .exe push, emulator health |
| test_resolution | reset, default game, write config, explicit game | emulator health, deps |
| test_autoselect_crash | reset, create pilot, write_autoselect, menu nav | emulator health |
| test_controller_compare | patch, introspect, game script, compare | emulator health, deps |

### Already optimal -- no change needed
9 standalone scripts: test_launch_to_automap, test_fire_primary, test_death,
test_axis_mapping, test_dpad_triggers, test_joystick_menu, test_keyboard_defaults,
test_keyboard_viewport, test_keyboard_manual

### Not applicable -- stays PS1-only
test_mp/lan/dual_emu (multi-emulator), test_cue_iso/fpcalc (desktop),
test_bot/server (Rust), test_all_extracts (meta), test_extract (host hashes),
test_autoselect_plx (static validation), test_saf_* (SAF intents)

## Decisions

- Mode switch syntax: `enter_launcher` / `enter_game` as action names
- Step handoff: intent extras on game launch, result file on game exit
- Script detection: first real step being `enter_launcher`
- Game exit trigger: `GameArg.SysQuit = 1` after writing LAUNCHER_CONTINUE
- Launcher executor: Kotlin coroutine in SetupActivity (not a separate service)
- PS1 keeps: file pushes, emulator health, APK install, multi-emu orchestration
- Backward compat: existing game-only scripts work via AUTOMATE unchanged

## Further considerations

1. Numeric assert comparators (gt/lt/gte/lte) -- needed for position_ms checks.
   Add to both launcher and game assert
2. Controller compare cross-phase data -- launcher reads both
   controller_introspect.json and introspect.json in final phase
3. JSON5 parsing in Kotlin -- strip // comments + trailing commas, then org.json
