# Immediate wall trigger chains

## Scope and source

Wall master (31), enable (32) and disable (33) actions use detected requirements
in the shared OpenGL D2 runtime. Link order, nested chains, top-level debounce,
actor restrictions, raw master counters and saved state are implemented without
production mission-name or hash exceptions

The reference is revision 2576a98b5de310e3ac7c9777df2677d1627b8bae,
main/trigger.cpp SHA-256
74f1284260fb4a4bb7396bc45eeb83868a7f2dfa5e3fdf0bcb767b94f1b12852

- DoMasterTrigger resolves each segment/side through its wall to a trigger,
  retaining authored order and passing shot=0 to children
- DoEnableTrigger decrements positive master counters by one raw integer unit,
  clearing disabled only when the counter reaches <=0
- DoDisableTrigger disables a target and increments a master counter by one
- Read resets master values for game data before 39; later positive authored
  values are converted from fixed point and set disabled. This happens during
  source translation, before ordinary save restoration overwrites live counts
- Operate limits recursion independently of one-shot state, which is updated
  after action execution. Nested children bypass the top-level 750 ms debounce
- Nonplayer actors can operate masters and teleport children, but cannot operate
  ordinary player actions or enable/disable actions directly
- A real exit child aborts later links and the master's one-shot update. Redux
  unwinds its dispatch scope on that path rather than retaining stale depth

The original tranche covered immediate actions and the low three flag bits.
[Scheduler support](trigger-scheduler.md) now adds delays and permanent,
alternate and autoplay flags. Orientation/silent flags, negative-side effect-object
targets, object-attached triggers and multiplayer retain explicit blockers.
Every other level action, capacity and asset requirement is checked separately.
Stock trigger behavior remains selected when no XL source is loaded

## Native checks

xl_wall_trigger_chains uses original Neptune geometry with explicitly derived
trigger and wall assignments. It covers ordered enable/teleport/disable actions,
nested dispatch, disabled targets, raw counter transitions, debounce and expiry,
bounded self-recursion, post-action one-shot state, actor filtering, physical
nonplayer crossing through a master, stock illusion-off/on children, source
save/restore, stock transition and real exit propagation

The independent executable compiles hash-pinned reference DoMasterTrigger,
DoEnableTrigger, DoDisableTrigger and Read with storage adapters. All 40 exported
runtime counter samples match. Fresh-read controls independently check the same
18 version/value combinations as the translator tests. Link ordering and exit
return propagation are also checked in the extracted reference

xl_inventory_contract checks all three admitted layouts, immediate and delayed
time boundaries including INT_MIN, unsupported flags, malformed destinations,
effect-object target diagnostics and browser feature status. Source master
values -65536, 0, 1, 65535, 65536 and 131072 cover reset, fractional and integer
normalization for each supported game-data version

The focused native regression selection passed 7/7, including teleport,
modern-property/water, original Neptune and Observatory descriptor controls.
Logs: temp/xl-trigger-chains-20261026 and temp/xl-stack-review/trigger-chains

Original geometry HOG SHA-256:
4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303

Derived chain HOG SHA-256:
b02d206a3ccfd1b753adb447eb68813e46af6caa6e3c7e7533be994d0b71f60b

Derived descriptor SHA-256:
5b66c41a46b472a8eb95603a21abbe8193fe1d64e050a80c494d0f74d0756575

## Device acceptance and mission scope

The registered Android integration runs explicit authored actions, inspects live
trigger counters/flags, saves and disturbs them, then restores within XL and
after switching to stock. It passed 62/62 steps on emulator-5554, run ID
f00428f7a6a64d22b885fc01d5e91a3a. Pre-test and restored app-private file hashes
are identical; emulator-5556 was not used

APK SHA-256:
a26a72556c27dff2fec54f6cd33eb8f8a884d284c9ba47c0602bc97186faec3d

The x86_64 Android build, scoped formatting/lint and both automation catalog
checks passed. Logs and receipts are in temp/xl-wall-trigger-chains-android and
temp/xl-trigger-chains-20261026. The trusted private original-fixture CI runner
now requires xl_wall_trigger_chains using its already-pinned Neptune package;
the extracted reference oracle also runs when a pinned reference root is supplied

After rebasing this chunk onto current cmake, all 12 private-fixture runner
policy tests pass. The receipt requires 41 established native results, including
the chain test, while preserving the upstream distinction for unfinished
campaign acceptance

The first device attempt exposed ignored post_delay_ms on set_debug actions:
two requests arrived within the debounce window, correctly preserving the
disabled state. The script now uses explicit wait_ms actions. Production code
was not changed to bypass debounce for automation

[The survey candidates](trigger-chain-candidates.json) retain eight payloads
containing these actions. Dinter, Sphere and Styx also contain unsupported chain
flags or delays. The two Anthology payloads have immediate masters, but earlier
native surveys stop at unsupported object layouts. No new original mission or
campaign is qualified by this derived fixture
