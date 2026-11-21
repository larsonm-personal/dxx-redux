# Next Pumo and XL gameplay tranche

Start with physical-phone feedback on the ordinary Pumo kits. Verify the APK
and supplied save with the kit manifest before testing. Start with the short
32030 Kartsal ending or 32080 Mineral Valley ending. Use `umbriel-blue` or
`mercury-arrival` for a longer session. Record the first movement, switch,
combat, save, or briefing action that differs from the tester's expectation.
Keep the kit version with the report and preserve both the supplied save and
the pilot's pre-issue save. [Phone directions](../XL_compat/PUMO_PLAYTEST.md)

| Approximate effort | Work and completion evidence |
| --- | --- |
| First hour | Intake and prioritize physical-phone results by data loss or crash, progression, controls and combat, save/load, then performance that prevents play. Replay the shortest sequence with the same original assets, checkpoint, difficulty and controls. |
| Three hours | While awaiting a report, repeat the verified Windows Toxic-D3 room 180 → 179 door flight on a rendered Android build. Select the pinned `toxic-d3.mn2` ordinal 1 from `levelpack.7z`, open the door at room 180 side 4 and restore a save taken during door movement. Compare visible pixels and collision on both sides after load. Repeat on the standalone distribution and keep its result separate. |
| Four hours | Reproduce the next startup gap. For pinned Pyrotech D3 ordinal 1, check whether bitmap 2344 on clip 27 has an indirect consumer. The pinned level has zero clip-27 wall records and zero direct texture-664 sides. For pinned Chaos Theory XL ordinals 4 and 6, establish door lifecycle and restore for the native 7/31 geometry before enabling packed wall strips there. Use the original and nearby controls and repeat all affected distribution cases. |
| Two hours | Run ordinary Windows gameplay and applicable Android checks, inspect selected hosted Linux receipts on each exact fork head, and publish a dated execution and continuation handoff. |

Keep same-distribution evidence distinct from alternate archives. The latest
Windows count is 1,539/1,654 successful 120-frame starts, with 115 remaining.
Preserve the distinction between
startup, a fixed-position check, connected ordinary-input steering, rendered
Android automation and a person playing on a phone. Follow the receipt of one
local gameplay worker. Keep private archives and saves on the build machine.
Keep XL policy and implementation in `android/XL_compat` and native hooks narrow.
[Windows cluster and remaining categories](../XL_compat/evidence/gameplay-pog-wall-startup-20261121.json)

Use ready-for-review PRs on `larsonm-personal/dxx-redux` and the immediate parent
branch for dependent changes. Register the GitHub stack, verify workflow
admission and selected results for every pushed or restacked head, and leave
the PRs open unless this run grants merge authorization.
[Existing stack](https://github.com/larsonm-personal/dxx-redux/stack/720)
