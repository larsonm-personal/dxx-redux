# Merged wall stale script cleanup plan

## Goal
- Remove stale `merged_wall_experiment` and `merged_wall_force_two_pass` use from Android automation scripts after the graphics cleanup removed those debug fields.
- Keep the remaining merged-wall scripts aligned with current native automation and introspection fields.

## Steps
- [x] Audit all script references to removed merged-wall debug fields.
- [x] Update or retire stale script steps and assertions.
- [x] Run scoped formatting and static checks for touched scripts.
- [x] Run focused lightweight verification where possible.

## Notes
- Removed stale reset/assert lines from the snapshot and door45 scripts.
- Removed the obsolete launcher pref assertions for `force_legacy_merged_wall_texmerge`.
- Deleted the two force-two-pass probe scripts because the native `merged_wall_force_two_pass` switch and introspection field no longer exist.

## Verification
- `.\android\run-code-quality.ps1 -Fix -Paths ...` passed for the touched scripts and plan.
- Live script/test-runner stale-reference scan found no remaining `merged_wall_experiment`, `merged_wall_force_two_pass`, `force_legacy_merged_wall_texmerge`, or deleted two-pass script references.
- The three remaining edited scripts parse after the same comment and trailing-comma stripping used by the test runner.
- No emulator was attached, so this pass did not run the on-device graphics scripts.
