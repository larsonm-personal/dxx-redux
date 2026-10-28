# Delayed nonplayer object chains

Native/source and Android acceptance pass within the scope below. Latest-head
CI is required before this tranche is considered ready for review

## Behavior and deliberate reference corrections

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae,
main/trigger.cpp SHA256
74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852
(the pinned CRLF working-tree bytes)

The actual DoMasterTrigger propagates bObjTrigger through its linked geometry
children. The geometry frame loop calls Countdown(false), however, and the
stored record has no corresponding context bit. Operate also uses bObjTrigger
to choose a trigger-array index, even for a geometry child. These are not
faithful lifecycle semantics to reproduce: the port already uses the real
geometry child's index, and now retains the event context across its delay

Each pending wall action owns the first activating object's index and signature,
shot flag, selected delay and attached-event context. Nested masters retain that
context, including across their own independent delays. Removal of the source
object does not erase authored geometry/resource work. A teleport requires the
same live actor; a missing owner or recycled slot produces no motion and no
destination RNG, while attached-event post-action flags still advance. An
ordinary XL wall timer whose actor was replaced is consumed without acting on
the replacement. Redux remains on its existing runtime alternate

The retained context and signature checks are intentional corrections. The
reference retains a reusable index and loses context on geometry countdown;
tests do not claim those unsafe cases match a full XL engine run. Defined delay
selection, fixed-point boundaries, first activation, disable/enable suspension,
permanent rearming, alternate/one-shot semantics and link order remain based on
the pinned functions

Development trailer schema 6 adds a bounded actor table to the existing XL
event/boss/equipment payload. It replaces earlier development schemas directly.
Capture/restore uses the same state for disk and rewind. All rows and other
feature state validate before publication; stale historical identities are
valid pending events, not a reason to bind them to current replacement objects

## Native acceptance

The derived objdelay mission uses unchanged original Neptune geometry as its
source and six authored wall actions. A robot's removal master schedules a
master, an alternating illusion, an object teleport, another delayed master,
shield damage and percentage energy drain. The runtime test verifies:

- Actual removal dispatch, slot reuse and geometry/resource execution after the
  original actor is gone, without moving the replacement
- Independent child deadlines and nested context, exact before/at boundaries,
  alternate behavior, live object-teleport orientation/velocity retention,
  first-actor ownership, suspension and permanent rearming
- Pending removed-owner disk restoration through stock, including its original
  remaining deadline and signature; ordinary XL wall slot-reuse cancellation
- Five malformed in-memory states and ten recomputed-checksum malformed actor
  trailers rejected without publishing an earlier valid state mutation
- 140 timing samples compared with the actual extracted Arm/IsDelayed/Delay/
  Countdown functions: 130 defined cases and ten INT_MIN safety controls. This
  compares timing, not the intentionally corrected missing context

The existing source-bound master oracle also checks attached link order and
actor arguments. The focused inventory, wall-chain, scheduler, object-event and
new delayed-chain tests pass. All 25 focused feature regressions pass in 123.63
seconds, including boss/equipment state, water/property controls and stock
isolation. Linux GCC also passes the 140 timing cases with -Wall -Werror. All
12 private-policy tests and both catalogs pass (397 top-level entries and 452
support scripts). The private receipt now requires the delayed-chain test

Fixture identities (not distributable original mission qualification):

- Original Neptune HOG:
  4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303
- Derived objdelay.hog:
  fb223c50cf0a39402243950bfcfb036db2393ff93c2a0f0b4ab3aaedc0d16d61
- Derived LF objdelay.mn2:
  fb3dbe5cf205c97b089a69c898ab76a5e478776a6d7d4d811bab88b8de056e25

## Android acceptance and limits

The reusable test_xl_object_chain_delays_runtime runner exercises lethal combat,
a pending-event checkpoint, disk through a different-difficulty stock mission,
delayed completion, a completed-state checkpoint and restoration of the original
pending disk save a second time. It backs up and restores protected app files
and checks their hashes. All 45 steps passed on emulator-5554 in 186220 ms,
run b26a6c34e68e4fbf80263be771297309, with matching protected-file hashes. APK
SHA256: 4d8f6db3ba9bac07dd68ec6b6a73d25603da216e0fbd59d8c02333721ad10592.
The final state has shields 97, energy 95, the illusion action alternated to
type 6, all pending actions consumed/rearmed and no teleport destination chosen
for the removed owner. The saved historical actor signature is 50 at every
observed pending checkpoint/disk assertion

This removes the delayed-descendant gate only for otherwise implemented actions
and admitted owners. Player owners, direct unsupported object action types,
expanded resources/capacities and unrelated mission requirements remain gated.
No original mission or campaign gains qualification from these derived controls
