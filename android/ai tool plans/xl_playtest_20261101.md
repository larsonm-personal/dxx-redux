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
