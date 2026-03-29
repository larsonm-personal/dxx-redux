# Test Report Fixes (report_20260328_192956)

## Failures fixed

### 1. test_server_integration (DONE)
- **Root cause**: `Instant::now() - Duration::from_secs(8000)` panics when system uptime < 8000s
- **Fix**: Added `cleanup_sessions_older_than(state, cutoff)` to relay.rs so the test can use a small cutoff (1s) with a 3-second-old session instead of requiring 8000s of uptime
- **Files**: server/src/relay.rs, server/tests/integration.rs
- **Verified**: cargo test + clippy pass

### 2. test_fire_primary (DONE)
- **Root cause**: Intermittent (1/8 runs). 1.679s stall between STEP_SELECT Phase 1 (cursor positioned on "Ok") and Phase 2 (enter injected). During stall, cursor likely drifted back to text input field. Enter on NM_TYPE_INPUT toggles edit mode instead of confirming, so callsign dialog persisted
- **Fix**: Added Phase 2 re-verification in game_automate.cpp STEP_SELECT -- re-reads cursor position before injecting enter, returns to Phase 1 if cursor drifted. Also increased timeout_ms on "New game" select from 5000 to 10000
- **Files**: android/app/src/main/cpp/shared/game_automate.cpp, android/game_scripts/test_fire_primary.json5

### 3. test_saf_archiver (DONE)
- **Root cause**: Total time budget (~233s) very close to 240s outer timeout. Includes redundant APK build, 120s monitoring timeout, and many sequential ADB operations on a stressed emulator after 7+ test cycles
- **Fix**: (a) Outer timeout 240->360s, (b) Monitoring timeout 120->60s (automation completes in <30s), (c) Skip build step when run from run_all_tests via -NoBuild, (d) Added output flushing at key progress points to diagnose future hangs
- **Files**: android/tests/test_saf_archiver.ps1, android/run_all_tests.ps1

## Speed survey

Tests >1 minute in the last run:

| Test | Time | Category | Speedup potential |
|------|------|----------|-------------------|
| test_all_extracts | 2:07 | I/O-bound extract operations | Low -- mostly file extraction time |
| test_gog_installer_redbook_unified | 1:13 | GOG import + Chromaprint | Low -- disk image processing time |
| test_axis_mapping | 1:03 | Game automation (JSON5) | Medium -- 18 steps at 500ms post_delay could be 200-300ms (saves ~5s) |
| test_mp | 1:00 | Multiplayer (2 emu + server) | Medium -- 7 poll loops at 1500ms could be 500ms (saves ~20-30s), but risky for race conditions |
| test_lan_discovery | 1:00 | LAN discovery broadcast | Easy -- hardcoded 60s DurationSec could be 30s |

### Recommendations (not implemented, for future work)
1. **test_lan_discovery**: Reduce DurationSec from 60 to 30. The test just needs to verify discovery works, not soak. This alone saves 30s
2. **test_mp**: Reduce PollMs from 1500 to 750 across 7 Wait-ForCondition calls. Saves ~15-20s. Test carefully for race conditions
3. **test_axis_mapping**: Reduce 500ms post_delay_ms to 250ms on send_axis steps. Saves ~5s
4. **test_all_extracts/test_gog_installer**: Minimal optimization potential -- dominated by I/O time
