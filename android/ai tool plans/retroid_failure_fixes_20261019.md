# Retroid failure fixes

Baseline: clean rp4p_tests at d8fab96c3; physical device JYPR42510121028
Evidence: retroid_full_validation_20261018_results/REVIEW.txt

- [ ] Diagnose and fix D2 texture precedence: test_android_mod_renderer, test_uud_mod_combinations, imported D1 test_mod_fuzz case
- [ ] Diagnose and fix HAM patch baseline normalization: test_android_mod_sounds, test_uud2sp, D2/Vertigo test_mod_fuzz cases
- [x] Fix D1 classic demo playback: test_fov_demo_compatibility
- [ ] Fix D1 failed graphics acceptance recovery: test_graphics_recovery
- [ ] Resolve first-analysis throughput and verify cache reuse: test_level_metadata_result_cache_reuse
- [ ] Diagnose late GuideBot objective transition: test_obsidian_level7_switch_guidance
- [ ] Fix accessible chooser layout and complete test_slider_navigation
- [x] Build current native and Android targets; run all ten owners serially on Retroid
- [x] Run scoped quality checks and relevant regressions; record results and remaining limitations

Preserve security validation and test assertions; investigate failures before changing expectations

## Progress

- test_android_mod_sounds passes: D1 1 case and D2 11 cases, temp/retroid-fixes-sounds
  Preserve nonnegative empty joint offsets for exact HAM patch preconditions; normalize negative offsets only
- test_android_mod_renderer passes: D1 11 cases and D2 23 cases, temp/retroid-fixes-renderer2
  Baseline log shows texture index failed at 4097 visits with only 2935 indexed textures
  PhysFS repeats shared virtual directories across mounts; deduplicate queued paths without raising limits
- D1 accept_publication_blocked recovery now passes in 6131 ms
  android/temp/graphics-recovery-20261019-094540/results.json
  Background ledger IOException previously killed the game; now report storage failure without process termination
  Full 18-case owner remains to be rerun
- Chooser width expansion and Cancel label pass all layout checks
  Full navigation owner now reaches a later failure: Dialog 1 did not repeat, initial row -1
  Instrumentation now explicitly enters keyboard mode in each new dialog before seeding focus
  Full owner now passes in temp/retroid-fixes-slider2.log
- D1 demo diagnostic reproduced clip=0 frame=0 clips=0 at offset 569
  Original D1 data uses populated fixed-size table despite zero Num_vclips
  Added same count fallback as network validator; actual frame validation retained
  Both D1 and D2 pass all 82 steps in temp/retroid-fixes-quick/test_fov_demo_compatibility_20261019_095110.log
  All five D1 host classic-demo unit tests pass
- Cache diagnosis: a full-CPU experiment still exceeded 600 seconds while making route progress
  Reverted the CPU-policy experiment after finding the documented Uneasy4 route timeout
  Obsidian replacement fixture passes in 54 seconds with the same persisted-hit assertion
- Obsidian original owner passed once in the quick batch (53 steps, 18 cold metadata levels)
  Earlier failure remains timing-sensitive; temporary Android trigger-actor logging in d2/main/switch.c
  Probe temp/retroid-obsidian-probe.jsonc reuses metadata caches to diagnose gameplay quickly
  Remove temporary native logging after diagnosis
- Builds 1, 2, and 3 succeeded; build 3 includes demo fix, Cancel label, and Compute faster action
- Scoped quality passed for first six implementation/test files
  Replacement texture safety guards and HAM transaction Python checks passed
- Both Windows engines build successfully; all 12 relevant classic-demo/HAM CTest checks pass
- Full host classic-demo compatibility corpus and malformed-record checks pass
- Both catalog checks pass: 364 owners, 426 support scripts
- Quick batch report: temp/retroid-fixes-quick/report_20261019_095110.md
  PASS FOV and Obsidian; FAIL cache timeout and later slider dialog setup

Still required: finish graphics recovery, UUD2SP, UUD combinations, 14-case fuzz,
and the final cold-cache Obsidian 7 rerun; publish final evidence

## Follow-up evidence

- Full slider owner passes in temp/retroid-fixes-slider2.log
  All three dialog input modes were Touch before explicit keyboard-mode focus setup
- Uneasy4 route preparation still exceeds 600 seconds at full CPU
  September 20 plan documents this as an accepted, mission-specific route-planning timeout
  Cache reuse now uses Obsidian with the same persisted-result assertion and original timeout
  Reverted the unrelated foreground CPU policy experiment
- Obsidian warm-cache gameplay probes pass both at original 500 ms and extended 2500 ms after warp
  No trigger 7 activation was observed; the companion-flare hypothesis was not confirmed
  Removed temporary native logging without changing gameplay or assertions
  Final cold-cache owner rerun remains required

## Completion

All ten owners have passing follow-up evidence. Final selected batch: five PASS,
zero FAIL; full graphics owner: 18/18 PASS. Graphics required a separate direct
run because the selected batch omitted the extended-graphics flag.
Full report and durable evidence: retroid_failure_fixes_20261019_results/REVIEW.txt
Final diff whitespace check passes; device power/log settings restored.
Obsidian remains a historical intermittent failure without a confirmed root cause;
two cold and two warm runs passed without altering its assertions.
