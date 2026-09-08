# Simplify coop spew recovery

## Goals and assessment

Return the player's remaining equipment, including eligible uncollected expired drops, without keeping a second copy in the mine or returning someone else's collection. Preserve ordinary pickup behavior.

The current implementation exceeded that scope. coop_recovery_pickup intercepts normal collision processing, grant_pickup computes and applies its own inventory changes, and coop_recovery_frame orders and replays remote inventory deltas. Receipt revisions consequently enter ship-status packets, death packets, save metadata and rejoin handling. The missing sound/flash was one consequence of bypassing do_powerup; HUD, auto-selection and other normal pickup behavior also need auditing if that architecture remains.

Useful existing pieces: stable player identity, linking drops to their owner, exact remaining quantities, distinguishing a living inventory from an already-dropped inventory, save/world generation fencing and the existing rejoin inventory restore. These should survive the redesign.

## Recommended boundary

Recovery tracks dropped objects; it does not implement routine inventory collection.

1. Normal collisions run the existing do_powerup path, including its sound, flash, HUD and selection behavior. Record the actual collection result after it succeeds.
2. Track only owner, stable drop/object identity, recoverable contents remaining, and whether those contents are in the mine, retained after allowed expiry, or consumed. Removal by collection must never be mistaken for expiry.
3. A collection consumes its recovery entitlement. A partial collection updates remaining contents. A refused collection changes nothing. A collected weapon/ammo cannot later be returned to its former owner.
4. On rejoin, use the existing inventory restore and add only still-recoverable contents. Remove the corresponding world contents and consume stored expiry contents as part of the same recovery operation. Repeating the operation must produce the same result, not another award.
5. Keep save/restore and host migration consistent with that object tracking. Use the shared world generation already introduced to fence stale object updates; do not revive per-pickup inventory revision chains just to solve object identity.

This should remove REC_GRANT, apply_grant, grant replay ordering, the duplicate live-pickup inventory implementation, and pickup-receipt dependencies in death and ship-status traffic. Inventory math needed specifically to restore a player still belongs in the existing restore path.

## What must be proven before replacing it

The existing object-removal message is too weak by itself. It marks an object dead but does not convey a partial-ammo result, a removal reason, or a rejoin boundary. D2 powerup.c already warns that unsynchronized partial Vulcan/Gauss ammo collection can duplicate ammo. Merely deleting the recovery collision hook is unsafe.

Prototype a short recovery boundary around rejoin: participating peers stop collecting the affected owner's drops, flush/acknowledge preceding collection updates, then apply the host's reclamation before resuming. Ordinary pickup should not incur a host round trip. The protocol must resolve collection versus reclamation in one order on every peer. A timer alone is not evidence that no collection happened. If a peer disconnects during this boundary, preserve unresolved state for a safe retry instead of guessing that it is recoverable.

Implemented a per-drop freeze before object synchronization. Replies contain the remaining contents themselves, so an acknowledgement overtaking an ordinary collection report does not lose that collection. Repeated freezes intersect with the client's already-reduced contents. No host authorization round trip is added to ordinary pickups.

A missing acknowledgement is not treated as permission to refund. A participant disappearing during a freeze leaves it pending; the code neither clears that participant's bit on timeout nor grants the uncertain contents. Save metadata is deferred while a host freeze is incomplete. This favors preserving ownership over guessing an inventory result during a partition; recovery may require the participant/session to resume.

Also explicitly cover multi-pack capacity behavior, Vulcan/Gauss ammo remaining in a gun, Omega charge, laser upgrades, armed mines consuming drop inventory, repeated deaths, and the configured expiry policy. Preserve normal pickup semantics; any loss of uncollected recoverable quantities needs an explicit solution, not silent discard.

## Migration sequence

- [x] Survey interception, grant application, object removal, partial ammo, expiry and rejoin code
- [x] Document unnecessary coupling and recommended object-tracking boundary
- [x] Prototype and native tests for collection/rejoin ordering, duplicate freeze retries and remove-before-result
- [x] Implement drop provenance and remaining-content updates from normal pickup outcomes
- [x] Switch rejoin/expiry recovery to consume those records once
- [x] Remove grant replay, inventory deltas, receipt rows and death-packet receipt serials. Remaining serials fence whole inventory restores only. Android protocol D1 30021/D2 30022; metadata v8/progress inventory v4
- [x] Validate D1/D2 drop tracking and representative engine/network paths (details below)

## Completed validation

- D1/D2 native drop tracking tests pass: recovery, partial quantities, repeated rejoin, host remapping, stale/duplicate reports, freeze ordering/retries and removal reasons
- First Android tag build passed; paired D2 process-loss/repeated-rejoin test passed twice (`temp/spew-tags-rejoin.log`)
- Scoped mixed-language formatting/lint passed
- Final Android debug APK builds passed; both Windows game targets built successfully without interrupting the unrelated headless simulation run
- Ten scoped CTest networking/coop/save-policy cases passed; D1/D2 recovery harness passed after the final engine change
- Paired D2 save/restore with swapped host/player slots, ordinary physical pickup and real freeze acknowledgement passed (`temp/spew-tags-restored-pickup.log`)
- Paired D1 partial pickup through the engine collision/pickup function passed: inventory 9 -> 10, exactly 3 homing missiles retained on both peers (`temp/spew-tags-d1-partial.log`)
- The partial fixture moves death spew away from spawn to prevent incidental pickups, then targets the four-pack directly. Ordinary physics-driven pickup remains covered by the D2 test
- Initial integration failures exposed test setup mistakes: set_debug ignores post_delay_ms, and D1 spawn-area pickups could consume the target before the partial test. The scripts now wait on actual freeze readiness and isolate the partial-pickup target
- All changed code/scripts passed scoped formatting/lint; final git diff whitespace check passed

The new co-op save metadata is v8 and progress inventory v4. Prior v7/v3 co-op recovery data is deliberately unsupported under the repository's pre-release format policy; use a fresh co-op session/save with this build. Both Android peers must update.

Coverage limits: physical pickup/rejoin/save-restore were exercised on emulators; expiry, object remapping on host migration, and adversarial packet ordering were exercised by the native harness. Separate end-to-end rewind/level-restart and host-loss-during-freeze runs were not performed. Missing freeze acknowledgements remain pending rather than refunding uncertain contents.

The temporary feedback helper has been removed with the grant engine. do_powerup supplies sound, flash, HUD and weapon-selection behavior again. Existing opt-in gameplay duplication policies remain gameplay policies; the recovery layer does not turn ordinary pickup networking into a general anti-cheat or transaction system.

