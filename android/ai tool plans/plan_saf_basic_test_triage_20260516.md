# Plan: SAF Basic Test Triage 2026-05-16

## Goal

- Reproduce the current `test_saf_basic.json5` result on a fresh isolated rerun and only fix it if it still fails now.

## Local hypothesis

- The stale report entry may already be fixed by earlier SAF and launcher import/export cleanup.
- If it still fails, the first local cause is likely a setup-screen state mismatch or a SAF picker flow regression.

## Cheap check

- Rerun only `test_saf_basic.json5` and inspect the durable automation result plus the last automation-log steps.

## Steps

- [x] Reproduce the current `test_saf_basic.json5` result on a fresh isolated rerun
- [x] If it fails, inspect the first local failing step and nearby state
- [x] Fix only the reproduced local root cause
- [x] Rerun `test_saf_basic.json5` to confirm the outcome
- [x] Update this plan with the final result

## Outcome

- Reproduced failure: after the intro, the script assumed a direct pilot-name `Ok` prompt, but the live menu was the `Select pilot` listbox with `player` selected.
- Fix: make the script tolerate either path by treating the initial `Ok` as optional and then optionally selecting `player` before continuing to the main menu.

## Validation

- Reproduced failing rerun: `{"result":"FAIL","steps_completed":5,"total_steps":16,"reason":"assert: SELECT: item \"Ok\" not found in menu (timed out)","elapsed_ms":5749}`
- Post-fix rerun passed: `{"result":"PASS","steps_completed":18,"total_steps":17,"elapsed_ms":9867}`