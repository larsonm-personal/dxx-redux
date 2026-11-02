# Original Pumo playtest - November 1, 2026

Cutoff: November 1, 2026 21:00 America/Los_Angeles (November 2 05:00 UTC).
Reserve the final 20 minutes for terminal tests, resource cleanup and handoff.

## Baseline

- Local and remote cmake: ea3e6f320fb513464dd45d1459401ee944b58583
- PR 277 is merged; its required work is present in cmake
- Preserve the existing untracked pumo_gameplay_first_playtest.md
- First branch: xl_playtest_123_opening, targeting cmake
- Only fork larsonm-personal/dxx-redux publication is authorized; no PR merges
- Existing emulator-5554 belongs to the user; this run owns emulator-5556
- Scratch receipts: temp/xl-playtest-20261101

## Work order

1. Verify original opening launch, ordinary input, combat, progression,
   damage/death/restart and save/load using protected fixed probes
2. Fix opening blockers and deliver an installable build with exact identifier,
   launch steps, verified boundaries and feedback instructions
3. Audit original pmlev1/2/3 gameplay consumers and unblock additional levels
   without changing mission content or bypassing unsupported gameplay
4. Use coherent approximately hourly stacked PRs; monitor ancestors, repair
   failures on their branches and propagate fixes to descendants
5. Leave graphics-only work until gameplay delivery; preserve hazard/information
   visuals and all working existing features
6. Record stack heads, validation, current-head CI and artifacts in fresh handoff

## Evidence limits

The earlier controlled original route covers hub and Mineral Valley through
the level-ten interlude. It does not establish ordinary combat or human traversal.
New fixed probes must retain that distinction. Device tests remain serial.

## Opening delivery checkpoint - 11:56 PST

Delivered local `temp/pumo-playtest-20261101.zip` (269 MiB), universal debug APK
and original HOG/descriptor/sound resources plus required effect images. Source
runtime is the merged baseline; this branch adds reusable verification, installer
and playtest notes. User-facing instructions: `android/XL_compat/PUMO_PLAYTEST.md`.

- Universal Android build passed in 10m48s; ARM64, ARMv7, x86_64 included
- Original opening owner passed 64/64 in 170021 ms on the universal APK
- Run ID: 63841fd3124044538b1e191493b754e7
- Normal menu start, ordinary equipment/input/fire, damage/death/restart,
  checkpoint and menu save/load, authored exit, physics pickup and robot kill
- Protected manifests match before/after; installer also preserves pilot hashes
- Fresh and already-present mission installation paths both pass
- Original package import test passed, 1 test, 0 failures, preserving 205 HOG
  members, 14 levels and 303 playable music entries; not device-import coverage
- Automation catalog: 99 standalone JSON, 502 support, 314 PowerShell tests
- Master catalog: 427 top-level entries; first attempt timed out under universal
  compilation, retry passed after compilation completed
- Scoped quality and diff whitespace checks pass

The first two opening attempts stopped on test assumptions (starting-level dialog
and numeric 100.0 comparisons), not gameplay defects. A 41-step intermediate pass
preceded the final 64-step pass. Installer probes corrected the retail-data path
assumption and empty-backup tar handling before delivery. No production gameplay
or admission code changed. First delivery took approximately 22 minutes.

Next: publish A, continue B from A, audit pmlev1's single camera and original
object-trigger binding before choosing the smallest gameplay-support increment.
All device/native processes from the opening checks are terminal. Owned emulator
5556 remains available and now has the local playtest resources installed.

## Camera gameplay increment B

Branch xl_playtest_124_camera_gameplay, based on A 9bc9dbac9a213cb8d5cf8db6956d9b25824382bd.
Extend existing camera contact/radial gameplay to concussion, homing, guided and
mercury missiles. Preserve source damage, force, visibility, sound, impact motion,
projectile retirement, camera indestructibility and persistent-hit ordering.
Keep flash, child-projectile, mine-chain, seismic and lightning profiles guarded.
Optional missile blast decoration is deferred under the gameplay-first priority;
ordinary explosion and impact visuals remain active. Original admission stays
closed until all required camera consumers and authored monitor information work.

Fresh current inspector confirms pmlev1 camera 102 binds monitor segment 719 side 4;
pmlev2 has cameras 206-208 and six monitor faces; pmlev3 has five cameras/monitors.
Pmlev1 also needs its authored lightning profile, and pmlev3 has three unsupported
lightning owners. These remain explicit next dependencies, not bypasses.

B native verification: 56 ordered contacts, 256 actual physics frames, 4800 radial
robot cases, 2160 source-compared explosion-motion samples and 32448 rotation
samples pass. Windows pinned-reference CTest passes. Fresh rebuild required the
MSVC environment (x86 runtime, x64 contracts); no new compiler warnings. Initial
scoped formatter could not launch clang-format in the sandbox; authorized retry
with the installed formatter completed successfully. No original admission change.

B final checks: Linux pinned-reference CTest also passes. Universal Android build
passes in 2m27s. Opening regression passes 64/64 in 116707 ms on emulator-5556;
protected before/after manifests match. Final native owner includes source motion,
rotation, controller and hitbox comparisons. Scoped quality passes; diff whitespace
check passes. The delivered ZIP remains the verified baseline opening artifact.

## Camera monitor increment C

Branch xl_playtest_125_camera_monitors from B 3dec70b7d7d8de78976c971ba5347202b451b15c.
PR 279 targets PR 278's branch. Integrate existing source-qualified bindings,
scheduler, capture and alignment with ordinary native face submission. Preserve
base/overlay selection, independent texture coordinates, vertex lighting, opaque
camera image semantics, visibility scheduling and context-loss recovery. Camera
views must not recurse or alter automap/gameplay visibility. Test through the
existing protected Android GPU owner; keep device tests serial. This work is
needed for the authored monitor information, not optional graphics polish.

C validation: Windows native build and full projectile/save owner pass. Protected
Android GPU owner passes 33/33, 52 monitor checks before and after real context
recreation, run 7839675773be4a0db3f6bf3eb3c54d99 (55929 ms). Protected manifests match.
Catalogs pass (99 standalone JSON, 502 support, 314 PowerShell; master 427 entries).
The initial device attempt installed the older outputs APK after an IDE-injected
ABI build wrote intermediates; its missing-counter failure did not test C. The
correct test-only APK passed 48 checks after installing with -t. Final rebuild uses
the repository-supported -PciTestAbi=x86_64 and the normal outputs APK, passing52.
No original mission bytes or admission gates changed. Remaining camera gameplay,
static camera behavior and authored lightning dependencies still block PMC levels.

### Stack D: passive camera contact and robot save state

- Preserve source-enabled robot/fireball collision pairs without introducing a
  damage, bounce or impact-effect response
- Reuse the qualified default sphere query and reverse-query unsticking, with
  unchanged-velocity retry through real physics
- Include robot repair state in complete identity-validated save snapshots
- Native projectile owner passes 48 passive frames and full player/robot disk
  restore through a stock mission; existing malformed-save checks also pass
- C's Linux isolated GPU harness link failure was fixed on C, locally tested,
  pushed, and fast-forwarded into D before restoring D's working changes
- Scoped quality and Android x86_64 compilation pass (3m7s); the combined-stack
  opening regression passes 64/64 steps in 111717 ms with protected data unchanged
- Continue source qualification of flash/child-projectile/static-camera behavior
  without relaxing admission

### Stack E: source-supported lightning presentation profile

- Audit original PMC effect owners and their source consumers before treating
  their visual-detail flags as gameplay blockers
- Select source global style 0 and parent-only lightning; preserve original
  metadata, visible cues, dynamic lighting, activation, audio and saved state
- Qualify the source's baseline-cursor clamp behavior, including original PMC
  life/node/population parameters, rather than replacing authored mission data
- Keep fixed endpoints, planes, moving waypoints and unknown gameplay guarded
- Windows/Linux source and admission controls pass, including 17,280 bolt ticks
- Native effect lifecycle passes; Android build passes in 4m15s and protected GPU
  regression passes 33/33 in 62123 ms with protected data unchanged
- Resume camera flash/child-projectile qualification after this smaller blocker
  is resolved; do not spend the session on higher-detail lightning appearance

### F - Camera flash missile gameplay (in progress)

- Child `xl_playtest_128_camera_missiles` of E `9579fd86b6296bf0303f98e2281fa3602d2c14c2`, PR282 targets PR281
- Preserve source flash robot stun, player pulse, fade delay, compositing, and saved faded color through an XL-only palette owner
- Source-extracted comparisons cover robot signed-byte skip behavior and palette clocks, random draws, self pulses, and save/restart fade semantics; Windows comparison passed
- Validate real camera impact, ordinary damage palette composition, pause, full save through stock, and malformed-state atomicity before publishing
- Other child/seismic/missile-lightning profiles and original camera admission remain gated

F validation checkpoint, 21:46 UTC: Windows and Linux source comparisons passed
(10800 robot stun cases, 140 player pulses, 166400 palette/save-composition ticks).
Native real camera impacts, active palette clock/FX isolation, full save through
stock and 34 malformed-state controls passed. Existing full effect/resource/save
owner also passed. Scoped mixed-language quality and diff whitespace checks pass.
Android x86_64 build passed in4m43s; original opening passed64/64 in146811ms,
run26fb405d9b0f475a93117b2bfc225584. Protected before/after manifests match.
Receipts: temp/xl-playtest-20261101/camera-flash-*.
A/B CI fully green; C Linux jobs all succeeded at21:45; D headed replay pending;
E Engine/Linux pending, other checks green. The delivered universal opening ZIP
is unchanged. F APK output is a test x86_64 build, not a replacement bundle.

### G - Smart child missiles and camera splash chains (in progress)

- Child xl_playtest_129_smart_children of F f0512165c412682278797c171a4f6aa130c24f81, PR283
- Port source target filters, exact distance/direction math, modulo target selection with repeats, random direction and six-child creation
- Qualify real child ownership, target/direction, retained projectile gameplay, mine-chain ordering and seismic consumers before camera admission
- Missile lightning rods are separately source-switchable decoration (lightning.bExplosions=false); retain primary explosion information and all damage/force/audio/glass/seismic behavior
- Original PMC camera save/restart/transition/device checks remain the next admission milestone

G validation: scoped quality passes; Windows/Linux source controls pass
(1400 profiles, 5040 directions). Native 90 ordered missile impacts/180 children,
36 targeting profiles/216 children, real child damage and full save through stock
pass. Existing effect/resource/save owner passes. Original camera admission stays
blocked pending earthquake glass exact-distance/trigger behavior and PMC tests.
Android build and protected opening regression are next. D CI fully green.

### H - Earthquake glass gameplay (in progress)

- Child xl_playtest_130_camera_glass of G31bcf2a82d305173e7758e32277702398ac52993, PR284
- Preserve source bidirectional uncached blast routes, exact-distance gates, glass effects and linked trigger operation
- Keep original PMC admission blocked until source/native controls and actual original camera lifecycle checks pass
- G Android build passed in5m1s; protected opening regression is running on owned5556

H controls: 66384 actual-source route cases pass Windows8.59s/Linux2.84s.
Nine native glass cases/four real shield-trigger effects and full save owner pass.
Permanent switches retain texture/light, animation flags/time and UV rotation
survive destruction. Glass-trigger level changes stop old traversal/children.
Final scoped quality passes. A header-order compile issue was fixed by using the
existing preview API header for the transition generation; Android rebuild pending.

### I - Original PMC camera playtest (in progress)

- Child xl_playtest_131_original_pmc of H fec2bdbfd3667d7c000aa1a3b1014e418bf80e89, PR286
- Narrow admission to stationary authored behavior128/129 camera objects and their ordinary monitor bindings; keep static135 and fixed-end lightning gated
- Reject unsupported custom splash/child/flash definitions during camera resource preflight
- Qualify unchanged pmlev1/2 through actual original hub exits, controls, combat, monitors, full save/load and death/restart on Android
- Deliver an expanded universal bundle after these checks, with original addon resources and preserved opening instructions

I is draft PR287 at ea25a8586a95aacee34382ba9be1e7adad72c444, targeting H.
Original PMC1 completed controls, camera contacts/monitors, exact life decrement,
static displays, disk restoration and active robot89 combat through step56 of
run6d9a9c7160204d56bb58fe8fd1b8ec7e. The complete114-step run failed at PMC2 HXM
sound dependency admission. Universal build3m34 passed. Installer and opening
qualification of the PMC1 bundle are running; no full PMC2 claim.

### J - Original PMC2 resource semantics (in progress)

- Child xl_playtest_132_original_pmc2 of I ea25a8586a95aacee34382ba9be1e7adad72c444, PR287
- Preserve source silent-sound table semantics for the original pmlev2.HXM
- Retain invalid-index and missing-mapped-sample rejection
- Finish original PMC2 gameplay, save/restart, monitor and context-loss qualification
- Continue source qualification of PMC3 only after delivering verified expanded slices

J validation: native silent/mapped/missing/invalid sound controls and full
combat/save-through-stock pass. Original PMC1/2 lifecycle114/114 passes in221309ms,
run7790e5cf218c4acbb74ac37d082c8fc1. Expanded117/117 passes in229829ms,
run1886847229b64b34a0e74a480f10918e, adding ordinary combat against original
PMC2 HXM robot174. Both protected manifests match. Scoped quality and catalogs
pass. The dedicated lightning source suite also passes Windows1.97s/Linux0.43s,
including the original-coordinate accumulation regression; the earlier effect
suite is separate coverage. Universal PMC2 packaging is next.
### Static robot contact correction (November 1)

Original PMC displays include behavior135 robots with zero authored velocity.
Initialization alone was insufficient: source static player contact suppresses
impact effects/AI, static energy hits suppress smart blobs, and static bump uses
CBumpForces rather than Redux impulse handling. The qualified default sphere
query also records target and reverse-query hit state. Gauss uses source signed
random thrust. Static companions/thieves and nonzero authored velocity remain
blocked.

5040 full pinned-source BumpTwoObjects cases match force/rotation/velocity on
Windows and Linux, including zero masses, player/weapon, normals, persistence
and boost. Native original Vertigo controls pass ordinary versus static energy
hits, player contact, FVI, reverse repair and full disk save/load. The derived
authored test fixture does not modify Pumo assets.

Source objects/objectio.cpp LoadState omits MT_NONE physics and CT_NONE AI
payloads, retaining freshly initialized level-slot data. Redux clears those
slots, so restore now preserves the original static baseline before that clear
and reapplies only omitted payloads to matching robot slots. No save-format
extension was added. Android PMC probes now compare behavior, mass, velocity
and position after restore. Device rerun and a replacement playtest are pending.
