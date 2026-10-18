# Integrated co-op validation

- Validate cmake commit 54e1e1e3c65d42ca19109e3dd3a038642c1c2478 without pushing or merging into cmake
- Confirm PR #89, #90 and #91 content and inspect co-op changes from b600672a7
- Prioritize the existing default dual-emulator LAN host/join/start/in-game runner without changing defaults
- Record Android provisioning blockers and distinguish setup-only, source-only, unit and real peer execution
- Build both native games and run changed co-op recovery, pickup, door, restart and rewind checks
- Reuse exact-head public engine, save/replay and tooling CI for broad validation
- Keep private matcen execution and crash investigation stopped
