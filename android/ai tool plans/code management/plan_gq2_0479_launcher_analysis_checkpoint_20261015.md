# Chunk 0479 launcher analysis checkpoint, 2026-10-15

Partial static consumer trace; source gate closed, four later gates open. Selected executor boundaries only, no runtime or full analyzer/cache/native mount proof.

test_preview_media_controls catches helper exceptions, fails script with error and returns; success advances. JSON alone does not expose helper controls. Actual preview/media-key helper and owned runner remain to trace.

clear_mods runs active-set manager and logs returned removal count. delete_mod validates a leaf filename, invokes deleteMod and logs deletion; selected call site does not prove physical deletion/cleanup outcome. import_mission_zip checks nullable result, writes import result and notifies route metadata content-import scheduler before success. That scheduler side effect is relevant to preemption, but no background-active acknowledgement is present in this selected block.

analyze_level_metadata builds one target on IO, fails missing target, then optionally identifies and reads persisted full-result cache against target/expected normal+secret count. Null cache fails the expectation. A hit is a real cache-read assertion, stronger than plain status; it then still calls LevelMetadataAnalyzer.analyze. Therefore no-analysis-start claim depends on analyzer's own cache fast path, not merely this precheck. Analyzer result written before checking expected status and minimum level count. Preserve report identity but do not equate file publication with accepted success.

analyze_level_metadata_all builds all targets, rejects empty, sequentially analyzes each, publishes results then checks every result status and summed minimum level count. Optional music predicates enforce all provided fields on one track, but assigned HXM/Lunar scripts do not request these. It does not directly assert worker PID/reuse or compare replacement definitions/mount snapshots. Level minimum is aggregate, not a per-target expected count. Keep actual all-status requirement, not just first result.

NEXT whole actual target builders/analyzer/cache admission/scheduler-worker/native request isolation; selected media helper and its runner; host background markers and native lifecycle; projected/drawn automap, matcen idle and touch/screen generation consumers. Reuse prior source only after exact current-byte checks. Existing-owner/prior-plan/disposition/publication remain open. No new finding/remediation/status/rating/runtime/original saving.
