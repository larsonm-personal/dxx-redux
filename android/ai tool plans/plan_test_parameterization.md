# Plan: Test Script Parameterization

## Goal
Merge test_mod_loading_{128,256,512}.json5 into a single parameterized file.

## Changes

### 1. Create test_mod_loading.json5 (merged)
- New `_info.params` block with RESOLUTION parameter
- Options: 128, 256, 512 with per-option DXA_SHA variable
- Uses ${RESOLUTION} and ${DXA_SHA} template vars throughout
- STATUS: not started

### 2. test_helpers.ps1 - Add Get-ScriptParams
- New function to read `_info.params` from script
- Returns params definition or $null
- STATUS: not started

### 3. test_helpers.ps1 - Modify Resolve-TestScript
- Add optional -Params hashtable parameter
- Merge param option vars (e.g., DXA_SHA for selected RESOLUTION) into $vars
- Add param value itself (e.g., RESOLUTION=256) to $vars
- STATUS: not started

### 4. test_helpers.ps1 - Modify Get-ScriptDeps
- Add optional -Vars hashtable parameter
- Do ${VAR} substitution in dep file and sha256 fields
- STATUS: not started

### 5. run_test.ps1 - Accept -Params
- New -Params parameter (hashtable)
- Build combined vars from game vars + param vars
- Pass vars to Get-ScriptDeps for dep resolution
- STATUS: not started

### 6. Run-TestMenu.ps1 - Prompt for params
- After game selection, detect _info.params
- Prompt user for each parameter
- Pass selected params to run_test.ps1
- STATUS: not started

### 7. Delete old files
- Remove test_mod_loading_{128,256,512}.json5
- STATUS: not started
