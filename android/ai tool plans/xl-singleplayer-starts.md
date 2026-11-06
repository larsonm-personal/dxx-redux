# XL singleplayer starts

Layered on #388. Pinned GameStartInitNetworkPlayers accepts16 starts, normalizes
player/ghost IDs, and removes coop starts. GameStartRemoveUnusedPlayers removes
selected starts1..15 in order. Redux uses eight network slots; retain them and
store only the surplus eight object indices until the same cleanup stage.
Do not expand network structures, admit XL multiplayer or change classic paths.
Reject17+ authored player/ghost starts. Preserve original bytes and runtime
cleanup ordering. Verify source selection/deletion, normal9/16 loads, ghosts,
coop interleaving, save through stock, repeated loads and unchanged Sphere
rendered120 ticks. Report any new blocker honestly.

Singleplayer spawn chooses Player_num (zero), never the network random-start
selection. The transient count is reset at init and cleared after cleanup;
NumNetPlayerPositions stays bounded at8. This internal capacity is intentionally
different from reference16 but must preserve singleplayer final world behavior.

## Qualification

Pinned reference revision2576a98b5de310e3ac7c9777df2677d1627b8bae:
gameio/loadgame.cpp SHA-256
5d7ff36d6121411e0250be94353bbe3268bdd02b9bd35c6161951d879fcecc55;
include/player.h SHA-256
4e134411347c2dee70f22c7b60c63717390496022964a77c1eb8dc5b5ff15cf4.
The oracle executes the actual init body through player-position assignment
(excluding later debug/OEM UI) and actual unused-player cleanup. It compares
all eight native cases, including object-release/allocator order. Native tests
assert surviving player position, normalized identity, untouched slots8..11,
normal1/8/9/16 loads and save through stock for16. Ghost selection is a runtime
control after validated loading; authored ghost-model admission is unchanged.
An initial authored-ghost fixture correctly rejected missing pship1.pof; no
model guard was relaxed to accommodate it.

Seventeen starts and extended starts without player object0 remain rejected.
The full private HXM owner passed in40.71s (unchanged270s deadline), and the
genuine-Vertigo signed-boss owner passed in18.25s. All252 animation/source
controls, HXM combat/ownership/save checks and existing boss-role controls
remain mandatory. Four damaged source transcripts (count, order, truncation,
trailing content) were rejected. Public source/GPU/audio owner:54 passed.
Classic upstream and trigger-demo CTests:2 passed. Scoped quality and catalog
validation pass; catalog execution remains blocked by the environment's
unavailable /proc/self/task/.../children interface (exit125).

## Unchanged original rendered result

Sphere archive member sp-02.rl2 SHA-256
a821cc678a6e9f574fe5ec96b9475533e687aa338676bd5377cde3818355146b;
companion HXM78686e51eaf9849e8cd9bb798920ce7d6eec81249a92c4f5ca40560383d128d9;
HOG01e1e33899c90fd8d820604b323252bc205956e4050fbec58eb32bcc96cd86fb
verified unchanged after testing. Genuine22-resource assets b5f68b8 plus
genuine Vertigo were used. Assets PR2 is now merged at
9b70662c2f42394f4580b7361bab4b4dae9376c5; its complete tree equals b5f68b8.

Final supplemental real-renderer proof: phase=loaded_ticked, sphere level2,
3986 segments, XL active,120 fixed ticks at60Hz,120 rendered frames, seed1,
assisted=false, llvmpipe LLVM19.1.7. Binary SHA-256
959abd7a8a42f50bb17fe9767581ae805acb0695c0450c22f72085e288ea4916.
This binary locally integrates #384's diagnostic entrypoint; that entrypoint
is not included in this patch. No original/cohort/scoring changes. This is a
local rendered load/tick pass, not campaign completion or an official readiness
status update. Hosted/Windows/Android and live network sessions remain pending.

Slow motion remains unimplemented: genuine slowdown/speedup audio exists, but
verified slowmotion#0.tga/bullettime#0.tga sprites and coherent world/player
clock, input, drain and save integration are still required. The newly merged
assets tree has no tracked files with those sprite names.
