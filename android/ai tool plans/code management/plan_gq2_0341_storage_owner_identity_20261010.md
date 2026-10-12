# GQ2 chunk0341: Storage Inspector owner identity, 2026-10-10

Diagnosis and implementation plan only. No product/test/helper changes or execution. This extends existing BR-0472 registry/byte ownership and BR-0103 extraction-generation acceptance; no new finding, remediation ID, rating or status is allocated.

## Confirmed source boundary

AdvancedSettingsPage.kt405-464 scans the internal root and any external active import root, but resolves linked extracted files only through MissionZipExtractionStore(filesDir), the global store. It records a bare linkedMissionZipOwner filename and Boolean source presence. ModManager.kt282-313 instead puts current set archives in <set>/.content/mods and extraction support in <set>/.content/mod_support. MissionZipExtractionStore.kt65-74 resolves those as distinct manifests and owner directories.

Consequently current set-owned source/extracted files included by the scan have no link from their actual manifest and can reach the generic entry.file.delete() branch. A displayed global linked row with a source present takes a different inconsistent path: AdvancedSettingsPage.kt2535 constructs ModManager.forActiveSet(filesDir) at confirmation time and passes only the global owner's filename. If that active set has a different mod with the same filename, its source, extraction support and registry row are selected for removal while the displayed global owner remains. This is a source-derived conditional witness, not an executed deletion or a claim that disposable old layouts must be migrated.

The GQ2 base already scans the global store at353-360, and uses the global ModManager for removal at2004-2011. Assigned frozen hunk35 changes only the removal manager to forActiveSet at2529; the frozen scanner409 stays global. HunkSHA393731c58a419b7a18e36a1acf561a69d01984b2046a7a2e7303f6827bd1b991. Current equivalents are405-464 and2491-2556. No original1996 line reduction is involved.

## Existing ownership and preserved repairs

GQ1-CHUNK-0488, imported evidence38171-38196, already records all-root inspection versus active-set removal and requires captured owner/set generation across asynchronous removal. This is more precise current producer/store/hunk evidence for the same BR-0472/BR-0103 boundary, not a new root. BR-0472 also owns hard-coded deletion success; BR-0103 owns removeOwner reporting existence as success despite failed recursive deletion. Preserve ModManager's content lock, reload inside atomic transaction, atomic registry save and MissionZipExtractionStore's shared per-store lock. Do not repeat historical assertions that those locks or atomic file writes are absent.

## Smallest complete implementation

- Carry an explicit owner identity from inventory to confirmation and execution: canonical set/storage root, source archive identity, extraction store and relevant registry generation. Derive it from the actual owning managers/catalogs for every scanned set; replacing the global scanner with only the active-set store would still miss inactive sets under the inspected root.
- Display the owning set and source when confirming grouped deletion. Use the captured owner manager/store, revalidate source and registry identity under the owning mutation boundary, and reject or refresh stale rows. Never resolve a captured row through whichever set happens to be active later or by basename alone.
- Resolve managed source and extracted entries through their owner before allowing generic file deletion. Standalone unowned files may keep the direct checked deletion path. Keep explicit global residue cleanup separate from current set ownership; do not add legacy schema migration or compatibility readers.
- Return truthful complete, partial, failed or stale outcomes with confirmed removals and retained retry identity. Coordinate registry/source/derived-tree removal under BR-0472 and BR-0103; do not remove ownership metadata on an unacknowledged cleanup and then report success.
- Include the same set-qualified ownership in diagnostic export. Keep scans, IO and mutations off the Compose thread and bound discovery before allocating inventories under GQR-0212. A rendering page limit is not scan admission.

## Allowed implementation surface and acceptance

Expected production surface: AdvancedSettingsPage.kt, ModManager.kt, MissionZipExtractionStore.kt and StorageInspectorDiagnostics.kt. Reuse FileSetManager/FileSetContentManager ownership APIs; extend their smallest existing API only if a reviewed gap prevents exact owner enumeration. Any new shared launcher helper belongs alongside these Android files. No d1/d2 format logic, engine reset semantics or compatibility migration belongs in this fix.

Use existing ModManagerMissionZipTest.kt, MissionZipExtractionStoreTest.kt and StorageInspectorDiagnosticsTest.kt for unique manager/export assertions; add a focused production-backed Storage Inspector owner-selection fixture if no existing entry point permits it. Register any new top-level integration runner with the existing catalog and master timeout policy. Do not substitute a duplicated test implementation for actual production owner resolution.

Future acceptance must cover active and inactive sets with identical archive filenames but distinct sentinel bytes, internal and external import roots, owner and extracted-file rows, source already missing, standalone files, global residue, active-set change between scan and confirmation, replaced source or registry generation, and delete/manifest failures. Require that only the displayed captured owner can change, unrelated sets and saves remain byte-identical, linked metadata matches the actual store, failed removals retain retry identity, and the UI/export reports exact outcomes. Exercise the actual visible confirmation and export after scoped Kotlin quality/JVM checks and the relevant Android build. Failure/process/provider probes remain deferred until authorized implementation/validation work.

## Relationship to other Advanced settings work

Route-cache clear's counted-before-delete success remains a separate BR-0259 cleanup acceptance extension, not part of this atomic fix. Demo grouped install/export stays BR-0222/BR-0443; custom URI lifecycle stays BR-0419; import-root publication/preservation stays BR-0441/BR-0461. Keep their patches and acceptance independently reviewable.
