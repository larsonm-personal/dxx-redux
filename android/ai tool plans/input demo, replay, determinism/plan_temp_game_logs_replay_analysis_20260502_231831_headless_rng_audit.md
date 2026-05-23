# Plan: temp_game_logs 231831 headless RNG/render audit 2026-05-02

## Goal

Audit rendering side effects and RNG stream separation for the host headless vs windowed replay mismatch on:

- android/temp_game_logs/d2_descent2_level2_20260501_231831.dximdemo

## Steps

- [completed] Identify render-path simulation side effects tied to replay determinism
- [completed] Audit RNG stream usage and compare-window filters for SIM vs FX events
- [completed] Run direct host windowed vs host headless A/B traces with explicit output paths
- [completed] Verify whether mismatch is RNG value divergence or frame-ordering divergence
- [completed] Try one minimal render-side warning parity patch and re-test
- [completed] Revert no-op patch after no parity improvement

## Findings

- Render-path simulation side effect still exists via rendered-object wakeups: do_render_object populates Window_rendered_data rendered object list and wake_up_rendered_objects mutates AI awareness from that list
- No direct SIM RNG calls were found in the core render/HUD files audited (render.c, gamerend.c, gauges.c)
- RNG streams are split in maths/rand.c with D_RNG_SIM and D_RNG_FX
- The first host windowed vs host headless RNG mismatch remains a frame/gt shift with identical call_count, state_before/state_after, and result at ai.c:create_awareness_event
- A visibility-gated headless warning probe patch in d2/main/object.c did not change the mismatch pattern and was reverted

## Current status

- Headless mismatch remains at first state mismatch frame 419 (score), with the same awareness RNG event occurring one frame earlier in headless
- Most likely remaining issue is event ordering/state gating, not raw RNG stream value divergence
