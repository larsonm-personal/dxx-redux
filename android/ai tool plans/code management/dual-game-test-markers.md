# Plan: Dual-Game Test Markers

## Goal
Mark .json5 test scripts that work for both D1 and D2 with a special info-only
first array element. Update .ps1 runners to detect this and either present a
game choice (interactive) or run both sequentially (non-interactive).

## Script Classification

| Script | Games | Notes |
|--------|-------|-------|
| test_controller_compare.json5 | d1, d2 | Game-agnostic: loads pilot, introspects |
| test_joystick_menu.json5 | d1, d2 | Game-agnostic: navigates menus |
| test_axis_mapping.json5 | d1, d2 | Parameterized mission via _info.vars |
| test_death.json5 | d1, d2 | Parameterized mission/level via _info.vars |
| test_launch_to_automap.json5 | d1, d2 | Parameterized mission via _info.vars |
| test_saf_basic.json5 | d2 only | D2 mission "Counterstrike", SAF-specific |
| test_extract_regression_template.json5 | N/A | Template, handled by run_extract_test.ps1 |

### Superseded scripts (can be deleted)
| Script | Replaced by |
|--------|-------------|
| test_death_d1_level11.json5 | test_death.json5 |
| test_launch_d1_automap.json5 | test_launch_to_automap.json5 |

## Info Element Format
```json5
{"_info": {"games": ["d1", "d2"]}}
```
First element of the array. The `_info` object has no `action` field so the
automation engine will skip it (or it can be filtered when reading).

## Changes

### 1. Add _info to dual-game scripts
- test_controller_compare.json5: add `{"_info": {"games": ["d1", "d2"]}}`
- test_joystick_menu.json5: add `{"_info": {"games": ["d1", "d2"]}}`

### 2. Add single-game _info to game-specific scripts
- test_axis_mapping.json5: `{"_info": {"games": ["d2"]}}`
- test_death_d1_level11.json5: `{"_info": {"games": ["d1"]}}`
- test_launch_d1_automap.json5: `{"_info": {"games": ["d1"]}}`
- test_launch_to_automap.json5: `{"_info": {"games": ["d2"]}}`
- test_saf_basic.json5: `{"_info": {"games": ["d2"]}}`
- test_extract_regression_template.json5: skip (template)

### 3. Add helper: Get-ScriptGameInfo (test_helpers.ps1)
Reads the .json5 file, strips comments, parses JSON, checks for _info element.
Returns the games array or $null.

### 4. Update run_test.ps1
- Add optional -Game parameter (d1/d2)
- Call Get-ScriptGameInfo; if multiple games and no -Game specified,
  run both sequentially (non-interactive runner)
- Replace the `_d1_` filename heuristic with _info-based detection

### 5. Update Run-TestMenu.ps1
- After user selects test, call Get-ScriptGameInfo
- If multiple games, present d1/d2/both sub-menu
- Pass -Game to run_test.ps1

### 6. Update run_controller_compare.ps1
- Call Get-ScriptGameInfo on its script
- Run both games sequentially (non-interactive)
