# Pumo playtest follow-up and animated POG startup

Start with the ordinary phone kits. Use 32030 for the Kartsal ending and 32080
for the Mineral Valley ending. Collect the APK version, phone, Android version,
difficulty, controls, episode, checkpoint, last successful action, expected and
observed behavior, and the result after a normal save and load. Preserve the
original save. Investigate a reproducible report before further survey work.
[Phone directions](../XL_compat/PUMO_PLAYTEST.md)

## Reproduced startup failures

The Windows census recorded 1,534 of 1,654 starts at 120 unassisted frames.
Rerunning its exact engine and asset inventory reproduced all 17 animated POG
rejections at frame zero. Four are from the pinned distribution and thirteen
from separate archives. The individual case IDs and original-engine outcomes
are in the census. [Baseline](../XL_compat/evidence/gameplay-census-integrated-20261120.json)

The pinned original engine at `2576a98b5de310e3ac7c9777df2677d1627b8bae`
again selected Chaos Theory XL directly from `levelpack.7z`, descriptor
`darkflamewolf/ctheo-xl.mn2`. Ordinal 4 (`standoff.rl2`) and ordinal 6
(`clminate.rl2`) each ran 120 frames at the default texture quality. Redux
rejected the first packed door strip at bitmap 2434 and 2280, respectively.
Original clock traces associate those slots with wall clips 36 and 20.
Both levels store native 7/31 geometry in an XL mission.

## Implementation and acceptance

Trace original POG binding and source frame selection. Reuse the existing XL
wall clock, immutable side views and save ownership wherever those paths
already apply. Keep gates for unsupported uses and compare each newly exposed
first blocker. Repeat all 17 cases, keeping different distributions separate.
Use the original levels and a targeted native control to check playback as
well as startup. A 120-frame idle probe measures mission admission and
simulation only. Use one local gameplay worker and hosted Linux checks.

Publish locally tested layers as ready-for-review PRs to the personal fork.
Register the stack, verify selected CI jobs and leave the PRs open. Keep game
data, profiles, raw private receipts and saves on the build machine. Carry
forward the exact remaining cases and the first unresolved gameplay action.
