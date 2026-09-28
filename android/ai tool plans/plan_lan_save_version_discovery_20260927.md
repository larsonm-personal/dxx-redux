# LAN save-version preflight and active discovery

## Evidence

- Build 23391: both peers reach level 2 and complete save transfer
- Host restores slot 6, whose co-op trailer version 13 passes launcher preflight
- Both engines reject D1-in-D2 save version 31 because supported asset-identity formats are 40 and 42
- Client sees no LAN packets until Find Last Host sends a direct query; broadcasts use 192.168.88.255
- Waiting longer cannot resolve the save rejection; the earlier preflight checked only the co-op trailer

## Work

- Share the engine's D1-in-D2 save-version policy with launcher native validation
- Resolve mission content game for base and custom missions; warn before launch with the rejected version
- Make discovery actively query broadcasts and remembered hosts, without auto-joining
- Identify direct query replies in exported logs
- Extend native and emulator regressions to cover current co-op metadata plus an obsolete engine save version
- Run scoped formatting, relevant builds/tests, and emulator coverage

## Validation

- Scoped code quality checks passed
- Android x86_64 debug build passed, including both D1 and D2 native targets
- Windows D2 build and test_coop_save_format.exe passed
- 13 Kotlin tests passed (lobby protocol and mission scanner)
- Emulator test_coop_save_compatibility.ps1 passed: malformed save rejected, valid co-op trailer plus D1-in-D2 version 31 rejected with host/client warning, versions 40/42 admitted by preflight, Start fresh clears warning
- Emulator test_lan_active_discovery.ps1 passed: a remembered query-only host is discovered automatically and is not auto-joined
- Discovery fixture uses an independent UDP source port for emulator redirection because netsimd owns the outgoing query mapping
- The physical Wi-Fi broadcast loss is not established by the logs; active remembered-host queries avoid depending on those broadcasts
