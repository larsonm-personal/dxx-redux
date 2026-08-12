# Touch layout overwrite investigation

## Goal

Identify which change from the last day caused an on-device customized touch layout to be corrupted or replaced by an app-bundled default.

## Plan

- [x] Read repository instructions and identify commits in the reported window
- [x] Inspect touch layout persistence, migration, import, and bundled-default diffs
- [x] Reproduce the risky code path with focused tests or a minimal local check
- [x] Report the responsible change, exact failure mode, and recovery implications

## Notes

- Diagnostic only. Do not modify production code without a follow-up request.
- Preserve the unrelated modified `android/test_fixtures/secret_area_base_game_baseline.json` file.
- Commit `0498798fc927581626c3f5978e219c68e64990c0` added strict validation to raw active layouts and human-readable slot layouts.
- A validation failure is swallowed by both repositories and replaced in memory with the bundled default. Saving after fallback rewrites both persisted generations with that default.
- Previously supported editor state can fail the new validator, including crossed or collapsed floating/axis zones authored by the old independent sliders. The numeric pre-validator also rejects an app-serialized `1.57f` gyro limit because it becomes `1.57000005245209` while the hard-coded JSON maximum is `1.57`.
- No bundled touch preset changed during the reported window, and recent deployment-script changes made app-data preservation safer rather than clearing it.
- Focused `TouchEditorZoneEdgeTest` and `ConfigSlotRepositoryTest` JVM suites passed. They cover the new strict behavior but not upgrade compatibility for previously persisted layouts.
- The available emulator was offline, so the actual device-private JSON could not be inspected. Exact offending field remains unconfirmed until `touch_layout.json` and `touch_layout_slots.json` are recovered from the affected device.
