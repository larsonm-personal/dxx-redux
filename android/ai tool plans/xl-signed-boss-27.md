# Signed boss profile 27

Layered on #387. Audit original Sphere sp-02: object239..242 robot17=-1,
object311 robot45=-23, object423 robot75=-27. These are all placed boss roles
in the unchanged level and companion. Only -27 remains to qualify.

Pinned reference profile27 (bossProps[0][6]) teleports, spews on energy hits,
is immune to kinetic direct hits, takes energy damage, quarters radial damage,
and has no vulnerable-spot/extra-spawn/teleport-spawn behavior. Signed role
clears bEndsLevel. Retain raw signed definitions and fail other unknown profiles.

The inherited generic radial-damage helper indexes the raw role, unlike the
already-corrected XL camera radial path. Normalize only under active XL; keep
classic/D1 actor logic and all bounds. Test signed direct/radial behavior,
cloak, optional objective, disk identity; reference properties and actual
pinned radial rule; full boss/HXM/classic/public checks and original rendering.

## Complete input role audit

All placed robots are replaced by HXM except IDs7/67. Retail HAM robot7 has
boss flag0; genuine Vertigo HAM robot67 also has flag0 (Vertigo HAM SHA-256
faa67189ccf5c7d6cdc8e37fa009fb21a1d57ee2c6c212ce091642e4366e3c0f).
HXM also has unplaced robot23 with positive boss2; no matcen selects robot23.
That unqualified profile remains rejected if actually required. Original HXM
raw roles are read at robot-record byte280, with 480-byte definitions; extra HAM
record widths are verified against d2_ham_validation.c (weapons125/robots480).

Reference revision2576a98b5de310e3ac7c9777df2677d1627b8bae; exact source pins
for robot reader, bossProps and death retained. New exact effects/fireball.cpp
pin e91c43142cbb1f85c0274aac19e97c95d62e95bc5aa31c1d2537db2113387bf4
executes the actual radial resistance conditional, including integer truncation
at damage0/1/3/4/1001/65536, for each supported signed role.

## Startup capacity blocker and test ownership

The actual unchanged Sphere reaches startup after boss admission, then crashed
in obj_delete (object.c:1783), called from gameseq_remove_unused_players
(gameseq.c:406). A diagnostic signal-handler stack trace identifies the path;
Sphere has OBJ_PLAYER starts0..15 plus three cooperative starts.
gameseq_init_network_players writes Player_init[k] without a bound; this
array has MAX_PLAYERS=8 entries. The patch rejects excess player/ghost starts
before runtime startup. It does not truncate originals or claim16-player support.
A serialized 9-start fixture checks rejection without live-definition changes
and recovery of the original level afterward. Cooperative starts do not consume
the singleplayer array, matching the current startup selection rule.

Profile27's spawn dependency includes genuine Vertigo robot69. The retail-only
HXM suite retains its original roles and mandatory tests. A distinct signed27
mode requires loaded78-robot genuine Vertigo data and exercises all signed
roles with real combat/save/stock isolation. Configure XL_HXM_VERTIGO_DATA to
register xl_signed_boss27_native under the existing private HXM fixture owner;
no existing FAST cohort member or count changes. The explicit CMake test
runner supports MODE=signed27 and rejects unknown modes.

Initial retail-only -27 fixture correctly failed on missing robot69. No fake
Vertigo definitions were substituted. The subsequent genuine-Vertigo mode
passed. The original rendered crash is retained as a failure, not a successful
load/tick result. Final capacity admission must fail cleanly and readiness
remains unchanged.

## Final measured results

Original Sphere sp-02.rl2 SHA-256
a821cc678a6e9f574fe5ec96b9475533e687aa338676bd5377cde3818355146b; HXM
78686e51eaf9849e8cd9bb798920ce7d6eec81249a92c4f5ca40560383d128d9.
Real assets revision b5f68b8f433e0fc170306a168534304ae64f7720 plus genuine
Vertigo. Final real-renderer diagnostic binary SHA-256
ad4d69dde3f9ae85b720d53356c1ec97e1296720d868b4f6a47dd42553a54d99
(with #384 diagnostic entrypoint locally integrated, not committed here) exits1
at level_load with player-start capacity rejection. No signal crash, no
120-tick success, no official readiness/status update.

Public source/GPU/audio owner:54 passed. Classic upstream and trigger-demo
CTests:2 passed. Boss spawning50 source/RNG cases and both objective orders
passed. Genuine-Vertigo signed27 native combat/save suite passed. Catalog
validation and scoped quality passed; catalog execution blocked by the
known unavailable /proc/self/task/.../children host interface (exit125).
Windows/Android/hosted exact-head results remain unverified locally.

The first capacity test attempted to shadow an already-selected archive
member, so it loaded the original instead of its modified loose file. The
corrected control uses unique overstart.rl2, avoiding archive precedence.
Do not count that first test failure as a successful negative control.

Registered CTest results on the final native build: xl_hxm_native passed
(25.29s), xl_signed_boss27_native passed (16.09s). Capacity-control rejection
and original recovery are mandatory inside the former. The latter requires
genuine Vertigo and passed direct energy/kinetic, radial boundary, cloak,
death/save identity and optional-objective checks. Existing private FAST
membership remains unchanged; the additional Vertigo CTest requires explicit
configuration and is not claimed as hosted coverage.
