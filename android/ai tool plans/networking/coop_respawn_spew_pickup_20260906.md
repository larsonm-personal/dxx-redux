# Coop respawn spew pickup

Host log 215106 and client log 215048: Castaway level 5 restored from coop slot 6, client killed around 21:54:51, then unable to collect spew

## Confirmed cause

Fresh-game client death, respawn and physical homing pickup passed. Repeating after a synchronized save restore with a nonempty recovery ledger failed. Targeted diagnostics showed client epoch 4 versus host epoch 3. Each peer advanced its local epoch during restore; differing local reset histories left them inconsistent. A nonempty ledger prevents implicit epoch adoption, so the client rejected new host rows and visible flagged spew remained unbound. Wrong-epoch pickup requests are also rejected by the host.

## Fix

Carry a host-selected 32-bit recovery epoch in the Android save-transfer BEGIN packet. Scope that epoch over state loading so repeated local resets use the same value. Use the same path for restore, rewind and level restart. Clear queued pre-restore rejoin inventory before applying the authoritative save. Keep ownership, revision, life, capacity and duplicate-grant checks intact. Bump Android protocol versions together; desktop protocol and save formats are unchanged.

## Validation

- [x] Instrument binding/request/arbitration and reproduce the failure
- [x] Fresh D2 client death/respawn/physical pickup baseline passed
- [x] Restored D2 session with a nonempty ledger failed before the fix
- [x] Implement synchronized epoch and remove collision-spam diagnostics
- [x] Add paired physical pickup automation and native stale/duplicate request regression
- [x] Scoped quality passed (mixed-language invocation, then PS-only recheck after strengthening assertions); D1/D2 recovery harness passed, plus 10 scoped CTest cases
- [x] Android debug APK build and Windows D1/D2 builds passed
- [x] Paired D2 restored-session physical pickup passed after fix (`temp/spew-pickup-restored-fixed.log`)
- [x] D1 fresh physical pickup passed with matching epochs and bounded ammunition totals (`temp/spew-pickup-d1-fixed.log`)
- [x] D2 repeated in-game rejoin recovery passed: gear returned once, collected items excluded, no duplicate world spew after two process restarts (`temp/spew-recovery-d2-fixed.log`)

Reproduction command: `android/tests/test_lan.ps1 -Game d2 -SkipBuild -GuidebotSlotRemapRestore -SpewPickup`. The combination seeds an absent player's recovery credit, saves, restarts with swapped host/player slots, restores, kills the client, respawns, and approaches the spew using real collision/network pickup. `-SpewPickup` alone covers a fresh session for either game.
