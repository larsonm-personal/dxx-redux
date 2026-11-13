# XL gameplay work: wind-down report

Prepared November 12, 2026, following the user's 09:19 PST wind-down request.
The last gameplay implementation chunk at wind-down is complete and committed. No HAM-reader refactor
was started after that request. This report describes the achieved scope and
remaining work; it does not claim 90% of levels playable.

Reviewed and expanded on November 12 against the implementation at `3660e9a14`,
the documentation handoff at `013b7234`, the original G0-G7 requirements, retained
logs/manifests and the checked-in catalog. This revision is a source/evidence
review, not a fresh engine test run. Remote status below is the **09:29 PST
November 12 snapshot**, not a claim about subsequent GitHub activity.

A subsequent user-requested architecture cleanup was published as `0d3456979` on
[non-draft PR #468](https://github.com/larsonm-personal/dxx-redux/pull/468), based on
layer 02. Its initial CI has real failures; the
[CI repair report](pr468_ci_fixes_20261112.md) records the current repair and checks.
The publication table below remains the historical wind-down snapshot. See the
[layer-03 boundary review](xl_layer03_architecture_review_20261112.md) for moved
code, final-source validation and new binary identities. It does not complete G2.
The historical executable paths below have since been rebuilt; use the cleanup's
manifest when identifying the binaries currently at those paths.

The practical result is a repaired CI baseline, a bounded desktop input-timeline
contract, and useful but incomplete save-continuation infrastructure. The most
urgent correctness defect remains a failed save load that changes the live game.
There is also a coverage gap: its failing regression is opt-in, outside the
canonical projectile test invocation. No measured increase in playable-level
coverage is available.

## Checkout and publication at the original wind-down

| Item                       | Current identity                                                               |
| -------------------------- | ------------------------------------------------------------------------------ |
| Checkout                   | `C:/local/dxx-redux`                                                           |
| Branch                     | `xl_gameplay_20261111_03_continuation`                                         |
| Last implementation commit | `3660e9a14b9c7651c5f686ce7b4ceeefd779172d`                                     |
| Implementation tree        | `89ae4d157132c115d973846eb6be38037b40ab7b`                                     |
| Documentation handoff      | `013b7234f644b0b5b80d153f631c8203d38f398b`                                     |
| Published parent           | `0a2df4aebc4e87be3d1ae802f05c2fce64b55072`                                     |
| Local layer 03             | 15 implementation/checkpoint commits; 86 changed files relative to that parent |
| Publication                | Layer 03 has no push, PR, hosted CI or G2 acceptance                           |

The handoff commit changes documentation only. Gameplay implementation was
committed at wind-down; the later architecture cleanup is now published as noted above. The
independent untracked `pr464_ci_review_20261110.md` and original stashes are
preserved. No merge, auto-merge or upstream push occurred.

GitHub metadata was refreshed during this wind-down. The published chain remains:

- [#464](https://github.com/larsonm-personal/dxx-redux/pull/464), prerequisite,
  `e87f4bfaeec60fd7a9697488413e02cc88230ade`, based on `cmake` at `b3bb12cbe`.
- [#465](https://github.com/larsonm-personal/dxx-redux/pull/465), G0 CI repairs,
  `96ce2f8be5f917178024298cf51272781abc0caa`, based on #464.
- [#467](https://github.com/larsonm-personal/dxx-redux/pull/467), G1 timelines,
  `0a2df4aebc4e87be3d1ae802f05c2fce64b55072`, based on #465.

At that snapshot all three PRs were open, unmerged and marked ready for review. Published
PR metadata links [stack 466](https://github.com/larsonm-personal/dxx-redux/stack/466).
The local continuation branch is not a member: `gh stack view --json` reports
that fact. Do not run stack checkout merely to inspect it or move this checkout
away from the tested implementation.

For subsequent work, fork PR publication and passing applicable CI are part of
each checkpoint, including coherent partial G2 work. Do not wait for full G2
closure before pushing. Create/update non-draft immediate-parent PRs in
`larsonm-personal/dxx-redux`, monitor CI while continuing independent work, repair
failures and verify the final candidate. Never merge or publish upstream. The
[next-window plan](xl_gameplay_next_12_hours_20261112.md) and
[kickoff prompt](xl_gameplay_kickoff_20261112.txt) contain the current procedure.

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

Use these distinctions when interpreting a green workflow or a large skip count:

| Result class                                          | What it establishes                                             | Required follow-up                                                          |
| ----------------------------------------------------- | --------------------------------------------------------------- | --------------------------------------------------------------------------- |
| Replay job skipped after its build failed             | No replay ran                                                   | Fix the build and obtain an actual downstream result                        |
| Path/manual/platform condition false                  | The workflow deliberately selected no work                      | Check that the condition covers the changed code and required platform      |
| Missing optional addon or full Warp resources         | Only the remaining bounded owner ran                            | Record the missing input and a separate full-resource receipt, if available |
| Expected NDEBUG connectivity-warning skip             | A debug-only assertion owner was excluded in that configuration | Keep the reason explicit; do not count it as gameplay coverage              |
| Historical readiness workflow passes its expected 4/8 | The frozen baseline behaved as specified                        | Publish current playability separately; the four failures remain failures   |
| Required original-asset owner passes                  | Its required 113-test cohort executed on its named candidate    | Inspect the downstream receipt, not just the request/controller workflow    |
| New failing probe is never selected                   | Neither pass nor skip represents that probe                     | Integrate the repaired regression into its existing canonical owner         |

The original review also exposed provisioning failures, such as apt DNS errors.
Those are distinguishable from GCC compile errors and runtime failures in their
logs. Retrying an infrastructure failure can be appropriate; it supplies no
explanation or exemption for a reproducible engine defect.

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
  basenames in live lookup. Busy handles block commit/discard. Invalid initial
  descriptors and the tested late archive loss/corruption cases preserve the
  previous mission. A descriptor changed after preparation instead commits the
  retained valid snapshot without reparsing the changed file. Filename case follows the
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

### What the current staging actually guarantees

Source inspection narrows several claims that would otherwise be easy to overread:

| Boundary                                      | Present implementation                                                                               | Still needed for G2                                                                                                                                                                                                                            |
| --------------------------------------------- | ---------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Mission descriptor and private archive access | `mission_prepare_*` owns parsed custom D2 descriptor data and permits origin-checked candidate reads | `mission_commit_prepared` still calls the native mutating loader; this is not an atomic world commit, and builtin/D1 mission paths need their own compatibility coverage                                                                       |
| Definition dependency traversal               | `MissionDefinitions` retains copied tables and binds their views to the surviving owner              | Its initial data comes from live globals; `mission_base` is captured after mission HAM initialization. It cannot yet independently construct a different target mission's complete definition environment                                      |
| Resource preparation                          | HXM/model/POG and other existing resource owners prepare substantial data                            | Companion lookup, level ordinal lookup, bitmap/sample banks and some GL preparation still depend on the current installation. `ObjectDependencies::bitmap` pages a live bitmap; copied definition tables alone do not make this operation pure |
| Resource installation                         | Prepared owners retain payloads and the camera move now rebinds pointers                             | `install_pog` can still report allocation failure. All recoverable commit-time failures and ownership transfers must be accounted for before claiming atomic publication                                                                       |
| Native save records                           | AI and object/player decoding have candidate-aware pieces                                            | The enclosing restore still selects a mission, changes clocks/player state and starts the level before late validation                                                                                                                         |
| Observation                                   | Many typed native/XL fields are compared, with genuine cold tests for bounded scenarios              | `named_audio_snapshot` destroys stale object audio, reconciles owners and can clear the loop. A before/after snapshot can itself change state; observer purity is an acceptance prerequisite                                                   |
| Guide-Bot continuation                        | Saved metadata, fixed state and some route behavior have witnesses                                   | Async completion/cache state and the Android certifier's 2,000 us / 512-work budget remain unqualified for deterministic publication                                                                                                           |

The primary source anchors are [state.c](../../d2/main/state.c),
[mission.c](../../d2/main/mission.c), [bm.c](../../d2/main/bm.c),
[resource_validation.cpp](../XL_compat/runtime/resource_validation.cpp),
[asset_dependencies.h](../XL_compat/runtime/asset_dependencies.h),
[named_audio.cpp](../XL_compat/runtime/named_audio.cpp) and
[secretarea.c](../app/src/main/cpp/shared/secretarea.c). In `state_restore_all_sub`,
`load_mission_by_name`, clock assignment, player initialization and
`StartNewLevelSub` precede `state_validate_runtime_state` and
`xl_wall_trigger_read`. On Android, additional adaptation/metadata work follows
that XL read, so moving just the XL checksum check earlier cannot establish the
entire restore contract.

The rejected-save witness lives in
[projectile_test.h](../XL_compat/runtime/projectile_test.h), mode `birth-atomic`.
It asserts rejection and unchanged energy; the retained failure reports
`accepted=false`, `energy_before=5963776` (91), `energy_after=6553600` (100).
The [canonical projectile owner](../XL_compat/tests/projectile.cmake) invokes
`birth` and `birth-resume`, but not `birth-atomic`. Thus its green result is
consistent with this real defect. Repair and add this case to the existing owner,
without changing the required top-level cohort or weakening its timeout. Expand
the unchanged-state assertion beyond energy and test future behavior after a
rejected restore; energy alone is a sufficient failure witness, not a sufficient
atomicity proof.

## Latest verification and runnable artifacts

These are checks of the final implementation at `3660e9a14`, before the
documentation-only handoff commit:

| Check                                                                | Result and scope                                                                                                                                             |
| -------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Windows/Linux developer-ON desktop and metadata builds               | Pass                                                                                                                                                         |
| Windows/Linux ordinary developer-OFF engine and compatibility builds | Pass                                                                                                                                                         |
| Selected D2 `test_upstream_compat` owner                             | Windows 61.11s; Linux 36.80s; `^test_upstream_compat$` selects one combined CTest entry each, zero skips; not a rerun of the full native CTest suite         |
| Original Observatory owner on both hosts                             | Pass, including retained definitions, uppercase private read, malformed/archive/identity controls, persistence and pinned 12 firing/four contact comparisons |
| Ordinary save-start replay                                           | Two clean Windows processes and one Linux process pass strict result/assets/clean exit                                                                       |
| Detailed live-capture comparison for all three replays               | 12 object frames, 16 world records, 21 SIM and 12 FX events each; no differences                                                                             |
| Automation catalogs and scoped mixed quality                         | Pass; 99 standalone JSON, 315 standalone PowerShell, 547 support scripts; 428 master top-level entries                                                       |

The short replay is original WShock20 level 6, with five setup position assists,
zero inventory assists and zero assists during recording. It is not a fresh-start
route or completion. The additional Windows HXM/Warp5 warm/cold actor owner passed
before the final camera/case correction; its assisted actor setup and unexecuted
optional camera subcase remain explicit. Do not report it as final full-corpus CI.

The 2,549-frame G1 capture and earlier G2 projectile/route/producer/time owners
remain valuable historical evidence at their recorded revisions. They were not
all rerun on `3660e9a14`. The final 12-frame check demonstrates a real regression
was repaired and that this short continuation matches across hosts; it does not
cover a sustained encounter, a complete level, every state owner or every earlier
branch change. Linux's final replay used the visual runner, not true headless.
The combined compatibility owner, reference tests and original-content play also
have different scopes; their counts must not be added into a playable-level score.

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

Evidence shortcuts (local artifacts may need restoring from preserved storage):

- [Final source/binary/input manifest](../../temp/pr464-ci-fixes-20261110/g2-dependency-definitions-build-identities.json)
  and [three final comparisons](../../temp/pr464-ci-fixes-20261110/g2-dependency-definitions-fixed-compare.json).
- [Known atomic-restore failure](../../temp/pr464-ci-fixes-20261110/g2-atomic-after-birth.log).
- [Windows replay invocation](../../temp/pr464-ci-fixes-20261110/g2-dependency-definitions-fixed-replay-windows.ps1)
  and [Linux replay invocation](../../temp/pr464-ci-fixes-20261110/g2-dependency-definitions-fixed-replay-linux.ps1).
  These scripts use fixed output directories and are receipts, not idempotent
  rerun commands. Use a new retained output path and the existing runner when
  repeating them; preserve previous successful and failed output.
- [09:29 PST remote snapshot](../../temp/pr464-ci-fixes-20261110/winddown-20261112-remote-receipt.json).

Retain the manifest with the actual tested source diff and executable hashes.
A cached build label or a synthetic commit timestamp is not proof of source
identity. The repository post-commit hook can amend timestamps; record the
post-hook HEAD/tree and wall-clock execution time separately.

## Where the 90% target stands

**Current playable-level percentage: not established.** There is no defensible
percentage gain to report from these commits yet.

The immutable catalog has 1,111 advertised level entries in 23 archives, including
classic/imported content, multiplayer content and repeated levels in different
packages. Its SHA-256 is
`bf4356a7a5fbe64832a40d7065ea02d8b81276ebd8bbba6874012f7851fa0cf8`.
Recounting the checked-in JSON gives 1,020 normal entries, 91 secret entries and
356 distinct archive/descriptor pairs. `levelpack.7z` alone contributes 866
entries (78.0% of the raw total). These are advertised entries, not 1,111 unique
level-byte hashes or confirmed XL-active levels. The JSON has no per-entry
single-player eligibility or current playability result.

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

The next worker should produce the following accounting through the existing
inventory tooling, without modifying the frozen catalog or adding a new harness:

1. Preserve each catalog ID and its archive/descriptor identity, normal/secret
   ordinal, level hash when available, resource/definition profile and evidence
   of single-player eligibility. Do not deduplicate by filename or level bytes
   alone: identical level bytes under different HAM/HXM/POG, mission order or
   secret-return rules can have different gameplay.
2. Report raw entries, proven exclusions and unresolved eligibility separately.
   For a conservative interim bound, keep unresolved entries in the denominator.
   Also report classic/imported versus XL-active counts and package-level results,
   so a large stock-content pack cannot conceal poor XL support.
3. Record independent dimensions: parse/admit, ordinary render/control, connected
   route/combat, save/cold continuation, completion/transition and replay/platform
   qualification. Record assists and source/binary identities separately. A
   single `pass` label would hide material differences in evidence.
4. Keep directly qualified, feature-inferred, known blocked and untested counts
   distinct. If `E` is the frozen eligible count and `P` the directly qualified
   count, the conservative demonstrated rate is `P/E`, with target
   `ceil(0.9 * E)`. An inference is a separate estimate unless the user has
   explicitly accepted it as the measurement standard. Do not pool a Windows
   success and a different Android success into a platform-wide percentage.
5. Attach the complete known blocker set to each entry. Count the union of
   affected entries, and estimate how many become unblocked only after a proposed
   set of fixes. Summing individual blocker frequencies double-counts levels and
   overstates the gain. A first parser rejection does not identify all blockers.

The original G0-G7 closure gate and the later 90% request are related but different:
G5 asks for a compact set of connected witnesses and an original completion; G6
asks for a small durable cross-platform corpus. Passing those gates does not
automatically certify 90% of a large catalog. Conversely, a high desktop loading
rate would not close the required save/replay, Android or delivery gates.

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

| Work                                                             | Completion evidence                                                                                                                                                                                          | Remaining hours |
| ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | --------------: |
| Freeze the 90% denominator and count blockers across the catalog | Per-entry identities, eligibility/unknowns, overlapping blocker sets and package/XL-active breakdown; first census may remain partial                                                                        |             3-6 |
| Finish G2 continuation and atomic restore                        | Target HAM/resource ownership; native level/world/runtime/XL staging; non-failing publication for recoverable errors; pure observation; Guide-Bot scheduling audit; negative and cold/uninterrupted controls |           18-32 |
| G3 simulation/presentation separation                            | Logical audio/event state independent of mixer success; genuine no-device XL replay; normal rendered behavior and diagnosis of Android latency                                                               |           10-18 |
| G4 remaining source-qualified gameplay                           | Catalog-ranked source predicates, complete dispatch and resource/lifecycle semantics; original inputs and malformed controls                                                                                 |           12-24 |
| G5 connected original gameplay                                   | Earned Warp5/time-equipment and Dinter routes, real combat/loot, cold continuation, death/secret/transition and an original completion                                                                       |            8-16 |
| G6 platform and durable corpus qualification                     | Compact live/immediate/repeated rendered/headless corpus; Android import/save/record/export and Android-to-host comparison; forward contract                                                                 |            8-16 |
| G7 final CI, fork publication and packages                       | Final candidate public/private/affected CI, exact skip reasons, coherent stack, runnable Windows folders and universal APK with hashes                                                                       |             4-8 |
| **Revised engineering planning allowance**                       | **Focused G0-G7 closure work, subject to census and architecture checkpoints**                                                                                                                               |      **63-120** |

This replaces the earlier **45-80 hours** as the working allowance. That range was
an optimistic judgment, not a throughput-derived forecast. The increase reflects
the still-live resource dependencies, fallible installation, unstaged level/world
state, missing canonical negative coverage and unresolved Android/Guide-Bot
boundaries found above. No measured task velocity supports a confident deadline;
63-120 hours is also a low-confidence planning range, not a confidence interval
or a commitment to reaching 90% within it. A newly discovered format family or
pervasive rendering/AI defect can exceed the upper end.

The rows assume sequential work, reuse of the existing owners, accessible genuine
inputs and no broad new framework. Count shared work once: G2 owns pure snapshots
and persistence; G3 owns device-independent audio/event behavior; G6 owns the
final platform/corpus qualification. Include local build/test time in each row;
unattended CI queues and asset acquisition delays can add calendar time. Diagnose
Android latency in G3, validate it in G6, rather than billing the same fix twice.

Catalog-wide play qualification is **additional and currently unestimated**.
For illustration only, 1,000 witnesses at five to ten minutes each require
83-167 hours of execution before setup, failures and retries. Five to ten minutes
is not a measured route duration and may be far too short for full completion.
Measure total owner/setup/restore time on a representative set, then forecast
`remaining entries * measured mean time / 60` with an explicit failure allowance.
Reuse an earlier result only where its source/asset identities and affected-code
scope justify it. Neither the engineering range nor this arithmetic proves the
threshold will be reached; census and measured throughput must replace assumptions.
Missing genuine gameplay-critical assets remain content blockers rather than code
workarounds. No credible promise of reaching 90% in the next 12 hours is supported.

Specific unresolved predicates must stay visible until classified:

- Smoke actions 20-24 are source no-op leaves, but activation, delay, master
  traversal, flags and owner lifetime still execute. Action 27 needs a separate
  audit; action 35 reprograms robots and is not a cosmetic action.
- Equipment IDs 7/8/9/24/26/27/46/47 need creation, selection RNG, occupancy,
  resources and release/single-player behavior traced. An inert pickup reward
  does not establish that deleting the generated object is valid.
- `subzero.rl2` object 4 movement 255 remains a record-boundary/runtime question;
  the current pinned-input offset noted in the audit is 830889. Do not reuse the
  older 923615 offset without its different input identity, or normalize unknown
  movement tags wholesale.
- Older version pairs 11/31, 17/35, 18/36 and 19/40, colored walls including Glass
  House's 126 type-8 records, cameras, geometry mappings, static movement,
  waypoint controllers and other source predicates require actual occurrence
  counts and native behavior, not blanket guard removal.
- Missing Boiling waves remain exact resource/policy questions. Missing
  Waterworks POG may affect collision/visibility; it cannot be waived as cosmetic.
  Dinter's disconnected raw segment components require authored event-route
  evidence, not a teleport assist or an assumption that the geometry is broken.

The [capability audit](../XL_compat/evidence/gameplay-capabilities-20261111.md)
contains source anchors for these cases, but its G1/G2 status rows are historical.
Update only the affected rows when new evidence exists; do not treat an old
blanket-rejection description as proof that already implemented features are absent.

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

Do not merge or publish incomplete layer 03 as accepted G2. At wind-down about
15.5 GiB was free. Official idle cleanup preview/apply found zero eligible artifacts while
preserving authentic inputs, evidence, native caches, replay sandboxes and emulator
workspaces; the 50 GiB target was not reached. Follow `android/CLEANUP.md` before
large builds and recheck live space/jobs rather than assuming this snapshot still
holds. No native build, formatter or test was intentionally left running at
wind-down. Preserve all current unrelated/untracked files, including any `.codex/`
configuration, as well as the independent review and stashes.
