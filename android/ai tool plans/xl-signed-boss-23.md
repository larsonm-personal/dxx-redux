# Signed boss profile 23

Layered on #386. Sphere original object 311 is robot 45, signed HXM boss -23.
Pinned robot reader converts negative bossFlag to magnitude and clears bEndsLevel.
Admit only newly qualified -23, not other negative profiles. Preserve raw identity.
Audit every profile-index consumer: direct weapon collision/dispatch, XL spew,
camera radial damage, spawn selection, death/objective state. Classic uses raw role.
Reference profile 23: cloak/teleport, kinetic spew, energy immunity, kinetic damage;
no spot immunity, teleport spew or multi-spew. Existing radial kinetic resistance
is false for both raw -23 and effective 23; D1 wrapper behavior remains unchanged.

Validate pinned signed reader/properties/death, native energy/kinetic hit behavior,
spawn selection, cloak, death, save through stock and objective exclusion. Re-run
existing boss RNG/lifecycle, HXM, classic/public contracts and unchanged Sphere
rendered diagnostic. No readiness improvement unless actual load/120 ticks passes.

## Evidence and results

Pinned source revision 2576a98b5de310e3ac7c9777df2677d1627b8bae: objects/robot.cpp lines 284-286 decode magnitude and bEndsLevel; ai/aiframe.cpp bossProps[0][2] supplies profile23 semantics; ai/aiboss.cpp supplies spawning and conditional death completion. Existing exact source SHA pins remain mandatory. Reference test scans all 256 signed bytes, admits exactly ten qualified profiles, and executes the pinned death function for both ending/non-ending roles.

Native HXM suite passes with -1 and -23 controls: normal startup, laser energy immunity only for -23, Vulcan damage, cloak, lethal death roll, disk through classic restore, signed raw role persistence, and last optional boss not triggering countdown. Existing 252 animation cases still match pinned source. Existing boss fixture passes damage dispatch, two owners, morph/lifetime/cooldown, stock isolation and 50 pinned selection/placement/motion/RNG comparisons plus both objective combat orders. Public owner passes all 54 GPU/audio/source contracts. Scoped quality and automation catalog validation pass. Catalog execution remains blocked (exit125) by the host's unavailable /proc/self/task/.../children interface.

Unchanged Sphere sp-02.rl2 SHA-256 a821cc678a6e9f574fe5ec96b9475533e687aa338676bd5377cde3818355146b, HXM 78686e51eaf9849e8cd9bb798920ce7d6eec81249a92c4f5ca40560383d128d9: genuine resources b5f68b8f433e0fc170306a168534304ae64f7720 plus genuine Vertigo. Supplemental real GL run integrates #384 diagnostic entrypoint locally (not in patch); binary SHA-256 1f2aabed63d9fae7f2aa2c02b4e332b2d62e8387151defc29c8f0f07fa8c9949. Object311/robot45 now passes. Level load stops at object423/robot75, signed boss -27. No 120-tick success or official readiness update. This profile remains rejected. Windows/Android and hosted exact-head tests are not claimed by local qualification.

Warp Shock still requires real slow-motion clocks, inventory/input/save/audio behavior and genuine addon sprites. Existing real slowdown.wav and speedup.wav do not replace the missing slowmotion#0.tga and bullettime#0.tga. No stub pickup or altered original is supplied.
