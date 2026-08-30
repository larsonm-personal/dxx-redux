# Third-party co-op mission discovery and synchronization

## Goal

Let an Android host select the same installed third-party missions that the engine can select, detect missing or different mission archives before launch, and optionally offer the host's exact managed archive to joining players.

The requested synchronization identity is the imported wrapper archive, such as `castaway_redux.zip`. Two archives with different bytes are therefore different even if they happen to extract to equivalent mission data.

## Scope and non-goals

- This document began as a code study and now records the implemented launcher, lobby, identity, and LAN transfer work.
- Cover D1 hosting, D2 hosting, and D1 missions launched through D2.
- Keep the D1 and D2 engine changes small, Android-specific where possible, and paired.
- Preserve Windows, Linux, and macOS behavior.
- Validate the selected mission wrapper. A later full mod-stack manifest would be needed to prove that unrelated enabled DXA or replacement mods also match.
- Do not silently download, install, enable, replace, or reorder content. The host offer defaults on, but the joining player must explicitly accept a download.
- Never offer the base licensed D1 or D2 missions, their HOGs, or any other base-game file for transfer. The transfer allowlist is limited to retained app-managed mission wrappers.
- Show transfer progress on both endpoints, support bounded concurrent downloads from one host, and preserve verified partial downloads for resume and retry.

## Executive findings

1. The missing mission is caused by two different mission catalogs.
   - `MissionScanner.scan` in `multiplayer/MissionPicker.kt` sees built-ins and physical `.msn` or `.mn2` files in only the active set root and its `missions` directory.
   - Imported mission archives live under the active set's managed mod storage and are projected into the engine search path by `ModManager` during launch preparation.
   - The engine enumerates the merged PhysFS `missions/` namespace after those paths are mounted. That is why PTMC Castaway Redux can be selected in the engine but is absent from Host Co-op.

2. Reusing only the Kotlin descriptor scanner would remain fragile.
   - D1 and D2 `build_mission_list` implement built-in detection, extension rules, precedence, display naming, and the multiplayer versus single-player filter.
   - The current Kotlin code already duplicates several HOG size constants and names from C.
   - The long-term source of truth should be a small read-only native catalog API around the existing C mission enumeration, fed the same ordered launch paths as the real game.

3. Missing mission protection partly exists, but exact archive matching does not.
   - Both engines call `load_mission_by_name` when joining and show the existing mission-not-installed error if it fails.
   - Level synchronization later compares a 16-bit segment checksum. That is useful defense-in-depth, but it is late and does not cover every file or every future level in an archive.
   - Neither LAN nor online lobby messages currently carry mission size, SHA-256, or a per-player mission status.

4. Hash storage is partly present and can be generalized.
   - Extracted mission bundles already record wrapper size, modification time, and SHA-256 in `MissionZipExtractionStore`.
   - Small stored ZIP imports do not get that record.
   - `freshRecord` currently rehashes the wrapper and every extracted file, so it is not suitable for quick lobby rendering.
   - Managed loose file-set content already stores SHA-256 when it is published by `FileSetContentManager`.

5. Lobby enforcement currently belongs only to the UI.
   - LAN Host Start checks player count but not all players' readiness.
   - Online UI checks readiness, but the Rust server accepts a host `START_GAME` without enforcing readiness.
   - To guarantee the requested behavior, compatibility must be server or host-service state and Start must be rejected by the authority, not just disabled in Compose.

6. A first download implementation is practical on LAN. Online transfer needs a distinct data-plane decision.
   - LAN can use an app-private, token-scoped TCP/HTTP stream after lobby admission.
   - The current matchmaking WebSocket and gameplay UDP relay were not designed for large untrusted files. Relaying archives through them without quotas and backpressure would create a denial-of-service and operating-cost risk.

7. The existing host networking must not perform transfer work on its lobby receive or UI paths.
   - LAN chat, announcements, membership, readiness, and Start currently use a UDP lobby service with coroutine-based I/O.
   - Mission bytes need a separate bounded transfer service and independent I/O children so a slow client cannot delay chat or updates for anyone else.
   - Multiple clients can download the same immutable wrapper concurrently, but concurrency, memory, disk reads, progress updates, and aggregate bandwidth must all be bounded.

## Current architecture study

### Import, storage, and engine visibility

- `ModManager` retains a managed mission wrapper under the active file set's `.content/mods` storage and records it as `kind = mission_zip`.
- `MissionZip.inspect` discovers one or more mission sets in a wrapper and selects effective variants. A single wrapper can own several selectable missions.
- Small ZIPs can remain mounted archives. Large, nested, or non-ZIP inputs can use a durable extracted bundle, while the original managed wrapper remains available as the requested transfer and identity object.
- `SetupActivity.prepareGameLaunchFiles` reconciles content, evaluates D1-in-D2 readiness, builds content projections, and writes enabled mod paths.
- Native Android PhysFS setup mounts the active set, managed projections, enabled mission archives, and extracted mission paths. Mount ordering determines which duplicate mission basename wins.
- Therefore, mission availability is a property of the prepared ordered mount plan, not merely of files visible under `<set>/missions`.

### Host mission picker

- Both LAN and online host dialogs call `MissionScanner.scan(activeSetDir, game)`.
- The scanner does not inspect enabled mission wrappers, extracted stores, content projections, SAF-backed content, or generated active mod paths.
- It de-duplicates only by lower-cased descriptor basename and does not model which source wins according to PhysFS mount order.
- The selected value passed into the engine is the legacy mission key or basename, not the wrapper filename.
- The engine mission name field has legacy length constraints. Preserve the exact engine key returned by C and never derive it from the managed wrapper filename.

### D1, D2, and game-mode semantics

- D1 and D2 single-player New Game call `select_mission(0, ...)`, which excludes missions declared as anarchy-only.
- Multiplayer mission setup calls `select_mission(1, ...)`, which includes them.
- For Host Co-op, use the single-player-compatible filter so anarchy-only missions are not offered for co-op. For Anarchy and team modes, use the multiplayer filter.
- D2 can expose compatible D1 missions only when the active set passes the existing D1-in-D2 readiness check. The catalog must use the same boolean and mount plan as launch preparation.
- Built-in mission availability and names must come from C, including demo, OEM, and full-data variants.

### LAN lobby

- `LobbyProtocol.kt` uses compact JSON messages over UDP port 42400: `ANNOUNCE`, `JOIN`, `JOIN_ACK`, `READY`, `PLAYER_LIST`, and `START`.
- Announcements and acknowledgements carry the mission key and game settings, but no content identity.
- `LanPlayer` has no content compatibility state.
- A client can also launch through direct-IP fallback or mid-game discovery, bypassing the waiting-lobby UI.
- The receive buffer is currently 2048 bytes, so lobby identity fields must remain compact. Do not embed archives, manifests, long URLs, or arbitrary error text in discovery packets.

### Online lobby and server

- `CREATE_LOBBY.game_info` and `GAME_STARTING.game_info` are extensible JSON. The server caps serialized `game_info` at 5 KiB.
- `LobbyPlayerInfo` contains readiness and connection details only.
- `READY` carries only a Boolean.
- The server protocol version is currently 1 on both client and server.
- The server resets readiness when game info changes, which is a useful precedent for also resetting mission compatibility.
- The server must validate all new enums, hashes, lengths, sizes, and state transitions because clients are untrusted.

### Existing in-engine checks

- On join, both D1 and D2 try to load the host's mission key and display the existing mission-not-installed error if it is unavailable.
- After level loading, the network path compares the existing segment checksum and reports a level mismatch.
- These checks should remain. They cover engine-only and race cases, but they do not replace wrapper identity matching in the launcher lobby.

## Required behavior and invariants

### Mission catalog

- Host Co-op shows every mission the prepared engine view would show for co-op, and no shadowed or incompatible entry that the engine cannot load.
- Host Anarchy and team modes use the engine's multiplayer filter.
- Changing game, mode, active file set, mod enablement, mod order, or D1-in-D2 readiness invalidates the catalog and current selection.
- A selected entry resolves to exactly one effective source after mount precedence is applied.
- The catalog reports enough metadata for level range validation without reparsing the wrapper on every Compose recomposition.

### Identity and compatibility

- SHA-256 of the exact managed wrapper bytes is authoritative.
- Byte length is a fast diagnostic and pre-transfer check, not an identity by itself.
- Display name and filename are never identity fields.
- For a wrapper-backed mission, `MATCH` requires the selected effective local mission entry to be owned by the expected wrapper SHA-256 and size.
- If the same mission key is present from another wrapper, report size or hash mismatch instead of incorrectly reporting match.
- Hashing and archive inspection run off the UI thread.
- A lobby never becomes startable while a required player's status is checking, missing, mismatched, failed, downloading, or verifying.

### Download

- Host offer preference defaults to on.
- Offer availability is true only when the selected mission has a retained, app-managed wrapper that can be read safely.
- D1 and D2 built-ins, including demo/OEM variants selected from base HOGs, are always non-transferable. The service must never expose `descent.hog`, `descent2.hog`, `d2demo.hog`, or another active-set base file.
- This licensed-content exclusion is based on catalog provenance (`builtin_base` or another non-wrapper source), not display title, mission key, or filename. A crafted filename cannot make base data transferable or make an unmanaged file pass the allowlist.
- Under the requested narrow definition, only D1 and D2 base missions are classified as licensed base content. Other missions are still transferable only when they arrived as retained app-managed wrappers. The launcher does not try to infer publisher or license from a mission name.
- Built-ins and unmanaged loose missions can participate in compatibility checks but are never packaged or offered by this feature.
- The joining player sees source host, archive display filename, byte length, and hash prefix before accepting.
- Downloaded bytes are never mounted directly. They are staged, bounded, hashed, inspected with existing archive safety rules, atomically imported, then explicitly enabled and made effective.
- Status becomes `MATCH` only after the rebuilt authoritative catalog maps the selected mission key to the downloaded wrapper identity.
- Both host and joining client show determinate progress from verified bytes and total wrapper bytes. The host shows a separate status and progress bar beside every downloading player.
- A transient disconnect preserves complete verified chunks. Rejoining the same requirement can resume from that verified boundary with a newly authorized transfer session.

## Proposed data model

### `MissionCatalogEntry`

Keep this launcher-facing and immutable:

```text
game                    d1 or d2 descriptor compatibility
engineMissionKey        exact key accepted by load_mission_by_name
descriptorPath          normalized virtual path used to disambiguate archive members
displayName             engine-derived display label
normalLevelCount        positive and negative level admission metadata
anarchyOnly             mode filter
sourceKind              builtin, managed_wrapper, managed_loose, or unmanaged_loose
sourceId                stable local managed-content ID, never sent as identity
ownerIdentity           optional MissionArchiveIdentity
transferable            retained managed wrapper is available
transferPolicy          deny_base, deny_unmanaged, or allow_managed_wrapper
effective               this entry wins current mount precedence
problem                 nonfatal catalog explanation when not selectable
```

For multi-mission wrappers, several catalog entries share one `ownerIdentity` and differ by descriptor path and engine mission key.

### `MissionArchiveIdentity`

Use one compact wire and persistence contract:

```text
schema                   integer, initially 1
algorithm                sha256
sizeBytes                signed 64-bit locally, validated nonnegative on wire
sha256                   exactly 64 lower-case hexadecimal characters
managedFilename          UI hint only, bounded and sanitized on wire
archiveFormat            zip, rar, 7z, or other supported retained form
missionKey               selected engine mission key
descriptorPath           bounded normalized path for a multi-mission wrapper
```

The lobby requirement also carries `kind` (`builtin`, `wrapper`, or `loose`) and `offerAvailable`. Only `wrapper` has the raw-wrapper hash contract requested here.

`offerAvailable` is derived server-side on the host from `transferPolicy == allow_managed_wrapper` and a successful containment check under managed mission storage. It is never accepted as sufficient authorization by the transfer service.

### `TransferSnapshot`

Use one UI-facing progress model for LAN and online lobbies:

```text
transferId               random identifier for one authorized attempt series
playerId                 stable lobby identity, not callsign alone
contentId                expected whole-file SHA-256 and size
state                    queued, downloading, paused, retrying, verifying, failed, or complete
verifiedBytes            highest contiguous client-verified byte boundary
totalBytes               immutable expected wrapper size
attempt                  current connection attempt number
retryAtMs                optional monotonic retry deadline
bytesPerSecond           optional smoothed display value, never used for authority
failureCode              bounded machine-readable reason, no arbitrary remote text
```

Percentage is derived as `verifiedBytes / totalBytes` with overflow-safe arithmetic. Progress must be monotonic for one content ID except when corrupt retained chunks are discarded, which creates a new attempt and an explicit UI explanation.

### Cached persistence

Prefer a shared managed-content identity component used by both `ModManager` and `MissionZipExtractionStore`:

- Compute wrapper SHA-256 once when the final managed file is accepted, or while streaming it into managed staging when that avoids an extra pass.
- During that same pass, compute fixed-size chunk SHA-256 values for resumable transfer. Persist whole-file size, modification time, SHA-256, chunk size, ordered chunk hashes, format, and the effective mission descriptor keys discovered during the accepted scan.
- Reuse that identity in the extracted-bundle record rather than hashing the wrapper again.
- Split the current extraction lookup into a cheap trusted lookup and an explicit full verification path. Lobby display uses the cheap lookup; import, suspected mutation, and integrity repair use full verification.
- Because wrappers are app-private and published atomically, size plus modification time is a reasonable cache invalidation check. Recompute if the record is absent, the file was replaced, size or time changed, or explicit verification is requested.
- For current pre-release disposable Android schemas, replace the manifest schema directly. If an identity is absent in an existing development install, regenerate it lazily as cache population rather than adding a permanent compatibility migration.
- Do not store hashes only in Compose state or preferences. They belong beside managed content ownership metadata.

## Authoritative catalog design

### Preferred design

Add a small paired D1/D2 read-only C API that serializes the result of the existing mission enumeration into bounded JSON or fixed records. It should expose only stable values needed by the launcher and keep parsing, built-in detection, naming, and filtering in C.

The bridge must enumerate against a supplied prepared mount plan identical to launch:

1. Reconcile active file-set content and evaluate D1-in-D2 readiness.
2. Build the ordered active set, content projection, DXA, stored mission wrapper, and extracted mission paths through the same production functions used by `prepareGameLaunchFiles`.
3. In an isolated native catalog session, mount those paths, call the appropriate engine's mission-list function with the requested mode filter, serialize bounded entries, and tear down PhysFS state.
4. Join each engine-returned mission key to launcher ownership metadata by normalized virtual descriptor path and mount source. This supplies the wrapper identity and transferability without moving mission parsing into Kotlin.

The feasibility spike must verify whether temporary PhysFS initialization can safely occur in the launcher process before game startup. If shared global state or loading both engine libraries is unsafe, run the read-only enumerator in a small isolated Android service process with distinct D1 and D2 native entry points. Process isolation is preferable to maintaining another approximate parser.

### Fallback if native enumeration proves disproportionately invasive

Create one Kotlin `PreparedMissionCatalog` over the exact production mount plan and `MissionZip.ScanResult.effectiveMissionSets`, then add parity tests against the D1 and D2 headless analyzers for the full checked-in mission corpus. This fixes the immediate omission but remains a fallback because rules can drift from C.

Do not simply expand `MissionScanner` to scan `.content/mods`; that would still miss extraction, SAF projections, precedence, built-ins, and future mount sources.

## Compatibility state machine

Use a shared enum in LAN and online UI:

```text
CHECKING
MATCH
INSTALLED_DISABLED
MISSING
SIZE_MISMATCH
HASH_MISMATCH
UNSUPPORTED_SOURCE
ERROR
QUEUED
DOWNLOADING
PAUSED
RETRYING
FAILED_RESUMABLE
VERIFYING
```

Resolution order:

1. Validate requirement schema, game, mission key, size, and hash format.
2. Find the effective local catalog entry for the required engine key.
3. If none is effective, look for an installed but disabled matching hash and return `INSTALLED_DISABLED`; otherwise return `MISSING`.
4. If an effective wrapper candidate has a different size, return `SIZE_MISMATCH`.
5. If size matches but SHA-256 differs, return `HASH_MISMATCH`.
6. If hash and size match but descriptor path or engine key does not resolve after mount precedence, return `ERROR` with a local remediation message.
7. Only then return `MATCH`.

`INSTALLED_DISABLED` should offer an explicit Enable action before offering a network download. Enabling may change mod precedence, so rebuild and verify rather than assuming success.

## Lobby protocol plan

### Shared rules

- Host publishes one immutable `mission_requirement` with the selected game settings.
- Every player, including the host, resolves it locally and reports a bounded status tied to a requirement revision or content ID.
- Any mission or relevant mount-plan change increments the revision, clears readiness, and resets all statuses to `CHECKING`.
- Ready true is rejected unless that player's latest revision is `MATCH`.
- Start is rejected unless there are enough players, all required players are ready, and every status is `MATCH` for the current revision.
- The final launch message repeats the requirement. Each client rechecks immediately before starting `MainActivity` to close stale-state and race windows.
- UI should show `Checking mission`, `Mission ready`, `Mission missing`, `Archive size differs`, `Archive hash differs`, `Download in progress`, or a specific local error next to each player.
- Transfer progress is not readiness. `100% sent` remains `VERIFYING` until the client verifies the whole wrapper, imports it, rebuilds the catalog, and reports `MATCH`.
- The host's player list is the primary status dashboard. Every joined-player row shows the same compatibility warning the affected client sees, or a determinate progress bar with verified bytes, total bytes, percentage, and retry/pause state.
- Progress updates are coalesced by time and byte threshold so they cannot crowd out chat, liveness, readiness, or Start messages.

### LAN changes

- Add a launcher lobby protocol version/capability to discovery and join messages.
- Add compact requirement fields to `ANNOUNCE`, `JOIN_ACK`, and `START`.
- Add the client's requirement revision and compatibility status to `JOIN` and `READY`, or add a dedicated `MISSION_STATUS` message. A dedicated message is clearer for asynchronous hashing and download progress.
- Add status and revision to `LanPlayer` and `PLAYER_LIST`.
- Add a bounded transfer snapshot to each `LanPlayer`: transfer ID, state, verified bytes, total bytes, attempt number, and optional retry delay. Do not put tokens or local paths in `PLAYER_LIST`.
- Validate field lengths before allocation or state mutation and keep packets well below the current receive buffer.
- Make `LobbyService.startGame` perform the authoritative gate and return a structured refusal reason. Compose button state is only presentation.
- Re-evaluate late joiners and direct-launch announcements before launching. If no launcher metadata can be obtained, the existing native missing-mission check remains the minimum guard.

### Online changes

- Put the host requirement in bounded `game_info`; its compact size is comfortably inside 5 KiB.
- Add a client `MISSION_STATUS` message rather than allowing clients to edit host-owned `game_info`.
- Store validated status and revision on `LobbyPlayer` and include them in `LobbyPlayerInfo` broadcasts.
- Store only coarse, rate-limited transfer progress in lobby state. Data bytes use the separate transfer data plane; normal WebSocket messages remain the control plane.
- On create, join, game-info update, return from game, and player reconnect, reset compatibility and readiness as appropriate.
- Reject `READY true` for nonmatching content and reject `START_GAME` unless readiness and compatibility invariants hold. Return stable error codes for client UI.
- Bump Android and Rust protocol versions together. This is a breaking pre-release launcher change; no compatibility shim is required.
- Apply rate limits to status updates and accept only known enum values, current revision, bounded names, valid sizes, and canonical hashes.
- Include the requirement in mid-game join state. A late joiner must match or download before the server begins connectivity negotiation and emits `GAME_STARTING`.

## Join and in-game guard plan

Use three layers:

1. Lobby check: visible per-player result and authoritative Start/Ready gate.
2. Pre-launch check: when `SetupActivity` receives `GameLaunchInfo`, rebuild or refresh the catalog and compare the repeated requirement before writing launch files or starting the game Activity. Show a blocking dialog that identifies missing, size mismatch, or hash mismatch and returns the player to the lobby.
3. Engine defense-in-depth: preserve D1/D2 `load_mission_by_name` and segment checksum errors. Improve Android launch error propagation so the launcher receives a structured failure instead of only leaving the player in an engine menu.

For launcher-originated joins, add the expected content ID to `GameLaunchInfo` and the per-launch handoff. This guarantees an exact recheck for normal LAN, online, and mid-game flows.

An engine-browser or raw direct-IP join cannot learn a wrapper hash from the legacy game packet. If exact archive enforcement is required there too, add a later paired D1/D2 Android companion handshake carrying a versioned 32-byte mission content ID. Keep it separate from upstream desktop packet layout unless cross-platform clients are deliberately upgraded. Until then, raw direct joins retain the existing missing mission and level checksum protections and should clearly be labeled as not archive-verified.

## Host-offered download plan

### User experience

- Add `Offer mission download` to host settings, default on.
- If the selected entry is not a retained managed wrapper, disable the toggle with `This mission cannot be shared automatically`.
- A mismatched joiner sees the exact state and one of:
  - `Enable installed matching archive`
  - `Download from host`
  - `Host is not offering this mission`
- Acceptance shows filename, size, host callsign, hash prefix, storage requirement, and a warning that host-provided mods are untrusted third-party content.
- Show progress, pause, resume, cancel, retry, verification, import, and final catalog refresh as separate states.
- Client progress shows verified bytes over total bytes and percentage. If a connection drops, retain the bar at the last verified boundary and show `Retrying` with the next attempt or `Resume download` after automatic retries are exhausted.
- The host sees one row per joined player with `Mission ready`, `Mission missing`, `Archive size differs`, `Archive hash differs`, `Queued`, `Downloading N%`, `Paused`, `Retrying`, `Verifying`, or a resumable failure.
- Host progress represents bytes the client has acknowledged as verified, not merely bytes handed to the host socket buffer.
- If a same-key archive already exists with another hash, keep it until the user approves the resulting enable/order change. Never overwrite it silently.

### LAN data plane, recommended first implementation

- Use a small app-private TCP/HTTP server bound only while hosting a lobby.
- Do not publish a reusable file URL in multicast announcements.
- After an admitted player requests the advertised content ID, issue a random one-time token scoped to lobby, player, hash, and expiry.
- Stream the retained wrapper from a fixed open file handle. Do not accept a client-supplied filesystem path.
- Permit one active transfer per peer and concurrent transfers for multiple joined players, bounded by the lobby's maximum non-host player count and a global safety cap. Excess work is visible as `QUEUED`, never silently dropped.
- Give each active client an independent file handle, bounded buffer, timeout, cancellation scope, and progress state. A stalled or malicious downloader must not hold a global content lock or block another downloader.
- The client writes to a private content-ID-named `.partial` file while enforcing declared length and a dedicated transfer maximum, and verifies chunks as they complete.
- On completion, compare length and hash before archive inspection. Then run `MissionZip.inspect` and all existing entry count, traversal, expansion ratio, per-entry, and expanded-size guards.
- Import through the existing atomic managed-content transaction, rebuild the native catalog, and report `MATCH` only if the expected entry is effective.
- Delete partial files on rejection or cancellation and clean stale partials on startup.

Do not use the 2 GiB extraction ceiling as a network transfer allowance. Measure the supported mission corpus and define a much smaller explicit wrapper cap. A 256 MiB starting cap is a reasonable design candidate, but it should be confirmed from real mission and soundtrack sizes before implementation.

### Resume and retry protocol

- Treat the whole-file SHA-256 plus size as the stable content ID across socket reconnects and lobby re-entry.
- Persist a small sidecar beside each private partial file containing schema, content ID, expected size, fixed chunk size, highest contiguous verified chunk, and last activity time. Publish sidecar updates atomically after the chunk bytes are flushed.
- Obtain the ordered chunk-hash manifest only after user consent and transfer authorization. It is not included in discovery or normal lobby packets.
- Resume only at a complete verified chunk boundary. Recheck retained complete chunks against the manifest before trusting a partial file after process death or a long interruption.
- Request the remaining byte range with the whole-file SHA-256 as an ETag or equivalent precondition. The host rejects an unaligned, out-of-range, stale, or different-content request.
- A renewed one-time token may authorize an existing partial after disconnect/reconnect, but only when lobby, player identity, and required whole-file content ID still match. Never persist or reuse the old token.
- Automatically retry transient timeout, connection reset, and temporary network loss with bounded exponential backoff and jitter while the player remains in the lobby. Do not retry policy rejection, changed identity, invalid range, insufficient storage, or archive validation failure.
- After automatic retries are exhausted, preserve verified partial data and offer a manual Retry/Resume action. Explicit `Delete partial download` removes it; ordinary Cancel may offer Keep for later or Delete.
- If final whole-file SHA-256 fails, mark the partial untrusted. Revalidate chunks to find the first bad boundary when possible; otherwise restart rather than importing any bytes.
- Expire abandoned partials by age and total cache budget with a visible storage-management path. Never evict an active transfer.

### Host responsiveness and concurrent transfer architecture

- Implement a dedicated `MissionTransferService` with a `SupervisorJob` and bounded `Dispatchers.IO` work, separate from `LobbyService`'s UDP receive, announce, chat, and membership jobs and separate from the Compose main thread.
- The accept loop performs only bounded parsing and authorization, then gives each admitted transfer its own cancellable child. No archive scan, whole-file hash, blocking stream copy, or progress formatting runs on the lobby receive loop.
- Snapshot an immutable authorized wrapper identity and open file handle before streaming. Do not hold `ModManager`, content manifest, lobby player-list, or Compose state locks while doing network or disk I/O.
- Use fixed-size pooled or per-transfer buffers with a strict aggregate memory ceiling. Never allocate a wrapper-sized byte array.
- Apply fair scheduling or per-client plus aggregate rate limits so one fast or slow downloader cannot monopolize host upload. Reserve control-plane capacity by keeping progress events coarse and prioritizing chat, liveness, readiness, and lobby updates.
- Support simultaneous downloads for all joined players up to the lobby limit, subject to the explicit global safety cap. Share immutable cached identity/chunk metadata, but never share mutable stream cursors between clients.
- Publish immutable per-player `TransferSnapshot` values through `StateFlow`. Coalesce UI/control updates to a maximum frequency and also emit immediately on state transitions and completion.
- Host progress advances from client acknowledgements of verified chunks. Socket bytes written may be retained as diagnostics but must not be presented as completed client progress.
- Failure, pause, cancel, player leave, mission change, host stop, and file identity change cancel only the affected child transfers unless the selected requirement itself changed. Other transfers and lobby traffic continue.
- Measure and test chat and lobby-update latency while the maximum supported number of clients download. Define a concrete latency budget during Phase 0 and make it an acceptance gate rather than relying only on visual responsiveness.

### Online data plane, separate phase

The current server should remain the control plane first. Evaluate these options with load and abuse tests:

1. Direct peer transfer over a new reliable data channel negotiated from the existing connectivity results, with relay fallback.
2. Bounded streaming relay through a separate server endpoint with backpressure, per-lobby authorization, per-user and global byte quotas, concurrency limits, timeouts, and no permanent storage.
3. Temporary object storage upload, which simplifies NAT traversal but adds persistence, cost, moderation, expiry, and legal requirements.

Prefer direct peer transfer with a tightly bounded streaming relay fallback if reliable transport can reuse the existing peer connectivity work. Do not place base64 file chunks in normal lobby JSON or the 5 KiB `game_info` field. Do not add permanent server storage in the first implementation.

## Security and integrity requirements

- Treat host files and every protocol field as malicious.
- Require explicit joiner consent even though the host's offer defaults on.
- Use cryptographically random, short-lived, single-content authorization tokens.
- Never serve arbitrary paths, directory listings, active-set base data, or other mods.
- Enforce archive byte cap before and during streaming, free-space checks, timeouts, cancellation, rate limits, and concurrency limits.
- Hash while streaming and compare before import; never trust the host's advertised hash alone.
- Re-run the existing safe archive scan after download. Continue rejecting traversal, absolute paths, excessive entries, oversized entries, excessive total expansion, and compression bombs.
- Publish managed content atomically and keep the old effective content until the new archive is fully verified.
- Log content ID prefixes and state transitions, not full local paths, tokens, or private addresses.
- Surface that the host is distributing third-party content and that permission to share remains the host's responsibility.

## Implementation phases

### Phase 0: contracts and native feasibility spike

- Define mission key, descriptor path, wrapper identity, source kind, and compatibility state contracts.
- Define the provenance-based transfer allowlist and prove that every D1/D2 base catalog entry resolves to `deny_base` even if host preferences or remote messages request sharing.
- Prototype bounded D1 and D2 mission enumeration against supplied mount paths.
- Verify launcher-process PhysFS lifecycle. Choose direct bridge or isolated service before building UI around it.
- Add fixtures showing the engine list for stock D1, stock D2, D1-in-D2, PTMC Castaway Redux, anarchy-only, multi-mission, and basename collision cases.
- Measure representative wrapper sizes and Android concurrent I/O behavior, then set the transfer cap, chunk size, global transfer cap, per-client/aggregate bandwidth policy, progress update rate, and chat/lobby latency budget.
- Exit criterion: a documented authoritative catalog approach whose fixture output matches both engines.

### Phase 1: durable identities and prepared mission catalog

- Generalize wrapper identity persistence for every mission archive import.
- Reuse the existing extracted-bundle hash rather than recomputing it.
- Build one prepared launch-path provider shared by launch preparation and catalog enumeration.
- Replace host dialogs' `MissionScanner` input with the authoritative catalog.
- Make catalog loading asynchronous with explicit loading and error UI.
- Apply game-mode filters and mount precedence, and invalidate stale selections.
- Exit criterion: Host Co-op can select and launch Castaway from the same active set in which New Game sees it.

### Phase 2: local compatibility resolver and launch gate

- Implement cached local resolution and the full status enum.
- Add expected requirement fields to `GameLaunchInfo` and multiplayer resume persistence where needed.
- Recheck immediately before launch and provide actionable blocking dialogs.
- Preserve and improve propagation of the paired D1/D2 native missing-mission failure.
- Exit criterion: missing, disabled, size-different, and hash-different local cases are distinguished without entering gameplay.

### Phase 3: LAN lobby synchronization

- Version LAN messages, advertise requirements, report player states, and render per-player warnings.
- Add the shared per-player compatibility/progress row to both host and joined-client lobby views.
- Enforce Ready and Start inside `LobbyService`.
- Carry the requirement through waiting-lobby, START, discovery, and mid-game paths.
- Exit criterion: a LAN host sees missing, size mismatch, hash mismatch, download, retry, and verification states beside the correct player, and cannot start while any player is not `MATCH`.

### Phase 4: online lobby synchronization

- Bump client and Rust protocol versions.
- Add bounded mission status storage, broadcasts, revision reset rules, and authoritative Ready/Start rejection.
- Render the same per-player compatibility/progress component used by LAN and rate-limit coarse progress relay through the server.
- Gate late-join connectivity setup on compatibility.
- Exit criterion: malicious or stale clients cannot bypass mission matching with a handcrafted Ready or Start message.

### Phase 5: LAN host-offered download

- Add default-on offer preference, the hard base-content exclusion, and transferability UX.
- Implement the separate bounded transfer service, cached chunk manifest, concurrent fair streaming, progress acknowledgements, pause/cancel, persisted partials, range resume, automatic retry, manual retry, hash verification, safe import, enable/order confirmation, and catalog recheck.
- Keep LAN UDP receive, chat, announcements, liveness, and player-list work independent from file streaming and progress production.
- Exit criterion: several missing LAN joiners can explicitly download Castaway at once, both endpoints show accurate per-player progress, a disconnected client resumes from verified data, all clients reach `MATCH`, and chat/lobby latency remains inside the Phase 0 budget.

### Phase 6: online transfer and raw engine join decisions

- Select and implement the online data plane after measuring traffic and NAT behavior.
- Require the online transport to preserve the same content-ID resume contract, per-player progress, independent failure domains, concurrent-host fairness, and control-plane latency guarantees as LAN.
- Decide whether exact wrapper identity must extend to raw engine-browser/direct-IP joins through an Android companion handshake.
- Exit criterion: online download has equivalent consent and safety properties, and any unverified join path is explicit.

### Phase 7: integration, regression, and documentation

- Run scoped code quality over every changed Kotlin, Rust, C/C++, script, and Markdown path.
- Run Android unit and integration tests, Rust lint/build/tests with the required timeout, and paired D1/D2 Windows host builds.
- Add emulator or device automation for host picker and launch gating. Use game introspection rather than screenshot parsing.
- Document host offer behavior, identity meaning, limits, error recovery, and the scope boundary for unrelated mods.

## Test plan

### Catalog and persistence unit tests

- Stored ZIP, extracted ZIP, RAR/7z retained wrapper, managed loose mission, and built-in sources.
- D1 engine, D2 engine, and D1 mission through D2 readiness on and off.
- Co-op exclusion and Anarchy inclusion of anarchy-only missions.
- Multi-mission wrapper ownership.
- Duplicate basename with mount-order winner and shadowed source.
- Hash creation during import, cheap cached lookup, invalidation on size/time change, and explicit full verification.
- Whole-file and fixed-chunk hashes are produced in one import pass and are stable across catalog reloads.
- D1 First Strike and D2 Counterstrike base variants always have `deny_base`; base HOG containment is rejected even if an internal caller supplies a forged transferable flag or managed-looking filename.
- Managed third-party wrappers are eligible, while built-in, managed loose, unmanaged loose, projection, and active-set base sources are ineligible.
- No repeated SHA-256 work during Compose recomposition or lobby broadcasts.

Likely homes include new catalog/identity tests plus extensions to `MissionZipExtractionStoreTest`, `ModManagerMissionZipTest`, `FileSetContentManagerTest`, and mission metadata analyzer fixtures.

### Protocol and authority tests

- LAN serialization round trips, packet size bounds, invalid hash/size/enum/revision rejection, and status reset.
- LAN Ready and Start refusal for every nonmatching state.
- Rust integration tests for create, join, status report, ready, start, game-info update, reconnect, and late join.
- Handcrafted client attempts to report an invalid status, stale revision, ready without match, start as non-host, and start with a mismatched player.
- Server rate-limit and maximum-field tests.
- Progress bounds reject negative, decreasing, over-total, stale-content, stale-transfer, wrong-player, and excessive-frequency updates without affecting chat or liveness.
- Player-list/UI mapping keeps every warning and progress snapshot attached to stable player identity across callsign collisions, reconnect, reorder, and leave.

### Transfer tests

- Correct stream, truncation, excess bytes, wrong size, wrong hash, timeout, cancellation, disconnect, retry, and insufficient space.
- Range resume at zero, a verified chunk boundary, final chunk, and completed file; reject unaligned, negative, beyond-end, and wrong-content ranges.
- Process death and lobby disconnect preserve an atomically recorded verified boundary; reconnect with the same content ID resumes using a new token.
- Requirement/hash change does not resume an old partial. Corrupt retained chunks roll back to a safe boundary or force a clean restart.
- Automatic retry uses bounded backoff and stops for permanent errors; manual Retry preserves valid chunks after transient retry exhaustion.
- Expired, reused, wrong-player, wrong-lobby, and wrong-content token rejection.
- Two and maximum-lobby concurrent downloads make fair forward progress; a stalled, cancelled, or failing peer does not pause another transfer.
- Concurrency cap, queue order, per-client and aggregate rate limits, fixed memory ceiling, and host file replacement during transfer.
- Chat, announcements, ready changes, player-list updates, and cancellation remain inside the Phase 0 latency budget during maximum concurrent transfer load.
- Progress bars on host and downloader track acknowledged verified bytes, survive retry/resume, reach 100 percent before verification, and remain non-Match until import/catalog verification completes.
- Hostile archive traversal, entry count, expansion ratio, oversized entry, excessive expanded total, and malformed descriptor cases.
- Same filename/different hash and same mission key/different wrapper coexistence without silent overwrite.
- Downloaded wrapper becomes effective only after explicit enable/order confirmation and authoritative catalog recheck.
- Requests for D1/D2 base missions or HOGs never create a token, open a file, or expose a transfer endpoint.

### Two-device or two-emulator acceptance matrix

- D2 host and joiner with the same Castaway wrapper: both show Match and start.
- Joiner missing Castaway: lobby warns and blocks.
- Joiner has same filename with different size: size warning and block.
- Joiner has same size but different SHA-256: hash warning and block.
- Joiner has matching wrapper disabled: enable action, recheck, then Match.
- Download offer on: explicit download, verify, import, Match, ready, start.
- Download offer off: clear unavailable message and no transfer endpoint exposure.
- D1/D2 built-in selected with offer preference on: compatibility works, but UI says base mission is not transferable and no endpoint exists.
- Two or more missing joiners download concurrently: host shows an independent progress bar and state beside each player; each client shows its own bar.
- One downloader disconnects while another continues: unaffected transfer and chat continue, and the disconnected client later resumes at its verified boundary.
- Slow or stalled downloader: other downloads, chat, readiness, announcements, and player status updates remain responsive.
- Transient failure retries automatically; retry exhaustion exposes manual Resume without losing verified chunks.
- D1 mission hosted in D1 and through D2.
- Stock D1 and D2 missions.
- Host changes mission or relevant mod order: all statuses and readiness reset.
- Join during an in-progress game: same gate before connectivity and engine launch.
- Raw direct-IP missing mission: existing native error remains visible and actionable.
- Level-data mismatch after a passed launcher check: existing engine checksum defense still rejects.

## Expected files and components affected during implementation

This is directional, not an exhaustive change list:

- `android/app/src/main/java/com/dxxredux/app/multiplayer/MissionPicker.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/CreateGameDialog.kt`
- `android/app/src/main/java/com/dxxredux/app/ModManager.kt`
- `android/app/src/main/java/com/dxxredux/app/MissionZipExtractionStore.kt`
- `android/app/src/main/java/com/dxxredux/app/FileSetContentManager.kt`
- `android/app/src/main/java/com/dxxredux/app/SetupActivity.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingState.kt`
- `android/app/src/main/java/com/dxxredux/app/lobby/LobbyProtocol.kt`
- `android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/NetworkProtocol.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/MatchmakingService.kt`
- `android/app/src/main/java/com/dxxredux/app/multiplayer/LobbyScreen.kt`
- New shared mission compatibility/progress UI and a dedicated `MissionTransferService` with partial-download storage
- `server/src/protocol.rs`, `server/src/lobby.rs`, and `server/src/ws_handler.rs`
- A small shared Android native catalog/identity handoff plus paired D1/D2 mission hooks if the feasibility spike confirms that approach

## Open decisions before implementation

1. Native catalog isolation: direct temporary PhysFS session versus a dedicated Android process.
2. Transfer byte cap after measuring real mission and soundtrack archives.
3. Online data plane and who pays relay bandwidth.
4. Chunk size, progress coalescing interval, retry count/backoff, partial retention age, aggregate partial cache budget, and upload fairness limits, all chosen from Phase 0 measurements rather than unbounded defaults.
5. Whether enabling an already installed matching archive may be one-tap or requires an additional mod-order confirmation.
6. Whether downloaded content persists as a normal enabled mod after leaving the lobby. Recommended behavior is normal managed import in the active file set, with the enable/order change shown to the user.
7. Cross-platform policy when a desktop client cannot supply Android wrapper identity. Do not silently label such a client as matched.
8. Whether raw engine-browser joins need the later companion content-ID handshake. Normal launcher lobby joins do not need to wait for this to gain strong pre-launch checks.

## Study completion

- [x] Traced launcher mission import, persistence, activation, and single-player handoff
- [x] Traced host co-op game and mission selection for D1 and D2
- [x] Traced LAN and online lobby discovery, join validation, launch, and in-engine mismatch behavior
- [x] Inventoried server protocol, trust boundaries, readiness enforcement, and transfer constraints
- [x] Designed an authoritative mission catalog and stable archive identity/cache model
- [x] Designed staged compatibility warnings and default-on host-offered download flow
- [x] Made D1/D2 base mission and base-file transfer denial an explicit provenance-based invariant
- [x] Designed host/client progress, partial persistence, verified range resume, reconnect retry, and failure recovery
- [x] Designed bounded concurrent host transfers isolated from chat and lobby control work, with per-player host progress
- [x] Defined implementation phases, security requirements, tests, risks, and open product decisions

Status: Implementation in progress.

## Implementation progress

- [x] Phase 0: contracts, baseline, and Kotlin prepared-catalog fallback decision
- [x] Phase 1: durable identities and prepared mission catalog
- [x] Phase 2: local compatibility resolver and launch gate
- [x] Phase 3: LAN lobby synchronization
- [x] Phase 4: online lobby synchronization
- [x] Phase 5: resumable concurrent LAN download
- [ ] Phase 6: online mission-byte data plane and optional raw direct-join companion handshake
- [x] Phase 7: scoped integration tests, Android build, Rust tests/lints, and documentation

The implemented online lobby exchanges and enforces exact wrapper requirements and per-player status, but it does not move mission bytes. Online hosting explicitly labels mission download as LAN-only rather than advertising an unusable offer. A production online transfer remains a separate data-plane project because normal lobby JSON and the gameplay UDP relay are intentionally not suitable for archive traffic.

The prepared catalog uses the documented Kotlin fallback over enabled managed wrappers and `MissionZip.effectiveMissionSets`. A temporary native PhysFS catalog session was not added because it would share process-global engine state; parity and managed-wrapper tests cover the immediate Android host-picker gap.
