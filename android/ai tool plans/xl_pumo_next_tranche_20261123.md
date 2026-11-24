# Next Pumo and XL tranche after the S21 run

## First action: physical phone feedback

Use ordinary kit **32322** and its manifest in ignored
`temp/xl-pumo-phone-s21-20261123-32322`. Start Mineral Valley on Rookie with
touch.
The reported failure was a missile shot in Crauphel Scilab, near room 129,
on SM-S931U, Android SDK 37, Play internal build 31950. Record the missile,
aim landmark, last successful action and whether saving before the shot,
closing, loading and repeating produces the same exit. Preserve the original
save. The ordinary kit 32322 passed 35/35 fixed-view S21 actions with a
concussion shot before and after quickload; its app data was restored. The
protected original-archive diagnostic route also passed. The ordinary S21
test used the original loose HOG and did not cover its 7z import or mission
track. An ADB-driven double tap on the ordinary touch overlay after automation
ended consumed one Concussion missile and left the S21 in active flight after
quickload. These fixed-view inputs do not reproduce human steering or the
reported aim. That touch shot remains the first unresolved gameplay action.

Keep **Show progress bars** enabled. Time difficulty selection to the first
briefing, score-screen advance to the next briefing and final briefing-page
dismissal to ship control as separate waits. Record the visible label, whether
the bar moves and approximate elapsed time. The score-to-briefing and
last-page-to-flight bars have an
[S21 diagnostic visual witness](../XL_compat/evidence/pumo-s21-loading-tail-32322-20261123.json).
Build 32322 retains the dark panel and high-contrast label and keeps the bar
moving with **STARTING LEVEL MUSIC** through the gap seen on 32321. A filmed
S21 ordinary-kit run shows the bar through that music interval immediately
before Crauphel flight. Check the same intervals on the reporter's phone. Ask
for the first words or a screenshot
of the green-on-green briefing page if it reappears; the reporter does not
remember which page. Each report needs kit version, phone,
Android version,
difficulty, input method, episode, checkpoint, last successful action and
save/load result.

For speed work, keep the two waits separate. The protected build 32322 S21
log measured Crauphel's prebriefing resource check at 7.50 seconds, split
roughly between level preparation (3.71 seconds) and scoped images (3.78
seconds). The postbriefing native load took 5.88 seconds, including 3.23
seconds of texture caching and 1.33 seconds of synchronous music startup.
Compare those phases on the ordinary phone report before selecting another
optimization; retain the asset-generation identity and source-order checks.

## Independent engineering while waiting

The frozen integrated Windows baseline is 1,534/1,654 starts at 120 frames.
The current-head rerun of all 120 prior misses measured 26/120 passing and
94/120 still blocked. Together with the 1,534 prior passes, this supports
**1,560/1,654**, leaving **94/1,654** if those prior passes remain unchanged.
This is not a full 1,654-start census rerun. Keep missing
advertised members and alternate distributions separate. The original Kore
archive is 1/1, Chaos Theory XL is 6/6, Playground XL is 8/8 and Anthology
`antho1-3` is 3/3 after this stack. The levelpack Playground equivalent also
passes at its separate advertised ordinal. These tests establish startup, not
complete gameplay or visual parity.
Original robotless Subspace D3 is the latest single-entry gain; its original
archive cluster, Forest robot-generator guard and complete 120-case retry are
recorded in [the Subspace evidence](../XL_compat/evidence/gameplay-subspace-robotless-20261123.json).
The later Water Arena CI guard again repeated the entire 120-case cohort with
the same 26 passes and no new blocker.

The next runnable-original lead is
`levelpack.7z/missions/darkflamewolf/ctheo-xl.mn2:4`. The texture fix exposed
two type-20 effect objects and two lightning payloads in a classic 7/31
level. The original multiplayer Chaos Theory archive passing level 4 is a
separate distribution. The existing lightning runtime binds translated
extended levels; the classic stock-load path does not bind those objects.
An [exact original audit](../XL_compat/evidence/gameplay-levelpack-chaos-classic-effect-audit-20261123.json)
also exposed an unqualified 7/31 serialization pair. The native stock reader
does not consume render-12 payloads, and the current translator rejects
classic level version 7. Prove a source-owned classic effect path and its
serialization, including native object identity and save/load, before changing
admission. Then replay all six ordinals of the exact levelpack descriptor and
record newly exposed blockers. Confirm an ordinary classic mission still uses
the stock path. The split-Anthology OOF cluster is a
separate lead: six original TXTR chunks overstate length, but bounded parser
resynchronization exposed `GPNT`; gun-point and animation behavior remains
unqualified. The [model audit](../XL_compat/evidence/gameplay-anthology-oof-audit-20261123.json)
retains hashes and chunk evidence without private model bytes.
Levelpack Sphere level 1 is another original lead: its HXM robot 2 has an
out-of-range model and gun submodel. A derived structural correction passed
the validator, but static absence in objects, containment and matcens is not
a complete spawn proof. Preserve its [HXM audit](../XL_compat/evidence/gameplay-sphere-hxm-audit-20261123.json)
and keep the original blocked until full runtime reachability and save/load
semantics are shown.
Levelpack Chaos Theory level 6 separately needs additive wall rendering plus
14 other requirements. The fixed Anthology, Playground and Bay32 entries
should not be counted again. Bay32's filename discrepancy is resolved by a
hash of the engine-read source and an isolated negative control.

The three bio/etomb archives require multiple segment behaviors and are a
larger project. The 16 light-index bounds include truly out-of-range packed
data and need a pinned source rule. The 34 missing advertised members include
27 FFYL ordinals whose levelpack archive contains a descriptor but no FFYL
HOG or RL2. The [other seven](../XL_compat/evidence/gameplay-missing-seven-audit-20261123.json)
were audited against their exact archives and original reference: four mission
rejections and three missing advertised-member lookups, with no runnable-original
recovery target. Keep the separate `FFYL.rar` distribution separate. Use one
local gameplay worker, read its receipts,
validate focused changes with Windows builds and exact archive replays, and
use hosted Linux fork CI for routine cross-platform checks. Use the S21 for
Android behavior. Keep private game assets, profiles, saves and logs off Git.

## Fork and handoff

The ready-for-review personal-fork PRs #726-#743 are registered in
[stack 720](https://github.com/larsonm-personal/dxx-redux/stack/720), each
based on its immediate predecessor. Check the final execution report for
exact heads and CI results. The private XL workflow may skip a PR below the
native stack tip; inspect admission evidence before interpreting skips.
Fix material failures, retry infrastructure failures with traceable evidence
and confirm selected tests ran. Register any new immediate-parent PR in the
stack. Leave all PRs open; merges need explicit authorization.

Maintain [the dated execution report](xl_pumo_s21_execution_20261123.md),
[remaining categories](xl_pumo_s21_execution_20261123.md#remaining-first-blockers)
and [human directions](../XL_compat/PUMO_PLAYTEST.md). Restore the S21's
original `stay_on_while_plugged_in` value after phone experiments.

## Paste-ready replacement kickoff

> Work in `C:/local/dxx-redux`. Read `AGENTS.md`,
> `.github/copilot-instructions.md`,
> `android/ai tool plans/xl_pumo_s21_execution_20261123.md`, this next
> tranche plan and `android/XL_compat/PUMO_PLAYTEST.md`. Begin with physical
> feedback on ordinary kit 32322. Record kit version, phone/Android version,
> difficulty, input method, episode, checkpoint, last successful action and
> save/load result. Reproduce the Crauphel Scilab room-129 human missile exit
> with the same aim and missile; the ordinary 32322 S21 fixed-view touch
> double tap fired successfully after quickload, but does not close the
> reported human aim. Check score-to-briefing and
> last-briefing-to-flight progress bars separately. If green-on-green text
> reappears, capture its first words or a screenshot; the reporter does not
> recall the original page. While waiting,
> investigate a runnable-original XL entry from the measured 94/120 remaining
> prior misses (94/1,654 if the prior 1,534 passes still hold), beginning with
> levelpack Chaos Theory level 4's classic 7/31 effect serialization, object
> identity and lightning. Bay32's source
> identity is resolved. Preserve
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
