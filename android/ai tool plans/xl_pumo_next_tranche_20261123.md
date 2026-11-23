# Next Pumo and XL tranche after the S21 run

## First action: physical phone feedback

Use ordinary kit **32320** and its manifest in ignored
`temp/xl-pumo-phone-s21-20261123`. Start Mineral Valley on Rookie with touch.
The reported failure was a missile shot in Crauphel Scilab, near room 129,
on SM-S931U, Android SDK 37, Play internal build 31950. Record the missile,
aim landmark, last successful action and whether saving before the shot,
closing, loading and repeating produces the same exit. Preserve the original
save. Fixed-pose and S21 automated shots passed, but neither reproduces human
steering. This touch shot remains the first unresolved gameplay action.

Keep **Show progress bars** enabled. Time difficulty selection to the first
briefing, score-screen advance to the next briefing and final briefing-page
dismissal to ship control as separate waits. Record the visible label, whether
the bar moves and approximate elapsed time. The score-to-briefing bar has an
S21 diagnostic witness; the last-page-to-flight gap still needs ordinary-phone
observation. Ask for the first words or a screenshot of the green-on-green
briefing page. Each report needs kit version, phone, Android version,
difficulty, input method, episode, checkpoint, last successful action and
save/load result.

## Independent engineering while waiting

The frozen integrated Windows baseline is 1,534/1,654 starts at 120 frames.
The current-head rerun of all 120 prior misses measured 24/120 passing and
96/120 still blocked. Together with the 1,534 prior passes, this supports
**1,558/1,654**, leaving **96/1,654** if those prior passes remain unchanged.
This is not a full 1,654-start census rerun. Keep missing
advertised members and alternate distributions separate. The original Kore
archive is 1/1, Chaos Theory XL is 6/6, Playground XL is 8/8 and Anthology
`antho1-3` is 3/3 after this stack. The levelpack Playground equivalent also
passes at its separate advertised ordinal. These tests establish startup, not
complete gameplay or visual parity.

The next narrow runnable-original lead is
`levelpack.7z/missions/lotw/bay32.msn:1`. The frozen survey ran 120 frames but
reported `missions/BAY32.rdl` for advertised `bay32.rdl`; determine whether
this is a case-insensitive resolution difference, a different member, or a
survey identity error before changing either engine behavior or the census.
If a change is justified, replay the exact levelpack descriptor and its
related cases with unchanged ordinal and unassisted inputs. Record the first
newly exposed blocker. The already fixed Anthology powerup and levelpack
Playground entries should not be counted again.

The levelpack Chaos Theory level 4 now reaches XL effect objects and lightning;
level 6 reaches additive wall rendering plus 14 other requirements. The three
bio/etomb archives require multiple segment behaviors and are a
larger project. The 16 light-index bounds include truly out-of-range packed
data and need a pinned source rule. The 34 missing advertised members need
distribution analysis. Use one local gameplay worker, read its receipts,
validate focused changes with Windows builds and exact archive replays, and
use hosted Linux fork CI for routine cross-platform checks. Use the S21 for
Android behavior. Keep private game assets, profiles, saves and logs off Git.

## Fork and handoff

The ready-for-review personal-fork PRs #726-#735 are registered in
[stack 720](https://github.com/larsonm-personal/dxx-redux/stack/720), each
based on its immediate predecessor. Check the final execution report for
exact heads and CI results. The private XL workflow may skip a PR below the
native stack tip; inspect admission evidence before interpreting skips.
Fix material failures, retry infrastructure failures with traceable evidence
and confirm selected tests ran. Register any new immediate-parent PR in the
stack. Leave all PRs open; merges need explicit authorization.

Maintain [the dated execution report](xl_pumo_s21_execution_20261123.md),
[remaining categories](xl_pumo_s21_execution_20261123.md#remaining-first-blocker-projection)
and [human directions](../XL_compat/PUMO_PLAYTEST.md). Restore the S21's
original `stay_on_while_plugged_in` value after phone experiments.

## Paste-ready replacement kickoff

> Work in `C:/local/dxx-redux`. Read `AGENTS.md`,
> `.github/copilot-instructions.md`,
> `android/ai tool plans/xl_pumo_s21_execution_20261123.md`, this next
> tranche plan and `android/XL_compat/PUMO_PLAYTEST.md`. Begin with physical
> feedback on ordinary kit 32320. Record kit version, phone/Android version,
> difficulty, input method, episode, checkpoint, last successful action and
> save/load result. Reproduce the Crauphel Scilab room-129 human missile exit
> with the same aim and missile; fixed-pose and automated S21 shots do not
> close it. Check score-to-briefing and last-briefing-to-flight progress bars
> separately and identify the green-on-green briefing page. While waiting,
> investigate a runnable-original XL entry from the measured 96/120 remaining
> prior misses (96/1,654 if the prior 1,534 passes still hold),
> remaining, beginning with the Bay32 resolved filename discrepancy. Preserve
> exact archive, ordinal and inputs;
> replay the entire affected cluster and record newly exposed blockers. Use
> one local gameplay worker, hosted Linux CI and the S21 as needed. Keep
> private assets, saves, profiles and logs off Git. Commit and push coherent
> tested layers; open non-draft PRs only in `larsonm-personal/dxx-redux`, each
> based on its immediate predecessor; register stack #720 and monitor every
> pushed head. Diagnose skips from workflow admission evidence, obtain
> passing applicable fork PR CI and leave PRs open without merge. Deliver a
> dated report, remaining categories with denominators, updated human
> directions, exact PR/check results and the first unresolved gameplay action.
