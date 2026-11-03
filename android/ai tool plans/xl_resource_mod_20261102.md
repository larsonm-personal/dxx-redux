# User-imported XL resources, managed together as a mod

## User intent and constraints

Import only the supported resources from the user's D2X-XL distribution 7z and
allow raw-file additions. Keep camera HOG, separately supplied Vertigo HAM,
particle/lightning/shield/glare images and shared sounds together in Mods.
There must be no generated DXA, resource-pack builder, exporter, or redistribution
workflow. Files are user supplied and never bundled into APK/Git. "Mod" means
activation and management, not a packaged archive. This correction supersedes the
earlier DXA implementation, whose builder and tests have now been removed.

Keep normal tap-APK-install and launcher Import Files. Keep the original archive
untouched; select entries from its catalog before decoding. Seekable 7z providers
read directly; streaming sources use a bounded staged-copy fallback. Solid blocks
may require preceding data. Preserve mission-local resources and ordinary retail
imports. Mixed retail distributions import supported resources then continue the
retail flow. Exclude extra.hog, exit.ham, replacement texture/model collections,
movies, executables, caches and settings until supported consumers justify them.

## Current implementation

- ResourceModCatalog maps source paths to supported private paths, recognizes raw
  standard sound names, and rejects collisions including PNG/TGA alternatives.
- ResourceModStore extracts into a temporary private directory and records paths,
  source paths, sizes and SHA256 hashes in an internal inventory. It verifies then
  atomically publishes the directory; failed updates preserve earlier contents.
  Incremental imports retain untouched components and replace alternate encodings.
  It contains no ZIP/DXA writer or resource archive generation.
- ResourceModImport handles ZIP/7z/raw sources, uses existing storage/copy limits
  and calls ModManager. General Import Files invokes it before existing flows.
- ModManager has an imported_resources kind backed by a directory. Import keeps
  its toggle/order; enabling verifies inventory; enabled paths are published as
  explicit native resource owners. Details show imported files by category;
  deletion removes that group's private directory. Retail D2 tracker exceptions
  and the old selective-addon helper/test have been removed.
- Native particle_resources.cpp accepts owned PNG alongside TGA, with mission
  precedence, bounded true-color input and existing XL alpha/atlas handling.
  Android uses its existing stb decoder without cosmetic fitting. Desktop uses
  its PNG reader after size preflight. A synthetic PNG/TGA orientation/soft-alpha
  comparison was added to the existing effect-inspection owner.
- Camera missing-resource errors now point to importing/enabling XL resources.
- XlAddonImportChecks exercises the real importer with isolated directory storage,
  seekable/pipe 7z, ZIP/raw updates, preserved disabled state, no D2 tracker entries,
  no generated DXA, and the existing progress-toggle checks.

## Evidence and remaining work

Earlier DXA tests are historical and do not prove the corrected directory store.
Directory tests and Android-test compilation run under directory-tests.log.
An initial compile error used a suspend hash helper synchronously; it was replaced
with streaming synchronous hashing and rerun. Inspect the final process/result.

Android x86_64 D2 native library compiled/linked with PNG changes (png-android-build.log).
The broad default Windows build failed in unrelated test_game_automate_input due
to missing SDL_types.h. Targeted native lifecycle build uses the existing cache
at temp/xl-particles-20261030/native-windows and target dxx-redux-d2-headless-metadata;
its OUTPUT_NAME is xl-preview-lifecycle.exe. Inspect png-host-controls-build.log.
Run the existing android/XL_compat/tests/effect_inspection.cmake against that new
executable with isolated output, original Neptune, retail data and fixture tool.
Prior CTest arguments are in temp/xl-particles-20261030/contracts-windows64/CTestTestfile.cmake.
Do not run unchanged exhaustive qualification.

Before completion:

1. Verify directory JVM tests and updated instrumentation compile; fix failures.
2. Run native PNG resource controls and check actual archive PNG compatibility.
3. Serial owned-emulator checks: direct imports, one Mods row/details, toggle and
   ownership isolation, mixed archive behavior, original Pumo transition/save and
   protected restore. No user-phone mutations; phone permission stays read-only.
4. Audit metadata/browser ownership as well as gameplay; directory backing must
   not get treated as an archive by another mod consumer. Check mixed-retail and
   mission-package classification, cancellation/progress and missing-file errors.
5. Commit only when locally validated, build normal app after commit, verify all
   native IDs/no proprietary assets, and test exact APK if delivering one.
6. Complete self-contained handoff with changes and WHY, artifacts/IDs/hashes/UI
   instructions, tested/untested boundaries, stack/PR targets/exact heads and CI,
   preservation/cleanup, ranked next work and a copyable continuation prompt.

## Preservation and continuation

Branch xl_resource_mod_import starts at020c322894e56a507d8f4c89d678854e489dd522,
PR328 remains unmerged at last check. All new work is local/uncommitted; no new
APK or PR yet. Keep preexisting stash, all previous bundles and unrelated work.
Only fork larsonm-personal/dxx-redux may receive commits/push/PRs; never merge or
push upstream. Refresh refs/PR states/exact-head CI before publishing.

The old timed playtest goal remains paused. This is the active resource-import
goal, without a cutoff. Do not invent a schedule or mark it complete before the
corrected directory-based import-to-runtime behavior is verified. The previous
APK022a192/version29330, hash/instructions, historical gameplay and GuideBot pilot,
stack and cleanup are recorded in temp/pumo-user-addon-playtest-20261102/final_handoff.md.
No new device has been started during this implementation so far.

Copyable continuation prompt: Continue the active XL resource-import goal in
C:\local\dxx-redux. Read AGENTS.md, .github/copilot-instructions.md, this plan and
current worktree/logs first. There must be no generated resource DXA or packaging
machinery: import selected user-owned 7z/raw files directly into private app
storage, managed as one mod. Finish the remaining validation above and preserve
all constraints here and in the previous handoff. Keep strict fog/progression
checks and mission assets; no forced progression. Use only owned disposable or
protected devices serially; preserve profiles and exact evidence. Every final
deliverable must repeat changes/WHY, artifact IDs/hashes/launch, supported and
untested boundaries, PR stack/targets/exact heads/CI, cleanup, ranked next work,
and a fresh prompt carrying these constraints and this repeating requirement.
If a later timed goal is requested, leave [USER-SUPPLIED ABSOLUTE CUTOFF AND TIME
ZONE] for the user when unknown; never start the following goal automatically.

## Validation update (directory implementation)

- Five JVM tests passed, including selective extraction from the supplied 1.5 GB
  distribution (directory-test-results.xml); no generated DXA. A follow-up run
  also covers raw folder recognition; inspect final-tests.log before publishing.
- Android instrumentation compiles; expanded checks include mixed retail archives
  and preservation of mission-local resources. Device execution is next.
- Native effect-inspection gate passed with the new exact PNG/TGA pixel comparison
  (png-effect-controls/controls.log). All eight selected source PNGs fit supported
  dimensions and RGB/RGBA encoding (png-source-dimensions.json).
- Automation and suite catalog checks passed (428 top-level entries, 533 support
  scripts). Scoped quality passed. Broad Windows failure remains unrelated and
  must not be described as a full build pass.
- Owned emulator DxxPhoneFog20261102 / emulator-5556 started for this task at
  18:38:28 on November 2, PID38516. Identity is recorded in owned-emulator.json.
  No phone mutations authorized. Preserve profile and stop only this owned process
  after serial validation; the earlier "no device started" note is superseded.
- Parent PR328 remains open at020c322894e56a507d8f4c89d678854e489dd522 after refresh.
