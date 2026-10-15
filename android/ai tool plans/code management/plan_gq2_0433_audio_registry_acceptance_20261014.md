# Chunk0433 audio registry and resource publication continuation, 2026-10-14

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. No product/test/helper edits or runtime acceptance.

## Preserve current repairs

AudioSourceManager now scopes registry storage to the selected file set, reloads changed bytes under AtomicFilePublication.transaction before mutation and atomically publishes registry JSON. Preserve this process-local repair and current multi-BIN URI arrays; do not repeat BR-0472's historical direct-write/unlocked-whole-file description as current AudioSourceManager behavior. Disabled managed content remains registered for Settings; pruning includes disabled content in existence resolution. Native playback, Settings inventory and user order are distinct domains.

Preserve the eight-source/100-track shared capacity declarations, checked aggregate subtraction, positive audio/total count admission and complete producer preflight before replacing playlist JSON. Complete current native parser/admission and source-selection acknowledgement still need explicit qualification in the next stage; archived BR-0326 is not reopened from source inspection alone.

Current generated-artifact classification uses canonical containment, rejects canonicalization failure and rechecks before deletion. Ordinary external custom-* names or generated markers alone do not authorize deletion. GOG files have explicit app/import-root handling. Do not undo archived BR-0453 or introduce a new migration. Removal now has a named confirmation in Setup and MusicPicker, and both selected callers provide context.

Single-source removal snapshots removed and retained tracked URIs, saves the reduced registry, then calls retained-aware permission cleanup. The cleanup retains direct or owning-tree grants still covered by retained tracked URIs. Preserve archived BR-0459 and its focused callback/registry fixture. That fixture does not execute a real provider, cross-process mutation, interruption or overlapping revocation.

Current preview helpers keep physical CUE identity distinct from audio ordinal and display track number. Missing names do not compact physical identity. Diagnostics probe content BINs and the local copied CUE, rather than requiring the original CUE import URI. Native preview execution and provider cancellation remain future qualification.

## Existing ownership and launch findings

BR-0472 still owns complete registry-to-artifact/grant mutation and corruption recovery. Removal deletes managed files before saving the reduced registry; clear-all ignores deletion results and deletes registry/playlist separately. A failed registry save can therefore leave prior registrations with missing managed artifacts. Parse failure still substitutes an empty list, and loaded bytes and parse text come from separate reads. The transaction lock is process-local, not a cross-process generation protocol. Preserve current atomic writer/reload while completing explicit recovery and all-resource outcomes.

Playlist publication is atomic for one JSON file, but preparation first closes the static activePfds list. Opening each source retains successful descriptors, catches per-source open failures, closes that source's partial opens and falls back to resolved binPaths. Whole-attempt failure and previous descriptor generation ownership are not established by retaining old JSON bytes. The empty-playlist unit fixture retains a literal last-good byte string only; it owns no live PFDs and exercises no native reader.

BR-0432/0506 own coherent accepted music/launch state. Current Setup readiness uses any enabled CD row, which repairs the old all-disabled-ready account but does not establish readable/capacity-valid media. Setup launch now rejects selected CD mode when writePlaylist returns false and catches ordinary preparation exceptions. Preserve those repairs; do not report the old blanket uncaught-exception or empty-CD-launch account as current Setup behavior.

MainActivity rebuilds the playlist in :game because Setup's descriptor numbers are process-local. Its Deferred<Unit> does not preserve writePlaylist's Boolean. The selected await catch logs ordinary preparation failure and then starts the engine; cancellation is rethrown. This still lacks one acknowledged launch generation spanning descriptors, playlist, config and native admission. The selected onDestroy path cancels startup then closes activePfds; it is not proof that blocking provider work has drained or cannot later publish. Existing ownership and launch findings cover this boundary; no new root or status change.

## Conditional simplification and future acceptance

During the existing repairs, keep one prepared typed playlist generation that owns its CUE staging, descriptors, exact source identities and native admission result. Use commit/retire semantics: build and validate a complete candidate, publish only on success, and retire the old generation after its native reader no longer owns it. On failure close only attempt-owned handles and retain prior usable state. Do not solve this with another independent registry or duplicated native format parser.

Unify readiness with the exact generation accepted for launch/source selection. Distinguish unavailable, disabled, over-capacity, provider failure and empty outcomes. Preserve current capacity and atomic-file controls. Complete registry deletion and grant release with explicit retained identity and truthful outcomes, including shared files/grants and storage failure.

Future acceptance must cover all source/track boundaries, one playable track, multi-BIN provider failures at every open, partial CUE staging, failed final replacement, old live PFD readers, different processes and file sets, queue/lifecycle cancellation, overlapping remove/add and shared-tree grants, corruption recovery and process interruption. Require one complete prior or new registry/artifact/grant/playlist/native generation and accurate UI/launch result. Existing source/unit evidence is not fresh runtime qualification.

Remaining0433: actual native playlist/progress/preview and registration boundaries, full owner/prior-plan synthesis, direct disposition and publication/replay/Drive. No new finding, status/rating mutation, inherited saving or runtime success.
