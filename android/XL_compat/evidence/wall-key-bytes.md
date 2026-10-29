# Full wall-key bytes

The inspector now retains all 256 authored wall-key byte values and reports
nonstandard values as observations. It does not normalize them to KEY_NONE or
mask them to the three color bits. The existing runtime already preserves and
consumes these bytes; no D1/D2 gameplay implementation or save schema changes.
Wider wall flags, unsupported wall types and other independent gates remain.

## Source behavior and comparisons

Pinned XL revision 2576a98b5de310e3ac7c9777df2677d1627b8bae declares CWall::keys
as uint8_t. Its consumers intentionally use different tests. Player wall hits
require exact blue, red or gold values. Closed-wall passage requires exactly
blue or red and the corresponding owned key. Brain/runaway/sniper door planning
tests the raw byte against player flags, including flags outside the key bits.
Consequently, 255 can allow a player's door hit while failing a robot's test;
mapping it to KEY_NONE would change gameplay.

The existing pinned CWall::IsPassable oracle now also extracts the complete
CWall::ProcessHit and AIDoorIsOpenable functions. Source/header hashes are
required by reference_keyed_wall.cmake. The native test calls the production
wall_hit_process and ai_door_is_openable functions; it does not duplicate their
decisions in an expected-value helper.

| Evidence                      | Controls | Scope                                                                                                                                         |
| ----------------------------- | -------: | --------------------------------------------------------------------------------------------------------------------------------------------- |
| Format and translation        |    1,280 | Every byte in levels 15, 17, 23, 25 and 27, retaining the exact native byte; wide-flag rejection controls                                     |
| Closed-wall source comparison |   72,576 | Existing canonical visibility/actor matrix plus every extra byte for all nine actor/query kinds and eight owned-key combinations              |
| Player-hit source comparison  |    6,144 | Every byte, eight owned-key combinations, normal walls, locked closed doors and already-opening doors                                         |
| Robot-door source comparison  |    6,144 | Actual brain object, every byte, twelve player-flag profiles and locked/unlocked doors                                                        |
| Physical engine controls      |      480 | Real FVI and movement, both portal sides, player/robot/pickup/projectile actors, blocked destinations and visibility; raw 16/128/255 variants |

The robot oracle compiles the complete source function but exercises its brain
branch. It does not claim guidebot behavior or navigation coverage. The hit
matrix deliberately avoids starting door animation; the existing separate door
lifecycle tests cover animation. Changing one recorded result for key 255 makes
the independent source comparison fail at sample 6120.

The unchanged collision and automap consumers were also audited for raw-byte
checks. Runtime load, disk through a stock mission and Android memory/disk
restoration keep the authored 255 values on fixture walls 6 and 7. Existing
closed-wall behavior and Redux recovery controls remain in the same test.

## Original impact and limits

The unchanged Pumo HOG has SHA-256
6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043.
A fresh inventory removes exactly 73 wall_keys rejections across nine levels,
replacing them with nonstandard_wall_keys observations. Every other inventory
field is unchanged. All fourteen levels still have independent gameplay
blockers. In particular, modern wall flags above 255 are still rejected rather
than narrowed. This correction does not qualify any original mission.

## Android and host validation

The derived keywall fixture changes only walls 6 and 7 in addition to the
earlier [closed-wall fixture](keyed-closed-walls.md). Its current hashes are:

- keywall.hog: 3408935bb892773d6b0bc7d295db923919e001d916eff5aef7e0ec1bbeddd8fe
- keywall.mn2: e3c111d43ae21e63ae7d3e23e33bbbbe78b4976fb44e89831c50775c23db3577

The normal Android APK, SHA-256
e14800e88091473b3cac05429ce3133b255a1645e4fcccfcf6c6b68d51fad61a,
passes all 90 existing and extended controls in 73476 ms, run
636ea61b6dab48068e763060d5646439. All 26 protected file hashes match after
restoration. Checks cover loaded raw bytes, controlled physical crossings,
visibility, live keys, quickload, checkpoints and disk through stock. These
controlled poses and inputs do not establish original campaign progression.

Windows passes all 40 required public cases in 34.06 seconds of CTest time
(50.531 seconds for the incremental owner), plus all three extracted-source
wall-key comparisons. Linux native keyed-wall and doorway comparisons pass,
including all three source matrices. Both automation catalogs pass: 99
standalone JSON tests, 478 support scripts, 308 standalone PowerShell tests and
421 top-level entries.
Required test membership is unchanged: 40/41 public cases and 96 private cases.
The intermediate Windows-mounted private run encountered three native timeouts
and was stopped after retaining its failures. All three cases then pass with
the exact same executables and unchanged assertions/timeouts, changing only
DATA and OUTPUT to native-filesystem paths. This isolates the observed mount
access slowdown; the interrupted selection is not a passing full-suite result.
The complete clean native-filesystem rerun is tracked in the PR's validation
record and must still satisfy the owner's exact 96-case membership.

Local diagnostics, the original inventory comparison and Android output are in
temp/xl-wall-keys-20261028 and temp/xl-keyed_wall-android. The original comparison
requires all other inventory fields and runtime blockers to match after removing
only the corresponding key diagnostics. Original HOG hashes remain unchanged.
