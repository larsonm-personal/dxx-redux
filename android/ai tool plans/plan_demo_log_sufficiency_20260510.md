# Demo Log Sufficiency Audit 2026-05-10

## Goal
- verify that newly recorded input-demo logs will contain enough information to narrow remaining nondeterminism quickly
- inspect current trace/log outputs and close small diagnostic gaps before recording more demos

## Steps
- [in-progress] inventory existing demo state traces, RNG traces, automation logs, and replay output files
- [not-started] compare current log fields against remaining roadmap risks: runtime state, object/list order, RNG stream discipline, FrameTime math, FVI/contact physics
- [not-started] identify small high-value diagnostic gaps and implement them if safe before new recordings
- [not-started] run focused validation and update roadmap notes

## Notes
- Prefer additive trace fields and comparer summaries over large verbose logs
- Keep D1 and D2 changes mirrored where the hook exists in both games