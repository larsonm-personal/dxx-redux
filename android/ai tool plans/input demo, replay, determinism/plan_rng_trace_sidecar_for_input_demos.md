# RNG Trace Sidecar Plan

## Goal

Add opt-in engine RNG instrumentation that records annotated per-call trace data during input-demo recording and writes a sidecar log next to the saved demo.

## Planned Steps

- [x] Confirm the RNG wrapper, recorder lifetime, and demo flush path.
- [x] Add a shared RNG trace buffer and sidecar writer in the input-demo shared code.
- [x] Route `d1` and `d2` RNG calls through annotated wrappers that can report caller labels.
- [x] Tie trace start, frame context, and flush lifecycle to input-demo recording.
- [x] Build and run focused validation for the new trace output.

## Notes

- The final demo path is only known at flush time, so the trace should buffer in memory during recording and write `<demo>.rngtrace.jsonl` on save.
- `d1` and `d2` both already compile the shared input-demo recorder code on desktop and Android, so the sidecar writer can stay shared.