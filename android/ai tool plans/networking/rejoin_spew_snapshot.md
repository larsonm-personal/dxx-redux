# Rejoin spew snapshot cleanup

- [x] Compare both build 25013 exported Network logs
- [x] Exclude the returning player's frozen recovery objects from their initial world snapshot
- [x] Include energy/shield spew in recovery cleanup, preserving existing saved resource amounts
- [x] Extend migration coverage to prove the snapshot exclusion and physical cleanup
- [x] Format, build both engines, and run relevant integration checks

## Evidence

`debuglog_20261012_223520.txt` records the host reclaiming six concussion missiles
at 22:36:23.310-323, with valid object bindings. This happens after object transfer.
`debuglog_20261012_223513.txt` records the returning client receiving those ledger
rows as TAKEN with remote_index=-1 and no prior local binding. The ledger cannot
remove a snapshot copy in this state. The analogous reverse join reclaims five
missiles; one was already consumed. Identity matching and host-side reclamation
are working. Energy/shield eggs are explicitly excluded from the ledger.

Do not change player identity matching, cross-level reclamation, or saved ledger
persistence. Keep the world objects on the host until join commits, so cancelling
a join cannot destroy the player's loot. Exclude only that returning player's
frozen objects from the snapshot; do not depend on subsequent removal traffic.

Energy/shield eggs now get cleanup-only ledger rows. The absent player record
continues to supply energy/shields, without an additional orb bonus. These rows
use the existing persistence and cross-level reconciliation paths. The migration
test requires both orb types to be tracked before rejoining and no recovery
objects on either peer afterward, along with conserved missile inventory.
The user confirmed this policy: restore saved amounts and remove orbs.

## Validation

- ARM64 D1/D2 CMake targets and the x86_64 debug APK built successfully
- D1-in-D2 level 8 and native D1 level 8 `test_lan.ps1 -HostMigration` passed
  both host swaps and rejoins, early SYNC/end handling, object parity, missile
  conservation, and no remaining recovery objects on either peer
- The native D1 exported Network log records snapshot exclusions for concussion
  powerups 10/11 and energy/shield powerups 1/2
- Both engines' native recovery harness passed, including a new saved-orb test:
  preserve energy 137 and shields 64, retain host objects until commit, exclude
  only the returning owner's objects, and remove old orbs after reclaiming on
  another level and later restoring their original world
- Scoped formatting, both automation catalog validators, and `git diff --check`
  passed

Stable local output: `temp/rejoin-spew-{build,arm64,d1,d2,native,quality}.log`
and `temp/rejoin-spew-receiver.txt`. Real-device confirmation remains useful;
these fixes do not retroactively add ownership records to older untracked orbs.
