# Bounded 17/36 and D2 companion brightness

Started 2026-10-25 19:49 UTC on clean branch xl_more_2, base c25ccae81.
User budget: 3-4 hours maximum; finish or report remaining gaps by 23:49 UTC.
No automated navigation/combat campaign. Use targeted native and Android checks.

Completed 2026-10-25 20:28 UTC, approximately 39 minutes after selection began.
Changes are left uncommitted on xl_more_2 for review. All acceptance results and
explicit scope limits are recorded below.

## Selection and evidence

Target the locally available original Konflict distribution: one 17/36 level,
1603 vertices, 505 segments, 54 walls, 71 objects, one exit trigger, four
changing-light index records and 282 deltas. Objects comprise eight player
starts, 62 pickups and one reactor. No robots, matcens, equipment generators,
object triggers or unsupported segment functions. Descriptor declares no custom
robots or textures. Current native inventory has no semantic blockers beyond
the general lighting/runtime qualification observations; admission rejects 17/36.

The HOG also contains konflict.lgt: 89 nonzero entries, 82 differences from
the built-in XL D2 brightness table. Do not enable loading while ignoring it.
Pinned local source identities:

- HOG: 914231998b80f3900508fa6847adb8d60b6b78670cf7afa1e20e90c9170e46e1
- MN2: 26865fb664d93eaa54acaec9d857bff15b192e1dfcca1598229388f6f77e3d94
- RL2: b306eab41c685e88cc9ed7360cb68023ad3246b71399d13304cef77d859bfcfe
- LGT: 2a01adf723744f2495612c2e32eb84544f1f4a53d02b74ab2cae32acba691261

Scratch evidence: temp/xl-next-tranche-20261025/konflict-before.json.
Crownpoint X was rejected as a candidate: 11/31 also needs wall type 8 and
trigger type 16. Observatory still needs no-damage semantics. Neither fits
this tranche as well. The preserved survey is a shortlist, not runtime evidence.

## Work plan and acceptance

1. Audit pinned reference and existing 17/36 translation: header removal,
   object records, packed light indices, colors and reserved flags. Promote
   only layouts whose decoded requirements are supported; preserve rejection
   of no-damage, equipment, object triggers, capacity excess and unknown data.
2. Add owned D2 LGT brightness to the existing prepared-resource lifecycle.
   Check reference precedence and every current brightness consumer. Validate
   malformed/foreign companions before replacing active state. No mission-name
   allowlist; retain defaults only when the companion is absent.
3. Add a private original-Konflict integration check through normal descriptor
   selection and level loading. Verify source/translated geometry, light indices
   and actual brightness consumers, representative native reactor/exit/door
   behavior, save/load, stock isolation and failed-load retention. Controlled
   fixtures are feature evidence, not a claim of uninterrupted completion.
4. Extend focused public contracts for newly admitted layout boundaries and
   companion policy. Build opt-in and default-off paths; run the relevant
   private checks and an Android loading/persistence/render witness if feasible.
   Do not spend the budget rerunning unrelated CI or navigating the level.
5. Synchronize browser requirements/support wording, EXTENSIONS.md and results.
   Report exactly what is enabled and verified. Unsupported features and
   multiplayer remain rejected. Do not label an entire campaign supported.

Time allocation: about 30 minutes audit/selection, 90 minutes implementation,
60 minutes focused validation, 30 minutes repair/documentation. If validation
finds additional semantics outside this bound, retain explicit blockers and
report partial work rather than silently widening admission.

## Progress

- Current merged inspector rebuilt and original candidate inspected
- Confirmed non-neutral LGT companion; both layout and brightness are required
- Implemented bounded normal 17/36 admission through the existing translator
- Removed the unconditional d2x-name runtime rejection only in opt-in builds;
  alternate HAMs and stock-format levels inside this dialect remain guarded
- Added a validated owned LGT snapshot to prepared D2 resources, consumed by
  texture emitters; zero entries override defaults, absence retains defaults
- Added explicit rejection of malformed/foreign/loose lighting companions and
  D2 CLR overrides, with explanatory error messages
- Added hash-pinned `xl_konflict_features` native CTest and explicit Android
  `test_xl_konflict_runtime.ps1` runner, with catalog and timeout registration
- Updated browser support descriptions, README and EXTENSIONS

## Reference and scope audit

The preserved survey's Konflict RL2 hash equals the independently extracted local
original above. This is Konflict, not the separate Konflict at Karon campaign.
Reference `gameio/loadgame.cpp` initializes the default brightness table and then
calls `LoadTextureBrightness(level)`. `gameio/loadgamedata.cpp:665` reads all 910
signed 32-bit entries from the level-named LGT. Source hashes used for this audit:

- loadgame.cpp: 5d7ff36d6121411e0250be94353bbe3268bdd02b9bd35c6161951d879fcecc55
- loadgamedata.cpp: 64ed9a54e91a744500106c8691c5dea7a4b496a4b122e0f51e766ee2c8b50bd0

The 17/36 layout audit already recorded in inventory.cpp and the Observatory
prerequisite remains the translation basis: empty equipment section only,
packed count/side decoding, ordered 127-entry chunks, fixed texture colors,
neutral sparse object references and preserved reserved segment flags. Normal
admission now uses that translator without erasing any semantic blocker.
No-damage, nonempty equipment, object triggers, unknown layouts, expanded
capacity requirements and multiplayer are not enabled by this change.

The rendering scope remains the existing extended-level profile: embedded
colored vertices/faces, classic dynamic lighting and texture coronas. In the
pinned reference, LoadVertLightsCompiled reads embedded ambient data for both
15 and 17, and LoadTexColorsCompiled changes from float to fixed after 15.
This is not a new per-pixel, lightmap, shadow or recomputed geometry-light mode.
There is no claim of pixel parity with XL. No mission-name exception, modified
acceptance level, automatic navigation or combat-route attempt was used.

## Acceptance evidence

Native original-file checks pass under `temp/xl-next-tranche-20261025/native`:

- Original descriptor and normal StartNewGame, all vertex coordinates and
  segment vertex/function records, all translated packed light indices and
  delta contributions match the original inventory
- All 910 LGT entries survive preparation; 67 eligible runtime texture emitters,
  including 22 using textures whose brightness differs from the default table
- A derived zero-LGT control disables every runtime texture emitter, proving
  that the live consumer uses the table rather than merely retaining it
- Switched light UVLs, player energy, mission identity and LGT survive native
  save/restore; stock transition clears XL and the brightness override
- Native reactor destruction starts the countdown and toggles authored linked
  doors; direct native exit-trigger dispatch completes a living player using
  the existing bounded completion test hook
- Foreign LGT rejection preserves active geometry, owned lighting and SIM RNG
  call count; removing the shadow recovers admission
- Separate derived controls reject malformed LGT, unsupported CLR and loose
  lighting companions; absent LGT uses defaults, including after prior overrides

The water regression initially exposed an overbroad check requiring selected-HOG
level ownership even when no lighting companion existed. Restricted the new
ownership requirement to present LGT/CLR. Original water resource rejection,
recovery and replay checks now pass again. No unrelated resource policy was
widened to make the test pass.

Android emulator-5554 has passed 31/31 startup/render/save/menu/stock-transition
checks twice so far. The second run was
`5f588541c5f6461283df3e1e9f6fcff8`, 81952 ms. The wrapper verifies hashes of restored
pilot/config/preference files. Screenshot and introspection show a rendered
original Konflict scene with active XL shading; this is a rendering witness,
not a reference-image comparison. Emulator-5556 was not used.

Default-off Windows D2 and opt-in native builds pass. Changed inventory and
lighting contracts, original Observatory inventory and descriptor rejection,
Konflict features and water checks have passed. Automation and runner catalogs
pass. Scoped formatter/linter passes; the two-line stock-loader guard preserves
the surrounding upstream formatting.

Final native selection passes 6/6 in `qualified-tests.log`: changed inventory
and lighting contracts, original Observatory inventory, original Observatory
descriptor rejection, Konflict features and original water arena. The final
Konflict witness-only diagnostic addition also passed its CTest separately.

Final APK SHA-256:
`9b67f89b9bc778ec9e6205c6a956010e308e039d212815e8d58306a4216407b5`.
Built with `:app:assembleDebug -PciTestAbi=x86_64 -PxlPilot=true` after the
ownership correction. Final Android run `edb61ffbf9f7449781da51041ec96484`
passes 33/33 in 72765 ms, including a controlled face-view pose at original
segment 32/side 1, texture 306. Final introspection reports 158 corona draws,
zero corona depth failures, 2372 shaded vertices and five XL menu-indicator
draws. The native fixture identifies this face as an authored-LGT difference.
State restoration hashes match. Evidence is in `temp/xl-konflict-android` and
`temp/xl-next-tranche-20261025/android-test-qualified.log`.

The scene is witnessed, not reference-matched: the close light-panel screenshot
is not a pixel-parity or visual-quality score. The original spawn screenshot
from the prior run is retained by the artifact helper. Konflict is a robot-free,
arena-like level; this tranche qualifies the tested singleplayer feature slice,
not multiplayer competition.

## Explicit remaining limits

No uninterrupted playthrough, route completeness, matched reference frames,
multiplayer synchronization, D2 CLR, advanced lighting modes or general XL
campaign qualification is claimed. The controlled reactor/exit check does not
prove the player can find every route. These are disclosed scope limits, not
features silently treated as working. The original Konflict distribution's
detected requirements are admitted; other missions retain feature/resource gates.
