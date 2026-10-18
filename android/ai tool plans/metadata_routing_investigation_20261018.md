# Metadata and routing regression investigation

User request: chase down each possible problem identified in the regenerated JSON

Baseline: regeneration from commit 9302a5a4b, preserved under temp/metadata_routing_refresh/run_20261018_091329. Canonical JSON changes already in the worktree are evidence, not accepted baselines. Other session quality-validation files must be preserved

1. Reproduce Harqyjia level 2 timeout and af_d1_beta level 7 edition rejection using the captured host engine and fresh user state
2. Trace widespread simulation RNG consumption changes against the source revision behind the saved baseline; distinguish intentional changes from nondeterminism or defects
3. Trace timing outliers, including BelialSystemXL level 12, Disint_Beta_3 level 2, and the incontin.rl2 timeout improvement; check repeatability and causes
4. Reproduce and explain nonzero masks on logically unbound matcens in Ironblade level 3 and Orion D1/D2 level 7, including cross-level state and historical behavior
5. Fix confirmed defects with focused integration coverage, build both affected engines, run relevant checks and scoped formatting
6. Record evidence and remaining limitations for every item and audit completion

Use host processes, preserving shared emulators. Keep at least 100 GiB free. Store investigation output under temp/metadata_routing_investigation/run_20261018 and durable findings beside this plan

Completed: all six investigation steps. Findings and evidence are in `metadata_routing_investigation_20261018_RESULTS.txt`; the machine-readable verification is `temp/metadata_routing_investigation/run_20261018/completion_audit.json`. The confirmed wall-cloaking overflow is fixed with a failing old-code control and passing native coverage. Harqyjia was republished with three identical runs per level. RNG/timing changes and all cold/warm matcen differences are causally explained within the stated test scope; the authored invalid robot mask remains rejected
