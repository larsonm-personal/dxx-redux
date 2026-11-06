# Sphere unused animation joints

Continue from reviewed candidate #385 without changing its published branch.
Pinned ai/aianimate.cpp skips a joint when jointnum >= the active model count.
Sphere object 243 uses robot 18, HXM model 29 with three submodels, while retaining
stock joint ranges 360..379. Establish actual row targets before admission.

Retain raw tables/counts/ranges/source identity. Require nonnegative targets
within MAX_SUBMODELS and valid offset/count bounds. Keep gun/parent/model guards.
For XL only, skip nonrendered submodel rows before any angle-array pointer or
classic debug trap. Preserve classic behavior and the source test-only state
setter's bounded storage behavior. Compare source/native animation with mixed
valid/unused rows, all states, valid boundary and malformed cases; test disk
restore and classic isolation, then unchanged Sphere under the genuine renderer.
Do not claim readiness until actual original load and 120 ticks complete.

## Exact input and reference evidence

Sphere sp-02.rl2 SHA-256 a821cc678a6e9f574fe5ec96b9475533e687aa338676bd5377cde3818355146b; companion sp-02.hxm 78686e51eaf9849e8cd9bb798920ce7d6eec81249a92c4f5ca40560383d128d9. Object 243 is robot 18, replaced model 29 (three submodels), two guns, retained ranges 360..379. The unchanged retail descent2.ham SHA-256 5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d has 1,145 joint rows at byte 81,094: rows 360..369 alternate targets 3/4 and rows 370..379 alternate 1/2. HXM does not replace these rows.

Pinned reference revision 2576a98b5de310e3ac7c9777df2677d1627b8bae, ai/aianimate.cpp DoSillyAnimation skips targets >= ModelCount before forming pointers. Extracted function is compared directly with 252 native cases. Every raw row/range stays intact. Admission still rejects negative/storage-overflow indices, invalid ranges, missing gun submodels and invalid model parents. Newly skipped rows require positive gun count: Redux's pre-existing zero-gun early return differs from the reference and is not expanded here. Classic path remains unchanged.

The remaining consumers are robot_get_anim_state (raw range), ai_frame_animation (actual model count), and test-only set_robot_state (bounded fixed angle storage); save/restore retains raw gameplay definitions and source identity. No table normalization or index remapping.

## Validation and remaining limits

- Pinned source/native animation: 252 exact comparisons, all seven goal states, attack profiles, model counts 1/3/10, gun counts 1/2/8, overlapping ranges, mixed and all-unused rows. Complete angle/goal/delta/state output compared.
- Native HXM suite: original load/combat/resources, bounded unused-row normal load, raw definition identity through disk save / stock load / restore; negative and overflow indices remain rejected. The formerly rejected bounded index 9 remains an explicit positive test. Unqualified zero-gun rows are explicitly rejected.
- Public owner: all 54 contracts including source, GPU and audio pass. Classic upstream and trigger-demo CTests: 2/2 pass. Scoped mixed-language quality passes. Automation catalog validation passes; catalog execution is blocked by the Linux child supervisor's unavailable /proc/self/task/.../children interface (exit 125), not counted as passing.
- Supplemental genuine-renderer original reproduction uses this branch + #384's diagnostic entrypoint only, genuine 22-resource assets revision b5f68b8f433e0fc170306a168534304ae64f7720 and genuine Vertigo d2x.hog. Binary SHA-256 57197129d29e2be0fe14f0b4b7d078f26edb83ca058f344c1f6529ea894b1cf6. Sphere passes the object 243 joint blocker but still fails level_load at object 311 (robot 45), HXM boss byte 233 / signed -23. No rendered 120-tick success and no official readiness status update. Diagnostic entrypoint is not part of this patch.
- No Windows/Android runtime or hosted exact-head result claimed by local validation.

## Independent Warp Shock follow-up

Powerup 48 is actual slow-motion equipment (49 is bullet time), not an alias for an existing pickup. Pinned objects/powerup.cpp PickupEquipment and physics/slowmotion.cpp require ownership flags, world/player clock state, energy drain, transitions, input and save state; physics, AI, weapons and effects consume the separate clocks. Genuine slowdown.wav/speedup.wav are present in the authorized d2x-xl.hog. Required addon sprites slowmotion#0.tga/bullettime#0.tga were not found in the staged assets or addon archive. No partial admission or substituted rendering is implemented. A coherent clock/runtime integration and genuine sprite inputs remain prerequisites.
