# Plan: Axis Fix for Existing Players + Parameterized Test Scripts

## Task 1: Fix LY axis mapping for existing players

### Root cause
`android_apply_gamepad_defaults()` is only called for NEW players (in
`MakeNewPlayerFile()`). When loading an EXISTING .plr file via
`read_player_file()`, the axis bindings stored in the file are used as-is.
Old .plr files have DOS defaults: Pitch=axis 1 (LY), Throttle=unbound.

Additionally, if a stale `controller_config.json` exists from before the
Kotlin DEFAULT_BINDINGS fix, new players ALSO get wrong values because
`android_apply_gamepad_defaults()` reads controller_config.json first.

### Fix
1. d2/main/menu.c: After `read_player_file()` in `player_menu_handler()`,
   add `#ifdef ANDROID` block calling `android_apply_gamepad_defaults()` +
   `kc_set_controls()`
2. d1/main/menu.c: Same change
3. run_test.ps1: Also delete controller_config.json in $preLaunch to avoid
   stale config interfering with tests

## Task 2: Parameterized test scripts

### Design: _info.vars + per-step "when" field

The `_info` element gains a `vars` section keyed by game ID. Steps can use
`${VAR}` placeholders that the PowerShell runner resolves before pushing.
Steps can have a `"when": "d1"` or `"when": "d2"` field; non-matching steps
are filtered out by the runner.

Example:
```json5
[
    {"_info": {
        "games": ["d1", "d2"],
        "vars": {
            "d1": {"MISSION": "First Strike", "LEVEL": "11"},
            "d2": {"MISSION": "Counterstrike", "LEVEL": "23"}
        }
    }},
    {"action": "select", "text": "${MISSION}"},
    {"action": "key", "key": "backspace", "when": "d1"},
    {"action": "key", "key": "1", "when": "d1"},
    {"action": "key", "key": "1", "when": "d1"}
]
```

### Implementation
1. Add `Resolve-TestScript` to test_helpers.ps1: reads .json5, resolves
   vars, filters `when` clauses, writes temp file, returns temp path
2. Update run_test.ps1 to call Resolve-TestScript before pushing
3. Rewrite death test as parameterized d1+d2
4. Merge automap tests into one parameterized script

## Task 3: Review other scripts for parameterization

- test_launch_to_automap + test_launch_d1_automap --> merge into one
  parameterized script
- test_axis_mapping: uses Counterstrike (D2) and D2-specific introspection
  fields; keep D2-only for now
- test_saf_basic: D2-specific SAF archiver test; keep D2-only
