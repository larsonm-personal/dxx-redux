# Fix Test Failures - 2026-03-29

## Analysis

Test report shows 9 failures/timeouts across 2 runs (20260328 and 20260329). Critical pattern: failures alternate between D1 and D2 runs across reports, indicating **flakiness** not systematic breakage.

### Root Causes

1. **run_test_menu.sh**: Only discovers json5 tests (17 tests), misses ps1 tests in `android/tests/`. Also has MSYS/Git Bash path mangling issue.

2. **JSON5 game test flakiness** (test_launch_to_automap, test_death, test_keyboard_viewport, test_axis_mapping, test_dpad_triggers, test_keyboard_defaults): Menu transition timing varies on emulator. Steps like `select "Rookie"` timeout because the previous menu (level number "Ok") hasn't fully dismissed before the next menu (difficulty) appears. Fix: add `wait_ms` delays between menu-dismissing actions and subsequent selects.

3. **test_keyboard_viewport D2 no automation log**: Launcher preamble failed silently (game never launched). Likely the `tap_button "Launch Descent 2"` timed out because the button didn't appear in time. Fix: increase `post_delay_ms` on game selection button.

4. **test_death D2**: player_dead timeout (30s) is sometimes insufficient on emulator when enemies don't attack quickly on level 24. Fix: increase timeout to 60s.

5. **test_axis_mapping D2**: pitch_time assertion fires before the game processes the axis input. Fix: add wait_ms before assert.

6. **test_saf_archiver**: descent2.ham re-push puts files in wrong location. Need to check Resolve-GameDataDeps.

7. **test_all_extracts**: exit 1 with empty log - likely a regression spec file discovery issue.

8. **test_gog_installer_redbook_unified**: 5-minute timeout - GOG extraction is slow.

## Plan

### Phase 1: Fix run_test_menu.sh
- [x] Add ps1 test discovery from `android/tests/test_*.ps1`
- [x] Fix path construction for cross-platform use
- [x] Call ps1 tests via `run_test.ps1` or `pwsh`, json5 tests via `run_automation.sh`

### Phase 2: Fix JSON5 test flakiness
- [x] Add `wait_ms` delays after menu-dismissing select steps in all affected tests
- [x] Increase timeouts for timing-sensitive assertions
- [x] Increase test_death player_dead timeout

### Phase 3: Investigate ps1 failures
- [x] Check test_saf_archiver re-push logic
- [x] Check test_all_extracts spec discovery
- [x] Check test_gog_installer_redbook_unified timeout

### Phase 4: Verify
- [ ] Run linters
- [ ] Build
- [ ] Run tests
