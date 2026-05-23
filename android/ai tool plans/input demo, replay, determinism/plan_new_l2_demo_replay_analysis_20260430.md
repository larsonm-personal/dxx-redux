# Plan: new D2 L2 demo replay analysis

Date: 2026-04-30

Goal: analyze the newest non-`old/` demo artifacts in `android/temp_game_logs`, replay the input-type demo, compare the terminal result against the recorded final state, and if it diverges, narrow the first mismatch against record-side logs and PC replay output before fixing or adding targeted logging.

## Phases

| phase | task | status |
|---|---|---|
| 1 | Inventory current non-`old/` demo artifacts and identify the newest matching `.dximdemo`, `.rngtrace.jsonl`, `.dem`, and debug log. | completed |
| 2 | Run the input demo through the host replay wrapper and capture the final-state comparison. | completed |
| 3 | If replay diverges, compare record-side debug logs against host replay logs to find the first mismatch. | completed |
| 4 | Use the classic `.dem` dump only for direct observed object/player positions where useful, not for hidden AI state. | completed |
| 5 | Fix the root cause if clear, or add the smallest targeted logging needed for the next run. | completed |
| 6 | Rebuild and rerun the affected replay/test path after any code changes. | completed |

## Notes

- Ignore files under `android/temp_game_logs/old/`.
- Do not interpolate classic `.dem` positions. Use only directly observed classic frames and directly observed record/replay logs.
- Prior investigation showed classic `.dem` playback object positions are useful, but hidden robot AI fields from `Ai_local_info` and `obj->ctype.ai_info` are not authoritative in the dump path.
- Newest bundle is `d2_descent2_level2_20260430_135527.*` with record-side log `debuglog_20260430_135357.txt`; header metadata is populated (`build_number=12680`, `git_version=99948d4`).
- Host replay passed with `android/tests/run_input_demo_replay.ps1 -DemoPath .\android\temp_game_logs\d2_descent2_level2_20260430_135527.dximdemo -Game d2 -Mode accelerated -KeepSandbox`; actual result matched embedded trailer at frame 748, so no divergence comparison, classic dump, fix, or rebuild was needed for this demo.