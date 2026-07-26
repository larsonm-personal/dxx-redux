# Report 20260725 184825 failure investigation

## Goal

Investigate and dispose of both failures in
`temp/test_reports/report_20260725_184825.md`, including order-dependent state
and infrastructure evidence.

## Plan

- [x] Record the report and scope
- [x] Extract both failure summaries and durable logs
- [x] Compare each failure with recent passing and failing runs
- [x] Identify product, test, state-leak, or infrastructure causes
- [x] Implement narrow fixes when supported by evidence
- [x] Validate in order-sensitive sequences
- [x] Run scoped quality and catalog validation
- [x] Record final dispositions and verification

## Dispositions

### test_death

The D1 variant spent almost its entire 600 second launcher timeout looking for a
`Descent 1` button after `reset_state`. The reset clears launcher preferences
after SetupActivity has already rendered, so the script was searching a stale
button inventory. The D2 variant then passed.

Replace the manual game-selection button taps with `enter_game`. This uses the
automation launch preflight and requests the intended engine directly after the
reset.

Validation passed in report order:

1. `test_d2_level7_reactor_water_profile.json5`
2. `test_death.json5`, both D1 and D2 variants

### test_obsidian_level1_objective_markers

The failed snapshot expected `6: Reactor` after firing trigger 8 but observed
`5: Fly-through trigger 8`. Trigger 8 starts an opening-wall transition. The
test waited only a fixed 200 ms before opening the automap and sampling the
route, so a loaded runner could observe the valid in-progress route.

Replace that immediate assertion with a bounded state wait for the same complete
route snapshot. This does not relax the expectation or add a blanket delay.

Validation passed in report order:

1. `test_newmenu_render_paths_unified.json5`, both D1 and D2 variants
2. `test_obsidian_level1_objective_markers.json5`

The full 86-test batch was not rerun.
