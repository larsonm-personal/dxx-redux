# Plan: thief checkpoint state implementation 2026-05-03

## Goal

Add input demo checkpoint thief state capture and restore path with shared fixture/recorder/replay schema support

## Steps

- [x] Add input_demo_checkpoint_thief_state to shared fixture schema
- [x] Add parse and write support for thief checkpoint fields in fixture codec
- [x] Plumb thief checkpoint state through recorder and replay session APIs
- [x] Capture thief checkpoint state from D2 runtime when writing checkpoint demos
- [x] Restore thief checkpoint state during D2 runtime rebuild after checkpoint load
- [x] Update shared fixture and replay unit tests for thief checkpoint fields
- [x] Run compile and diagnostics validation

## Notes

- Thief fields added: stolen_item_index, re_init_thief_time, last_thief_hit_time
- Replay getter added for checkpoint thief state
