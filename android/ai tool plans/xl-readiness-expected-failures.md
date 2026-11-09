# Temporary sampled readiness expected failures

## Goal and rollout

Treat the four known per-case load failures as expected for the reporting job,
without changing the actual readiness score or the separate smoke/regression
result. The trusted controller must receive this change through an independent
reviewed PR; candidate-side changes do not update a workflow_run controller.

## Reviewed evidence

- PR #440 target: a4217ba4000d14fa495ac015f5263baee0c8883f
- Source run: https://github.com/larsonm-personal/dxx-redux/actions/runs/42518168350
- Artifact: xl-readiness, ID 13632690953
- Download SHA-256: d0a60c6ae1b5c1518c71b921ba6ca22fd714b45670cae98c9548c65e4f05ec33
- Pinned asset revision, selection seed and exact catalog digest are recorded in
  xl_readiness_expected.json alongside all eight case IDs and classifications

## Contract

1. Keep exact-head verification, receipt validation and the original fixed cohort
2. Require a successful measurement job and complete per-case evidence
3. Accept each known load_failed case only with the same published outcome,
   zero frames, unknown runtime path and unrun assisted navigation
4. Require every formerly passing case to remain loaded_ticked with valid bounded
   ticking evidence; allow known failures to improve without accepting a new failure
5. Fail on changed outcomes, missing fields, infrastructure errors, untested cases,
   identity changes or a changed published failure_signature
6. Keep the readiness commit status unsuccessful below 8/8 and show the honest
   4/8 score with an explicit expected-failure explanation in the job summary

The v1 producer publishes only a coarse load_failed classification, not a
root-cause fingerprint. A different underlying error with the same published
fields cannot be detected. Do not claim this baseline proves root-cause identity.
The exception does not apply to assisted navigation, rendering, smoke tests or
regression tests. Remove or revise it explicitly after the known gaps are fixed.

## Verification plan

Extend the existing ReadinessTests owner with matching baseline, improvements,
same-score case swaps, new/changed failures, incomplete evidence, measurement-job
failure, SHA/cohort/catalog checks and live report exit/status separation. Run
asset-free private-asset and workflow-controller tests plus scoped formatting.
Full native measurement remains a hosted CI check; no private assets need to be
staged for this reporting-only change.
