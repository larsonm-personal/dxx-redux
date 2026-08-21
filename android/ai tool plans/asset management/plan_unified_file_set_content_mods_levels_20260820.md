# Unified file-set content in Mods/Levels

## Goal

Move every active file-set file that is not represented by the Descent 1 or
Descent 2 base-file lists into the Mods/Levels section. Treat loose missions
such as `panic.hog` and their descriptors as selectable, inspectable, and
deletable content while keeping their ownership in the active file set.
Preserve file-set switching as the operation that swaps the complete content
environment, including base data, music, mods, and levels. Avoid orphaned
payload files and stale metadata under every mutation and recovery path.

## Work plan

- [x] Trace the current file-set mission inventory, Mods UI, import routing,
  metadata browser, launch mounting, manifests, and deletion paths.
- [x] Define a unified content-entry model that can represent global mod-store
  entries and active-file-set-owned entries without moving ownership.
- [x] Define enable/disable, ordering, launch, switching, deletion, import, and
  metadata behavior for every non-base-list file.
- [x] Specify orphan prevention, reconciliation, transaction boundaries, and
  focused automated coverage.
- [x] Record recommended implementation phases and open policy choices.

## Status

Phases 1 through 6 are implemented. The active set now owns, reconciles,
toggles, orders, deletes, projects, launches, displays, and exposes loose
content to metadata and automation through one stable entry model. Existing
DXA and mission-archive mods, CD sources, custom music registries, copied music,
and support caches are also set-scoped. Loose tracks and CUE-referenced BIN
bundles are safely adopted into Mods/Levels.

## Implementation progress

- [x] Add `FileSetContentCatalog` with exact base, launcher, player, and managed
  content classification.
- [x] Group same-stem and descriptor-referenced loose mission files into one
  stable logical owner, including connected shared dependencies.
- [x] Keep malformed descriptors and arbitrary unknown files visible instead
  of silently dropping them.
- [x] Route the existing included-mission inventory through the catalog so its
  delete operation removes every owned constituent.
- [x] Add focused JVM tests for Panic-style grouping, base exclusion, unknown
  content, shared ownership, stable IDs, malformed descriptors, and deletion.
- [x] Persist each owner under `.content`, reconcile loose and staged payloads,
  and add enabled/order state.
- [x] Publish the enabled loose-content projection and integrate it with the
  launch path writer.
- [x] Move rows and metadata details into Mods/Levels, then remove the old
  included-missions presentation.
- [x] Serialize all content operations across manager instances and cover
  concurrent reconciliation from UI, launch, metadata, and automation users.
- [x] Recover payloads with damaged owner manifests as visible, deletable
  `Other content` entries instead of leaving unreachable files.
- [x] Run scoped code quality and the focused 63-test catalog, manager,
  inventory, launch-path, and mod-details suite with zero failures.
- [x] Scope existing global mods and audio-source state to each file set, then
  adopt music payloads without invalidating registered CUE/BIN paths.
- [x] Scope custom music settings and copied payloads to each file set and
  expose custom music sets in Mods/Levels with toggle, order, details, and
  delete actions.
- [x] Add focused two-set isolation coverage for mission archives, generated
  launch paths, CD sources, custom music state, and copied music payloads.
- [x] Reconcile content from the setup screen lifecycle on startup, refresh,
  and set switch instead of depending on Mods/Levels UI composition.
- [x] Audit SAF ownership across base links, CD sources, and custom music in
  every set; revoke only permissions with no remaining cross-set owner.
- [x] Run an introspection-driven emulator flow covering loose Panic import,
  durable adoption, disable, switch isolation, restored state, content delete,
  set delete, and physical storage cleanup.
- [x] Check in `test_unified_file_set_content.jsonc` and run its 18-step
  launcher automation flow to a file-based PASS on the emulator.
- [x] Synchronize Compose with the persisted active set and prevent stale
  content or mod managers from recreating storage after their set is deleted.
- [x] Run a checked-in emulator flow proving a managed loose mission appears
  in the in-game mission picker when enabled and disappears when disabled.
- [x] Exercise shared persisted URI ownership through a real Android document
  provider, including retention after the first set is deleted and revocation
  after the final owner is deleted.
- [x] Project root-level loose missions under the engine's `missions/` search
  directory and consume their verified source files after durable adoption.

## Current behavior and gaps

- `ALL_GAME_FILENAMES` is the implemented base-name boundary. Every other loose
  user file becomes managed content unless it is launcher or player state.
- Mods/Levels now combines traditional mods, loose content, and custom music.
  CD CUE/BIN payloads appear through their managed music entry while their
  playback registry remains set-scoped.
- Existing pre-change global `filesDir/mods`, `audio_sources.json`, and custom
  music registries are intentionally not migrated because launcher data is
  disposable before release. A data reset is required for old development
  installs.
- Scoped JVM tests, a debug APK build including all three Android native ABIs,
  and the introspection-driven emulator ownership flow pass.
- The checked-in loose-mission picker flow passes on the emulator for both
  enabled and disabled states. A real DownloadsProvider URI remains persisted
  after its first owning set is cleared and is revoked after its final owner is
  deleted.

## Classification boundary

Classify every file under a set into exactly one of these classes:

1. Base file: its portable leaf-name identity is in `ALL_GAME_FILENAMES`. It
   remains in the D1 or D2 base-file UI and is never toggled as a content item.
2. Launcher state: manifests, staging markers, generated projections, caches,
   and transaction trash. It is not user content and is visible only in the
   advanced storage inspector.
3. Player state: pilots, saves, and game configuration. It remains managed by
   the existing player/configuration views.
4. Managed content: every other user-supplied file. It must belong to exactly
   one logical content entry shown in Mods/Levels.

This avoids accidentally showing `assets.json` as a mod while satisfying the
requirement that no user payload outside the D1/D2 lists remains invisible.
Unknown but present payloads become an `Other content` entry instead of being
ignored or deleted.

## Recommended set layout

Keep the active base directory compatible with the engine, but add a managed
subtree to every set:

```text
sets/<set>/
  <D1 and D2 base files>
  .content/
    <stable-entry-id>/
      entry.json
      payload/...
      cache/...
    .staging/...
    .trash/...
  content_state.json
```

`entry.json` is the ownership record colocated with its payload. It contains a
stable ID, display name, kind, game, category, import/source information, and a
normalized list of every owned relative path with size and optional hash. The
small set-level `content_state.json` contains only enabled state and order.
Scanning entry directories is authoritative for existence; state is repaired
from those directories rather than allowing an unlisted payload directory.

Use a set-scoped content manager as the source of truth. It can absorb most of
`ModManager`, `FileSetMissionInventory`, `MissionZipExtractionStore`, and the
mission-music sidecar ownership behavior. A single `ContentEntry` model should
cover at least:

- loose mission or level bundle;
- mission ZIP, 7z, or RAR bundle;
- DXA or other overlay mod;
- music pack;
- demo bundle;
- arbitrary other content.

UI and metadata code should consume the public model, not inspect storage
locations or branch on a second inventory.

## Grouping and ownership

- Parse every valid mission descriptor and claim its same-stem HOG plus files
  explicitly referenced by descriptor fields. Build connected components when
  two descriptors share a physical dependency, so a file never has two owners.
- Keep an imported archive as one owner and show its constituents as children,
  matching the existing mission-ZIP details UI.
- Keep a DXA as one owner. A collection imported as one documented pack may be
  one owner only when the import source provides an explicit package boundary.
- Make each remaining unclaimed file its own entry. Do not guess ownership from
  a broad import operation, since deleting that entry could remove unrelated
  content.
- The Panic example becomes one loose-mission entry owning `panic.msn` and
  `panic.hog`, with the mission title as its display name and both files listed
  in its details.

## Enable, order, and launch behavior

- Rename the section and automation labels from `Mods` to `Mods/Levels`.
- Show only entries in the active set. Switching sets reloads the list, enabled
  state, order, metadata jobs, selected music, and launch projection.
- Every content row has enable, details, order, and delete controls. Categories
  can visually group Levels, Mods, Music, Demos, and Other without creating
  separate ownership systems.
- Disabled payload stays under `.content` and is absent from engine search
  paths. It remains available to the metadata browser; background jobs carry
  its disabled priority just as imported mods do today.
- Build one generated projection directory for enabled loose files, preserving
  their virtual relative paths and resolved load-order winner. Prefer hard
  links within the set filesystem, with verified copy fallback. Mount this
  directory once. Archives that must be mounted, such as DXA overlays, continue
  to receive individual `.active_mod_paths` lines.
- Generate the projection in a sibling temporary directory, validate it, then
  atomically replace the previous projection. Never edit the live projection
  incrementally.
- Run the existing compatibility and patch-conflict checks over unified enabled
  entries before publication. Count the final generated and archive mount paths
  against `ACTIVE_MOD_PATH_LIMIT`, rather than counting logical entries.
- Keep D1-in-D2 filtering explicit in the unified launch selector.

The projection avoids mounting every loose mission separately, which would
quickly exhaust the current 64-path native limit for mission CDs.

## Metadata browser behavior

- Replace the current two-source discovery (`walkTopDown` HOG scan plus global
  `ModManager.listMods`) with one content-catalog enumeration for the active
  set, plus explicit base-game targets.
- Reuse the existing detail adapters: archive inspection for mission archives,
  DXA details for overlay mods, and `LevelMetadataTargets.directFile` for loose
  HOGs. Add a loose-bundle adapter that uses the owning descriptor and can show
  child details for every constituent.
- Key caches and jobs by stable entry ID plus payload fingerprint, not display
  name or mutable path. Deletion can then remove all route, extraction, music,
  and texture caches owned by that ID.
- Preserve metadata access for disabled entries. Only scheduling priority and
  launch visibility depend on enabled state.

## Import and reconciliation

- Route direct non-base files, mission archives, recommended downloads, disc
  extraction extras, demos, and custom music into the active set's content
  manager. Base-list filenames still go to the base root and `AssetManifest`.
- Stage and hash the complete prospective entry first. Publish the entry
  directory atomically, then update `content_state.json`. A published entry
  missing state is recoverable and is adopted on the next scan.
- After installer or disc extraction, classify the whole staged result before
  publishing anything. This prevents a descriptor being published without its
  HOG or a later failure leaving half an entry.
- On startup, set switch, and storage-inspector refresh, reconcile the active
  set:
  - remove stale state records whose entry directories are gone;
  - adopt valid entry directories missing state;
  - remove abandoned staging and trash directories;
  - move any non-base, non-launcher, non-player loose payload into managed
    entries using the grouping rules;
  - report malformed or unclaimable payload as visible `Other content` rather
    than silently ignoring it.

Because Android launcher data is explicitly disposable before release, this
should be a direct storage-format replacement. A one-time conversion can still
adopt development-device loose files and global mods so testing devices do not
retain invisible payload.

## Delete and orphan invariants

Deletion is by stable entry ID, never by a UI-supplied filename:

1. Resolve and canonicalize every owned payload and cache path under the entry
   directory.
2. Atomically rename the entry directory into `.trash/<id>`.
3. Remove its state record and rebuild the live launch projection.
4. Delete the trash tree and any generated game-directory artifact bearing the
   same owner ID.
5. Revoke SAF permissions only after checking references across all file sets,
   audio sources, and remaining entries.

If the process stops after step 2, reconciliation removes the stale state and
finishes trash cleanup. If it stops before step 2, the complete entry remains.
No state can point at a partial payload, and no successfully published payload
can be invisible because its own `entry.json` is authoritative.

Set deletion should delete the entire set root, including content and caches,
then revoke only unreferenced URIs. `clearAllMods` should become a filtered
catalog deletion and must not maintain a separate global directory. All cache
directories should either live under their owner or use an owner-indexed
manifest with reconciliation.

## Implementation phases

1. Add classification and `ContentEntry`/set-scoped manager tests. Keep the UI
   unchanged while proving Panic pairing, connected dependencies, unknown-file
   visibility, exact base exclusion, state repair, and path containment.
2. Add transactional import, deletion, reconciliation, and set deletion. Test
   failures at every publication/delete boundary and assert that every payload
   is either staged, owned, trashed, or a base/player/launcher file.
3. Add the generated loose-content projection and unified launch-path writer.
   Extend native mount tests and integration introspection for enabled,
   disabled, order, D1/D2 filtering, conflicts, and mount-capacity behavior.
4. Move the UI and details browser to the unified catalog, remove the included
   missions UI from `SetManagementDialog` and the main summary, and rename the
   section to Mods/Levels. Update Setup automation JSON to expose content IDs,
   ownership, enable state, and constituents instead of `included_missions`.
5. Move background route/music discovery to catalog entries and add deletion
   cache cleanup tests. Exercise Panic metadata while enabled and disabled.
6. Scope existing global mods, audio sources, custom music, demos, and their
   settings to the active file set. Add a two-set emulator integration test
   proving that switching swaps base files, mods/levels, music, enable state,
   metadata jobs, and launch mounts with no cross-set leakage.
7. Run scoped code quality, JVM tests, native PhysFS tests, Android Gradle
   build/tests, and the high-level emulator import/toggle/inspect/delete/switch
   flow. Finish with a storage audit that finds zero unclassified user files,
   stale owner records, unowned caches, or leaked SAF references.

## Policy recommendations

- Default adopted loose content to enabled so existing sets retain their launch
  behavior. Default newly imported recognized content to enabled. Keep unknown
  content disabled until it is classified as engine-safe.
- Keep base files non-toggleable. If a user wants another base version, that is
  a file-set operation rather than a mod toggle.
- Show one row per logical owner, not one row per constituent file. The details
  view guarantees every physical file is visible and delete scope is clear.
- Make file-set identity explicit but quiet: the section can say `From set:
  <name>` in its summary, while the file-set dialog focuses only on set CRUD,
  size, and switching.
