# Rejoin crash diagnostics

User will reproduce original-host departure, client promotion, and original-host
rejoin using the launcher's exported logs, without adb logging

- [x] Add shared Android diagnostics for raw transfer packets, parsing, world
  allocation counts, player mappings, migration, and pre-assert state in D1/D2
- [x] Route join/lifecycle events through the existing Network category in the
  exported logger; skip packet hashing and world scans when disabled
- [x] Preserve protocol, admission, allocation, and assertion behavior
- [x] Run scoped quality and compile both Android engines
- [x] Give the user export instructions for both peers

Do not infer transport loss from missing post-validation breadcrumbs. Match raw
sender/receiver packet hashes and inspect processing decisions first

## Validation and reproduction

- Scoped mixed-language quality and git diff whitespace checks passed
- Android arm64 RelWithDebInfo D1/D2 CMake targets compiled and linked
- Existing D1/D2 warnings about unused locals and array comparisons remain;
  no diagnostics-related compiler warnings
- Initial compilation encountered stale generated build metadata required by
  existing working-tree changes; running :app:generateNativeBuildInfo resolved
  this without editing those changes
- Diagnostics use debug_log(DLOG_NETWORK) and respect the Network toggle
- No device reproduction or APK packaging in this change

Install a build containing these changes on both devices. Reproduce host leave,
migration, and rejoin. Export the app debug log through Advanced on each device,
plus the crashed device's crash report. Enable Network logging on both devices
before reproducing

Lines prefixed [REJOIN] correlate raw socket TX/RX and dispatch by packet hash,
record parser decisions and completion, and show allocation/player mappings at
migration, transfer start/end, sync acceptance, level preparation and failure

## Network category follow-up

At the user's request, diagnostics now respect the Network logging toggle and
skip packet hashing/world scans while disabled. Scoped quality and compilation
of all affected D1/D2 ARM64 translation units passed. Full relinking was blocked
by the unrelated in-progress mod_save_compat.cpp std::min/macro compilation error
