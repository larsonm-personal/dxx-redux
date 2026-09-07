# Coop spew recovery: current state and migration plan

Status: client-disconnect/rejoin recovery implemented and validated; host-loss-during-transfer limitation recorded below

## Accepted implementation policy

- Return owned gear and avoid duplicates are the controlling requirements
- Uncollected expired spew becomes recovery credit when expiration is allowed
- Level departure converts uncollected spew to credit rather than discarding it
- Keep normal no-expire behavior when that option is enabled
- Track created drop contents and undropped remainder separately from retained inventory

Implementation checklist:

- [x] Shared ownership ledger and exact gear transfers
- [x] Paired drop/pickup/expiry/level hooks and host arbitration
- [x] Rejoin delivery, snapshot and persistence integration
- [x] Native conservation/integration coverage and serial D1/D2 recovery emulator validation
- [x] Scoped formatting and Android/Windows builds

## Requested behavior

A returning player receives gear still belonging to them: retained inventory that was never dropped, remaining contents of their live spew, and uncollected spew credited on timeout when absorption is enabled. Recovery consumes those claims and removes the corresponding world contents. Items collected by another player are never restored to the original owner, even if the collector later drops them again

This replaces the earlier snapshot-plus-spew-cleanup design documented in coop-inventory-rejoin-research.md. That historical plan is not sufficient for this requirement

## Baseline survey (before this implementation)

Paths below are relative to the repository root; D2 examples have corresponding D1 hooks unless stated otherwise

| Area | Current behavior and evidence | Gap |
| --- | --- | --- |
| Inventory cache | `android/app/src/main/cpp/shared/coop/coop_save.c`: `coop_track_absent_player`, `coop_snapshot_player`, absent list | Snapshots inventory, not ownership of individual drops; cache holds 16 remembered players and evicts the oldest |
| Disconnect | `d1/main/multi.c`, `d2/main/multi.c`: `multi_disconnect_player` snapshots the remote player; `multi_leave_game` drops eggs and sends `MULTI_PLAYER_DROP` | A graceful leave emits drops; the timeout/disconnect function itself does not synthesize equivalent spew for a crashed client |
| Rejoin | `coop/coop_multi_status.c`: `coop_send_restore_inventory` takes and removes the absent record, deletes same-level powerups with `object_owner[i] == pnum`, then sends an 86-byte inventory packet | No subtraction for teammate pickups or expiry; record is consumed before an application acknowledgment |
| Rejoin timing | `d1/main/net_udp.c`, `d2/main/net_udp.c`: object-transfer completion sends rejoin sync, then inventory restore, then starts extras | Spew may already be in the joining client's object snapshot before cleanup; sync, removals, and inventory apply need a coordinated boundary |
| Applying inventory | `coop_save.c`: `coop_apply_record_to_player` assigns weapons/ammo/resources/stats and ORs durable flags; same-level keys are restored | Not an exact transactional replacement of every inventory field; cannot directly reuse this as the new recovery commit |
| Drop creation | `d1/main/collide.c`, `d2/main/collide.c`: `drop_player_eggs_remote`; `multi.c`: `multi_do_player_explode` creates remote eggs and maps their object numbers | Peer object creation and numeric player ownership are transport details, not durable item provenance |
| Spew flag/lifetime | Paired `fireball.c` marks network-created player eggs `OF_PLAYER_DROPPED`; `PlayerSpewNoExpire` makes them immortal; `coop_restore_player_spew_lifetimes` reapplies policy after load | Flag identifies dropped objects but not original owner, batch, exact recoverable contents, or removal outcome |
| Pickups | Paired `collide.c` calls local `do_powerup`, then marks/removes consumed powerups; `multi_do_remobj` receives object number, owner mapping and duplicate suppression data | Removal has no semantic reason, collector receipt, or recoverable quantity; pickups are already awarded before peers hear about them |
| Expiry | Paired `powerup.c`/`object.c` process lifetime exhaustion and mark objects dead | No absorbed-inventory credit ledger found |
| Existing pickup ledger | `coop/coop_powerup_duplication.c` tracks per-player collections for duplicated energy/shields, with save and snapshot support | Explicitly excludes player-dropped objects and prunes vanished objects; cannot serve as a historical spew ledger |
| Persistence | `coop_save.c/.h` stores active/absent player records in save metadata and a progress-inventory sidecar | No drop ownership, quantities, terminal outcomes, or pending recovery transactions; absent records loaded from metadata inherit the save's level |
| Identity | UDP reconnect now has a persisted signing identity; inventory records still match client UUID then callsign | Bind recovery ownership to authenticated identity; numeric player slots and unverified callsign matches are insufficient |

Existing settings are `FullDeathSpew` and `PlayerSpewNoExpire`. No gameplay implementation of timeout absorption was found in the surveyed native and Kotlin code. Absorption is therefore a new feature, not an existing balance that can simply be queried

## Why a smaller cleanup patch is insufficient

- If Alice drops a plasma cannon and Bob collects it, restoring Alice's cached weapon flags duplicates the cannon even when the mine scan finds nothing to delete
- `object_owner` means the network creator. A scan can remove manually dropped or other creator-owned powerups and cannot distinguish separate deaths, reused slots, or saved/remapped identities
- A missing object could have been picked up, expired, destroyed, discarded at level teardown, or never created because the object table was full. Absence is not evidence of absorption
- Pickup and rejoin can race: Bob may have already received the local reward when the host decides to reclaim the object for Alice
- Cached ship status is useful but is not an atomic inventory/drop history. A delayed pre-death status must not reintroduce inventory already represented by a drop batch

## Proposed ownership model

Add shared native recovery state under `android/app/src/main/cpp/shared/coop/`, with narrow D1/D2 hooks

Each drop batch has a session/campaign identity, timeline/level-instance generation, authenticated owner identity, life/departure sequence, and unique batch ID. Each entry has a unique item ID, typed contents, remaining quantity/charge, revision, and optional world-object binding

Object indices, signatures and remote mappings locate a current object; they are not the durable item ID. A numeric level is not a level-instance ID: restarting or revisiting the same level must not match stale drops

Track these states explicitly:

| State | Meaning | Rejoin action |
| --- | --- | --- |
| Retained | Confirmed inventory never transferred into a drop | Restore once as the player's existing inventory |
| In mine | Remaining contents of a live owned drop | Reserve, remove or reduce world contents, transfer to player |
| Absorbed | Remaining uncollected contents credited at a confirmed timeout | Transfer credit to player once |
| Collected | Ownership transferred by a committed pickup | Never restore to the former owner |
| Lost/consumed | Destruction, nonabsorbing expiry, armed/exploded mine, or another explicit nonrecoverable outcome | Do not restore |
| Reserved | Pickup or recovery transaction is in progress | Resolve the existing transaction; do not start another claim |

Conservation rule: every recoverable unit occupies exactly one inventory, world entry, credit entry, or pending transfer. It cannot simultaneously be restored inventory and collectible spew

Include all outstanding batches owned by the returning player. A successful pickup retires the original claim, including a pickup by the owner. If a collector later drops the item, create a new claim for that collector rather than reviving the original owner's claim

Separate nondroppable progress from recoverable goods: score, kill statistics, play time and level keys need their existing restoration policies, not subtraction based on powerup objects

## Exact contents and policy recommendations

Do not invert powerup IDs into guessed inventory. Record the actual inventory-to-drop transfer while creating the drop manifest, including quantities that cannot be represented by ordinary eggs

- Missile singles/four-packs and mine remainders need exact units. Armed death mines count as spent, not uncollected gear
- Vulcan/Gauss weapons can contain shared-pool ammunition. Track the weapon entitlement and ammo contents separately; partial collection retires only the quantity transferred
- D2 Omega charge needs an explicit field. The current player record has afterburner charge but no Omega charge; remote death creation currently substitutes maximum Omega charge
- Laser and super-laser drops must preserve the actual upgrade entitlement. Simple counts of powerup IDs do not fully describe a higher-level laser loadout
- Drop caps, minimum weapon ammo, rounding, and object-creation failures mean the old egg generator is not a lossless inventory serialization
- Always-created shield/energy eggs are gameplay bonuses, not copies of the player's exact old shield/energy values. Keep them distinct from retained resource values to avoid restoring both representations of the same claim
- Recovery must not replay pickup score, duplicate-weapon-to-energy rewards, or restart cloak/invulnerability timers. Preserve remaining durations only if a separate explicit policy supports them
- Capacity limits and already-owned weapons must not silently erase outstanding entitlement. Recommend retaining excess recovery as credit rather than minting bonus energy or respawning duplicate weapons

Keep `PlayerSpewNoExpire`, and add an independent host setting such as `AbsorbExpiredPlayerSpew`. No-expire takes precedence because no timeout occurs. At timeout, sample the absorption setting and record the result permanently; later toggling the option must not resurrect lost items or revoke already-earned credit

Recommended scope is recovery on successful rejoin. Do not automatically grant absorbed gear to a still-connected dead/respawned player without a separate requested policy

Two gameplay boundaries should be settled before implementation:

1. **Leaving a level while its owner is absent:** level teardown is not a timeout. Under the strict requested rule, old-level live spew is not restored in the new mine; already absorbed credit and never-dropped inventory survive. If carrying old-level uncollected spew forward is desired, define it explicitly rather than silently treating teardown as absorption
2. **Artificial death bonuses and undroppable remainders:** recommend preserving ordinary respawn/resource rules and accounting explicitly for true retained gear, generated bonuses, consumed mines, and lost quantities. Do not use the old whole-player snapshot as an implicit balancing credit

## Networking and rejoin transaction

The host must serialize pickup, expiry and recovery for tracked spew. Scope this authority change to recoverable player drops; ordinary mine pickups can retain their existing behavior

1. A tracked pickup requests an item ID/revision and quantity. It does not award the local reward first. The host verifies claimant, item state and relevant inventory revision, commits the exact transfer, then sends an idempotent grant and world update
2. Host expiry creates an absorbed credit or a lost outcome for the remaining contents. Clients may animate expiry but cannot independently grant absorption or delete the authoritative claim
3. On authenticated rejoin, freeze the returning player's gameplay inventory changes and reserve their recoverable entries. Snapshot/extras must exclude reserved contents, and existing peers must stop accepting pickup claims for them
4. Compute a complete resulting inventory from confirmed retained state plus eligible transfers. Preserve stats separately, set durable equipment flags exactly, account for spawn defaults, and use game-specific capacity/weapon adapters
5. Send a transaction ID, authority/timeline epoch, inventory revision, resulting inventory and consumed item revisions. Apply once at a game-frame boundary after sync/save restore; acknowledge the applied revision
6. Commit/retain the result on the host and release the joiner into play. Retransmission must reuse the same transaction, not add inventory again. Keep recoverable transaction state until delivery/application is resolved

Do not release reserved world items merely because an acknowledgment timed out: the client may have applied the grant before losing its connection. Reconcile or resume that transaction on retry

For a crash without a committed drop event, preserve the last host-confirmed retained inventory; do not synthesize another drop batch from a stale snapshot. If exact crash recovery beyond the last confirmed state is required, inventory mutations and drop boundaries need acknowledged revisions. Unsent client changes cannot be reconstructed from periodic status alone

Reliable UDP delivery alone does not provide application-level exactly-once behavior. Add transaction IDs and explicit apply acknowledgments rather than relying on the current remove-before-send absent-list operation

## Saves, rewind, migration and settings

- Persist the ledger, item IDs/revisions, ownership bindings, absorbed balances, retained inventory revisions, pending transfers and applied transaction IDs with the same save snapshot as world/player state
- Restore and remap object bindings on both games. Rewind restores the whole historical ownership state and advances the timeline epoch so delayed future packets cannot affect it
- Extend checkpoint/progress data with eligible off-world state. Do not load live-object claims into a newly generated mine without an explicit conversion policy
- Replicate commits and pending transfers to eligible successor hosts before exposing irreversible grants. Fence the former host by authority epoch. A successor missing committed state must reconcile or defer recovery, not guess from an older inventory snapshot
- Replace silent absent-record eviction for outstanding gear with a defined bound/backpressure policy. Prune terminal history only when old messages cannot apply in the current epoch
- Version the Android coop protocol and metadata; reject incompatible peers. These formats are pre-release, so replace the Android schema directly rather than designing legacy provenance inference or compatibility readers
- Update native netgame settings serialization, saves, host migration settings, JNI/auto-net launch parameters, Kotlin host defaults, lobby/game-info messages and UI together
- Keep standalone/non-Android D1/D2 builds working; leave upstream save formats and non-coop behavior unchanged

## Implementation sequence and completion gates

1. **Policy and diagnostics:** settle the two gameplay boundaries above, define the field-by-field inventory/drop table for both games, and add introspection for inventory revisions, drop IDs, remaining contents and recovery status
2. **Ledger and manifests:** implement shared ownership transitions; instrument actual drop creation, death/leave batches and unexpected disconnects. Test conservation, repeated deaths, slot/object reuse, caps and allocation failures
3. **Pickup and expiry commits:** introduce tracked-spew claims/grants, exact partial contents, semantic removal reasons and the absorption option. Preserve behavior of the separate energy/shield duplication ledger
4. **Transactional rejoin:** replace broad creator-slot cleanup and cached full-inventory grant. Coordinate object sync, extras, frame-boundary application, retries and interrupted rejoins
5. **Persistence and host migration:** round-trip world plus ledger and transaction state, remap identities/objects, fence timelines and replicate committed state. Do not ship the new behavior before these paths work
6. **Integration validation:** run serial two-peer emulator tests for D1 and D2 through a reusable runner, plus scoped formatting, native CMake tests, Android builds and Windows builds

Required integration cases:

- All spew remains: recover it once and verify it disappears on every peer
- Teammate takes some weapons/ammo: recover only the remaining contents
- Teammate takes everything, then drops it again: original owner receives none of those claims
- Owner collected earlier spew, dies again, then rejoins: no repeated credit; independent uncollected batches still recover
- Timeout with absorption enabled/disabled and with no-expire enabled: correct credit/loss/live behavior
- Simultaneous teammate pickup, timeout and rejoin claim: exactly one committed outcome
- Crash without leave, lost/delayed drop/status packets, and repeated failed rejoins: no snapshot plus spew duplication
- Drop limits, armed mine remainders, D2 shared ammo/Omega charge, partial pickups and inventory-cap overflow
- Interrupted sync, lost/reordered/duplicated grant or acknowledgment, and client crash before/after apply
- Save/load, rewind, level restart/change, object/slot reuse, identity change and host migration during each transfer phase

This is a shared gameplay-state and protocol migration, not a local change to coop_send_restore_inventory. Existing snapshot serialization, reconnect authentication, reliable messaging and save/snapshot plumbing are reusable, but none currently supplies the missing ownership accounting

## Baseline survey validation

The baseline table describes the source before implementation. The sections below record the resulting implementation and actual validation


## Implemented behavior

Shared implementation: `android/app/src/main/cpp/shared/coop/coop_recovery.c`, with paired D1/D2 hooks

- Android coop QoL only; every participating Android peer must use the new protocol. The user confirmed all participating players are Android. Standalone desktop builds retain their existing protocol and do not implement this Android feature
- Stable identity owns each drop, independent of player slot. Host-authorized pickups transfer only remaining contents; committed receipts and inventory revisions suppress duplicates and reordered delivery
- Actual created eggs consume the real inventory pool. Successfully armed mines are consumed; allocation failures and undropped gear become credit. Generated energy/shield bonuses and timed effects keep ordinary pickup behavior
- Rejoin strips inventory represented by prior drops from the absent snapshot, consumes remaining world/credit entries, removes matching world objects, and replaces inventory at a game-frame boundary. Host retains the resulting inventory before sending, so another failed join can retry without granting snapshot plus spew
- Expiring uncollected gear becomes credit when expiration is allowed. No separate absorption toggle was added: this implementation follows the controlling give-back policy under coop QoL, with `PlayerSpewNoExpire` preserving physical spew instead
- Level departure and checkpoint resume carry remaining claims forward as credit. Inventory-cap overflow stays credit for later recovery
- Metadata version 7 and progress-inventory version 3 persist ownership and inventory revisions, including D2 Omega. Legacy Android coop metadata is intentionally incompatible under the repository's pre-release format policy
- Surviving host migration republishes live claims with the new host object mapping. Saved object bindings and active player inventories are remapped on restore. Terminal pickup receipts do not replay into newly assigned slots
- Robot position send/receive now rejects deleted robots and invalid segments before `extract_shortpos`. A two-peer test reproduced that assertion after queued positions outlived deleted robots. This is a confirmed crash path, not proof of the cause of the user's original process disappearance

## Validation results

- Native D1/D2 recovery harness passes eight scenario groups: partial collection plus expiry/retry, capacity/partial Vulcan/save-object remapping, crash without drop and allocation overflow, grant racing death, reordered/duplicate grants and snapshots, identity/level/Omega, late drop plus successful/failed armed mines, host-migration object mapping
- D2 CTest coop/reconnect/initial-sync/save-transfer subset: 10 tests pass
- Two Android emulators, D2: all-uncollected recovery and two process restarts pass
- Two Android emulators, D1 and D2: host collects four of six homing missiles; returning client receives plasma and only two missiles; host keeps four; no owned spew remains on either peer. A second client process restart preserves those counts. Both games pass using `android/tests/test_lan.ps1 -Game d1|d2 -SkipBuild -SpewRecovery`
- Final D1/D2 Windows builds and Android builds for arm64-v8a, armeabi-v7a, and x86_64 pass; no new compiler warnings from these changes
- Scoped mixed-language formatting/lint passes, including final protocol, migration, and runner changes
- Full D2 synchronized save/restore with host/player-slot swap: first run restored both peers but sent automation to the client before its receiver started because an old introspection file survived restart. Runner now removes that stale file before relaunch; rerun passes on both peers

## Remaining validation limits

The larger survey matrix above is a design/stress-test inventory, not a claim that every listed case has been exercised on devices. In particular, host migration at each in-flight transfer boundary, three-peer partition/reconciliation, exhaustive packet-loss injection, and full object-table exhaustion across peers remain unvalidated. Pickup source updates and grant receipts are separate reliable messages; replication is not a quorum commit protocol, so a successor missing an in-flight update is an unresolved failure boundary. Ordinary never-dropped inventory can only be recovered from the last host-confirmed status; unsent client mutations cannot be reconstructed
