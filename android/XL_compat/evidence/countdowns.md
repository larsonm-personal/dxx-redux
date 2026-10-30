# Owned XL countdowns

Wall and supported object type-25 actions now start an owned XL countdown.
The XL frame consumer supplies source warning timing, ship rotation and terminal
death while Redux keeps its existing alternate. Countdown state is included in
ordinary disk saves and rewind. Other requirements still block every original
Pumo level; this increment does not qualify a campaign.

The compatibility viewer counts wall and object countdown actions, reports
invalid durations as blocked, and retains a partial status until loaded sound
and owner requirements are checked. Countdown seconds are not displayed as
delayed scheduling. Supported texture-action flags and delays also stop using
the viewer's stale unsupported-action classification.

## Source and clock contract

The existing trigger source owner executes `InitCountdown` and
`DoCountdownFrame` from `objects/reactor.cpp`, plus `GetSlowTicks` from
`main/gametime.cpp`, at XL commit
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. Consumed files and constants are
hash-checked. The older `main/cntrlcen.cpp` copy is not the reference: it differs
in player-death handling and the terminal flash boundary.

The comparison covers 370,998 initialization, cadence and frame cases, including
180 episodes that must actually reach terminal death. Independent state and
output adapters compare timer, displayed seconds, total, warning IDs/order/gain,
rotation and random-call consumption with the same supplied random sequence,
palette intensity and death. Durations include zero, short, 14-, 30- and
99-second episodes; frame intervals range from 5 to 1,000 ms. Both limited and
unlimited source modes are exercised, with paused/dead frames. Initialization
also covers all difficulties, default/custom bases and source D1/D2 tables.

An action's positive `time` is whole seconds, not scheduler milliseconds.
Nonpositive values select the difficulty-adjusted level default. `value` and
the target list are inert. Initialization retains the previously displayed
seconds and pending frame accumulator. The scheduler source owner separately
executes fifteen countdown/message/sound time exceptions, including values
that would normally arm a delayed action. Fixed-point duration overflow is
rejected before admission or runtime mutation.

The production XL path uses the source's default 40 Hz ceiling, discarding tick
overshoot while accumulating every intervening simulation frame. A saved
simulation clock replaces XL's process-global SDL wall clock. Its millisecond
phase and fractional remainder survive pause, disk and rewind. This deliberate
clock boundary provides reproducible continuation; it is not a claim of matching
wall-clock jitter across engines. Alternate slow-motion and timing profiles
remain outside the qualified runtime profile.

Warnings use the ordinary loaded sound resources, which admission now includes
in the dependency closure. Countdown actions do not bypass the rejection of
unapplied ordinary replacement WAVs. Pumo's replacement audio therefore remains
an independent requirement.

## Runtime and persistence controls

The derived, explicitly labeled `xcount` fixture retains Neptune geometry and
companions. It supplies canonical 23/40 and 27/40 layouts, eight wall actions
and two attached actions. Ignored out-of-range targets survive both loader
passes without becoming geometry references. Native consumers exercise player
touch, master chains, one-shot/disabled actions, wall/object autoplay, object
removal and all difficulty defaults. Normal Android startup is allowed to
advance an autoplay timer; the control checks its exact total and that another
frame cannot reinitialize it. Native startup checks the exact initial value.

Controlled frame sequences check pending sub-tick time, retrigger retention,
discarded cadence overshoot, warning crossings, actual player rotation and
palette output. A dead player clears pending time without advancing the timer.
No autonomous level traversal is used.

Trailer schema 18 adds destroyed state, timer, displayed seconds, total duration,
pending frame time, cadence phase and fractional remainder. All fields are
validated before publication, including agreement with the native saved timer.
Fifteen malformed variants must reject without changing any owned trailer byte.
Live total duration is bounded rather than restricted to authored action values:
the special reactor robot can legitimately extend it. The persistence fixture
earns this extension before capturing its partially accumulated next frame.

Normal saves pass through a stock mission and restore the exact XL state.
The inactive XL consumer must leave the stock timer unchanged. Android adds
normal rewind/disk paths, HUD framebuffer comparisons, actual warning PCM and
terminal death through the ordinary life-loss and next-level path.

## Original requirement delta

The unchanged Pumo HOG has SHA-256
`6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043`.
Fresh inspection removes exactly these nine type-25 runtime rejections:

| Member     | Trigger |
| ---------- | ------: |
| mvlev1.rl2 |      21 |
| mvlev2.rl2 |      26 |
| mvlev3.rl2 |      11 |
| mvlev4.rl2 |      44 |
| mvlev6.rl2 |      30 |
| tulev1.rl2 |      47 |
| tulev2.rl2 |      29 |
| tulev4.rl2 |      28 |
| mvlev5.rl2 |      21 |

All nine use 99 seconds. Every other inventory field, archive member and runtime
blocker is unchanged. Unsupported wall-action kinds now occur in two levels;
two mine shakes and one disarm remain. All fourteen levels retain independent
blockers. Package resource and human campaign acceptance remain separate.

## Validation receipts

Public Windows and Linux owners pass all 43 and 46 required contracts in
17.86 and 8.57 test seconds. The source comparisons reuse existing owners.
One new native runtime owner brings required private membership to 105.
Both Windows native layouts pass the finalized extended-countdown persistence
and fifteen malformed-state controls. The isolated Linux private owner passes
all 105 required contracts in 349.379 test seconds, with sixteen original-format
outcomes and three full modern source-reader comparisons. Its countdown owner
passes in 7.778 seconds. Final browser classification changes pass the focused
Windows and Linux inventory contract in 11.06 and 1.70 seconds.

Android run `9a551265796e453dbcbe0ef019dbf56f` passes all 80 steps in
199,198 ms. Its final report records two frame sequences, sixteen exact-state
verifications, 4,424 normal HUD glyph pixels and one ordinary terminal death.
The warning capture contains 32,768 interleaved output samples, of which 14,920
differ from the separately captured background music. This establishes actual
audible output alongside exact source warning IDs; it is not a whole-engine
audio identity claim. All 26 protected app-file hashes match after restoration.

Both automation catalogs and direct PowerShell analysis pass. The private-owner
membership and omission tests pass all nineteen cases. The first private suite
attempt timed out its boss-spawning owner at 80 seconds while concurrent builds
were running; its diagnostic log is retained. The isolated final rerun passes
that unchanged owner in 63.684 seconds, without increasing its timeout.

Generated fixture SHA-256 values:

- HOG: `bf9bbaf3ba13345354e3539721dd86f5cdfceab4be003b2ade9f19f19cd4695e`
- Descriptor: `52636d2e26d6f65b239b30f001af3a8f2346ce28208188f3e2537f131ac68b55`

Artifacts and the exact original delta are under `temp/xl-countdown-20261029`;
Android integration output is under `temp/xl-countdown-actions-android`.
