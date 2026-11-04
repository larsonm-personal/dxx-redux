# Cleanup CMake sibling recovery, November 4 2026

Separate user-requested cleanup task; no compatibility goal is resumed.

Fresh inventory found 46.5 GiB free and a 1.05 GiB registered-worktree CMake
build stranded by a vcpkg directory junction. Existing zero-retention Gradle/Cargo
cleanup already splits protected children; CMake generation cleanup does not.

1. Extend explicit KeepBuildGenerations=0 CMake cleanup through the existing
   generated-output splitter. Retain links, excluded and Git-visible children,
   nested repositories and locks; leave ordinary generation retention unchanged
2. Exercise a real junction with disposable siblings, exclusions and a held
   producer lock in the synthetic cleanup suite; run scoped quality and catalogs
3. Preview and apply only verified regenerable build outputs. Preserve all
   current/prior APKs, exact native executables, assets, reports, pilots/saves,
   stashes and worktree source. Archive native settings before removal
4. Record actual free bytes and preserved state. Do not delete emulator images,
   unmatched Git packs or evidence to meet a quota. No APK/device operations
