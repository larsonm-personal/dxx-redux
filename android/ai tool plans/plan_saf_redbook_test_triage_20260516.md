# Plan: SAF Redbook Test Triage 2026-05-16

## Goal

- Reproduce the current `test_saf_redbook.json5` result on a fresh isolated rerun and only fix it if it still fails now.

## Local hypothesis

- The reproduced failure uses the same launch path drift as `test_saf_basic.json5`: after launch, the script can land on the select-pilot listbox instead of the direct pilot-name `Ok` prompt.

## Cheap check

- Reproduce the failure and confirm the first local failing step is the initial `select "Ok"` after launch.

## Steps

- [x] Reproduce the current `test_saf_redbook.json5` result on a fresh isolated rerun
- [x] If it fails, inspect the first local failing step and nearby state
- [x] Fix only the reproduced local root cause
- [x] Rerun `test_saf_redbook.json5` to confirm the outcome
- [x] Update this plan with the final result

## Outcome

- Reproduced failure 1: the script timed out selecting `Ok` right after launch.
- Reproduced failure 2 after the first patch: the script was still selecting while the front window was not a `newmenu` or `listbox`, indicating it was on the intro/movie path rather than a selectable menu.
- Fix: add the same explicit intro handling used by other launcher-to-game tests, then keep the initial pilot step tolerant of either the direct `Ok` prompt or the select-pilot `player` row.

## Validation

- Reproduced failing rerun: `{"result":"FAIL","steps_completed":10,"total_steps":26,"reason":"SELECT: item \"Ok\" not found in menu (timed out)","elapsed_ms":5012}`
- Post-fix rerun passed: `{"result":"PASS","steps_completed":30,"total_steps":29,"elapsed_ms":11701}`