# Suppress natural boss spew for every attached object trigger

Pinned arbruijn/d2x-xl commit 2576a98b5de310e3ac7c9777df2677d1627b8bae,
main/trigger.cpp FindObjTrigger ignores its requested type and returns the
first authored attachment. It also ignores disabled flags and consumed
trigger ownership; ExecObjTriggers leaves the object's reference range.
Pumo level9 sound-only boss attachment can therefore suppress natural spew.

Add an attachment-presence predicate using authored owner and live signature,
including consumed/disabled entries. Use it only for non-triggered boss
spawning. Keep explicit trigger spawning, other owners, stale signatures,
profiles, RNG, cooldown and inactive/multiplayer behavior unchanged. Retain
the existing type26 query for its other callers.

The existing linked boss_gameplay_cpu target loads real production trigger
records. Its sound-only baseline failed on unchanged production at cdd37407
in hosted run42572869799 job129014043870. One initial fixture seed call
retained an unused recorder symbol; switching to d_rand_set_state fixed
linkage without adding a stub. Controls cover teleport/spawn/mixed,
disabled/consumed, other owner, no attachment, signature reuse, explicit
spawning, inactive and multiplayer; profile21 definitions38/40 are usable.

Replace reference_boss.cpp's has_spawn stub with extraction of the actual
hash-pinned FindObjTrigger. Required source checkout uses CRLF, pinned at
74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852.
Check first-trigger ordering, ignored type/flags/consumption and actual
sound-attached BossSpewRobot RNG/cooldown suppression.

No new harness or local build. Corrected hosted linked CPU and source-oracle
execution must pass before qualification. Mutations remain unrun to avoid
adding CI machinery in this bounded packet. Verify synthetic merge tree
identity against the published head. Native Pumo replay and inherited
xl_projectiles failure remain unqualified. No merge.
