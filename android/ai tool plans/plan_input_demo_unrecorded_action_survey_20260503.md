# Input Demo Unrecorded Action Survey 2026-05-03

## Goal

Survey gameplay actions that may not be recorded correctly by the input-demo system, using the confirmed pre-frame weapon-select pulse loss as the reference failure mode.

## Hypothesis

Actions are at risk when they either:

- are represented as pulse-style control fields that can be consumed before `input_demo_record_game_frame()` captures the frame
- bypass the recorded control snapshot entirely and mutate gameplay state through direct key-command or JNI paths without a mirrored recorder event or durable state capture

## Checks

1. Inspect the shared input-demo control schema to see which actions are explicitly serialized in frame input
2. Trace `ReadControls()` and adjacent handlers in D1 and D2 for immediate consumers that clear counts or toggle state before frame capture
3. Inspect non-control command paths for guidebot, headlight, automap marker, and similar actions that may mutate game state outside the serialized input path
4. Classify findings as:
   - already covered by recorded state or checkpoint data
   - likely safe because replay re-runs the same command path from serialized input
   - at risk because no serialized input or mirrored state capture exists

## Deliverable

Produce a concise action survey with concrete file anchors and recommended next tests or fixes for any risky categories.

## Findings

Completed survey summary:

- Covered by serialized control frames and therefore expected to replay correctly:
   - headlight toggle
   - toggle bomb type
   - automap open/close pulse
   - rear-view toggle pulse
   - weapon cycle/select and bomb-drop pulses
- Reason these look safe:
   - they exist in `input_demo_control_pulse` / `input_demo_control_state`
   - D2 maps them to and from `control_info`
   - replay restores `Controls` directly from the serialized frame before calling `ReadControlsReplayFrame()`
   - the recent staged-pulse fix covers the ones that are consumed before frame entry
- Still at risk because they bypass the control-frame schema and run through direct key-command or Android-only command paths:
   - guidebot command hotkeys (`Shift+0` through `Shift+9`)
   - guidebot menu selections, which call the same `set_escort_special_goal()` path from a menu key handler
   - guidebot rename / guidebot menu open hotkeys are also outside frame controls, though rename is mostly cosmetic
   - marker drop and marker text entry (`F4`, then `MarkerInputMessage()`), which mutate marker objects and marker text after replay start
   - drop-current-weapon / drop-secondary-weapon hotkeys, which create powerups and mutate inventory directly
   - Android-only guidebot release control meta action, which sets a pending flag consumed directly in `HandleGameKey()` and never becomes a serialized control pulse
- Reduced-risk note for guidebot commands:
   - escort checkpoint state already captures `Escort_special_goal`, `Escort_goal_object`, `Escort_goal_index`, marker search state, and related escort runtime fields, so commands that happened before the replay start checkpoint are restored correctly
   - the gap is for commands issued after replay start

## Recommended Next Work

1. Add an explicit input-demo event channel for direct gameplay commands that do not live in `control_info`
2. Start with guidebot special-goal changes, marker drop/confirm, and weapon-drop hotkeys because those are the clearest gameplay-affecting gaps
3. Treat guidebot rename and menu-open as lower priority unless a test depends on them

## Implementation Tranche

- [completed] Reuse frame `events` for direct gameplay commands without requiring `record_per_frame_state`
- [completed] Stage pre-frame direct-command events into the next recorded frame, mirroring the pending-pulse fix
- [completed] Expose current-frame direct-command events to C replay code through typed replay helpers
- [completed] Record and replay these D2 commands:
   - guidebot hotkey/menu goal changes
   - marker drop confirm
   - drop current weapon
   - drop secondary weapon
   - drop flag
   - Android guidebot release control
- [completed] Validate with focused D2 builds
- [completed] Cross-check shared input-demo changes with a D1 build