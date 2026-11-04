# LAN launch preparation network evidence

- Inspect failure at 8a4b7e511008359f4daa0b706acd4c90165a18e8, run 41906225644, job 126882833308
- Preserve all lobby, readiness, launch and gameplay assertions and production networking
- Add the existing direct-LAN owner's bounded shared-Wi-Fi readiness concept to launch preparation, with bidirectional ping receipts and address, route, neighbor and connectivity diagnostics
- Extend the existing host-only profile controls for transient recovery, permanent and asymmetric failure, missing addresses and diagnostic errors
- Run scoped quality, profile controls and both automation catalogs; publish a separate draft for device validation

## Evidence and limits

The delegated artifact analysis reports broadcast QUERY/ANNOUNCE delivery but more than 40 client JOIN sends with no host JOIN receives. The workflow log confirms the dual-emulator job failure; the delegated artifact analysis identifies the 25-second lobby timeout. This supports testing infrastructure readiness but does not establish the precise route/neighbor fault or prove ICMP success implies app UDP delivery. This environment has no Android SDK/emulator or /dev/kvm. The GitHub connector fetched job logs and an artifact reference; local gh artifact download was denied with HTTP 403 at the blob redirect. No production network fix is justified from this evidence alone.

## Host validation

- Existing LAN profile suite passes, including new bidirectional, transient, persistent, missing-address, duplicate-address, no-output and diagnostic-error controls; actual session orchestration propagates preflight failure before lobby commands
- Scoped quality passes with pinned PSScriptAnalyzer 1.25.0 and repository-locked Prettier
- Automation catalog passes: 99 standalone JSON tests, 533 support scripts, 315 standalone PowerShell tests
- Direct master catalog generation passes: 428 entries, retail owner timeout 360 seconds, public owner remains 1500 seconds
- Full master-catalog test remains blocked by the environment's unavailable `/proc/self/task/<pid>/children` interface, including outside the command sandbox; this is not a passing result
- No emulator, raw UDP experiment or on-device success claimed; original CI failure and precise routing cause remain unverified until a device run
