# Input Demo Per-Frame State Plan

## Goal

Record the same tracked state fields that currently appear only in the final
input demo result on every `.dximdemo` frame, then surface the first replay
frame that diverges without adding more transient replay-only probes.

## Phases

1. [completed] Extend the shared `.dximdemo` schema and codec with an optional
   per-frame `state` payload that reuses the compact result-field shape where
   possible
2. [completed] Extend the shared recorder and replay session to store and load
   per-frame tracked-state snapshots
3. [completed] Thread the existing D1/D2 `input_demo_capture_current_result()`
   snapshot into per-frame recording and replay-side frame comparison logging
4. [completed] Update schema docs and add or extend a focused shared test for
   frame-state parse/write and first-desync reporting
5. [completed] Run focused validation, then a bounded build/test pass for the
   touched slice

## Notes

- Start with the values already tracked in `input_demo_result`
- Keep the format extensible so robot summaries can be added later without
  reworking the frame record shape
- Prefer shared serializer logic over a second bespoke per-frame schema