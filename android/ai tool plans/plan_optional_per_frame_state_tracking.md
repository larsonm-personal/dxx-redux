# Plan: Optional Per-Frame State Tracking in .dximdemo Files

## Goal

Make per-frame state snapshot and event logging optional in .dximdemo files:
- **Default** (OFF): Absolute minimum needed to replay and check final state (v2 format)
- **Optional** (ON): Include per-frame state and events for detailed debugging (v3/v4 format)
- Add launcher toggle in Advanced tab under "Newly-Recorded Demos" section

## Implementation Phases

### Phase 1: Core Infrastructure [IN PROGRESS]
- [x] Add `record_per_frame_state` field to `input_demo_recorder_settings` struct
- [ ] Modify `input_demo_recorder_capture_frame()` to conditionally capture state based on flag
- [ ] Update `input_demo_recorder_build_demo()` to skip per-frame state when disabled
- [ ] Verify demo version detection (v2 minimal, v3+ with state/events)

### Phase 2: Launcher UI
- [ ] Add toggle in RecordedInputDemosSection of AdvancedSettingsPage.kt
- [ ] Store preference in SharedPreferences as "demo_record_per_frame_state" (default false)
- [ ] Read preference and pass to native code at game start

### Phase 3: Native Integration
- [ ] Update input_demo_prepare_recorder_settings() in d1/d2 newdemo.c to read launcher preference
- [ ] Pass flag through to input_demo_recorder_start()

### Phase 4: Validation
- [ ] Record demo with setting ON - verify state/events in JSON
- [ ] Record demo with setting OFF - verify minimal v2 format
- [ ] Run existing regression tests with setting OFF to confirm they still work
- [ ] Test that replay works correctly with both formats

## File Changes Summary

**Shared C++ (input_demo_recorder.h/cpp, input_demo_fixture.cpp):**
- Conditionally capture/serialize per-frame state

**Launcher Kotlin (AdvancedSettingsPage.kt):**
- Add UI toggle in RecordedInputDemosSection
- Read/write SharedPreferences

**D1/D2 (newdemo.c):**
- Read launcher preference and pass to recorder

**JNI Bridge (if needed):**
- Expose preference read function to native code

## Notes

- Format compatibility: demos without per-frame state remain valid v2 format
- Backwards compatible: replay already handles optional state field
- Performance: disabling reduces file size and JSON serialization overhead
- Test strategy: use existing .dximdemo test fixtures with setting OFF to verify minimal format works
