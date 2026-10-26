# Neptune XL and level/game 23/40 support

Objective: faithfully decode the 23/40 layout and qualify unchanged Neptune XL
through normal feature-based admission, resource checks and controlled runtime
checks. Preserve existing work and stock behavior; do not navigate levels.

Status: the bounded support tranche is complete. See the
[acceptance reconciliation](../../XL_compat/evidence/neptune-acceptance.md) for
current evidence and explicit limits. The checkpoints below are chronological;
their earlier open items are superseded by subsequent results, not current blockers.

1. Pin original source files and reference readers; inventory every layout change,
   including split segment properties, object multiplayer flags and trigger tails.
2. Implement bounded native inspection with malformed/version/semantic negative
   controls. Keep gameplay blocked until translation and runtime gates pass.
3. Translate only supported semantics, preserving source identity and checking
   every relocated section and decoded object against the stock representation.
4. Verify original resources, geometry, authored actions, rendering, save/restore,
   stock switching and rejection behavior with native and normal-APK controls.
5. Run affected older-layout/stock tests, update browser/catalog evidence and
   retain explicit unknowns. Other version-23 missions still require feature checks.

Original files initially located under
`temp/metadata_routing_investigation/run_20261018/reproduce/raw/levelpack_7B436FB834/missions/spotlight/`.
Neptune HOG SHA256: 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303.
Artifacts: `temp/xl-neptune-20261025`.

Initial source findings: v21+ separates segment function, property bits and two
signed damage values. Game versions above 37 add an object multiplayer byte;
game 40 changes object-trigger encoding. Copying old game records byte-for-byte
would therefore be unsafe. Existing runtime admission still rejects 23/40.

Inspection checkpoint: the original HOG contains 93 segments, 49 objects,
30 walls and one wall trigger. No nonzero split properties, segment damage or
multiplayer-only object flags were detected. It also contains neptunxl.lgt,
neptunxl.clr and neptunxl.pog; the descriptor's custom_textures=no is not reliable
resource evidence. D2 CLR precedence/application remains a required investigation.

The qualification-only translator now appends stock-width objects and walls,
rebases their section pointers, removes the equipment header and reconstructs
packed light indices. Production admission remains blocked. The original-byte
test checks every retained object/wall byte and rejects injected multiplayer
flags on each of the 49 objects and wide flags on each of the 30 walls.
Synthetic tests cover split property bits, signed damage limits, modern function
IDs that must not inherit old water/no-damage semantics, version mismatch and
truncation. These passed alongside the existing inventory contracts. This is
structural evidence only: stock-loader comparison, authored behavior, resources,
rendering, normal-APK lifecycle and stock isolation are still outstanding.

Native layout checkpoint: the actual stock loader accepts the translated original
and passes geometry, vertex references, UVLs, segment functions/light, walls,
object identities/positions, trigger payload/targets and return-to-stock checks.
This test does not mount the original resource companions or activate XL rendering.
It explicitly accounts for the stock loader rebuilding station indices and wall
reverse-trigger links. Object 13 has stale serialized segment membership 68;
the loader repairs it to 67 without changing position. Both Redux gamesave.c and
the pinned XL loadobjects.cpp call their object-segment repair routine on load.
Reference runtime confirmation of this particular repair remains outstanding.

The browser now counts modern environment properties from split property bits,
not from legacy combined function IDs. Modern function 7/13 must never appear as
legacy water/no-damage. Modern no-damage remains blocked until its runtime
translation is implemented, even though legacy no-damage is supported.

Resource source audit: pinned loadgame.cpp loads CLR before InitTexColors and
LoadLevelData. loadgeometry.cpp LoadTexColorsCompiled then reads all 910 embedded
texture colors for an XL level, overwriting those earlier values. This suggests
the existing blanket D2 CLR rejection is unnecessarily restrictive for complete
extended levels, but it has not yet been removed. Confirm ordering against an
executable source adapter/reference and retain owned-file/malformed checks.
Source SHA256 pins:

- gameio/loadgame.cpp: 5d7ff36d6121411e0250be94353bbe3268bdd02b9bd35c6161951d879fcecc55
- gameio/loadtextures.cpp: bf1ae25e058d6ca745e5833ea0e2c0e425c5716571f6dc5f109a49d67c5452b6
- gameio/loadgeometry.cpp: dd9335a95f69cb91a244ba6352ea381d4531fcddb1a12cf06c619d9ab6cf857f

Runtime checkpoint: the pinned ReadColor, LoadTextureColors and
LoadTexColorsCompiled functions now execute unchanged in an I/O/state adapter.
All 910 embedded texture colors replace original, conflicting and absent CLR;
the loader call order is checked against the pinned source. Normal feature-based
23/40 admission is now enabled, with complete embedded-table evidence required
to accept an otherwise valid owned CLR. The previous blanket rejection remains
for other profiles. No mission name or hash is used in production admission.

The original descriptor/HOG now pass normal native startup, geometry and packed
light/delta checks, all authored LGT entries, switched-light/player save restoration,
reactor countdown and linked-door dispatch, controlled exit, stock isolation and
stock-to-Neptune save restoration. All 24 texture emitters consume authored
brightness differing from defaults. Foreign LGT/CLR rejection preserves active
light state and simulation RNG calls. Malformed owned CLR is rejected; conflicting
and absent CLR retain embedded emitter colors in the actual runtime.
Descriptor SHA256: f5d3cc0a2f56acbafafc282883bf0e5078aff60706d8bf0fd03f4c5dc311c753.

Still required: normal-APK integration/negative controls, actual POG rendering
witness, matched lighting/rendering evidence, remaining authored gameplay checks,
and affected stock/older-layout regressions. This checkpoint is not full pilot
qualification. The private exit test dispatches the trigger directly and does
not claim navigational completion.

Normal Android checkpoint: `test_xl_neptune_runtime.ps1` and its owned JSON script
are registered with the catalog and a 360-second timeout. Both catalog checks and
scoped formatting pass. The normal x86_64 APK built successfully and passed all
33 steps on emulator-5554, run `5b9f32f0a9e04bbab9c453cef8c3bb93` in 56,617 ms.
APK SHA256: 2fee1e459e4e6df19c1f02cd3b718b494942626f25049d9543a24a899446be56.
Evidence is in `temp/xl-neptune-android` and the build/test logs under
`temp/xl-neptune-20261025`. Backup/restore file hashes match exactly.

This verifies original mission-list selection (`XL: Neptune (XL)`), startup,
active vertex shading, no unresolved emitters, quick-save/difficulty restoration,
XL menu indication, stock Counterstrike isolation, stock-to-Neptune restoration,
and an authored texture-295 emitter view at segment 9 side 2. The final capture
has 101 recorded corona draws and zero corona depth failures. The viewed screenshot
shows the expected light-panel surface; it is not a matched reference comparison.

POG evidence remains incomplete: the original contains seven replacement bitmap
indices 1243, 1246, 1256, 1289, 1290, 1291 and 1442, referenced by textures
0, 3, 13, 46, 47, 48 and 209. Android reports textures 3, 13 and 46 as GPU-resident
at the final snapshot. `indexed_mod_owned` does not track this POG installation
path, so neither that field nor residency certifies the replacement bytes.
Add an installed-bitmap comparison/receipt before claiming custom-texture fidelity.
Android negative admission controls and matched lighting/rendering evidence remain
outstanding as well.

Installed-texture checkpoint: read-only `xl_prepared_pog_receipt` checks that the
live bitmap points into the current replacement buffer, then fingerprints its
installed payload. Native original-mission tests pass all seven 4096-byte payload
receipts after startup and restore, verify their absence in stock, and verify
their reconstruction when restoring the Neptune save from stock. Android now
checks the same receipts plus GPU residency at explicit authored surfaces.
The initial texture-0 camera used an open portal because generic texture-reference
introspection includes portal defaults; that rendering assertion correctly failed.
The fixture is corrected to the actual texture-0 wall at segment 91 side 0.

Pinned executable reference checkpoint: isolated original Neptune loads with
287 vertices, 93 segments, lighting method 0 and registered baked colors. All
287 RGB values in its runtime ambient table match the original embedded colors
converted through XL's integer-to-float path within 0.00000003 (JSON capture
precision). The read-only capture is
`temp/xl-neptune-20261025/reference/original-lighting.json`; the launched reference
process was stopped afterward. Its 32 static face lights include eight
texture-353 energy-spark surfaces, which the pinned `effects/glare.cpp` excludes
from coronas, consistent with 24 corona-eligible emitters. This compares runtime
lighting inputs, not final framebuffer pixels. Source SHA256 for glare.cpp:
b7bac14bf8bd92b2f0e088955daf09ad58bd02b85bceb7e04dc054ded074a2cb.

Final POG checkpoint: the normal Android APK passes all 47 steps, including all
seven installed payload receipts and GPU-resident surface views after restoring
from stock. Run `2ac00bf014e64f189d9339a3a1dae005`, 58,590 ms; prior app-state hashes
match after cleanup. APK SHA256:
f83b28080e73b9c3b2df222d871d5c7a0d3bfafeb9371109c1dcbad4c95fbbad.
Each payload has 4096 bytes; texture/FNV-1a pairs are 0/1802764526,
3/928977817, 13/929496110, 46/3423223186, 47/4198776589, 48/547759460,
209/3566167754. These noncryptographic diagnostic fingerprints supplement the
original-HOG SHA256 and explicit live-buffer identity check. The test uses string
expectations to avoid the automation framework's signed-int32 numeric narrowing.
This verifies installed texture data and rendering residency; final-frame lighting
comparison and Android negative admission controls remain open.

Android rejection checkpoint: the registered runner now creates three private
negative fixtures with the native `xl_rejection_fixture` tool. The normal APK
rejects modern property 8 at record 0, multiplayer-only object 0, and a one-byte
CLR with their specific error messages, then loads the unchanged original.
All 65 steps pass, run `2ecd83d92a674dfe83cdd86b40754809`, 108,166 ms.
The APK hash is unchanged from the final POG checkpoint. Before/after app-state
hashes match after cleanup. This supersedes the outstanding Android negative
control item above; it does not close final-frame rendering qualification.

Rendering comparison checkpoint: a separate orientation-only visual fixture
replaces the player's slightly non-orthogonal serialized matrix with exact
axis vectors, preserving all geometry, lights, textures and other objects.
Fixture SHA256: 5bb1ea52867d11b63b458843936a1be52a12a1d7f541fd9acbe10092b375fc22.
Both captures use identical position/orientation and 1024-square viewports;
effective projection differs by 0.000008472 relative. The reference is explicitly
the existing still/backbuffer/visibility diagnostic executable, not the unchanged
release binary. Nearest filtering was matched after retaining an initial mismatched
capture. Five static surface regions have mean absolute RGB channel errors
0.053, 0.072, 0.182, 0.255 and 0.128 on the 0-255 scale. The separately measured
light-panel region differs by 42.674: the reference profile had effects disabled.
An effects-enabled control is required before judging corona fidelity. Receipts,
region coordinates and pixel hashes are in
`temp/xl-neptune-20261025/nearest-comparison.json`; the reproducible comparison
script sits beside it. These bounded views exclude the rotating pickup and HUD
and do not prove general scene parity or normal gameplay completion.

The effects-enabled reference control is now captured and measured. It shows
the reference's wider/brighter bitmap glare, compared with Redux's existing
procedural soft halo. The light-panel error is 48.138; spill into the back-wall
region raises its error to 10.689. This is retained as a rendering-profile
difference, not hidden by a passing surface-only metric. Exact image parity was
excluded in the earlier pilot acceptance; no mission-specific appearance tuning
is introduced. See [the bounded rendering record](../../XL_compat/evidence/neptune-rendering.md)
for provenance, results and remaining limits. A final requirement audit is still
needed before declaring this pilot complete.

Authored-action audit: the native feature test now exercises all three fuel
stations, checks zero fuel delivery in every other segment, and checks zero
repair delivery everywhere. The initial test incorrectly expected function 5
to be a repair station; both source enums identify it as the blue goal, and
the failing expectation was corrected without changing runtime code. All
eleven original powerup kinds are collectible by a fresh player, followed by
checkpoint restoration. All fourteen door sides are exercised through the
ordinary interaction handler: twelve open and two remain locked. Nonlethal
player damage reduces the original reactor's shields without starting the
countdown; lethal damage then starts countdown and opens its linked doors.
The existing exit-dispatch and stock-to-save controls still pass afterward.
`xl_neptune_native_features` passes in 5.82 seconds with required witness strings
for these actions. These are controlled handler tests, not traversal, projectile
flight or multiplayer-goal qualification. No robots are authored in Neptune.

Final reference check: unchanged reference executable and original HOG hashes
were verified before a read-only object-13 inspection. Two decoded representations
have segment 67, matching Redux's repair from serialized segment 68, while retaining
position, orientation, size and shields. Allocation ownership was not distinguished.
`reference/object13-repair.json` retains the matching fields and candidate bytes.
The owned process was stopped and the private visual fixture restored afterward.

Final affected CTest run passes 9/9 in 26.05 seconds; both automation catalog
checks pass. The acceptance record reconciles all five plan steps. Renderer
appearance differences, multiplayer, unimplemented modern features and traversal
remain explicitly outside the qualification claim rather than silently passing.
