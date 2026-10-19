# Locked exit prerequisites

Base: `3bab162341dd85dab3bb7978a68d5ab17b99ff4d` (`cmake`)

Implement one candidate: resolve a locked final exit door's remote opener before
appending the exit objective. Preserve ordinary keyed exits and reject inaccessible
openers. Do not change controller retry behavior in this candidate.

1. Add synthetic planner controls and demonstrate the missing prerequisite on base
2. Implement the general prerequisite resolution and run the planner tests
3. Run the same full corpus before and after, inspect every changed status,
   objective sequence, frame count, timeout and missing/skipped case
4. Publish a draft PR with exact validation limits; do not publish new baselines

## Runtime prerequisite

The supported Linux route runner uses `process_lifetime_linux.py`, which reads
`/proc/self/task/<pid>/children` before spawning work. That read was denied earlier
in this workspace. The exact original stderr/PID was not retained in the available
logs, so it must not be fabricated. The user prohibits retrying that denied path or
bypassing the supervisor. Native corpus evaluation needs a sanctioned environment
where the unchanged runner has its required child-process supervision access.
Windows is a documented runner platform with job-object supervision, but no Windows
environment is attached; switching platforms would require establishing both
before and after baselines there, not treating results as equivalent by assumption.

No full-corpus execution baseline has been established here. The checked-in
reference contains 2,639 level records (2,201 ok); it is comparison input only.
Archive coverage must be checked before claiming a full-corpus run. Missing assets
and skipped levels must remain explicit, not count as improvements.

## Authored evidence

Existing authenticated access downloaded `Countd2.zip` from
`larsonm-personal/dxx-redux-test-assets/missions`.
SHA256: `0377a47d23645061f22bc219e047c9be89d0261747f84f8c47bbd4ee87690c8e`

Levels 1-6 have locked exit doors and separate opening triggers omitted from the
published routes. Level 1's exit at 69:4 is opened by trigger 1 at 150:4. Level 7
passes with an unlocked red-keyed exit. `prepare_exit_access` handles a solid wall's
opener, but previously only acquired keys for an unopened door.

## Validation

The positive synthetic test fails on the unchanged base: it returns an ok exit
route without the required opener (`opened.steps.size() == 3` assertion fails).
With the candidate, ordered opener-before-exit checks pass for both a shootable
switch and an open-passage trigger. An isolated opener is rejected and an ordinary
unlocked red-keyed exit still collects the key and exits.

Both D1 and D2 Debug CMake configurations pass `test_route_snapshot`,
`test_guidebot_route_certifier`, and `test_guidebot_route_decision` (3/3 each).
Scoped `android/run-code-quality.ps1 -Fix -Paths` passed for the two source files
using clang-format 20.1.0. `git diff --check` passed.

Full-corpus before/after execution remains blocked by the runtime prerequisite.
No claim of improved native pass count or absence of corpus regressions is made.
A local manifest preserves SHA256 hashes for all 280 checked-in mission metadata
and simulation files at the base. Those files are unchanged. No raw logs or large
generated reports are included in the PR. Keep the PR draft until equivalent
before/after corpus runs and all changed outcomes have been reviewed.
