# LAN transition latency experiment

Baseline: origin/cmake 66e5e27da9850a9f142ac5c13d9e65432d48f4e3,
committed 2026-10-19T22:08:01-07:00, fetched 2026-10-20. Clean original
`work` checkout preserved; experiments use a new branch.

Plan:
1. Verify unchanged public dual-emulator smoke in existing GitHub CI (local
   /dev/kvm and Android tools unavailable). Inspect PRs 99, 100 and 101.
2. Add opt-in measurements to the existing public asset owner, enable briefing
   automation, collect per-peer monotonic observation bounds plus native logs,
   repeat native D1/D2 cold-process and warm-process transitions.
3. Chart actual observations and audit fixed waits. Only change engine behavior
   when measurements justify it; repeat identical experiments and check sync.
4. Publish compact data/report and harness; keep bulky logs in CI artifacts.

Never subtract clocks between devices. Controller Stopwatch durations bound
state observations; native automation elapsed_ms is local to each script.
Observation latency includes ADB and polling and must not be called engine CPU
or network time. Deliberate skip/score input and process setup are separate.
`trigger_endlevel` is a controlled real-engine exit fixture, not a played mine;
reactor seed is a test setup operation, not measured player combat.
