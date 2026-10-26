# Neptune XL / 23/40 acceptance record

The bounded 23/40 singleplayer support tranche is implemented in the shared
OpenGL binary. Unchanged Neptune XL is its verified original-mission fixture.
Admission depends on detected requirements and owned resources, not its name
or hash. Hashes bind the tests, not production admission.

Qualification covers decoded features, controlled authored actions, resource
application, rendering, save/restore and stock isolation. It does not claim
a traversed route, multiplayer support, exact XL images or all 23/40 missions.
Automated navigation is deliberately excluded, as requested.

## Source identity

- HOG: `4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303`
- MN2: `f5d3cc0a2f56acbafafc282883bf0e5078aff60706d8bf0fd03f4c5dc311c753`
- Reference source: arbruijn `2576a98b5de310e3ac7c9777df2677d1627b8bae`
- Normal APK: `f83b28080e73b9c3b2df222d871d5c7a0d3bfafeb9371109c1dcbad4c95fbbad`

Original files remain unchanged. Neptune contains one 23/40 level, 93 segments,
287 vertices, 49 serialized objects, 30 walls, one exit trigger, three light
indices and 216 deltas, plus LGT, CLR and POG companions. It contains no robots,
generators, object triggers, nonzero modern segment properties/damage or
multiplayer-only object flags.

## Requirement reconciliation

| Requirement               | Evidence and result                                                                                                                                                                                                                                                                                                                 |
| ------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Record widths and offsets | `xl_neptune_inventory` checks original retained object/wall bytes and injected flags. `xl_neptune_native_layout` checks actual loader geometry, UVLs, topology, walls, objects and trigger targets                                                                                                                                  |
| Modern properties         | Parser and browser distinguish property bits from function IDs. Modern functions 7/13 cannot silently inherit old water/no-damage behavior; unsupported fields reject                                                                                                                                                               |
| Object normalization      | Redux repairs object 13 from segment 68 to 67 without moving it. The unchanged reference loading the original HOG contains two matching decoded copies with segment 67 and original position, orientation, size and shields. Allocation ownership was not distinguished; this corroborates normalization, not live-pointer identity |
| Changing lights           | Native indices and every delta payload match originals. Switched UVLs survive save/restore and stock-to-save restoration                                                                                                                                                                                                            |
| LGT                       | Every authored entry reaches the owned table. All 24 eligible emitters consume brightness differing from defaults                                                                                                                                                                                                                   |
| CLR precedence            | Hash-pinned reference functions in an adapter verify all 910 embedded colors supersede original, conflicting and absent CLR. Native runtime repeats conflicting/absent controls. Foreign or malformed companions reject                                                                                                             |
| POG                       | All seven installed payloads match original fingerprints and live-buffer ownership. Android visits authored surfaces and verifies GPU residency after restoring from stock                                                                                                                                                          |
| Baked rendering           | All 287 reference ambient RGB values match within capture precision. Matched native surface captures support bounded scene fidelity; the halo difference remains explicit below                                                                                                                                                     |
| Stations                  | Three fuel stations deliver bounded fuel; every other segment delivers none. No segment delivers repairs. Function 5 is a blue goal, not a repair station                                                                                                                                                                           |
| Pickups                   | Actual pickup dispatch accepts all eleven authored powerup kinds for a fresh player, then restores the checkpoint                                                                                                                                                                                                                   |
| Doors                     | Actual interaction dispatch opens twelve ordinary door sides and keeps two locked sides closed before reactor destruction                                                                                                                                                                                                           |
| Reactor/exit              | Nonlethal player damage reduces shields without destruction. Lethal damage starts countdown and activates linked doors. Exit-trigger dispatch completes the living player; this is not projectile-flight or escape-traversal evidence                                                                                               |
| Android lifecycle         | Registered 65-step runner passes original selection/startup, XL indication, shading, save/restore, stock switching, POG receipts and emitter rendering                                                                                                                                                                              |
| Rejection UI              | Same APK rejects injected property 8, multiplayer-only object 0 and malformed CLR with specific messages, then loads unchanged Neptune                                                                                                                                                                                              |
| Stock isolation           | Native and Android clear XL resource state in Counterstrike and restore Neptune from save. Android cleanup's before/after app-state hashes match                                                                                                                                                                                    |
| Older-layout regressions  | Final nine-test run passes Neptune, inventory/lighting contracts and affected Observatory/Konflict checks in 26.05 seconds                                                                                                                                                                                                          |
| Test integration          | Both automation catalogs pass. The runner and owned JSON script are registered with a 360-second timeout; CMake tests require local original fixtures and pinned reference sources                                                                                                                                                  |

`xl_neptune_native_features` requires witness strings for station, pickup, door,
reactor, exit and persistence checks, preventing an early exit from counting as
completion. Android run `2ecd83d92a674dfe83cdd86b40754809` passed 65/65 steps in
108,166 ms. Artifacts are under `temp/xl-neptune-20261025`,
`temp/xl-neptune-android` and `temp/xl-stack-review/neptune-features`.
Original fixture availability is not assumed in CI.

## Retained limits

Redux uses its existing procedural soft halo. XL's bitmap glare is broader and
brighter in the matched view. This is a known visual-profile difference, not
pixel parity; see [rendering measurements](neptune-rendering.md). Exact images
were excluded in the earlier pilot scope. Static captures do not newly qualify
dynamic lighting, alternate reference render modes or arbitrary views. Existing
implemented dynamic-light behavior is unchanged.

Modern properties/damage, multiplayer-only objects, wide wall flags, object
triggers, populated equipment data and unsupported resource requirements remain
blocked. Software rendering and multiplayer are not admitted. Other 23/40 levels
must independently pass feature, capacity and asset checks; Neptune does not
certify a whole format or campaign.
