# Plan: test_dpad_triggers follow-up from report 20260522_232545

- [x] Create tranche plan file and capture the failing report context
- [ ] Inspect the failing `test_dpad_triggers` report section and local automation script
- [ ] Trace the owning D2 joystick-control index/binding path for item 50
- [ ] Form one local hypothesis for why D2 reports `255` instead of `150`
- [ ] Apply the smallest fix in the owning D2 binding/introspection path
- [ ] Run focused validation for `test_dpad_triggers`
- [ ] Update this note with the outcome and any remaining blockers

## Notes

- Report under review: `C:\local\dxx-redux\temp\test_reports\report_20260522_232545.md`
- Failing assertion in D2: `joystick_controls.items[50].value == 150` but introspection reported `255`
- D1 pass in the same run suggests either a D2-only binding table mismatch or a D2-only introspection ordering mismatch
