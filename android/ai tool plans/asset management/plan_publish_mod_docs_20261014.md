# Publish mod documentation

User authorized updating adapted mods and publishing mods plus documentation bundles to the existing GitHub release.

- [x] Author pack-specific Redux notes for all eleven adapted release variants
- [x] Extend release preparation to assemble documented packs and deterministic companions, preserving original documents and every gameplay entry
- [x] Verify small-fixture failure cases, all thirteen real packs, unchanged upstream archives, and repeat preparation
- [x] Update catalog sizes and release instructions; run scoped formatting and applicable checks
- [x] Publish to texture-packs-v1, verify remote identities and downloaded doc packs, then verify repeat publication uploads nothing

Keep original conversion outputs intact. Assemble final documented packs under the release output directory, making documentation a repeatable release-preparation step without rerunning lossy conversions. The two original Rebirth packs are uploaded directly, without rewriting their ZIPs. Launcher document UI implementation remains outside this publication task.

## Validation and publication

- All thirteen baseline inputs matched the old public release hashes before preparation
- Final packs preserve 9,940 original document and gameplay entries; all eleven adapted packs contain their own README.dxx-redux.md
- Both unchanged Rebirth archive hashes match the audited upstream downloads
- Six fixture integration tests pass, including refusal to replace unknown author text and refusal to publish a changed companion
- Full release validation passes for all thirteen packs; repeat preparation preserves bytes and modification times for all 29 generated outputs
- Scoped formatting and Kotlin compilation pass; the nine launcher catalog sizes match final pack sizes
- Publication replaced eleven adapted packs, uploaded thirteen companions and updated three metadata assets; both Rebirth packs were skipped
- GitHub size/SHA-256 verification passes for all 29 assets; all thirteen downloaded companions and their seventeen document entries match the local manifest
- Repeat publication reports zero changed assets and 29 unchanged, with all remote identities verified again

Release: https://github.com/larsonm-personal/dxx-redux/releases/tag/texture-packs-v1

Evidence and final adapted archives: `android/temp/mod-docs-release-20261014/`. This includes `documentation-preservation.json`, `repeat-preparation-result.json`, `downloaded-docs-verification.json`, fixture/build logs, and publication logs. Final adapted DXAs are under `packs/`; unchanged Rebirth DXAs remain in their original conversion workspace paths
