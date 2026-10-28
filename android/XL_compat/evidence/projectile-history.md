# XL projectile hit history and impact lifetime

XL now has its own bounded persistent-projectile hit history and impact lifetime
path in the shared binary. Redux keeps its existing bitmap suppression, reactor
damage multiplier and persistent-projectile behavior. The later [object-capacity tranche](object-capacity.md) adds source-sized XL pools up to
32768 slots; Redux retains 1000

Source/native and 53-step Android acceptance pass within the scope below.
This does not qualify an original campaign or complete XL combat/AI fidelity

## Reference semantics and explicit corrections

Reference: arbruijn/d2x-xl 2576a98b5de310e3ac7c9777df2677d1627b8bae

- physics/collide.cpp: d10f88f82e5d283077914881d1acb719f06785a86be22e50617d92d6fce47bc9
- include/object.h: ed546edd2bbda56a84e9fef8543706707d47a08b48e36616852346b5a2f96127
- objects/objectio.cpp: 528f64e09469b5a452e9c6cf0ae8e9d5dd3eaffeaf5478378669f0783deaf68f
- objects/object.cpp: e71b0a906cd686549c783563ab93365d1bbf5cf05f08c1971ef08c7116c67a64
- include/fireweapon.h: 894ca0c457a70156e159f7175ca7cf321677c6b04a02525c7c23ed181371ceb4

The source stores 20 victim slot IDs per projectile. Robot/player persistent
contacts register a victim and reject repeated contacts before damage/effects.
Reactor contacts have neither this suppression nor Redux's doubled persistent
damage. XL impact lifetime subtracts one quarter of a nonpersistent target's
remaining shields, or one half for weapon targets. Persistent targets do not
weaken the projectile. Mines, including robot smart mines, die on impact; other
nonpersistent projectiles also die. The caller ordering matters: robot/reactor
attrition follows damage, while player attrition precedes damage

Two intentional corrections are explicitly outside source parity:

- At capacity the reference uses overlapping memcpy, shifts the history right,
  duplicates its oldest victim and overwrites its newest entry. This operation
  has undefined behavior. The port deterministically retains the latest 20
  distinct accepted hits by evicting the oldest entry. Source comparisons stop
  before undefined copies; an independent sequence model checks the correction
- The source writes only the latest victim and restores a one-entry history.
  The port preserves the complete earned order across disk and memory saves so
  loading does not grant repeat damage against previously hit targets

Victims deliberately retain source slot identity, even after victim deletion or
reuse, until evicted. Projectile ownership instead includes its live signature;
allocation, deletion and level cleanup discard obsolete owner state. The packed
Redux last_hitobj field remains the latest victim ID, not the XL history count

## Persistence and isolation

Development schema 11 replaces schema 10 and adds an ordered projectile table.
Rows identify a live weapon/control owner and its signature, with 1-20 unique
bounded victim IDs. The last victim must match the core object field. Every live
weapon with a recorded hit must have exactly one row. Victims need not still be
alive. All rows and the other XL components validate before table installation

Malformed identities, overflow, booleans, duplicates, missing rows, empty or
oversized histories and inconsistent last victims reject. The enclosing source
identity and checksum still apply. Saving also checks completeness. This is
atomic XL-table publication, not whole-world rollback of the legacy save loader

Stock object/save widths remain unchanged. XL collisions do not populate the
Redux bitmap. Introspection exposes actual XL history order and owner signatures;
object traces select the appropriate XL or Redux representation. D1 retains its
existing path. No name/hash admission exception or newly admitted mission is
introduced by this change

## Native evidence

The private xl_projectiles integration uses unchanged Neptune geometry and
controlled actors. It verifies:

- Actual robot, player and reactor collision dispatch, damage, impact lifetime,
  repeat suppression and Redux alternatives
- Victim-slot reuse retains source identity; projectile-slot reuse starts clean
- Production physics advances a real fusion projectile through two positioned
  robots, yielding sequential damage and the correct two-entry history
- Twenty-two distinct contacts reach the history boundary; retained victims stay
  suppressed, evicted victims can be hit again, and separate projectiles do not
  share history
- Complete disk restoration through a stock mission, earlier-victim suppression
  after restoration, fourteen malformed trailer controls and valid recovery

The extracted FindHitObject/AddHitObject controls compare 1150 defined source
cases, with 500 additional explicit FIFO-correction cases. Another 1920 actual
native impact samples match the extracted MaybeKillWeapon. MSVC and strict GCC
(-Wall -Wextra -Wpedantic -Werror) pass these source comparisons

Stock test_upstream_compat passes. Related scheduler, boost, blocked, trigger
capacity, lava, water-actor, boss spawning/transport, modern property/water and
HXM integrations pass. Android Debug builds both games successfully

## Android acceptance

The owned test_xl_projectiles_runtime integration passes all 53 steps in the
normal Debug APK. Run 3b16ff94c5314e91a8b3d49bd948af3d completed in 71153 ms.
The wrapper's protected player/configuration/mission hashes match after cleanup

The test creates 22 controlled contacts through production fusion firing and
robot collision, verifies FIFO eviction and independent projectile owners, and
then captures normal in-flight physics in a clear room. It settles the detached
checkpoint worker while paused, changes earned damage/history, and restores the
complete histories through three memory cycles. A disk save survives a stock
mission transition, where actual Redux repeat-hit behavior is separately checked.
Further projectile deletion/reuse and disk reload also pass

Fixture actors retain MT_PHYSICS throughout. Early MT_NONE experiments were
rejected: collision requires physical actors, and the stock save format correctly
omits physics fields for MT_NONE. The final fixture uses ordinary records and an
explicit checkpoint drain while paused. Runtime collision assertions and save
semantics were preserved

Physical travel is independently exercised by the native integration; controlled
Android contacts are not evidence of autonomous traversal or campaign playability.
The public xl_projectile_history_contract also checks retention and independent
owners through the full future 20000-ID range without private mission assets;
this does not admit that object capacity yet

Scoped mixed formatting and explicit PSScriptAnalyzer 1.25.0 pass. Both test
catalogs pass: 99 standalone JSON tests, 468 support scripts, 299 standalone
PowerShell tests and 412 master entries. Expanded object capacity, custom weapons,
other combat/AI differences, optional settings, multiplayer and full XL demo
fidelity retain their own acceptance requirements
