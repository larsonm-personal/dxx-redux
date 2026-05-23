## Goal

- continue the next small d1/d2 diff minimization batch after the coop shared-source cleanup
- prefer a nearby android-only or shared-helper extraction that reduces local divergence without widening scope

## Plan

- [completed] identify the next smallest concrete duplicated d1/d2 slice near the recent shared-helper work
- [completed] extract the duplicated Android `auto_net` source and declarations into one shared net source/header with no new `.c` include patterns
- [completed] validate the touched build path with `android\gradlew.bat :app:externalNativeBuildDebug --console=plain`, repair the one stale local-source CMake reference, and rerun the same build successfully
- [completed] rerun targeted searches to confirm only the shared `auto_net.c` and `auto_net.h` remain in the repo

## Outcome

- the next diff-minimization tranche was `d1/main/auto_net.c`, `d2/main/auto_net.c`, `d1/main/auto_net.h`, and `d2/main/auto_net.h`, which were effectively duplicate Android matchmaking glue next to the already-shared `net_udp_auto_*` implementation surface
- the shared implementation now lives in `android/app/src/main/cpp/shared/net/auto_net.c` and `android/app/src/main/cpp/shared/net/auto_net.h`
- the D1 and D2 local `auto_net.c` and `auto_net.h` copies were removed instead of replaced with wrapper includes
- both D1 and D2 Android CMake targets now add the shared net include directory and compile the shared `auto_net.c` directly