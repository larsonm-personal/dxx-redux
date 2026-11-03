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
