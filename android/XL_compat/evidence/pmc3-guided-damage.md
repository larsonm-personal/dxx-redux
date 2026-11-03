# PMC3 guided-flight damage diagnostic

The earlier guided-missile pilot could not distinguish projectile collision from
player damage releasing guidance. This follow-up uses the same preserved earned
checkpoint, not a granted missile or an assisted objective.

## Exact diagnostic and protection

Source `7f45fb2ac2c0f33c0227b1749776791e27dfd869`, version **29130**.
Universal APK SHA-256:
`cbda4ca36aa6e66d6b733763ea850a9b1f775e9fa98258b1deb92324e80a45d3`.
All six D1/D2 ABI IDs match. Universal build passed in 4m37s; Windows D2 and
scoped quality pass. The first quality run could not launch the pinned formatter
inside the sandbox; the scoped elevated rerun passed.

The installed APK hash matches and all 26 staged profile/settings files survived
installation unchanged. It is retained at
`temp/xl-phone-fog-20261102/candidate-7f45fb2-universal.apk` with identity and
installation receipts. This is a diagnostic, not a replacement for the published
version 29080 phone candidate. No physical Samsung validation is claimed.

Only owned `DxxPhoneFog20261102` / `emulator-5556` is used. Before staging, the
current profile was backed up in `pre-guided-route-profile.tar` with per-file
hashes. The earlier earned checkpoint archive SHA-256 was verified as
`8f64989397da744d003ef1ac68d9668125af676e8dbf42feefec84742557253a`.
Only its Players directory was staged; original resources remain unchanged.
The emulator display was reduced from 1080x1920 to 540x960 for timing probes;
hazard rendering, physics, damage and original bitmap apertures remain enabled.

## Finding and ordinary combat

The new Android debug `last_player_damage` snapshot is populated only after
existing damage admission. It records a process-lifetime sequence, game time,
level, amount, before/after shields, source identity/position and the active
missile snapshot. It does not change gameplay or saved state. The last event
remains visible across a quickload; compare its sequence/time before attributing
it to a later attempt. Non-introspection builds use a no-op stub; the call site
is Android D2 only. D1 still compiles in all three ABIs.

- Earned checkpoint load on the diagnostic: **9/9 PASS**, 19828ms.
- Damage observation: **9/9 PASS**, 19328ms. Source robot type 2, ID 29,
  signature 147 (live object 146), segment 1030 caused exactly 1 damage.
  Shields changed from 89.0431976 to 88.0431976 while the guided missile was
  still active in segment 1049 at approximately (-35.382, -577.814, -411.665).
  Guidance was inactive afterward. Snapshot arithmetic and source identity
  were checked against the final state. This explains a concrete combat
  interruption; it does not establish a steering or collision defect.
- Ordinary laser fire from accessible adjacent segment 1047 removed robot 146:
  **11/11 PASS**, 22244ms, unchanged shields, energy 57.86 to 55.17. The original
  guided missile remained available, cheats/invulnerability were off, and
  switch 3 remained unfired. Nearby segments 1049/1043/1042/1047/1030 connect
  without intervening walls; the local approach does not bypass the grates.
- Saving the three-threat-cleared launch checkpoint: **11/11 PASS**, 2629ms.
  Original robots 146, 169 and 193 are absent. New retained Players archive:
  `earned-guided-three-threats.tar`, 17324032 bytes, SHA-256
  `a1dfafa1030cbb67e47d91fdc02223324b9614003f4f64e440f8fc978c839956`.

## Current route limit

The subsequent bend attempts retain unchanged shields but lose active guidance;
switch 3 remains unfired. No red-route access is qualified. The original first
and second grates, projectile speed and damage-release rule remain unchanged.
Further measurement is bounded; a partial route is not a gameplay defect.

Receipts and scripts are under `temp/xl-phone-fog-20261102`, named
`guided-diagnostic-load`, `guided-damage-inspect`, `guided-threat-146`,
`guided-three-threats-save`, and subsequent `guided-turn-*` probes.
The existing input-demo recording attempt did not start in this checkpoint;
its rejection is retained, without claiming a supported XL recording workflow.
The rendered companion GuideBot pilot has separate sandbox/unsupported-exit
limits and is not evidence for this ordinary guided-missile route.

## Paused for renewed phone fog failure

During this follow-up the user reported that the published version 29080 still rejects
original hub fog on Samsung. Phone diagnosis takes priority; no further guided
route attempts are implied.

The wall-contact diagnostic was built from
`5862a0230765c9072db032dc28df6ffa081eadd7`, version 29140, all six IDs matching,
APK SHA-256 `91c7ad8cf07fab8e5c18471083e94f06ffd29d70d4835079ea550456a2f65d1b`.
Universal build passed in 2m53s; Windows D2 and scoped quality passed. Installation
preserved all 26 staged files. The earned save load passed 9/9 in 19301ms. The
wall-contact snapshot itself has not yet been exercised; it remains unqualified.

The owned emulator profile was restored from the pre-pilot archive with all 13
protected files matching. Display size was reset to its original 1080x1920.
The earned three-threat checkpoint, both diagnostic APKs and all failed turn
receipts remain preserved. The owned persistent Gradle daemon 32548 and worker
34636 from the first diagnostic build were identified by parent and creation time
and stopped after completion; the next build used a single-use daemon. No user
process or physical phone was operated during this pilot.

## Wall-contact qualification on delivered version 29160

After D24 candidate delivery, the same owned emulator loaded the preserved
three-threat-cleared checkpoint on exact 18d06ec3/version 29160: 9/9 PASS in
18923ms. A fresh profile backup preceded staging; only Players was replaced.

The ordinary flight diagnostic passes 12/12 in 19754ms. The guided missile
clears the first grate and hits authored segment1061 side5 wall97, base texture49
and animated fan overlay354, at (-35.1921,-641.2290,-428.4727). Guidance is active
at contact; last_player_damage is null and shields remain 89.0431976. The contact
identity agrees with the unchanged original pmlev3.rl2 wall record (type5).

Two bounded steering observations also hit wall97: approximately
(-35.6640,-648.3489,-428.4727) and (-35.5061,-652.9714,-428.4727). The first
steering expectation timed out before seeing its requested direction; the second
collision-observation probe passes 13/13 in 18642ms. These are diagnostic results,
not route passes. Switch3 remains unfired; no red-route access is earned. Coarse
emulator frame/input observation and animated aperture timing remain limits.
Do not classify this evidence as a gameplay defect or force the trigger.

All 15 protected profile/settings files restore exactly from
pre-guided-contact-profile.tar. Evidence is retained as guided-depth24-load,
guided-contact-observe, guided-fan-turn622, guided-fan-turn618 and
guided-contact-validation.json under temp/xl-phone-fog-20261102. The current
phone candidate and earlier bundles are unchanged. The wall-contact diagnostic
itself is now exercised; the guided progression route remains partial.
