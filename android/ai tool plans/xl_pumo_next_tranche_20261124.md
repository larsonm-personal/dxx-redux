# Get every cataloged D2X-XL level into flight

## Where we stand on November 24

The last complete Windows run started **1,595 of 1,654 advertised levels (96.4%)** and ran each for 120 frames at 60 Hz on Hotshot, with seed 1 and no player input, death or level change. The pinned catalog passed 1,070/1,111 and the local catalog passed 525/543. A catalog entry identifies the distribution, mission and advertised level number. Copies in different distributions count separately.

Track two milestones for each entry: first, the requested level actually enters gameplay; second, it passes the existing 120-frame test. Four idle ships died after loading, so **at least 1,599/1,654 entered gameplay**. Check the individual runtime records before assigning the remaining entries a load result. A timeout alone does not tell us how far the engine got.

[Full census and the 59 case IDs](../XL_compat/evidence/gameplay-census-alien-frontier-20261124.json)

| First problem recorded | Entries still short of 120 frames |
| --- | ---: |
| Level file named by the mission could not be found | 34 |
| Water or other fluid behavior | 4 |
| Rooms with geometry the Redux loader rejects | 4 |
| Idle pilot killed by robots | 4 |
| Unsupported level format | 3 |
| Custom base definitions, robot definitions, lightning, waypoint controllers | 2 each |
| Robot generator capacity | 1 |
| Timeout | 1 |
| **Total** | **59** |

The original D2X-XL started 23 of those 59, rejected 31 missions, could not find the advertised file in three cases, and has no verified simulation result for two. These observations describe the same 59 entries. The first-problem labels come from the prior register. Recheck the current error before changing code.

## Work order

1. **Start with Kalisto, level 1.** Use the local catalog entry `levels/kalisto.zip/kalisto/kalisto.mn2:1`. Reproduce the failure with the normal readiness runner. Inspect the rejected room shape and compare how pinned D2X-XL loads those same bytes. Fix the format handling in its existing owner. Repeat level 1 of Kalisto, Temple of Helix, Code Yena XL and Coordination XL, plus an already passing ordinary room as a control. Count each result independently. If a second problem appears after the first fix, record it against that entry.
2. **Audit the 34 missing-file entries.** Match each advertised filename against the exact mission package, including nested archives, paths and case. Check how the original resolves it. Fix reproducible lookup failures where the file exists. Record the exact missing member for incomplete packages, keep them in the denominator, and track any corrected distribution separately. Do not substitute another level.
3. **Clear the remaining loader failures by shared cause.** Start with the four fluid cases, then unsupported formats, definitions, effects, waypoints and generator capacity. Prefer changes that open multiple real levels and cases that already start in the original. Reorder when the receipts identify a larger shared cause. Timebox the single timeout by recording the last completed loading stage. Keep the four idle deaths in the post-load queue and preserve ordinary game rules.
4. **Rerun both complete catalogs on the final runtime change.** Compare by case ID against the frozen baseline. Report newly loaded levels, new 120-frame passes, regressions and the first remaining failure per entry. Keep targeted results provisional until the full comparison finishes. Report actual counts even if an archive is incomplete.

## How to test and ship

Use [the census runner](../tests/xl_readiness_full.py) with the matching inventory, engine, resources and archive hashes. Select a single ID with `--case`, repeat the flag for a cluster, and omit case filters for the full inventory. Keep one local gameplay worker and read its per-case receipts. Use the same level number, inputs and test settings in windowed D2X-XL and Redux. Verify a representative newly loading level through the ordinary Android launcher. Keep device display settings, private game inputs, profiles, saves, screenshots and raw logs local. Use hosted Linux for routine cross-platform tests.

Publish coherent, locally tested layers as ready-for-review PRs only in `larsonm-personal/dxx-redux`. Fetch and inspect the live stack before branching. Base each layer on its immediate predecessor, verify the predecessor's public replay admission before opening the dependent PR, and register it in the active stack. Verify the pushed commit and the tests that ran. Inspect admission logs for skips, retain retry evidence and fix material failures. Leave PRs open for review.

End each run with a dated report, updated counts and failure groups, the next specific level to try, and a replacement prompt. The next run can begin on the build machine without a phone playtest from the user.
