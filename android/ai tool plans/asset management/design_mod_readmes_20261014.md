# Shared README viewing for installed and recommended mods

Proposed design, 2026-10-14

Add View readme for a single document and View docs for multiple documents to mod information. View docs opens a document selection page where users can open each document in turn. Share discovery, selection, and viewing with levels and recommended mod rows. Publish a small documentation archive beside each recommended mod so users can read its documentation before downloading the mod.

## Existing behavior

- `MissionZip.kt` selects and reads documentation, including multiple documents. It prefers inline documents, then external documents; within each group it prefers an exact README basename, then the archive stem prefix, then the largest document. Existing path ordering resolves ties.
- Inline viewing currently supports TXT and DOCX. PDF, RTF, and DOC use Android viewers through the existing FileProvider grant handling. `GameFileFormats.kt` already classifies MD as documentation, but the README selector and viewer do not support it.
- `ModManager.getModDetails` already enumerates DXA entries but exposes README actions only through `missionZip`. Mission scanning requires playable mission content, so it cannot serve as a generic DXA documentation scanner.
- `RecommendedModsDialog.kt` has a filename and a derived GitHub release download URL for each recommendation. Its installed state currently matches filenames, which does not prove that the installed bytes match the current release.
- `prepare_texture_release.py` validates and publishes all 13 release packs, metadata, and hashes. The recommendation list currently contains nine of those packs. The publisher replaces changed assets under the stable `texture-packs-v1` tag.
- The local release inputs contain useful secondary documents: high-resolution packs include `README.md` and `README.dxx-redux.md`; UUD packs include a short `README.md` and original RTF documentation; the Mac sound pack also includes `Sounds/Move me.txt`.

## User experience

| Location                  | Action                                              | Content                                                    |
| ------------------------- | --------------------------------------------------- | ---------------------------------------------------------- |
| Installed mod information | View readme for one document; View docs for several | Documents from that installed archive, available offline   |
| Level information         | View readme for one document; View docs for several | Existing level documents through the shared implementation |
| Recommended mod row       | View docs, independent of Download or Installed     | Published documentation fetched on demand                  |

With one document, open it directly. With several, View docs opens a dedicated document selection page titled "Documents" with the mod or level name. List the preferred document first and all other supported documents below it. Each row shows the original filename and document type and opens that document; show full relative paths when basenames collide. Include both the author's documents and conversion notes. Installed archives without documents have no action.

Closing an inline document returns to the selection page with its scroll position and focus preserved, so the user can immediately open another document. Returning from an external viewer also leaves the selection page available. Back from the selection page returns to the originating information popup or recommended mods page. Use the same selection component for all three entry points.

Recommended rows use the stable label View docs because the document count is unknown before fetching. After loading, open a single document directly or show the selection page for multiple documents. Keep loading and retry states inside this documentation flow. Returning from a document to the selection page reuses the loaded inventory and does not trigger another download.

Recommended rows always open the published documentation, even when marked Installed. Label the viewer with the mod name and "Published documentation". The installed mod popup remains the source for documentation matching the installed copy. Do not substitute installed documentation based only on its filename.

Read Markdown as selectable plain text initially, using the existing text dialog. Preserve DOCX text extraction and external viewing for PDF, RTF, and DOC. Markdown rendering and automatic link handling can be added independently later.

## Shared implementation

Extract the existing document policy and decoding into `PackageDocuments.kt`, with a small document record containing relative path and uncompressed size. Keep game metadata, mission constituents, and extraction ownership in their existing owners.

The shared helper should provide classification, ordered document selection, bounded reading/decoding, and MIME selection. It should accept an entry inventory and the original package stem. Mission constituents and DXA ZIP entries adapt to this inventory; neither caller needs to scan or decompress gameplay assets to find documents.

Move the document dialog and external-open wiring from `SetupSections.kt` into `PackageDocumentDialog.kt`. Pass a document list and a source that can read the selected entry or materialize it for external viewing. Support archive entries and already extracted files; downloaded documentation archives use the archive source. Reuse `ArchiveInputStreams`, bounded reads, `ExtractionBudget`, existing text decoding, DOCX limits, and `FileProviderGrantStore` where applicable. Keep the existing mission extraction store responsible for resolving extracted mission entries.

Expose a common `documents` list on `ModDetails`, populated for both DXA and mission archives. Use it for the information popup and chooser. Keep mission-specific constituent details and their View actions connected to the same document viewer. Compute document inventories with details loading; avoid a new persistent index or changes to the native mod format.

### Discovery and preference

Use one policy for levels, installed mods, and downloaded documentation bundles:

1. Recognize TXT, MD, DOCX, PDF, RTF, and DOC case-insensitively at any archive directory depth. Also recognize an extensionless file whose leaf name is exactly README, treating it as text. Do not inspect nested archives recursively.
2. Retain every candidate. A file such as `README.dxx-redux.md` qualifies by extension even though its stem is not exactly README.
3. Preserve current preference rules: inline candidates before external candidates; exact README stem before package-stem prefix before largest document. Add MD and extensionless README to the inline group. Preserve existing deterministic path tie-breaking for already supported files.
4. Put the preferred candidate first, followed by the remaining candidates in the existing stable group/path order. Retain paths rather than reducing identity to a basename.

Update `GameFileFormats` classification for extensionless README as needed, using the same policy without creating a circular dependency. There should be one authority for document kind; game import eligibility remains separate. Add expectations for any intentional ordering change caused by newly recognized MD files.

## Published documentation contract

### Preserve originals and document adaptations

The [recommended mod documentation audit](audit_recommended_mod_docs_20261014.md) distinguishes seven adapted recommendations from two byte-identical upstream Rebirth packs. Preserve original document bytes, encodings, filenames, and relative paths. Never rewrite original installation instructions or replace originals with decoded previews.

For each adapted pack, include a separate, pack-specific `README.dxx-redux.md` explaining provenance, actual Redux changes, omissions, requirements, and relevant mod ordering. Label it "Redux adaptation notes" in the selection page while keeping every original document accessible. Consolidate only known Redux-generated prose; do not assume a README is ours from its name alone. Where a source contains no documents, state that in provenance rather than presenting generated notes as an original README.

Leave untouched upstream mod archives byte-for-byte unchanged, with no added Redux note. Their companion ZIPs copy original documents only. Keep launcher installation guidance outside those archives. A future unchanged pack with no original documents may have no companion; record that availability in the catalog rather than requiring an injected README.

### Companion archive naming

For every published mod asset, publish a ZIP containing only its supported documentation entries, with their original paths and bytes:

```text
uud2tp-textures.dxa
uud2tp-textures.dxa.readme.zip

d1-hires-256-textures-ktx2.dxa
d1-hires-256-textures-ktx2.dxa.readme.zip
```

Derive the documentation URL by appending `.readme.zip` to the final asset path segment of the canonical mod download URL. Keep the original repository and release tag. For today's catalog:

```text
https://github.com/larsonm-personal/dxx-redux/releases/download/texture-packs-v1/uud2tp-textures.dxa.readme.zip
```

These companion assets were published on 2026-10-14; see the [publication record](plan_publish_mod_docs_20261014.md). Use a URI builder so query strings, fragments, and percent encoding cannot become part of the asset name. Apply the convention only to catalog entries published under this contract; do not guess companion URLs for arbitrary imported mods or redirected CDN URLs. No separate URL list or GitHub API lookup is needed at runtime.

GitHub exposes direct release asset download URLs and supports downloading public assets without authentication. Use the public download URL with redirect handling. See [GitHub release asset documentation](https://docs.github.com/en/rest/releases/assets).

A documentation ZIP is preferable to one normalized TXT file here because it preserves original RTF files, multiple readmes, encodings, and filenames. It also lets the app reuse exactly the same selection policy without duplicating that policy in Python. A standalone published text file would omit useful documentation in the current packs. On GitHub itself, this link downloads the documentation ZIP; the launcher provides the reading experience.

Pass the original mod filename/stem into selection when reading this ZIP. The `.dxa.readme.zip` suffix must not alter package-name matching. The documentation archive is a cache input, never an imported or enabled mod.

## Release generation

Extend `game_data/mods/d2x-xl/prepare_texture_release.py`:

1. After validating each final DXA, collect all supported documentation entries and copy their bytes into its companion archive. Require a separate Redux adaptation note for modified packs and original-document hash fidelity wherever originals exist. All current recommendations have documents; future unchanged packs without documents may omit the companion and expose no catalog documentation action. Preserve originals; do not independently rewrite a second set of readmes.
2. Write deterministic ZIPs using sorted paths, fixed timestamps, fixed compression settings, and no generated timestamp in their contents. Reject ambiguous case-insensitive paths and unsafe names using existing archive validation. Retain unchanged output bytes and modification times.
3. Add a `documentation` object to each pack in `mod-packs.json` containing companion filename, byte size, SHA-256, and source pack SHA-256. Add companion hashes to `SHA256SUMS.txt` and include companions in upload and remote hash verification. Keep the runtime independent of this manifest for initial discovery.
4. Add a README link to each row in generated `RELEASE.md`. Generate companions for every release pack, including the texture resolutions absent from the recommendation list.
5. Complete all local validation before publication. Publish pack and companion assets, then metadata and release notes. Keep the existing unchanged-upload skipping and retry behavior. Replacement is not atomic; during an interrupted update a companion can briefly lag its pack. Metadata records the intended association but does not make mutable asset URLs immutable.

Python needs only document classification, not the app's ranking or decoding. Keep the supported filename rules aligned through shared test fixtures covering both languages. Document the duplicated classification contract beside both definitions, as required by repository guidance.

Publish and verify companions before distributing an app that exposes the recommended documentation buttons. Viewing alone needs no DXA changes, but the audited adaptation-note repairs require updating documentation in converted packs and refreshing their published sizes/hashes. Preserve gameplay payload entry bytes during those repairs. The two untouched Rebirth DXAs must retain their exact upstream archive hashes.

## Loading and cache behavior

Create a small `RecommendedModDocuments` loader outside the composable. Fetch only when View docs is selected, on the IO dispatcher, with cancellation and finite timeouts. Keep readme loading and errors independent of mod download progress.

Use the complete canonical documentation URL as the cache key, hashed into an app-cache filename. Download to a temporary file, validate the ZIP and document inventory, then atomically replace the cached copy. Bound downloads to 16 MiB, documentation entry count to 128, and total expanded content to 32 MiB; keep existing stricter per-document reading and DOCX limits. Enforce actual streamed bytes as well as declared sizes. The publisher must enforce the same bundle limits. Current inspected documents fit comfortably within them.

Revalidate on each new opening because assets at the current release tag are replaceable. Use conditional requests when response validators are available, otherwise redownload the small bundle. On network failure, allow a valid cached copy with a "Saved copy; could not check for updates" notice. Do not cache failures as successful content. No cache means an inline error and Retry; a 404 means "README is not available for this release". None of these failures block the mod's Download action.

External documents use the existing FileProvider grant lifetime handling. A document bundle must not write arbitrary archive paths into app storage. Reject traversal, duplicate normalized paths, malformed ZIPs, and unexpected non-document payloads. Handle missing external viewer apps with the existing user-visible error path. Text and Markdown are displayed as text without executing markup.

## Implementation and acceptance

1. Extract the shared policy and viewer; retain existing mission behavior and add MD/extensionless README support.
2. Populate DXA document details and connect the mod popup. Verify a real high-resolution pack, a UUD pack with RTF, and a mod with no documentation.
3. Add deterministic documentation bundle generation and local release checks. Verify each bundle entry against the original DXA and confirm repeated generation produces no changes.
4. Add the recommended README action, URL derivation, loader, cache, and error states. Keep the chooser and viewer shared with installed content.
5. Publish and verify all companion assets as a separate rollout step, then run real recommended-page acceptance against the published files.

Use focused JVM coverage for ordering, classification, original-stem handling, URL derivation, bounded reads, and cache refresh/failure behavior. Retain existing mission README and DOCX regressions. Release tests should compare complete document inventories and bytes, deterministic output, and missing/invalid document failures using small fixture packs.

Add one maintained launcher integration runner covering installed DXA and level documents, multiple-document selection, recommended viewing before installation, independent download controls, offline cache, and failed remote loading. Verify View readme versus View docs labels, opening two different documents in succession, preserved selection-page position on return, and Back to the originating popup. Include external-viewer behavior where a viewer is available. Register the runner in the automation catalogs, run both catalog validators, run relevant Gradle tests and APK build, and apply scoped code quality checks. No native source changes are expected.

Release preparation and publication are implemented: adapted packs and all thirteen documentation companions are published. The packaging step reads maintained per-pack notes and stages final archives without changing conversion inputs or rerunning lossy conversion. Launcher discovery, document selection, viewing and caching remain implementation work.
