# D1 level 16 and 17 demo replay plan

Goal: verify the new D1 level 16 and level 17 input demos in plain D1, then run the same recordings as d1-in-d2 and fix any reproducible desyncs.

Demo files:

- `android/regression_demos/d1_descent_level16_20260618_150235.dem`
- `android/regression_demos/d1_descent_level16_20260618_150235.dximdemo`
- `android/regression_demos/d1_descent_level16_20260618_150235.dximdemo.rngtrace.jsonl`
- `android/regression_demos/d1_descent_level17_20260618_150642.dem`
- `android/regression_demos/d1_descent_level17_20260618_150642.dximdemo`
- `android/regression_demos/d1_descent_level17_20260618_150642.dximdemo.rngtrace.jsonl`

Plan:

1. [done] Verify all listed demo files exist and have sidecar RNG traces.
2. [done] Run level 16 and level 17 in plain D1 with strict result checks.
3. [done] Run level 16 and level 17 as d1-in-d2.
4. [in progress] If a plain D1 or d1-in-d2 replay fails, use replay logs, state trace, and RNG trace to locate the first gameplay split.
5. [in progress] Apply the smallest engine or D1-in-D2 translation fix that addresses the split.
6. [pending] Re-run focused build, replay, and code-quality validation.

Notes:

- Keep D1 gameplay behavior frozen; D1-side changes should be diagnostic-only unless a plain D1 replay proves the current D1 replay harness is wrong.
- All six listed files are present; level 16 `.dximdemo` is about 35.9 MB and level 17 `.dximdemo` is about 80.1 MB.
- Added replay-checkpoint object chain preservation on the D1 restore path. This is input-demo checkpoint-only and avoids canonical relinking changing same-segment object processing order.
- Ported D2's replay-only secondary fire release/post-frame timing behavior into D1. Level 17 plain D1 now passes strict result comparison with the recorded final state.
- Level 16 plain D1 still fails strict comparison. Current actual result ends dead in segment 103 with score 3900 instead of the expected live segment 485 / score 10100 state.
- Level 16 state trace now matches through frame 380, has transient `robot_ai_static` differences at frames 381-383, then diverges durably at frame 384 when actual has one extra `danger_laser` robot and robot object state splits.
- Added broader per-robot AI-static diagnostic arrays to future state traces. Existing level 16 expected trace was recorded before these fields existed, so a fresh recording is needed to identify the exact robot/field at the 381-384 split.
- D1-in-D2 initially discarded D1 checkpoint object `next`/`prev` links in `d1_save_translate.c`; this has been fixed inside the D1 save translation path. The d1-in-d2 frame-0 segment object list hash now matches D1.
- D1-in-D2 still reports frame-0 diagnostic mismatches for player motion and AI static/local hashes under state comparison. Some AI hash differences are likely comparison-schema issues because the shared trace hashes D2-only `ai_static` fields even when D1 gameplay is active; gameplay parity still needs more work.
