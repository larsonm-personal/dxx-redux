# Replay Debug Demo 2026-04-28 141502

## Goal

Review the new D2 input demo and RNG trace, reproduce the current replay behavior, and either eliminate the desync or narrow it to a concrete owner with durable logging.

## Steps

- [x] Inspect the supplied demo and rngtrace artifacts plus the current replay-debug plan context.
- [x] Re-run the host replay wrapper on the supplied demo and capture the current failure mode.
- [x] Compare the replay output against the supplied rngtrace and existing replay instrumentation to identify the first discriminating mismatch owner.
- [x] Apply the smallest logging or behavior change needed to test the current local hypothesis.
- [x] Re-run focused validation and record the outcome.

## Notes

- Before the RNG split work, this demo diverged at the frame-1 boundary and finished with `p0.s` expected `166`, actual `171`.
- The first replay-only probe confirmed HUD/gauge RNG was being preserved on replay, so the frame-1 boundary issue was broader than just HUD restore.
- The new sim-vs-FX split landed for the RNG core, palette diminish, ambient water/lava sound, and the palette computed-color cache in both d1 and d2.
- After that split, the preserved replay sandbox at `temp\input_demo_runtime_wrapper\d2\d2_descent2_level1_20260428_141502\gamelog.txt` shows the first raw rngtrace mismatch moved from frame 1 to frame 2.
- The frame-2 replay path burst now matches the intended sim-only sequence exactly: replay logs `create_path_to_player` with `calls=0->11 before=2302307495 after=777648458`, which matches the recording once the legacy palette-only calls are removed from the old sidecar.
- The old `141502` `.rngtrace.jsonl` still contains pre-split palette consumers (`d2/main/game.c:diminish_palette_towards_normal` and `d2/2d/palette.c:add_computed_color`), so it is now a legacy full-stream trace rather than a clean sim-only trace.
- The final result still fails on the old demo (`player shields` expected `166`, actual `171`), so there is still a later gameplay divergence to chase after a fresh sim-only recording is available.