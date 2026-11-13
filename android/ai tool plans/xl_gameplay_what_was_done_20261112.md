# XL gameplay work: wind-down report

Prepared November 12, 2026, following the user's 09:19 PST wind-down request.
The last implementation chunk is complete and committed. No HAM-reader refactor
was started after that request. This report describes the achieved scope and
remaining work; it does not claim 90% of levels playable.

## Current checkout and publication

| Item                       | Current identity                                                               |
| -------------------------- | ------------------------------------------------------------------------------ |
| Checkout                   | `C:/local/dxx-redux`                                                           |
| Branch                     | `xl_gameplay_20261111_03_continuation`                                         |
| Last implementation commit | `3660e9a14b9c7651c5f686ce7b4ceeefd779172d`                                     |
| Implementation tree        | `89ae4d157132c115d973846eb6be38037b40ab7b`                                     |
| Published parent           | `0a2df4aebc4e87be3d1ae802f05c2fce64b55072`                                     |
| Local layer 03             | 15 implementation/checkpoint commits; 86 changed files relative to that parent |
| Publication                | Layer 03 has no push, PR, hosted CI or G2 acceptance                           |

The subsequent handoff commit changes documentation only. All implementation is
committed; the independent untracked `pr464_ci_review_20261110.md` and all three
original stashes are preserved. No merge, auto-merge or upstream push occurred.

GitHub metadata was refreshed during this wind-down. The published chain remains:

- [#464](https://github.com/larsonm-personal/dxx-redux/pull/464), prerequisite,
  `e87f4bfaeec60fd7a9697488413e02cc88230ade`, based on `cmake` at `b3bb12cbe`.
- [#465](https://github.com/larsonm-personal/dxx-redux/pull/465), G0 CI repairs,
  `96ce2f8be5f917178024298cf51272781abc0caa`, based on #464.
- [#467](https://github.com/larsonm-personal/dxx-redux/pull/467), G1 timelines,
  `0a2df4aebc4e87be3d1ae802f05c2fce64b55072`, based on #465.

All three PRs are open, unmerged and currently marked ready for review. Published
PR metadata links [stack 466](https://github.com/larsonm-personal/dxx-redux/stack/466).
The local continuation branch is not a member: `gh stack view --json` reports
that fact. Do not run stack checkout merely to inspect it or move this checkout
away from the tested implementation.

## What was done

### G0: repaired the real CI failures and missing coverage

- Fixed GCC packed-field forwarding into JSON, including the actual desktop
  executable that earlier metadata-only builds missed.
- Moved the required boss source-oracle contract with its actual execution owner.
  The canonical private suite again registers and executes all 113 required tests.
- Restored the standalone boss consumer's slow-motion shutdown dependency and
  checked that its countdown behavior remains observable.
- Expanded generic PR test selection for XL/runtime/importer changes and made
  developer workflows compile both desktop and metadata targets.
- Made optional Warp coverage and incomplete input sets explicit. Required cases,
  deadlines and immutable expected results were preserved.

The original skips had different causes: failed-build dependents supplied no
replay evidence; path/manual/platform exclusions were conditional policy; the
frozen red readiness baseline was a separate measurement. Details and original
failure links are in [the CI repair report](pr464_ci_fixes_20261110.md).

### G1: implemented and qualified ordered Omega/time input replay

The existing recorder/replayer now supports the bounded format-6
`xl-clock-input-v1` profile. It records external clock/render observations and
ordinary slow-motion/bullet-time controls at their consumption points, preserves
supported stock format-5 playback, and rejects malformed/unsupported timelines.
Checkpoint clock origins and required settings are bound explicitly.

Normal Windows/Linux OpenGL qualification includes actual Omega/time pickup and
combat witnesses. A large ordinary capture matches 2,549 frames, 12,067 SIM RNG
events and 5,103 FX events. Starting checkpoints for some witnesses were assisted;
there were no assists during those recordings. This proves the bounded timeline
contract, not connected campaign play or device independence.

G1's published-head workflows are green. A fresh connector query found nine
PR-triggered runs associated with `0a2df4aeb`, all completed successfully. These
workflow results do not mean every conditional job executed. Earlier exact-head
dispatch/receipt evidence includes [engine/ASan](https://github.com/larsonm-personal/dxx-redux/actions/runs/43165269464),
[public save/replay](https://github.com/larsonm-personal/dxx-redux/actions/runs/43165272925)
and the [trusted private owner](https://github.com/larsonm-personal/dxx-redux/actions/runs/43165315754).
The private owner passed 113 required tests with its recorded scope. Standard
camera subchecks explicitly lacked their optional addon; broader JVM execution
retains 12 private-content/live-service skips. None of these results validates
the unpublished layer-03 head.

### G2: substantial continuation work, still incomplete

The 15 local commits add or strengthen these existing owners:

- Persisted object birth/collision-delay state and detailed observation of native
  controls, fusion/guided state, fixed escort/thief state, route state, saved
  Guide-Bot metadata, raw pending events, mixed producers, motion/contact and
  environment cadence. The detailed XL world observer is at schema 19.
- Corrected paused/restored clock continuation and qualified active time
  transitions through native disk saves, warm restores and fresh-process restores.
  A Windows native save also continues on Linux with matching detailed state/RNG.
- Staged native AI restore into a retained candidate, with explicit epoch,
  geometry and capacity checks before application. Native object/player record
  decoders likewise accept an explicit candidate context and checked complete
  records rather than reinterpreting them against the live profile.
- Retained parsed mission descriptors and identity allocations. Candidate HOG
  members can be read privately, without selecting the mission or exposing their
  basenames in live lookup. Busy handles block commit/discard; malformed and
  late-mutated inputs retain the previous mission. Filename case now follows the
  native reader while enforcing the actual archive/directory origin.
- Bound resource dependency validation to retained weapon, powerup, animation,
  texture/material, robot/model and sound-map tables. Camera ownership transfer
  rebinds views to their surviving owner.
- Fixed screenshot readback framebuffer selection found by actual Mesa testing,
  preserving GL read state rather than suppressing errors.

The latest chunk initially introduced a dangling sound-map view during camera
preparation. The Windows replay exposed it; a diagnostics-only rebuild merely
changed its manifestation. The ownership transfer was repaired and both clean
Windows repeats and Linux playback then passed. Failed receipts remain retained.

**The material open defect is still whole-save atomicity:** a malformed XL trailer
is rejected after native restore has already changed the player's energy from
91 to 100. Mission/level/runtime publication precedes complete extension validation.
The staged pieces above are prerequisites; they do not fix that whole transaction.

## Latest verification and runnable artifacts

These are checks of the final implementation at `3660e9a14`, before the
documentation-only handoff commit:

| Check                                                                | Result and scope                                                                                                                                             |
| -------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Windows/Linux developer-ON desktop and metadata builds               | Pass                                                                                                                                                         |
| Windows/Linux ordinary developer-OFF engine and compatibility builds | Pass                                                                                                                                                         |
| Full D2 `test_upstream_compat`                                       | Windows 61.11s; Linux 36.80s; one combined CTest owner each, zero skips                                                                                      |
| Original Observatory owner on both hosts                             | Pass, including retained definitions, uppercase private read, malformed/archive/identity controls, persistence and pinned 12 firing/four contact comparisons |
| Ordinary save-start replay                                           | Two clean Windows processes and one Linux process pass strict result/assets/clean exit                                                                       |
| Detailed live-capture comparison for all three replays               | 12 object frames, 16 world records, 21 SIM and 12 FX events each; no differences                                                                             |
| Automation catalogs and scoped mixed quality                         | Pass; 99 standalone JSON, 315 standalone PowerShell, 547 support scripts; 428 master top-level entries                                                       |

The short replay is original WShock20 level 6, with five setup position assists,
zero inventory assists and zero assists during recording. It is not a fresh-start
route or completion. The additional Windows HXM/Warp5 warm/cold actor owner passed
before the final camera/case correction; its assisted actor setup and unexecuted
optional camera subcase remain explicit. Do not report it as final full-corpus CI.

| Artifact                                                                 | SHA-256                                                            |
| ------------------------------------------------------------------------ | ------------------------------------------------------------------ |
| `temp/xl-day-20261110/build-d2-reference/main/d2x-redux.exe`             | `4151d8f979fd479fb3afd685dedc2b242f4f74185f9f9d9c326b7580fd88d52d` |
| `/var/tmp/dxx-pr464-20261111/private/stock/main/d2x-redux` in Ubuntu WSL | `098bf0357ef5803f9ec2cc8439d751523836a30be54bc44a74ac62fe81b3a6f8` |

These are tested local D2 builds, not newly assembled final delivery folders.
No new universal APK or Android qualification was produced in G2. Native D1 was
last rebuilt in the earlier time-continuation checkpoint; its evidence is
separately dated in the ledger. Existing Android full-GPU validation still exceeds
its unchanged 480-second limit; protected profile restoration was verified.

Detailed evidence is under `temp/pr464-ci-fixes-20261110/`, especially
`g2-dependency-definitions-build-identities.json`,
`g2-dependency-definitions-fixed-compare.json` and the corresponding build,
CTest and reference-owner logs. The committed implementation patch SHA is
`542546800c636c7d20bf1582e3f2bae241bc6121800ca41b526e216bd979c6e6`.
The pinned XL reference remains `2576a98b5de310e3ac7c9777df2677d1627b8bae`.
Authentic assets and private checkpoint/demo bytes were not published.

## Where the 90% target stands

**Current playable-level percentage: not established.** There is no defensible
percentage gain to report from these commits yet.

The immutable catalog has 1,111 advertised level entries in 23 archives, including
classic/imported content, multiplayer content and repeated levels in different
packages. Its SHA-256 is
`bf4356a7a5fbe64832a40d7065ea02d8b81276ebd8bbba6874012f7851fa0cf8`.
The frozen eight-case result remains historical 4/8 load-and-tick readiness, with
the four successes XL inactive. It is neither 50% playable nor a current-tip
measurement. The fresh 29-level/four-package inventory and preserved 131 distinct
level-hash survey are also not playthrough evidence.

The working interpretation for the next plan is the original single-player scope:
freeze all eligible single-player entries from that catalog, retain the full raw
1,111-entry accounting, and require at least `ceil(0.9 * eligible_count)` qualified
entries. Multiplayer exclusions require explicit evidence and remain visible.
Unknown, untested, missing-content and rejected single-player entries must not be
silently removed. If the target is literally all 1,111 entries, it requires at
least 1,000 successes and may conflict with the existing multiplayer exclusion.
Resolve and record this before claiming a percentage; do not substitute the eight
sample cases or the easiest original packages.

For playability, loading is only the first gate. Required evidence must cover
usable normal rendering/controls, navigation, actual hostile combat, authored
mechanisms/resources, reliable save/continue, and progression through supported
content. Full pixel/sound fidelity is optional only where simulation independence
has been proved. A source-feature audit may support an inference about other
levels; inferred support must be reported separately from directly exercised play.

## Remaining work and estimate

These are sequential engineering estimates from the current implementation, not
calendar promises. The first census must attach each blocker to the number of
affected entries, so work can be ranked by actual coverage rather than anecdotes.

| Work                                                             | Completion evidence                                                                                                                                                                                                                   | Remaining hours |
| ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------: |
| Freeze the 90% denominator and count blockers across the catalog | Per-entry status, source/asset identity, explicit exclusions and blocker frequencies using existing inventory/owners                                                                                                                  |             2-4 |
| Finish G2 continuation and atomic restore                        | Prepare target HAM/resources and all native/runtime/XL owners with existing codecs; malformed restores leave the whole world unchanged; close Guide-Bot async/cache/Android budget and observation gaps; cold/uninterrupted agreement |           12-20 |
| G3 simulation/presentation separation                            | Deterministic logical audio/event state; mixer success cannot disable future triggers differently; true no-device XL replay matches rendered state/SIM RNG; diagnose Android latency                                                  |            8-14 |
| G4 remaining source-qualified gameplay                           | Smoke 20-24 full dispatch/flags, maker IDs, subzero record audit, wall cameras/colored walls, older layouts and catalog-ranked geometry/object/owner predicates; preserve genuine missing-POG guards                                  |            8-16 |
| G5 connected original gameplay                                   | Earned Warp5/time-equipment and Dinter event-connected routes, hostile damage/loot, cold continuation, death/secret/transition and an actual original completion                                                                      |            6-10 |
| G6 platform and durable corpus qualification                     | Live versus immediate/repeated rendered/headless replay, Android import/save/recording and Android-to-host state/RNG, exact profile restoration, enforceable forward contract                                                         |            6-10 |
| G7 final CI, fork publication and packages                       | Exact-tip public/private/affected CI, honest skips, coherent remaining layers, runnable Windows folders and universal APK with hashes                                                                                                 |             3-6 |
| **Provisional engineering total**                                | **Re-estimate after the catalog census**                                                                                                                                                                                              |       **45-80** |

Confidence is low because the full catalog has no current gameplay outcome matrix
and G2 still needs architectural work. The 45-80 hours covers implementation and
focused qualification, not 1,000 individual full-level playthroughs. If the 90%
standard requires a direct route/combat witness for every counted entry, budget
that execution separately: even five to ten minutes per 1,000 entries is roughly
83-167 hours before failures and retries. Neither estimate proves the threshold
will be reached; the census and measured owner throughput must replace assumptions.
Missing genuine gameplay-critical assets remain content blockers rather than code
workarounds. No credible promise of reaching 90% in the next 12 hours is supported.

The proposed next work window is in
[the next 12-hour outline](xl_gameplay_next_12_hours_20261112.md).
The [execution ledger](xl_gameplay_execution_20261110.md) retains all dated
receipts and superseded checkpoints; this wind-down report is the current summary.

## Resume and review

```powershell
Set-Location C:/local/dxx-redux
git status --short
git log --oneline 0a2df4aebc4e87be3d1ae802f05c2fce64b55072..HEAD
git diff --stat 0a2df4aebc4e87be3d1ae802f05c2fce64b55072..HEAD
git show 3660e9a14
gh pr view 467 --repo larsonm-personal/dxx-redux
gh pr checks 467 --repo larsonm-personal/dxx-redux
```

Do not merge or publish incomplete layer 03 as accepted G2. About 15.5 GiB remains
free. Official idle cleanup preview/apply found zero eligible artifacts while
preserving authentic inputs, evidence, native caches, replay sandboxes and emulator
workspaces; the 50 GiB target was not reached. Follow `android/CLEANUP.md` before
large builds. No native build, formatter or test is intentionally left running.
