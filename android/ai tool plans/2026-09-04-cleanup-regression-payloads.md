# Regression payload disk growth

- [x] Measure scratch usage and recent creation times
- [x] Extend clean-workspace.ps1 with automatic raw/stages payload cleanup for known metadata and guidebot runs
- [x] Keep reports, active work, Git-visible files, links, and recent payloads protected
- [x] Add fixture tests, run formatting, preview actual reclaimable bytes, and apply eligible cleanup when idle

Initial inventory: guidebot_simulation_regression 83.08 GiB, mission_zip_host_metadata 18.11 GiB
Three afternoon host runs retained 17.98 GiB, six guidebot runs retained about 20 GiB
Copied source modification times conceal the growth; file creation times identify today's copies

Validation: test_clean_workspace.ps1 and scoped code quality passed
Payload-only trees are grouped to avoid thousands of repeated process/Git checks; mixed raw/report folders retain per-mission cleanup
Focused preview identified 96.60 GiB of eligible payloads
Applied cleanup in two passes, preserving reports/logs; final free space was 106126884864 bytes (98.84 GiB), up from about 2.10 GiB
