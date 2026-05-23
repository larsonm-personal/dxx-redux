# Demo Log Sufficiency Audit 2026-05-10

## Goal
- verify that newly recorded input-demo logs will contain enough information to narrow remaining nondeterminism quickly
- inspect current trace/log outputs and close small diagnostic gaps before recording more demos

## Steps
- [completed] inventory existing demo state traces, RNG traces, automation logs, and replay output files
- [completed] compare current log fields against remaining roadmap risks: runtime state, object/list order, RNG stream discipline, FrameTime math, FVI/contact physics
- [completed] identify small high-value diagnostic gaps and implement them if safe before new recordings
- [completed] run focused validation and update roadmap notes

## Notes
- Prefer additive trace fields and comparer summaries over large verbose logs
- Keep D1 and D2 changes mirrored where the hook exists in both games
- Added compact per-frame runtime-state coverage for remaining checkpoint-sensitive globals:
	`object_signature_seed`, `object_free_list_count`, `object_free_list_hash`, `object_homer_frame_count`, `weapon_next_laser_delta`, `weapon_next_missile_delta`, `weapon_last_laser_delta`, `weapon_next_flare_delta`, `weapon_auto_fusion_delta`, `weapon_global_laser_firing_count`, `weapon_global_missile_firing_count`, and the existing laser runtime fields
- Updated the state-trace comparer to classify these fields as `runtime_state` so first mismatch reports point at allocator/weapon timers instead of a generic diag failure
- Focused host validation passed after formatting:
	D1 `buildd1\\maths\\test_input_demo_recorder.exe`
	D2 `buildd2\\maths\\test_input_demo_recorder.exe`
- Recording readiness:
	D2 recordings are now materially sufficient for the current nondeterminism hunt when Android `Record per-frame state` is enabled and the sidecar `.rngtrace.jsonl` is kept with the demo
	D1 recordings have the same state/RNG coverage, but D1 still has weaker durable collision/contact event parity than D2, so diagnosing contact-heavy divergences may still require follow-up instrumentation