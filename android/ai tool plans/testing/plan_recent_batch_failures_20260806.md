# Recent batch failure investigation

## Goal

Classify the failures in the attached recent test report as product
regressions, deterministic test defects, or infrastructure failures, and apply
narrow durable fixes without masking problems through timeout increases.

## Plan

- [x] Create the investigation plan
- [x] Read the report and identify failures, detailed logs, and predecessors
- [x] Correlate failures with recent source and test changes
- [x] Reproduce likely product regressions with durable diagnostics
- [x] Reproduce timing failures with relevant predecessor state
- [x] Implement narrow fixes supported by evidence
- [x] Validate affected tests in ordering-sensitive sequences
- [x] Run scoped quality checks and record final dispositions

## Disposition

- `test_trine2_d1_in_d2_custom_textures`: harness false positive. The launcher
  process was killed while the separate game process continued producing native
  and automation logs. Launcher-script monitoring now accepts either live app
  process and fails only when both have exited.
- `test_extract`: two harness limits were wrong. Long CD/ISO imports now use
  asynchronous broadcast delivery and continue to poll the durable import
  tracker and expected files. The standalone outer budget now matches the
  600-second per-sample extraction-suite budget.
- `test_saf_archiver`: no product regression reproduced after its original
  predecessor or after extraction state. Its custom monitor now treats 60
  seconds as an inactivity limit renewed only by a higher durable automation
  sequence number, rather than as a fixed wall-clock limit.
