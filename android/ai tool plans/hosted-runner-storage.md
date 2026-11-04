# Hosted Android runner storage

- Inspect the reusable Android lane and failed #333 job metadata
- Measure aggregate host usage before and after removing only unused hosted image tools
- Guard cleanup to hosted Ubuntu 24.04 and retain SDKs, Rust, PowerShell, caches, native binaries and fixtures
- Check conservative free-space budgets before downloads, APK compilation and emulator boot
- Validate refusal, low-space and reporting paths, lint workflows, open a draft PR and follow CI

The failed job log blob is unavailable. Its steps ended during APK compilation before tests. No largest-consumer attribution is possible from that evidence. Host checks do not diagnose Android guest storage failures during LAN fixture installation.
