# Original PMC3 guided-flight diagnostic pilot

The next unqualified progression step is switch 3, beyond a bent grated shaft.
The verified blue/gold state reaches the launch room but not the switch chamber.
This pilot does not place the player beyond the grates, grant equipment, change
damage rules or dispatch the switch. The authored switch route remains partial.

## Diagnostic change and exact build

Android debug introspection now exposes D2 guided_missile state. It validates the
live object identity, weapon ID and signature before reporting ownership,
position, velocity, forward direction and remaining life. It is read-only and
guarded out of D1. Inactive guidance reports active=false.

Committed source fbabcf01fbb24b737896eea354bc1147f44b3ac5, APK version28820,
83778370 bytes, SHA-256
6e29ef901f2a510a8e6eeec6f85cc3954690b33162b6ab9a034fe9aeabf4188d.
All six D1/D2 ABI markers match. The Gradle build passed in3m43s, scoped quality
passed, the installed APK hash matched and all26 owned profile files were unchanged.
Candidate: temp/pumo-guided-probe-20261102. The recommended published playtest
remains the unchanged selector bundle28810; this candidate is not a route release.

## Ordinary preparation and observed flight

- Original item80, ID40, is one guided missile. Normal flight from room2126 to
  item80 in2151 and ordinary weapon selection passed16/16 in44.957s on28810.
  An initial four-missile expectation was a test error, retained separately.
- Local launch staging and closed-grate flight passed13/13 in4.566s. Normal
  movement also consumed original shield16 in1048. No health grant was used.
- Original cloak68 was collected through normal flight from922 into923 on28820:
  16/16 in42.580s. The invulnerability item behind the grate was not collected.
- The first normal-fire diagnostic passed11/11 in54.309s, observing inactive to
  active guidance, owner/parent0 and ammo1 to0. Its final state was inactive after
  the missile struck the grate and its own blast damaged the player. This is
  diagnostic activation evidence, not successful navigation.
- The valid launch quadrant at(-36,-568.3207,-414), pitch16384/bank16384, crossed
  the first grate. Earlier attempts lost guidance while shields changed. The
  existing engine deliberately releases guidance when player shields change;
  that behavior was preserved.
- Four bounded ordinary laser bursts, with local repositioning and pauses for
  observation, reduced nearby robot169 from150 to78,60,24 and then destroyed it.
  A short ordinary burst destroyed robot193. These are controlled combat checks,
  not a human encounter qualification. Missed/occluded shots and an exhausted
  energy setup were retained as test limitations, not gameplay defects.
- The earned cleared-launch checkpoint passed10/10 in3.083s: both threats absent,
  one original guided missile, switch3 unfired and player shields89.043.
- Cleared flight passed8/8 in2.591s, with a rendered GUIDED view and unchanged
  shields. At the paused endpoint the active missile was object562/signature1553,
  weapon37, owner/parent0, segment1060, position
  (-35.997177,-620.893738,-411.665009), velocity(0,-60,0), forward(0,-1,0).

## Partial route and next probe

The first normal yaw attempt ended before completing the bend, with switch3
unfired and player shields unchanged. An earlier-turn replay lost guidance before
its turn threshold with a one-point shield change. No gameplay defect has been
validated. Timing, launch clearance and remaining combat exposure are unresolved.
The player remained above the grate throughout missile flight.

Separate corridor checks observed ordinary yaw changing the active forward vector
below z=-0.02, followed by an inactive transition after secondary input. Their
strict shield checks failed: in one trace a robot blast landed in the same frame
as the release input. A final isolated release attempt failed its active-guidance
guard. Clean release is therefore unqualified; these failed scripts are not
reported as passing control tests. No diagnostic or gameplay defect was validated.
The cleared checkpoint was restored after the pilot; no further route retries
are implied by this handoff.

Preserved cleared checkpoint: pre-pmc3-guided-cleared-player.tar,26 files,
16729088 bytes, SHA-256
8f64989397da744d003ef1ac68d9668125af676e8dbf42feefec84742557253a.
All earlier opening/blue/gold and guided checkpoints remain retained.
Scratch receipts, scripts, objective traces and rendered frames are in
temp/xl-traversal-20261102/pmc3-guided-*; run-owned-probe.ps1 requires the script
basename and verifies the owned AVD. collect-owned-probe.py preserves the result,
state, step trace, logcat and rendered frame. Device tests remain serial.

Next: restore this earned checkpoint, confirm the immediate threat/clearance
conditions, and measure yaw timing through1061 toward negative z, across the
second grate into1063/1065. Require normal shot activation of switch3 before
claiming its gate or red-route access. Human traversal and physical devices remain
unqualified. The separate rendered GuideBot pilot has different assistance and
unsupported-XL-exit boundaries; do not combine the two claims.
