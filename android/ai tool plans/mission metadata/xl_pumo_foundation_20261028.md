# Pumo Mines dependency work, 2026-10-28

## Run and objective

Continue XL single-player fidelity work for twelve hours, starting at
2026-10-28 16:54:16 UTC and ending at 2026-10-29 04:54:16 UTC. Continue from
xl_more_38_archives, 353f17c8bbd27058670a4dae4ffd97a49b7277e9. Its seven CI
workflows passed before this run. Keep reviewable dependency branches and PRs;
do not merge them or change the clean root cmake checkout.

Pumo Mines is a useful acceptance fixture, not a filename exception. Its
196505200-byte HOG now indexes. The new 25/27 inspection tranche decodes all
fourteen levels and compares their geometry, extras, fog and object readers to
the pinned source. Runtime admission and independent feature requirements remain
blocked. Loading the container or decoding its levels does not demonstrate gameplay.

## Dependency order

1. Complete extended-HOG launcher integration through shared native format
   interpretation. Cover import, asset/metadata catalogs, soundtrack discovery,
   music preview/staging and resource cleanup. Preserve bounded streaming and
   extraction budgets. Exercise ordinary archives, mixed extended records,
   malformed inputs and the unchanged Pumo distribution.
2. Audit the pinned XL version-25/27 readers and implement the actual layouts,
   with independent source comparisons, truncation/boundary tests, and original
   level inventories. Never admit unknown data by treating it as a supported
   older layout. Separate decoding from gameplay admission.
3. Use the complete Pumo inventory to select the next reusable gameplay or
   resource dependency. Implement XL semantics separately where Redux differs.
   Include owning state, save/restore, transitions and stock isolation.
4. Validate each published increment with relevant native and Android tests,
   source comparison where applicable, scoped formatting and CI. Update evidence
   and compatibility documentation with the exact verified scope and blockers.

No autonomous 3D navigation. Use controlled engine/event tests and reserve real
campaign playthrough judgments for human testing. Defer cosmetic polish and
multiplayer; retain gameplay-relevant visibility and audio requirements.

## Completion evidence

Record the final branch/commit/PR stack, exact tests and CI state, original
archive hashes, decoded requirements and remaining blockers. Account for the
full time window; do not declare the continuing run complete after one tranche.

## Launcher increment

Working branch: xl_more_39_hog_launcher. Shared C/native/JNI header parsing now
serves engine mounting, launcher import/catalogs and both music paths. Host
import of the unchanged Pumo distribution passes, while all fourteen levels
retain their version-27 rejection. The normal APK passes all 17 derived import,
preview and engine checks with 27 protected files restored exactly. The full JVM
suite reports 1238 tests, zero failures/errors and seven skips; the optional
original-Pumo test passed separately. Source/header/inventory and rebuilt native
archive lifecycle controls pass. See
[launcher evidence](../../XL_compat/evidence/extended-hog-launcher.md).

Both final catalog checks and scoped mixed-language formatting pass. PR #183
merged during this run. Fresh origin/cmake is
99f65b2679d5c7c891de1395f70418593b05006a; its source tree exactly matches the
validated 353f17c8b base. Publish only the new launcher commit on that cmake base.
Continue the run with source-audited version-25/27 inspection after this
increment; no original mission is newly qualified.

The user's timing question was answered with a provisional 3-6 further long
runs (36-72 hours) to serious Pumo campaign playtesting. This is an engineering
estimate, not measured completion. The subsequent complete inventory removes
noncanonical geometry from Pumo's known requirements: all its side mappings are
canonical. Fog/environment properties, sound/effect objects, event actions and
resource application remain substantial work. Re-estimate from those interacting
requirements rather than extrapolating completed PR counts.

The [layout tranche](xl_layouts_25_27_20261028.md) also corrects an older unsigned
ReadByte source adapter for light indices. The production decoder intentionally
retains unsigned encoded counts; the reference's signed-count anomaly is now
measured and documented instead of hidden by the adapter.
