# Test failure triage 2026-06-19

## Goal
Review the 2026-06-19 Android test failures without chasing every symptom, and fix one or two clearly broken paths that are unlikely to create fix ping-pong.

## Plan
- [x] Inspect the failing report/logs and identify clusters with a common cause
- [x] Fix the clearly broken `test_mp` manifest helper exception if it is local and low risk
- [x] Inspect one launcher/automation failure cluster for an obvious stale-state or runner issue
- [x] Run scoped quality checks and targeted tests where practical
- [x] Record what was fixed and what should be left for a separate focused pass

## Results
- Fixed `test_mp` pre-game failure in `Write-ResolvedGameDataAssetManifest`: strict-mode callers can now preserve optional `versionName` only when it exists, without throwing when an existing manifest entry lacks that field.
- Extended `test_game_data_asset_manifest_writer.ps1` to run under strict mode and cover an existing matching manifest entry with no `versionName`.
- Several json5 failures are D1 halves of multi-game launcher scripts that then pass for D2. The captured logs show D1 launcher taps and D1 config writes, but later D2 runtime state. This needs a narrow live repro with `MainActivity` launch/library logs before changing launch code.
- `test_quick_record_classic_sidecar_stage` is a UI-script drift around the Advanced page's staged demo controls. The direct `install_staged_demo` companion test already passed, so this is lower risk than the hard manifest exception.
- D2 demo regression failures were left alone because they are deterministic replay mismatches, not an obvious bad test infrastructure path.

## Second Pass Plan
- [x] Fix the multi-game launcher script D1/D2 mismatch at the runner or launch handoff level
- [x] Fix `test_quick_record_classic_sidecar_stage` so it verifies staged demo sidecars without relying on stale page navigation
- [x] Add or extend focused regression coverage for both fixes
- [x] Run scoped quality checks and the targeted tests that are practical in this session
- [x] Record any remaining risk or follow-up repro needed

## Second Pass Results
- Fixed launcher-runner stale process handling: `Wait-ProcessDead` now waits for both the default package process and the separate `:game` process before a follow-up launch can proceed. This targets the failure pattern where D1 launcher taps/config writes were followed by D2 runtime state.
- Added a host regression test for the two-process wait behavior.
- Added staged input-demo details to setup introspection and updated `test_quick_record_classic_sidecar_stage` to wait for the staged sidecar state directly before opening Advanced.
- Fixed launcher `reset_state` to clear staged input-demo recordings, so stale staged demos from previous scripts cannot satisfy or block the quick-record staging checks.
- Verified the quick-record stage script on emulator after rebuilding the APK. It passed and showed stale staged demos were deleted before recording the fresh one.
- Verified a focused D1 launch path with `test_axis_mapping.json5 -Game d1`; it passed and final introspection showed the D1 runtime.
