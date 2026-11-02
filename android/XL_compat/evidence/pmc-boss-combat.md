# Original PMC boss combat and vulnerable angles

Original PMC1 has six required static bosses of robot type 76. Normal starter
laser hits on boss 95 demonstrate frontal immunity and rear damage. A controlled
burst probe also retired that boss and reduced the required objectives from six
to five without a reactor countdown, but the player died. That is not a surviving
encounter or earned exit qualification.

## Difficulty correction and why

Pinned XL revision `2576a98b5de310e3ac7c9777df2677d1627b8bae`,
`ai/aiinit.cpp::InitRobotsForLevel`, sets the vulnerable-spot dot threshold to
`1/4 - difficulty/8`. `physics/collide.cpp::DoBossWeaponCollision` rejects hits
whose direction dot product exceeds that threshold. Our D2 runtime left its
threshold at zero for every difficulty. This made the accepted area too narrow
on Trainee/Rookie and too wide on Ace/Insane.

The XL-only initialization now sets the authored difficulty threshold. Starting
a stock mission resets it to the existing zero threshold. Save restoration
reinitializes it using the saved difficulty. Original boss definitions, mission
bytes, weapons and health are unchanged.

| Difficulty | XL threshold |
| ---------- | ------------ |
| Trainee    | 0.25         |
| Rookie     | 0.125        |
| Hotshot    | 0            |
| Ace        | -0.125       |
| Insane     | -0.25        |

The native boss integration test exercises ten actual collision decisions near
the side of a vulnerable-spot boss across all five difficulties, then saves,
changes difficulty and restores from disk. It also verifies the stock reset.
The test fails before the fix with `XL boss side-hit vulnerability does not
follow difficulty` and passes afterward. Existing independent boss lifecycle,
death-roll persistence, mixed reactor/boss objective ordering, malformed-save
guards and stock isolation also pass. The pinned spawning/objective oracle
still matches its 50 spawning cases and both objective destruction orders.
These are native controlled checks, not normal-player combat.

## Android scenario and limits

`test_xl_pog_runtime.ps1 -PmcBossCombat -PmcReactorPlaytest
-PmcSwitchPlaytest -PmcPlaytest -OriginalHub -MissionFlow`, with the complete
original resource arguments, appends a short local combat check to the normal
hub/switch/key/reactor scenario. It fires at the front, requires a recent hit
without health loss, then fires at the rear and requires damage. It retreats to
segment 664, saves and loads normally. The protected owner's fresh-process Load
Game check requires the exact saved boss health, while all six bosses remain
required and the countdown stays inactive.

The owner retains its pilot/settings backup, AVD identity checks and disabled
emulator recovery. Fixed poses isolate these interactions. No weapons, health,
damage or progression are granted. Full human combat, physical-device input,
connecting-gate traversal and an earned PMC exit remain unqualified.

Exploratory receipts on APK `c63885a409055938330680fc63bf6bdabea1cbdd`,
version 28790, are retained under `temp/xl-traversal-20261102`:

- `pmc-one-boss-fire`: setup/input steps pass, frontal hits leave 1375 shields
- `pmc-boss-rear-fire`: three seconds of rear laser fire leave 1141 shields;
  player survives with 51.814 shields
- `pmc-boss-rear-kill`: sustained fire loses aim under combat and times out;
  boss remains alive at about 354 shields
- `pmc-boss-rear-bursts`: boss retires and objectives become five, but the player
  dies; the survival assertion fails
- `pmc-boss-death-retreat`: the shortened attack loses aim before entering the
  death roll and times out; no successful death-roll save is claimed

Failed probe inputs were released and the owned game paused. These failures
are bounded combat/aiming or survival limitations, not evidence that rear
damage or objective retirement is unsupported. The difficulty mismatch was
found separately through source comparison and a reproducing native test.

The first protected combat script stopped before firing because its array-object
matcher stringifies scalar equality: integer `1375` did not equal reported
`1375.0`. The captured boss was at full health and the backup hashes matched
after restoration. The script now uses the matcher's numeric comparisons;
cold health equality is also checked directly by the protected owner without
rounding. This was a test assertion defect, not a gameplay failure.

The first short save/load probe did retain boss damage (1123 shields), but its
coordinate-free `pose_view` selected the world origin in segment 1167. Its
segment-664 expectation failed. The scenario now uses the existing reactor
probe's explicit retreat and alternate pose. This placement error is not a
boss-health persistence failure; the corrected endpoint is checked separately.

The corrected short continuation passes 11/11 steps in 30.966 seconds on the
same c638 APK. After warm save/load it is alive at segment 664, with boss 95
still at exactly 1123 shields, six remaining objectives and no countdown. Its
paused frame was inspected. This is the corrected local restore endpoint;
the full protected scenario still needs exact new-APK validation.
