# Fix Android net_udp build interruption

## plan

1. [x] Reproduce the failing Android native build after confirming no stale formatter task is still mutating files
2. [x] If the build still fails, inspect the exact broken D1/D2 `net_udp.c` slices and repair the smallest shared corruption
3. [x] Re-run the Android native build and update this note with the outcome

## outcome

- `android\stop-stale-formatters.ps1` reported no stale formatter tasks
- The current `d1/main/net_udp.c` and `d2/main/net_udp.c` source around the previously reported error lines was already structurally intact
- `android\gradlew.bat :app:externalNativeBuildDebug --no-daemon` succeeded, including successful compilation of both `d1/main/net_udp.c` and `d2/main/net_udp.c`
- `android\1_build-aab.ps1 -BuildType 3 -VersionCode 11901` also succeeded and produced `build-outputs\dxx-redux-internal-20260422-204423-v11901.aab`
- No source repair was required; the earlier failure was most likely a transient concurrent-write interruption during the previous build attempt