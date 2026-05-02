# Plan: RNG Full Instrumentation 20260501

## Scope

Broaden the input-demo RNG trace so the next hand-recorded replay run captures more RNG categories without depending on one exact frame or one exact demo.

## Hypothesis

The current `.rngtrace.jsonl` is not literally "all RNG calls". It currently records annotated engine RNG calls through the default simulation stream and can hide categories such as non-default streams or events that happen before useful frame context is available.

## Plan

1. [x] Widen the shared RNG trace event format to preserve stream and context information
2. [x] Stop dropping RNG events solely because frame context has not been set yet
3. [x] Update the D1 and D2 RNG hook points to pass stream identity into the shared trace
4. [x] Update the RNG compare helper so supplemental stream or context records do not obscure the canonical sim-trace mismatch
5. [x] Rebuild D2 and confirm the replay tooling still runs

## Findings

- The shared RNG trace now records non-default stream events with `stream` and marks pre-context events with `has_context=false`
- The D1 and D2 annotated RNG hooks now pass stream identity into the shared trace instead of silently discarding non-default streams
- The canonical RNG compare helper now filters supplemental events from the mismatch search, so old-style sim-stream desync localization still lands on the first gameplay-relevant mismatch
- Replaying `android/temp_game_logs/d2_descent2_level2_20260501_103312.dximdemo` still fails at the same real first RNG divergence after filtering:
	- expected `d2/main/ai.c:create_awareness_event` at frame `435`, `call_count=11697`
	- actual `d2/main/aipath.c:create_random_xlate` at frame `438`, `call_count=11698`
- The enriched replay-side trace now also exposes supplemental RNG activity that was previously invisible, including early `stream=1` / `has_context=false` records from palette setup
- A separate investigation note remains important: replay restores recorded RNG state and call count before each replay frame, so frame-state RNG equality is not enough to prove live control-flow equality between recording and replay

## Validation

- `run-windows-build.ps1 -Target d2`
- `android/run-code-quality.ps1 -Fix`
- `android/tests/compare_input_demo_rng_trace.ps1 -ExpectedPath .\android\temp_game_logs\d2_descent2_level2_20260501_103312.dximdemo.rngtrace.jsonl -ActualPath .\temp\input_demo_state_traces\d2_descent2_level2_20260501_103312.actual_rngtrace.jsonl`
