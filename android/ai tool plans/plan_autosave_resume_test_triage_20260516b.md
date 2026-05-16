# Plan: Autosave Resume Test Triage 2026-05-16

## Goal

- Reproduce the current `test_autosave_resume_unified.json5` and `test_autosave_resume_missing_pilot_unified.json5` failures on fresh isolated reruns and only fix them if they still fail now.

## Local hypothesis

- The two failures likely share the same launcher resume-offer or pilot-state path.
- If they still fail, the first reproduced local defect is more likely in the launcher/resume flow than in game simulation.

## Cheap check

- Rerun each test individually and inspect the durable automation result plus the last automation-log steps.

## Steps

- [ ] Reproduce `test_autosave_resume_unified.json5` on a fresh isolated rerun
- [ ] Reproduce `test_autosave_resume_missing_pilot_unified.json5` on a fresh isolated rerun
- [ ] Inspect the first reproduced local failing step and nearby state
- [ ] Fix only the reproduced local root cause
- [ ] Rerun the affected autosave tests to confirm the outcome
- [ ] Update this plan with the final result