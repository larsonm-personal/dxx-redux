# Plan: Door45 + SAF Archiver Triage 2026-05-16

## Goal

- Reproduce `test_door45_pose_repro` and `test_saf_archiver`, then fix only the failures that still reproduce now.

## Local hypothesis

- `test_door45_pose_repro` is likely failing on a brittle pose/assert threshold rather than a gameplay regression.
- `test_saf_archiver` may already be fixed by the recent SAF script hardening and just needs a fresh rerun.

## Cheap check

- Rerun `test_door45_pose_repro` individually and inspect the failing assertion.
- Rerun `test_saf_archiver` individually after the Door45 slice to see whether it still reproduces.

## Steps

- [ ] Reproduce `test_door45_pose_repro` on a fresh isolated rerun
- [ ] Fix only the reproduced local Door45 root cause
- [ ] Rerun `test_door45_pose_repro` to confirm the outcome
- [ ] Reproduce `test_saf_archiver` on a fresh isolated rerun
- [ ] Fix only the reproduced local SAF root cause if it still fails
- [ ] Update this plan with the final result