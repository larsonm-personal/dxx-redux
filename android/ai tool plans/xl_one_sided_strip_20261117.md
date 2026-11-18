# One-sided exploding strip follow-up

This follows the equipment checkpoint within the November 18, 10:00 AM PST
handoff. Keep the final 06:00-10:00 PST interval for validation, CI repair and
handoff. Do not change the original eight, frozen 64, 120-frame criterion,
watchdogs, authentic assets or trusted-controller publication policy.

## Demonstrated dependency

equipment-original-two-01 passes original Control for 120 ordinary developer-OFF
frames, but original Boiling Point level 5 stops at the reciprocal strip guard.
The native inspector identifies wall 31, segment 502 side 4, type blastable,
clip 46, 100 hitpoints, connected to segment 509. Its reciprocal side has no
wall. This is not an asymmetric two-wall clip pair. The current guard correctly
keeps this unqualified case out of runtime.

The exact readiness archive HOG is SHA256
f0c7363c992bdcf0222873e63bcf7fff699c6f53ca28ab8ea9437f953b190ce2,
staged by equipment-original-two-01/run/1. Native inspection receipt
boiling-blastable-survey-02 validates all seven members and identifies the absent
back wall as segment 509 side 5. The earlier survey-01 used the different
distribution HOG 228dc2c6f4b8fbd4ef108966c43ee127007295d4215ac233b8e4ca40abae700c;
its Control member fails packed-light inspection. Preserve that receipt, but do
not use it as readiness-input evidence. The exact readiness survey independently
confirms the same level-5 wall and all three mask rows used by the equipment
source comparisons. Original Android controls import BoilingPoint-XL.zip.

Pinned XL source 2576a98b5de310e3ac7c9777df2677d1627b8bae supports the missing
back wall: CSegment::DamageWall and BlastWall check IS_WALL before modifying a
back wall, and DoExplodingWallFrame checks the returned CWall pointer before
setting its flags. SetTexture still publishes phase to the reciprocal geometric
side. Redux's damage and immediate blast paths already test cwall_num, but its
delayed explosion completion indexes the reciprocal wall without that check.
The current XL side-phase owner also grants limits only to authored wall sides,
so admitting the input alone would leave a second ownership gap.

## Ordered work

1. Extend the existing authentic-geometry derived writer to author a genuinely
   one-sided blastable portal. Do not remove a live wall after loading or alter
   original campaign inputs. Keep the malformed paired-wall negative control.
2. Extend the extracted-source damage/explosion adapter to represent an absent
   back wall. Compare damage, strict greater-than-three-quarters passability,
   phase on both geometric sides, fireball count and retirement. Retain the
   disclosed large-timestep overshoot limitation of the prior explosion owner.
3. Own reciprocal geometric phase bounds before publication for this narrow
   shape. Require a real reciprocal portal and reject conflicting authored back
   walls, malformed links and out-of-range saved phases. Keep clip, alias,
   resource and end-of-mine restrictions. Guard native explosion completion and
   route notification against the absent wall, retaining the native timer and
   save codec.
4. Qualify incoming move/discard and outgoing live continuation, native damage,
   passability, mid-explosion and final cold restores through stock, independent
   same-clip walls and real 120-frame capture/clean replay with detailed world,
   object, XL and SIM state. Run Windows assertions, optimized source controls,
   rendering and protected Android controls; record any Linux runtime gap.
5. Only after those owners pass, rerun the unchanged original level in ordinary
   developer-OFF and rendered modes. Publish a separate immediate-parent fork
   PR and inspect exact-head CI, including unchanged original-eight/frozen-64
   readiness. Newly exposed failures remain failures. No whole-campaign or
   percentage-playability claim follows from this check.

PR #573 still requires user adoption before the trusted supplemental private
asset owner can run. Do not bypass that controller or merge the stack.

## Implemented checkpoint and local evidence

The candidate follows #580 (4a92b9e8515972aa372a965669c570b148ce8f9c).
Admission now requires exactly one reciprocal geometric side. An absent back
wall is allowed; an authored back wall still must be blastable with the same
clip. Missing/ambiguous geometry and conflicting walls remain rejected. The
side-phase owner grants the absent-wall geometric side its replacement frame
bound. The native delayed explosion checks the back-wall index before updating
flags or notifying the route owner; native clocks and save codecs are unchanged.
D1's analogous native path was inspected. This change is limited to D2, whose
XL admission demonstrates the dependency; no D1 XL support is introduced.

Receipts are under E:/dxx-redux-builds/xl-next-20261117:

- single-strip-full-02 passes the complete optimized native strip owner after
  explicit incoming prepare/move/discard with a pending outgoing explosion.
  All existing geometry, phase, damage, obstruction and clock controls pass,
  including four paired and four one-sided extracted-source explosion cases.
- single-strip-debug-02 passes the one-sided owner with assertions. Both owners
  verify three cold restores through stock, strict passability boundary,
  retirement, unrelated walls, invalid saved-phase rejection and a real
  120-frame capture/clean replay with detailed world/object/XL and exact SIM
  state/call counts. Raw observer equality remains false under the existing
  documented semantic equivalences; no RNG rewind or time compensation is used.
- single-strip-gpu-host-01 passes 24 actual native g3 texture draws and reuploads
  on Intel Arc 140T. Rendered binary SHA256:
  f2e038f2da4b56dcf2c722ab2a9c3d9f5853584c3d5b42e678cfb76065211ab5.
- android-single-strip-01 passes 36/36 in 35.545s, run
  8b42d4aed81c4af9b42944aa575c3130, including GPU controls before/after disk
  restore through stock. Protected before/after file hashes match.
  Frozen single-strip-apk-01/candidate.apk SHA256:
  fa034dc962c44d32f89b647a6d7e22467f2d3a8649e635175575519127ccac6a.
  The installed prior APK was backed up and verified as
  5c5df75e2fd54c7c12090d118c81eed4494462bc81dac4c7e1fa950ff84257b2.
- single-strip-original-01 passes exact original Boiling Point level 5 for 120
  unassisted ordinary developer-OFF frames in 3.641s. This is a diagnostic
  subset, with null target_sha and a separate candidate/source/binary receipt.
- single-strip-original-rendered-01 passes 120 unassisted rendered frames on
  the same original level. Original Android lifecycle validation is underway;
  do not credit it until its terminal receipt and protected hashes are checked.

Derived fixture HOG SHA256 is
9e8509fd843793936563f7e9a0fbcf9eec14b295a406920d927fbb056d820c7d;
its authored portal uses retained original geometry with only a front wall.
The texture hash remains 61acd14d16bcc724bd15e4bea627bfb150f92f90f66bcbe60b3bc25e62b493c1.
Derived controls deliberately isolate objects, relocate/shrink the player,
assign test textures and drive native clocks. They are not unassisted original
readiness. Original input assets are unchanged. General Android XL replay,
startup FX discrepancy and the G2 corrupt-trailer energy defect remain open.
Local Linux runtime is unavailable because WSL execution times out; compile
checks must not be credited as private runtime tests.
