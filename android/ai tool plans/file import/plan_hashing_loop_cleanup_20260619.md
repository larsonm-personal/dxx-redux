# hashing loop cleanup

Goal: remove temporary instrumentation now that the imported-file hashing loop
was diagnosed as case-variant duplicate game files.

Plan:

1. [done] Keep the durable launcher fix in `AssetManifest`.
2. [done] Keep regression coverage for duplicate manifest entries and
   case-variant disk files.
3. [done] Remove temporary live setup hashing diagnostics.
4. [done] Remove the one-off adb probe script.
5. [done] Add test-helper cleanup/guard so emulator tiers do not proceed
   with standard game-data case variants.
6. [blocked] Re-run Gradle/unit/code-quality validation from this agent.

Notes:

- Probe output showed the loop was caused by `DESCENT.HOG`/`descent.hog`
  and similar case variants sharing one lowercase manifest key.
- The permanent behavior is now canonicalized at the manifest layer: one
  physical file per lowercase game filename, preferring the exact lowercase
  disk name.
- `Resolve-GameDataDeps` now normalizes case variants on-device and verifies
  that standard game-data dependencies have one lowercase file after
  provisioning. This keeps emulator test tiers from proceeding with the bad
  `DESCENT.HOG` + `descent.hog` state.
- Validation is blocked in this agent because process launch still fails with
  `-1073741502` even for trivial commands.
