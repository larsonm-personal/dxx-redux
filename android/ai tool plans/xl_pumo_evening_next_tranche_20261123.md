# Next Pumo and XL tranche after the November 23 evening run

## Start with the ordinary phone kit

Use [ordinary 32325](../../temp/xl-pumo-phone-s21-20261123-32325/README.md)
with its exact manifest and earned checkpoints. Record the kit version and
phone model, Android version, difficulty, input method, episode, checkpoint or
fresh start, last successful action, first failed action and save/load outcome.
Preserve the original issue save. Keep fixed-view, automated steering and
physical-phone observations attached to their respective captures.

The first unresolved action is the original Rookie touch missile shot in
Mineral Valley level 2, Crauphel Scilab near room 129 on SM-S931U with Android
SDK 37 and Play internal build 31950. Use the same missile and aim if known.
Save the approach in a separate slot, fire, check steering, then close, load
and repeat. S21 fixed-view routes with a Concussion missile succeeded both
before and after quickload. The reporter's steering path, aim and reload
result still need a physical-phone report.

Enable **Show progress bars** and **Mission zip** music. Measure difficulty
selection to briefing, hub score advance to the Crauphel briefing, and final
page dismissal to flight as three intervals. The resource check and native
texture cache remain separate from level music startup. Include the visible
label and whether the bar moved. For green text on green artwork, send the
first words or a screenshot with the same version and phone fields.
Compare the same page on the [ordinary 32328 contrast kit](../../temp/xl-pumo-phone-s21-20261123-32328/README.md)
with the original pilot and checkpoint selection preserved.

For the next loading pass, use the 32325 phase receipt before changing the
cache. Both fresh prebriefing waits were dominated by level resources and
scoped image preparation. The loader reused that work after the briefing,
while the native texture cache remained a separate cost. Compare fresh hub,
fresh Crauphel and Crauphel quickload using the same original asset owner.

The current [playtest directions](../XL_compat/PUMO_PLAYTEST.md) list both
earned endings and optional routes. Mineral Valley follows levels 1 through
9 to the level 10 closing briefing, with the optional 4 to 14 to 5 Coldrom
detour. Kartsal follows levels 1, 11, 12 and 13. The original closing screens
rendered from the earned checkpoints on ordinary 32325 on the S21 and earlier
on the Android emulator. After the missile test, use `mineral-ending` or
`kartsal-final-checkpoint` for a manually played phone result.

## Independent original compatibility work

Start with the remaining first blockers in the evening execution report.
The frozen Windows catalogs measured 1,563/1,654 starts, leaving 91, of
which 54 previously started and ran on the pinned original engine.
Choose a runnable original, pin its distribution and member, and compare
the same ordinal and inputs. Replay the whole affected cluster, including
separate editions, and record what the first actual failure becomes. Keep
startup, assisted continuation and rendered behavior attached to their own
receipts. Keep XL format rules in `android/XL_compat` and native hooks narrow.

Split Anthology is the next source audit. Six original TXTR chunks overstate
their decoded length. The alternate Anthology distribution and split
ordinals make up four current OOF misses. The distributions share the same
seven numbered OOF members byte for byte, with distinct mission identities.
Correct boundaries alone expose
GPNT, PANI, WBAT and authored rendering properties. The pinned reader
consumes GPNT and weapon batteries, and the original optimized converter
sets gun points. Break the work into bounded reading, native gun-point
ownership and the authored facing, glow and rotation profiles. The source
profiles include `$fov`, `$glow`, `$facing` and `$rotate`; exercise each with
its actual consumer before claiming a gameplay gain. Keep the four failures
in the denominator until the exact originals pass.
Model 85 carries nine GPNT records. The pinned reader consumes the full list
and the optimized model consumer caps its gun slots separately. The pinned
model list contains all seven numbered keys and tries the alias before the
numeric file. Observe actual binding and fallback before changing native
ownership.
[Original OOF audit](../XL_compat/evidence/gameplay-anthology-oof-audit-20261123.json).

Keep the 34 missing advertised members assigned to their exact archives.
The 16 light-index bounds need an exact pinned serialization rule. Use one
local gameplay worker and read progress from receipts. Use hosted Linux for
routine cross-platform validation. Keep private assets, captures, profiles
and saves on the build machine.

For desktop rendered tests prefer windowed mode. If a test needs to change
a display mode, record it before starting and restore it in cleanup. For
Android, record `wm size` and `wm density`, including any prior override,
and restore the same values. Verify the mode and any stay-awake setting
after the test.

## Fork handoff

The evening branches are in [personal-fork stack #746](https://github.com/larsonm-personal/dxx-redux/stack/746).
The [execution report](xl_pumo_evening_execution_20261123.md) records the exact
heads, test selection, skips and infrastructure retries. Fetch `origin` and
inspect the current branches and PR state before choosing a continuation
base. Keep the PRs ready for review, each based on its immediate predecessor.

## Paste-ready replacement kickoff

> Work in `C:/local/dxx-redux`. Read `AGENTS.md`,
> `.github/copilot-instructions.md`,
> `android/ai tool plans/xl_pumo_evening_execution_20261123.md`,
> `android/ai tool plans/xl_pumo_evening_next_tranche_20261123.md` and
> `android/XL_compat/PUMO_PLAYTEST.md`. Begin with
> ordinary phone kit 32325 and keep the build with every report. Record phone,
> Android version, difficulty, input method, episode, checkpoint, last good
> action and whether save, close and load reproduces the issue. Start with
> the reported Rookie touch missile exit in Crauphel Scilab near room 129,
> using the same missile and aim if known. Preserve the original issue save.
> Check progress bars before the first briefing, from the hub score screen
> to the next briefing, and from the last briefing page to flight. Record
> the label and timing. Capture the words or picture of any low-contrast
> briefing page and repeat it on ordinary kit 32328. Keep physical-phone play, fixed-position tests and scripted
> steering attached to their own evidence. On 32325, the S21 rendered both
> original endings from cold-loaded earned checkpoints. Follow the checkpoint
> directions for a manually played result.
>
> While awaiting phone feedback, choose a runnable original from the
> 91 remaining cases in the reproduced 1,654-case comparison. Start with the Anthology OOF
> source path. Pin the exact distribution and ordinal, keep the same inputs,
> repeat the entire affected cluster and record the next actual blocker.
> Keep XL behavior in `android/XL_compat`, native hooks narrow, and one local
> gameplay worker active. Read its receipts and use hosted Linux CI for
> routine validation. Keep private inputs and profiles local. Use windowed
> rendering unless the test requires a display change. Record, restore and
> verify any desktop or emulator resolution, density or stay-awake change.
>
> Fetch the personal fork and inspect the branch, worktrees and open PRs.
> Commit and push coherent, locally tested layers in ready-for-review PRs
> to `larsonm-personal/dxx-redux` only, each based on its immediate predecessor.
> Register the GitHub stack and check the state of stack #746. Monitor every
> triggered workflow and inspect its
> admission evidence for skips, retry traceable infrastructure failures and
> repair material failures. Verify selected tests actually ran and obtain
> passing applicable fork PR CI. Leave the PRs open for review. Commit a dated
> execution report, remaining-level categories with denominators, a next
> tranche plan and updated human playtest directions.
