# Recent test report fixes plan

## Goal
Fix one or two failures from `temp/test_reports/report_20260611_234545.md` and add end-of-suite emulator shutdown to `android/run_all_tests.ps1`.

## Steps
- [x] Read repo instructions and the failing report
- [x] Fix the GOG installer wrapper/template parameter failure
- [x] Add run_all_tests cleanup that shuts down both emulators after unattended runs
- [x] Run focused validation for the changed scripts
- [x] Record remaining risks and follow-up failures
