# Vertigo HAM analysis failure

## Goal

Make Vertigo metadata analysis use the installed expansion data correctly when `d2x.hog` is present and `d2x.ham` is stored inside it rather than listed as a standalone launcher file.

## Plan

- [x] Trace Vertigo discovery, staging, PHYSFS mount order, and HAM loading for gameplay and metadata workers.
- [x] Reproduce the failure with the local Vertigo data or a focused fixture and confirm whether `d2x.ham` is embedded in `d2x.hog`.
- [x] Fix the narrow analyzer path without duplicating HAM parsing in Kotlin.
- [x] Add regression coverage for Vertigo analysis from the supported installed-file layout.
- [x] Run scoped quality checks, the focused canonical-data test, and the Android build.

## Findings

- Canonical Vertigo stores `d2x.ham` inside `d2x.hog`; a standalone HAM is not required or expected in the launcher file list.
- The Android worker successfully analyzed all 20 normal and 3 secret Vertigo levels from the installed-file layout, including after a background D2 request had initialized the reusable worker.
- A missing or unreadable embedded HAM previously reached the engine fatal-error path. The metadata worker now checks the mounted Vertigo data first and returns a precise failed result instead of terminating its process.
- The final focused Android regression passed against the canonical installed layout through both `d2x.hog` and managed `d2x.mn2`, reporting all 23 levels for each entry.

## MN2 entry follow-up

- [x] Reproduce and trace target construction when `d2x.mn2` is stored in managed content and `d2x.hog` is at the file-set root.
- [x] Make direct descriptor targets resolve a same-stem HOG from either the descriptor directory or file-set root without copying data.
- [x] Add unit and Android integration coverage for metadata analysis through the MN2 entry.
- [x] Verify map preview through the MN2 target, run scoped quality checks, and rebuild the APK.

## MN2 entry findings

- Managed descriptors are stored below `.content/entries/.../payload/missions`, while their adopted HOG can remain at the file-set root. Target construction now performs one case-insensitive root lookup when no adjacent HOG exists.
- Preview requests now carry the descriptor directory separately from the base-data directory and root HOG. Native preview mounts that directory at `missions`, so `load_mission_by_name("d2x")` sees the real current file layout without copying files.
- The preview process also lacked the native debug-log callback methods used during loading. It now implements the same callbacks as the other native-host activities instead of terminating on the first loading log.
- Strict post-build introspection confirmed Vertigo level 15 (`d2xlvl15.rl2`) active in the automap, the first frame ready in 339 ms, and both the managed mission directory and root `d2x.hog` in the PHYSFS search path.
