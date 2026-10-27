# Separate XL doorway semantics

Admitted XL wall types 0 through 7 now compute their full doorway result in an
XL-owned function. Redux retains its original alternate. Closing doors remain
transparent after physical passage stops, only doors honor the door-opened bit,
and cloaked walls retain their dedicated draw flag. Actor-specific blue/red
keys and outer blocked/boost constraints continue to apply

## Source and correction

The oracle extracts CWall::IsPassable from arbruijn/d2x-xl commit
2576a98b5de310e3ac7c9777df2677d1627b8bae. The pinned source hashes remain those in
[keyed-closed-walls.md](keyed-closed-walls.md). The expanded comparison exposed
an adapter mistake in the preceding keyed-wall tranche: cloakValue was modeled
as int, although the source field is int8_t. Serialized byte 255 is -1, not 255.
The adapter now uses the actual signed type, and the runtime's ordinary-wall
condition tests nonzero rather than positive. Both signed boundaries are covered

8,192 matrix cases cover types 0..7, eight wall-flag combinations, eight states,
eight cloak bytes including 127/128/255, and opaque/transparent texture flags.
The source function independently checks every result. Another 148 samples from
74 actual animation frames check both connected sides, for 8,340 comparisons.
The corrected 55,296 actor/key comparisons also pass. MSVC and Linux GCC with
-Wall -Werror pass without warning suppressions

## Runtime and lifecycle

The native fixture invokes authored open/close triggers, processes real door
animation and runs FVI physical and visibility rays at every sampled state.
A closing door is saved, a stock mission loaded, and the save restored; its
active animation time and physical/visibility distinction survive. A player
wall hit reverses the restored closing animation without another active slot

An open trigger deliberately does not reverse a closing door: pinned
CSegment::ToggleWall only opens a closed door. The initial test assumed otherwise;
source inspection corrected the test rather than changing gameplay. A player
hit uses the separate source reversal behavior. The final close removes the
active animation and physical permission. Stock retains its own flag ordering

All 16 focused regressions pass in a combined 77.71 seconds. These cover
inventory, teleports, wall chains/scheduling, object events, boosts, blocked
segments, keys/doorways, repair, fluids, boss transport/groups, flags and modern
properties. The final formatted authored-trigger control passes in 3.30 seconds.
All 12 private-policy checks pass, with 66 required core feature tests

## Fixture and Android

The derived doorway fixture uses the preceding scaled geometry, replacing six
portals with a normal door, cloaked walls and normal walls. Non-door portals
carry the misleading door-opened bit. It adds separate open/close actions and
clears the earlier blocked/boost test properties. Original mission bytes remain
unchanged

- Original Neptune HOG: 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303
- Derived doorway.hog: c2791388371d73334cc5ea1b6c8cd9bde3efd19e61343b0e03c59ed32683a677
- Derived doorway.mn2: 4592f3800caf7cfc66af3f3274bb0a3b83cee03674466e4547a9769bbaeec322

The registered Android runner checks controlled physical rejection on both sides
of cloaked and normal walls, real open-door crossings, authored opening/closing,
visibility flags, and disk/rewind/stock restoration. The first run exposed a
missing close action in the derived fixture; the final fixture contains two
actions. The timing diagnostic also confirmed that a repeated action must wait
through the source's 750 ms debounce; the script uses explicit wait_ms rather
than a delay field ignored by wait_for

All 85 final Android steps pass in 102,466 ms on emulator-5554, run
413d49f1b2cf42c892cc44c896eafa12. Protected file hashes match before and after;
emulator-5556 was untouched. The final APK SHA-256 is
2767182e833fa0d19d3225ee9e03da3e93ef7d1652fa960fc8688c2bf8b481dd.
This includes actual crossings after restored open-door state and rejection on
both sides of the normal/cloaked portals. Player-hit reversal is native evidence;
Android checks the authored open trigger's deliberate non-reversal instead

Both automation catalogs pass: 405 top-level entries, 460 support scripts,
99 standalone JSON and 292 standalone PowerShell tests. Mixed-language quality
and pinned PowerShell lint pass

## Limits

This qualifies bounded doorway decisions and the recorded interactions. It does
not qualify all wall animation, optional physics settings, colored/team walls,
wide flags, multiplayer or an original campaign. Existing candidate reports
retain their independent decoding and gameplay requirements. No original mission
is promoted by these controls. No autonomous level navigation was attempted
