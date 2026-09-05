# Metadata worker failures

1. Reproduce the three failed archives without replacing regression data and retain native crash diagnostics
2. Identify the stack-overflow call path and correct its cause in shared code where possible
3. Rebuild affected Windows workers, rerun all three archives, and run scoped quality checks

## Findings and verification

- Original run: 129 passed, 1 skipped, 3 failed
- All failures reproduced with stack overflow (0xC00000FD)
- Native trace identified alternating fire_trigger and prepare_unreachable_trigger_source calls
- Preparation could recurse before the normal activation guard, and activation rollback also removed that guard
- Preparation now marks its own dependency while exploring candidates and restores it afterward
- Both Windows engines rebuilt; all three archives passed in the parallel runner without replacing regression files
- All 47 configured D2 CTest tests passed, including a new cyclic switch preparation fixture
- Worker EOF failures now retain logs and report the process exit code
