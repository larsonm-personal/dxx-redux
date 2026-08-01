# Luna max adversarial review experiment

## Plan

- [x] Confirm whether Luna max is callable in the current environment
- [x] Measure current queue sizes, risk mix, and finding density
- [x] Select representative blinded comparison chunks
- [x] Propose Luna-specific chunk budgets and review strategy
- [x] Run available comparisons or provide exact ready-to-run prompts
- [x] Record conclusions without changing canonical queue coverage

## Result

- Confirmed `gpt-5.6-luna` with max reasoning is callable.
- Ran blinded read-only comparisons against historical R1 chunks without
  changing either ledger or canonical queue state.
- Luna recalled the mutable admin-tray identity root and the unguarded Store
  fallback, missed the two-task Play Core root, and produced several new
  candidates requiring independent verification.
- A 385-line high-fan-out scope exceeded 20 minutes and was stopped without a
  final report, establishing the need for semantic splitting and a time-boxed
  evidence checkpoint.
- Recommendations and full preliminary results are recorded in
  `luna_max_audit_trial.md`.
