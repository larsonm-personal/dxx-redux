# Cooperative energy and shield pickups per player

## Goal

Add an independent cooperative host QoL option named:

`Energy and shields duplicated for each player`

When enabled, each energy or shield powerup can be collected once by every
player. A successful collection consumes that pickup only for the collecting
player. Other players can still see and collect the same pickup. When disabled,
pickup behavior remains unchanged.

Player-spewed powerups are explicitly excluded. Spewed energy and shields keep
the normal shared behavior and can be picked up only once total.

## Planning status

- [x] Trace the D1 and D2 powerup collision and removal paths
- [x] Trace the existing coop QoL host option through native and launcher code
- [x] Identify network object mapping, host migration, and coop save concerns
- [x] Choose a proposed state and synchronization design
- [x] Reconcile the plan with the cooperative duplicate-weapon energy change
- [x] Define lossless coop disk-save and in-memory rewind serialization
- [x] Implement the feature
- [ ] Add and run two-client regression coverage
- [x] Run scoped formatting checks, Android builds, and launcher tests

## Implementation progress, 2026-07-25

Implemented in D1 and D2:

- Added the independent, default-off host toggle through the native netgame
  packet, Android host UI, launcher defaults, config export/import, LAN and
  matchmaking start data, resume data, and host-migration data.
- Persisted the changed future-host default in both launcher preferences and
  native netgame profile configuration.
- Added a shared tracker keyed by object signature plus stable client ID, with
  callsign fallback. Player slots are used only to resolve the current stable
  identity.
- Kept player death spew and D2 player-spat powerups on the original globally
  single-use path by excluding `OF_PLAYER_DROPPED` and `PF_SPAT_BY_PLAYER`.
- Kept weapon objects out of the tracker, including weapons that award energy
  through the all-players-own-the-weapon enhancement.
- Added mapped collection packets and a complete host snapshot at the end of
  late-join object synchronization. Every peer therefore retains the history
  required for later host migration.
- Added local collision and rendering filtering, plus D2 guidebot objective
  filtering. The existing automap only renders key powerups, so it does not
  expose collected energy or shield objects.
- Added a framed coop save trailer containing the saved toggle and every live
  object-to-stable-player collection relation. Disk saves and in-memory
  rewind/level-restart saves use the same payload writer and parser.
- Restore now validates the footer, version, sizes, checksum, identities,
  duplicate relations, object signatures, object IDs, and eligibility before
  atomically replacing tracker state. Missing old-format coop metadata is an
  explicit restore failure, as compatibility was not required.
- Save write failures now propagate to the game save operation. Single-player
  saves retain their prior metadata path and do not require a coop trailer.
- Expired or removed dynamic robot drops are pruned before snapshot or save;
  live dynamic drops use existing remote/local object mapping.

Verification completed:

- `git diff --check`: passed.
- `:app:compileDebugKotlin :app:externalNativeBuildDebug`: passed for Kotlin
  and Android arm64-v8a, armeabi-v7a, and x86_64.
- `:app:testDebugUnitTest` for `LobbyProtocolStartOptionsTest` and
  `MultiplayerResumePrefsTest`: passed.
- `:app:assembleDebug`: passed and produced the debug APK.
- Fixed `test_coop_player_session` to inherit the discovered PhysFS and SDL
  include directories in both D1 and D2.
- `run-windows-build.ps1 -Target d1`: passed.
- `run-windows-build.ps1 -Target d2`: passed.
- Both generated `test_coop_player_session.exe` binaries: passed.

Still deferred:

- Maintained two-client emulator coverage for pickup races, late join, save
  restore, reconnect, and host migration.
- Dedicated malformed-save and allocation-failure native policy tests.

## Current behavior and constraints

- `collide_player_and_powerup()` in both `d1/main/collide.c` and
  `d2/main/collide.c` calls `do_powerup()` only for the local player.
- After a successful pickup, the object is marked `OF_SHOULD_BE_DEAD` and
  `multi_send_remobj()` removes it from every peer.
- Energy and shields are not consumed when the player is already at the
  applicable maximum. The new behavior should preserve that rule: a failed
  pickup must not mark the pickup as collected for that player.
- Both games mark multiplayer player-death spew with `OF_PLAYER_DROPPED` in
  `mark_player_spew_objects()`. D2 also has `PF_SPAT_BY_PLAYER` for its direct
  player-spat path. These markers must be audited together so every form of
  player spew stays globally single-use.
- The new cooperative duplicate-weapon rule in `powerup.c` calls
  `pick_up_energy()` when the local player already owns a primary and every
  connected coop player owns it. The source object is still a weapon powerup,
  and the existing collision path removes it globally after the energy award.
- That weapon rule now depends on current inventory broadcasts:
  `multi_send_ship_status()` is scheduled after a successful primary pickup,
  coop ship status is broadcast to all peers, and D2's
  `MULTI_SHIP_STATUS` payload is now 70 bytes so it carries the upper primary
  weapon flag bits.
- The existing `Netgame.game_flags` field is a fully occupied byte, especially
  in D2. The new option must not consume another bit in that field.
- Static level objects and dynamically dropped objects can have different local
  object numbers on different peers. Existing `objnum_local_to_remote()` and
  `objnum_remote_to_local()` mappings must be used in any new packet.
- Mutating `object.type`, `object.flags`, or `render_type` to hide a collected
  pickup locally would make object transfer, late join, save/restore, and slot
  reuse fragile. Per-player state should live outside the base object layout.
- Both D1 and D2 already serialize each object's signature and the global
  object-signature seed. A restored powerup therefore retains a stable identity
  that can be validated against its saved object index, type, and powerup ID.
- Coop restore remaps saved players to current slots by stable `client_id`, with
  callsign fallback. Saving only a numeric player bit mask would lose or
  misattribute pickup history when slots change.
- The current coop metadata is a raw fixed-size trailer followed by optional
  Android metadata on disk. A variable pickup section needs explicit framing
  and length discovery; it cannot be appended to the raw struct and located
  safely with `sizeof(coop_save_metadata)`.
- D1 and D2 gameplay hooks should stay small and matching. New state management
  and protocol logic should be shared from `android/app/src/main/cpp/shared`.

## Proposed behavior

1. The option is available only as a cooperative host setting and defaults off.
2. With the option off, all existing pickup and removal behavior is untouched.
3. With the option on, only eligible `POW_ENERGY` and `POW_SHIELD_BOOST`
   objects use the new path.
4. Any energy or shield object marked as player spew keeps the original
   behavior: the first successful pickup marks it dead and sends
   `multi_send_remobj()` so it disappears for everyone.
5. A player at maximum energy or shields does not consume that pickup and may
   return for it later.
6. After a successful collection, that player can no longer collide with, see,
   target, or select that pickup as an automap or guidebot objective.
7. Other players retain the normal visible and collectible object.
8. The underlying eligible object remains alive for the level. It is not globally
   removed after all currently connected players collect it, because a late
   joiner must still receive a copy.
9. A genuinely new player identity gets a fresh opportunity to collect every
   qualifying pickup. A reconnecting player with the same `client_id`, or the
   same callsign when no client ID exists, retains their collection history
   even if their numeric player slot changes.
10. Level transitions reset the tracker. Coop save/restore and rewind restore
    the tracker to the saved point rather than granting duplicate pickups again.
11. Weapon objects never enter the per-player tracker, even when
    `do_powerup()` grants energy through the cooperative duplicate-weapon rule.
    They retain that rule's existing globally single-use behavior.

## Default-off invariants

The missing-setting and first-run default is false. When false, behavior must be
identical to the current game:

- `collide_player_and_powerup()` calls `do_powerup()` and, on success, marks the
  object `OF_SHOULD_BE_DEAD` and calls `multi_send_remobj()` exactly as before
- energy and shields disappear globally after the first successful pickup
- rendering, automap, guidebot, and Android objective code do not filter objects
- no collection event or snapshot is sent
- no collection relation is created or consulted
- player spew, robot drops, level objects, and duplicate weapons all retain
  their current behavior
- single-player, competitive multiplayer, and non-coop team modes never enter
  the new path

Implement this as the first inexpensive gate in every gameplay, rendering, and
objective hook. Do not run source-object classification and then attempt to
reconstruct the old behavior later.

The tracker may remain unallocated while the setting is false. A disabled save
still writes the setting as false and an exact zero relation count. Loading it
must clear any stale in-memory tracker before gameplay resumes.

## Integration with cooperative duplicate-weapon energy

The two enhancements share coop inventory and connection state, but make
different decisions:

- Cooperative duplicate-weapon energy asks whether all active coop players
  already own the source weapon.
- Energy/shield duplication asks whether the source object itself is an
  eligible `POW_ENERGY` or `POW_SHIELD_BOOST` and whether the local player has
  collected that object.

Do not infer per-player duplication from the result of `pick_up_energy()` or
from a change in the player's energy value. Eligibility must be decided from
the source object's powerup ID and spew markers before `do_powerup()` runs.
This prevents a duplicate Spreadfire, Plasma, Fusion, Helix, Phoenix, or Omega
pickup from being mistaken for a duplicated energy powerup.

Use the same definition of active coop players as the weapon rule where a
complete player set is needed: skip an observer host, consider player slots
through `N_players`, and exclude `CONNECT_DISCONNECTED` slots. Centralize this
small player-set rule if implementation can do so without expanding the D1/D2
diff; otherwise keep matching loops and tests.

Preserve the current inventory synchronization changes. In particular:

- do not restore the removed Android guards around coop ship-status broadcasts
- do not reduce D2 `MULTI_SHIP_STATUS` from 70 bytes
- do not remove the high primary-weapon flag byte or immediate status update
- do not put sparse per-object collection masks into `MULTI_SHIP_STATUS`

The per-object tracker needs its own bounded messages and snapshots. Ship
status remains the source of truth for the all-players-own-a-weapon decision.

## State design

Add a small shared module, tentatively:

- `android/app/src/main/cpp/shared/coop/coop_powerup_duplication.c`
- `android/app/src/main/cpp/shared/coop/coop_powerup_duplication.h`

The module should maintain a sparse set of successful collection relations:

- object identity: local object index plus object signature and powerup ID
- player identity: `client_id` when available, otherwise normalized callsign

Numeric player slots are only transport-time lookups. When a local or remote
pickup event names a player slot, resolve it immediately to the stable identity
and store that identity in the tracker. This makes reconnect, player-slot
reuse, coop restore remapping, and host migration deterministic.

The tracker should validate the object index, signature, type, and powerup ID on
every read or update. A signature mismatch means the object slot was reused and
old relations for that object must not apply. Do not extend the packed `object`
structure or its read/write variants.

Use dynamically sized, bounded storage for the sparse relations rather than a
fixed eight-player mask. There must be no silent truncation. Allocation or
format-limit failures must stop the save or snapshot operation with an explicit
error instead of dropping collection records.

Provide narrow operations for:

- checking whether the option is active
- checking whether an object is eligible by source powerup ID, including
  exclusion of player spew and all weapon objects
- checking whether the local player may collide with a pickup
- recording a successful local collection
- applying a validated remote collection event
- deciding whether the pickup is visible or eligible as a local objective
- clearing all state at level lifecycle boundaries
- resolving current player slots to stable pickup identities
- exporting and importing every sparse relation for synchronization and save
  metadata
- replacing the live tracker atomically after a validated load

## Host option and configuration flow

Add a dedicated `ubyte` field such as `DuplicateEnergyShields` to both D1 and D2
`netgame_info` structures. Serialize it next to the existing branch-owned
`FullDeathSpew` and `PlayerSpewNoExpire` fields in full netgame packets and
player-file defaults. Do not add it to `game_flags`.

Propagate a `duplicate_energy_shields` or `duplicateEnergyShields` value through
the same paths as `coop_qol`:

- native D1 and D2 advanced netgame options
- `net_udp_android_autonet_shared` and `auto_net`
- `jni_main.c` auto-host arguments
- launcher create-game UI and saved host defaults
- LAN lobby and matchmaking metadata
- resume preferences and launch intents
- configuration import/export
- host migration metadata

The exact visible label is `Energy and shields duplicated for each player`.
Keep the preference independent from the existing umbrella coop QoL option, but
apply it only when `GM_MULTI_COOP` is active.

Persist the host's last explicit selection as the default for the next hosted
game:

- add `host_duplicate_energy_shields` to `HostGameDefaults.Defaults`, `load()`,
  and `save()`, with false only when the preference has never been written
- save the selection when either LAN or matchmaking host creation is confirmed
- include it in configuration export/import and multiplayer resume records
- add the matching native player-file/netgame default field in both D1 and D2
  for hosts that use the native advanced netgame menu
- carry it through host migration so a migrated host does not fall back to its
  unrelated local preference

Loading a coop save restores that saved session's setting for the resumed game,
but does not overwrite the host's persisted future-game default. Only an
explicit host-setting change updates the persisted default.

Because JNI signatures and Kotlin declarations are positional, update all
declarations and callers in one tranche and add a launcher unit test that
round-trips the new value through defaults, resume data, and lobby JSON.

## Gameplay and rendering hooks

In both D1 and D2:

1. Check the coop toggle first. If disabled, execute the existing function
   without collection-state reads, writes, packets, or render filtering.
2. Classify the source object before `do_powerup()`. Only a non-spewed
   `POW_ENERGY` or `POW_SHIELD_BOOST` can select the new path.
3. Before `do_powerup()`, return without applying it when the local player has
   already collected that eligible object under the new option.
4. After `do_powerup()` succeeds for an eligible energy or shield pickup,
   record the collection and send the new collection event.
5. Skip the existing `OF_SHOULD_BE_DEAD` and `multi_send_remobj()` calls only
   for a successful duplicated energy or shield pickup. Leave every other
   powerup on the current path, including energy and shields marked as player
   spew and weapons that happen to award energy.
6. Add a small early visibility check in the common object rendering path so a
   locally collected pickup is not drawn.
7. Apply the same eligibility check to automap powerup display and D2 guidebot
   energy/shield goal selection. Check any Android route/objective overlay that
   enumerates powerups directly.

The shared policy should decide whether the special path applies. This keeps
the D1 and D2 source edits limited to short Android hooks.

Use `OF_PLAYER_DROPPED` as the common player-spew exclusion. Audit D2's
`PF_SPAT_BY_PLAYER` path and either require it as an additional exclusion or
ensure that every network-visible player-spat object also receives
`OF_PLAYER_DROPPED`. Prefer one canonical marker across D1 and D2 if this can be
done without changing unrelated pickup behavior.

## Network synchronization

Add a fixed-size Android multiplayer command, tentatively
`MULTI_COOP_POWERUP_COLLECTED`, to both command tables and dispatch switches.
Its payload should contain:

- remote object number
- network object owner
- collecting player number
- expected powerup ID
- enough object identity data to reject a stale mapping

Sending uses `objnum_local_to_remote()`. Receiving uses
`objnum_remote_to_local()` and then validates the mapped local object and its
signature/type/ID before recording the collecting player's stable identity. Use
the reliable/high-priority path already used for important gameplay state.

The receiver must reject malformed object numbers, owners, player numbers,
disconnected player claims, non-powerup objects, and powerup IDs other than
energy or shields. Confirm whether the multiplayer dispatcher exposes the
sender identity; if it does, require the claimed player to match the sender.
When the synchronized netgame option is false, receiving either a collection
event or snapshot must not mutate tracker state.

Add a sparse state snapshot for:

- a player completing initial object synchronization after joining
- reconciliation after packet loss
- a new host taking over

The authoritative snapshot should be keyed by network object identity rather
than local object index and must carry stable player identities for historical
entries whose players are not currently connected. Keep normal collection
events small; do not send the entire tracker on every pickup.

Base new command values and lengths on the updated multiplayer command tables.
The implementation must preserve D2's 70-byte `MULTI_SHIP_STATUS` declaration
and account for both the weapon change and the new commands in any multiplayer
protocol-version update.

## Lifecycle, save, and restore

Wire resets and persistence explicitly:

- clear all tracker state before or during a new level object load
- do not clear collection history merely because a numeric player slot is
  reused; the new slot resolves to a different stable identity
- retain local state across ordinary death and respawn
- include every sparse tracker relation in the Android coop save trailer
- restore the saved option value along with tracker state
- bump the coop metadata version and introduce length-delimited framing
- restore tracker state after saved objects and network mappings are available
- include the same state in the in-memory save path used by rewind and level
  restart
- retain or resynchronize it during host migration

Use object signatures plus saved object identity when restoring. For a
new-format save that declares the feature enabled, a missing, truncated, or
invalid pickup section is a restore error, not an empty collection history.

## Lossless coop save format

Evolve the coop trailer to a framed version instead of enlarging and writing the
raw `coop_save_metadata` struct:

1. Keep the base D1 or D2 save data unchanged.
2. Write a versioned coop payload containing the existing player metadata, the
   saved toggle value, and a pickup-state subchunk.
3. Write a fixed footer immediately before the optional Android metadata. The
   footer contains a distinct magic value, format version, coop payload length,
   and checksum.
4. On memory saves, the same footer is the final data in the buffer. On disk
   saves, the existing Android metadata remains final and the coop footer is
   located immediately before it.
5. Read the footer first, validate its length against the file boundaries, then
   validate the checksum before parsing any records.
6. Require the new coop trailer version. Existing coop saves are intentionally
   unsupported and should produce a clear version error rather than guessing
   pickup history or silently loading altered state.

Use explicit little-endian field writers and readers. Do not persist compiler
padding, pointer values, local object-owner arrays, or an in-memory C struct
image for the new payload.

The pickup-state subchunk contains:

- subchunk tag and version
- exact relation count
- for each successful collection:
  - saved object index
  - object signature
  - expected powerup ID
  - player `client_id` length and bytes
  - normalized callsign length and bytes as fallback

One record represents one successful collection of one eligible object by one
stable player identity. Duplicate records are rejected or canonicalized during
validation, but never applied twice.

The subchunk is present even when the feature is disabled. In that case the
saved toggle is false and the relation count must be zero. A disabled save with
nonzero relations is invalid, since accepting stale records would make later
runtime behavior ambiguous.

The serialized object index is a fast-path locator. After base objects load:

1. Check that the saved index still contains an object with the saved signature,
   `OBJ_POWERUP` type, and expected energy or shield ID.
2. If the index changed, scan loaded objects for the unique matching signature
   and validate its type and ID.
3. Reject ambiguous signatures, invalid IDs, player-spew objects, and entries
   that exceed validated file boundaries.
4. Build a temporary tracker from the complete subchunk.
5. Swap the temporary tracker into live state only after every record and the
   checksum pass. A failed load must abort the overall restore rather than
   partially applying pickup history.

The base save already captures whether an object exists. Together, the base
objects and coop subchunk preserve the full point-in-time result:

- an eligible energy or shield object remains in the base object list
- the subchunk records exactly which stable players consumed it
- a consumed player does not see or collect it after restore
- an unconsumed or genuinely new player can still collect it
- consumed player spew is absent from the base object list and has no tracker
  record
- unconsumed player spew remains an ordinary globally single-use object

Change metadata writes to return success or failure all the way through
`state_save_all_sub()`. Do not report a successful save if allocation, trailer
write, checksum, or close fails. Preserve the previous disk slot until the new
save is complete by using a temporary file and atomic replacement if the
existing save path does not already provide that guarantee.

Both normal coop saves and the rewind or level-restart memory buffers call
`state_android_write_save_metadata()`, so they must use the same serializer and
parser. Add byte-for-byte parity tests between disk-backed and memory-backed
coop payloads.

After the host restores, publish the validated tracker snapshot after base
object synchronization and player identity remapping complete. Peers replace
their tracker atomically and acknowledge the snapshot generation before
gameplay resumes.

## Diagnostics and introspection

Add concise `DLOG_NETWORK` or `COOPLOG` entries for:

- a successful local per-player collection
- a rejected malformed collection packet
- snapshot send/apply counts
- tracker restore/reset events

Extend introspection with normalized data for qualifying powerups:

- local object number and signature
- powerup ID
- collected stable player identities
- whether it is visible and collectible for the local player

This enables automated checks without image analysis.

## Test plan

### Focused native policy tests

Extract the signature and stable-identity relation rules into dependency-light
functions and cover:

- option inactive outside coop or when disabled
- disabled mode does not allocate or consult tracker state and serializes zero
  relations
- one successful collection per player
- one player does not block another
- player-spewed energy and shields are ineligible for duplication
- a weapon object that grants duplicate-weapon energy is ineligible for the
  per-player tracker and remains globally single-use
- object slot reuse resets stale state
- player slot reuse does not inherit another identity's history
- the same client identity retains history after reconnecting in another slot
- invalid object/player indices and IDs are rejected
- sparse snapshot round-trip and bounded malformed input
- disk and memory save serialization are byte-for-byte equivalent
- truncated, oversized, duplicate, ambiguous-signature, bad-checksum, and
  allocation-failure cases never partially apply state

Run the same policy tests against D1 and D2 constants where they differ.

### Launcher tests

Extend multiplayer configuration tests to verify:

- default is off
- changing the host setting to on makes on the next hosted-game default
- changing it back to off persists off
- config export/import round-trips the value
- LAN and matchmaking JSON preserve the value
- resume and host migration records preserve the value
- missing fields from older local launcher data safely default off
- loading a coop save uses the saved session value without changing the
  persisted future-game host default

### Two-client integration coverage

Add maintained scripts under `android/game_scripts` and use introspection to
verify in both D1 and D2:

1. Toggle off: player A collects energy or shields and the object is removed for
   both players through the same removal packet and object-death path as the
   current baseline.
2. Toggle on: player A collects it, gains the resource, and cannot collect it a
   second time.
3. Player B still sees the object, collects it once, and gains the resource.
4. A player already at maximum does not consume its personal copy.
5. When every player owns a weapon, its duplicate grants the existing energy
   bonus and is removed for both players even while the new toggle is enabled.
6. When any player lacks the weapon, the existing weapon remains available and
   the new energy/shield toggle does not change that result.
7. A player-death-spewed energy or shield collected by player A is removed for
   both players and cannot be collected by player B.
8. A non-player-spewed dynamic energy or shield pickup, such as a robot drop,
   maps correctly and remains once-per-player.
9. A late joiner can collect an eligible pickup that existing players already
   collected, but cannot recover player spew that was already consumed.
10. A reused player slot starts with clear collection state for its new stable
    identity and does not leave stale ownership data that changes the
    duplicate-weapon decision.
11. Coop save/restore and rewind restore every collection relation from the
    saved point.
12. Host migration retains the toggle, collection state, and synchronized
    weapon ownership.
13. A reconnecting client in a different slot retains exactly its saved pickup
    history, while a new client reusing the old slot does not inherit it.
14. Corrupt or truncated pickup metadata causes a clear restore failure and
    never silently resets collection history.
15. A disabled save writes false with zero relations, clears stale tracker state
    on load, and retains globally single-use pickup behavior.

## Verification

After implementation:

1. Run scoped code quality over every changed C, header, Kotlin, and test path.
2. Run focused native policy tests and Android launcher unit tests.
3. Run the D1 and D2 two-client integration cases.
4. Run the repository Windows host build through `run-windows-build.ps1`.
5. Build the Android debug APK with JDK 21 and run the relevant integration
   suite on emulators.
6. Record exact commands, pass/fail results, and any deferred cases in this
   plan file.

## Main risks

- Object identity can diverge for dropped powerups. Always use existing network
  mapping and validate signatures instead of assuming local indices match.
- Treating all dynamic objects alike would duplicate player death spew. The
  eligibility check must distinguish `OF_PLAYER_DROPPED` player spew from robot
  drops and other eligible dynamic powerups.
- Looking only at the effect of `do_powerup()` would misclassify a duplicate
  weapon that awards energy. Classify the source object before applying it.
- Reworking ship-status synchronization could regress the all-player weapon
  ownership rule. Preserve its broadcast scope, D2 high weapon bits, and
  updated message length while adding separate per-object state messages.
- Player slot reuse can incorrectly inherit a departed player's collection
  history. Store stable client identity, not the slot number.
- A render-only filter is insufficient. Collision, automap, guidebot, and
  Android objective enumeration must share the same eligibility policy.
- Saving only the live object leaves the powerup present but loses the
  per-player history. Persist every stable identity relation in both disk and
  memory saves and treat incomplete new-format state as an error.
- Changing positional JNI or lobby data without updating every caller can
  silently apply the wrong host setting. Round-trip tests are required.
- Existing untracked gameplay plan files are user work and must remain
  untouched during implementation.
