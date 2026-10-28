# XL trigger capacity and byte identities

XL now admits up to 254 wall triggers, with IDs 0 through 253 preserved through
native loading, wall dispatch, master chains, scheduling, saves and metadata.
Stock and imported D1 retain their 100-trigger limit in the shared binary. Wall
records keep their packed byte layout; consumers interpret 255 as missing and
all other bytes as unsigned identities

## Reference and implementation

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae

- include/trigger.h: 8f539027666ce768f467226381e82fd62c07a3cd3642fe97f857980091cf4dfd
- main/wall.cpp: 05eaa2cab8dd7f5661afd210e0edc8dea4fcd2bb2075c00d3012bec395ab0c42
- gameio/savegame.cpp: 4784770058b2d4880c33a81eb1a7611738959113a80bce48de248eee7a54a795
- gameio/loadobjects.cpp: 01fe3cbc1cf781d1180921c9e235593f1b79d68924c0d49a42502ef8c3ffcd3c
- main/gamedata.cpp: 782acd85fae6fa5fbb0162a9f05474ae449063d70bf566d172c9d47ae4ed1a12

The reference dynamically allocates declared trigger rows during loading, but
its saved wall and object tables are bounded to 254. MAX_TRIGGERS and
MAX_OBJ_TRIGGERS are both 254. The extracted CWall::Read and
CSaveGameManager::ReadBoundedInt functions independently establish byte decoding
and inclusive count bounds. All 1,037 cases pass under MSVC and strict GCC

The native trigger array and XL scheduler use the wider physical capacity.
Only the validated XL loader grants temporary permission to exceed 100; the
permission clears on success, failure and mission cleanup. Imported D1 arrays
and validation explicitly retain 100. Metadata snapshots now accept 254 triggers
and 2047 walls, matching the implemented table capacities. Both full counts are
asserted in their native integration snapshots

Saved counts are checked before packed rows are read into temporary storage.
An XL count must also match its authored table. Negative, oversized, mismatched
and truncated tables reject before trigger installation. Other legacy save
sections may already have loaded: this does not claim whole-world atomic rollback

Object-action admission now uses the same 254 bound, correcting the previous
255-row allowance. The existing source sorting controls cover the corrected
endpoint; rejecting 255 leaves the existing object table intact. Other object
actions, owners and object allocation capacity retain independent requirements

The legacy activation mask remains available for IDs below 64. A per-ID
activated_since_load diagnostic covers the full table and clears on level load;
it is deliberately distinct from persisted scheduler state. Android wall rows
also expose normalized trigger and controlling-trigger identities

## Native acceptance

The public xl_inventory_contract tests 0, 1, 99, 100, 101, 127, 128 and 254 rows
in 15/33, 17/36 and 23/40 layouts. Translation retains row contents and byte IDs
99, 100, 127, 128 and 253, plus sentinel 255. Count 255 and invalid reference 254
reject with specific reasons

The private xl_trigger_capacity integration derives a table from the existing
close-door fixture on original Neptune geometry. It retains vertex identities
and connectivity, uses the same 30 walls and supplies 254 rows while retaining the original object-action trailer. Unused rows are
inert light-on actions with no targets. Its authored actions cover:

- Wall dispatch at IDs 99, 100, 127, 128, 200, 252 and 253
- Immediate and delayed masters resolving high target IDs
- High-target disable/enable, alternate type changes and one-shot flags
- Object enable/disable links to IDs 128/253, with missing 255 and invalid 254
  skipped before indexing; synthetic controls reload the authored object binding
  before the subsequent source-based persistence checks
- Actual paired cloak completion and physical/visibility ray behavior
- Pending disk save through stock, remaining deadline, actor and earned energy
- Four malformed XL saved tables and three stock controls, each followed by
  successful recovery from a valid save
- A metadata snapshot containing the complete trigger table
- Actual high-ID crossing recording, playback through check_trigger and
  byte-preserving demo rewrite
- Read-only rejection of count 255, invalid wall reference and truncated source
- An unowned translated level rejected by the stock 100-trigger loader guard

The demo control covers the classic trigger event carrying segment/side/object/
shot, which resolves the high wall identity during playback. It does not establish
complete XL demo support for every action or owner

The stock D2 compatibility suite and existing wall-chain, scheduler, object-event,
delayed-object-chain, trigger-flag, wide-wall and cloak integrations also pass.
Object-event source comparisons retain 2,184 event samples, 56 sorting cases
through 254 and 720 teleport/RNG samples

Original Neptune HOG remains
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303

Derived fixture hashes:

- HOG: 7ee5cd2029d1f24990d5d882f1dcd35fd084e439352c5c942a14b858c51f0dbf
- Descriptor: e843e80a0e56225d10a6a71510a6b637039fa6d3915748b1e40d39cae85c85b0

## Android acceptance

The owned test_xl_trigger_capacity_runtime.ps1/JSON integration is registered
with a 600-second master timeout. Final formatted-APK run
6efd1d1ffad24c12b1e195bff637fbd9 passes all 77 steps in 70508 ms, following
the earlier 77-step pass 3b043b9a8d3e42ce9f055650103b6b38 in 71447 ms. It checks high-ID wall dispatch, actual controlled portal
crossing, alternate/one-shot state, delayed masters, memory checkpoint recovery
and pending disk restoration through a stock mission. The runner restores all
protected app files and verifies matching before/after hashes

The automation fire_wall_trigger field resolves a wall's authored segment/side
through check_trigger instead of calling a trigger row directly. The separate
physical crossing additionally verifies ordinary movement dispatch. No autonomous
level navigation or full-mission playthrough is claimed

## Original missions and remaining work

The [original audit](trigger-capacity-originals.json) freshly hashes the two
surveyed high-trigger members and runs current admission without modifying them.
Dinter has 246 wall triggers and 13 remaining blocker categories. Antholgy has
103 and five remaining categories. Trigger capacity is no longer a blocker;
neither original is newly qualified as playable

Remaining object/light/door capacities, unsupported actions and owners, other
layouts, resources, settings, effects, campaign interactions and full demo support
remain separate work. This batch does not establish multiplayer synchronization

Scoped mixed formatting and explicit pinned PowerShell analysis pass. Both required
automation catalogs pass: 99 standalone JSON tests, 467 support scripts,
298 standalone PowerShell tests and 411 master entries
