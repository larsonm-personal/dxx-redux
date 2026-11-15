# XL gameplay tranche, November 14, 2026

Readiness remains **4/8**, not campaign qualification. The exact ordinary Linux
CI target reproduced all four frozen zero-frame failures. Ambient/effect output
now retains logical objects and linked owners when PCM is absent, without a fake
wave or successful voice. Required named action/transition sounds, malformed
files and foreign origins still reject. Subsequent barriers are slowdown.wav
(BoilingPoint/WShock20), smokingfire (Sphere) and falling-rocks.wav (Dinter).

## Window and publication

The requested 49-minute delay completed; work started November 13 at 22:51 PST.
The window ends November 14 at 10:51 PST. Execution was interrupted between
approximately 23:36 and 10:34 PST; old test handles and emulator were absent on
resumption. This is not eleven hours of validated work or an hourly watch.
Only validation/publication/handoff resumed. No new feature work starts now.

Base/published PR 494: d37a794c5204803c093cbd010a6e8ee711414bdb; merge
7a175e188ff076146c825e7d3f2da59ca3e03046. Parent PR 493:
845500bad07847b97d44c0873abc40371299885c. New branch is
xl_gameplay_20261114_29_headless_readiness, based directly on
xl_gameplay_20261113_28_ci_handoff. Only origin larsonm-personal/dxx-redux is
authorized, non-draft immediate-parent PR linked stack 471. No merges/upstream.
Publication/check IDs will be appended after actual publication.

Baseline checks: 33 success, six intentional skips, readiness FAIL, none pending.
Private run 43551566845 attempt 1 passed 113 required owners, 16 witnesses,
three modern levels and four assisted anchors in 775.577s:
https://github.com/larsonm-personal/dxx-redux/actions/runs/43551566845
This does not qualify the new candidate. Controller:
bf40de9bc3b85c2279fe8163c621bb6c052dec12. Frozen readiness assets:
9dad21bd65ba05665ac6fe315f37fe7461e85725. Runner/catalog/verifier match controller.

## Receipts and limits

Private evidence: temp/xl-readiness-tranche-20261114. Keep every failed receipt
and authentic input. baseline/ contains exact per-case diagnostics: lightning.wav
(BoilingPoint/Sphere), falling-rocks.wav (Dinter), computer.wav (WShock20).
Eleven retail sources passed their verifier. Baseline engine SHA-256:
e6b33022a7311ffcc3c0a18d7241dc1b3eacb69fed0f581bed45bced6921f65f.

Final code is base plus this patch. Saved raw ambient-checkpoint.patch SHA-256 is
40a0996feb7365f9996363896a9089d571d72ec1c0c9fe69112142aaa53705eb
(before final documentation). Binary SHA-256 receipts:

| Owner                           | SHA-256                                                          |
| ------------------------------- | ---------------------------------------------------------------- |
| Linux ordinary route            | 447a224af5cfb8cf7524e99973cd23a1a1808c6e1a54460f6d7ef0ba13e08d0b |
| Linux optimized Debug metadata  | 674c018918cb1090bee090fff7ed15f19355baa8ab71d22a0b1b4fc9bcfd3907 |
| Windows ordinary route          | e418bece651ca523b0bd99f810d14a48806479eb1d92a040ac6f1c81213132a1 |
| Windows RelWithDebInfo metadata | ebb9be4388785ddaea2209d27527b49c174bee99dba5f54606ce8c3c527ce425 |
| Android ordinary three-ABI APK  | e78c11875a871153e1d25c2b01c50417aee4ac918178efa8aad1e3d3aebfb33f |

Full audio owner PASS: Linux optimized Debug/assertions 92s
(ambient-debug-linux-07.exit); Windows RelWithDebInfo 115.25s
(ambient-release-windows-02-receipt.json). Unchanged 150s master/child deadlines.
Missing/present 23/40 and 27/40 activation receipts match; actual-source owners,
PCM, scopes, save/restore, malformed output and fabricated-voice negatives pass.
Windows Debug runtime controls FAIL by timeout, including optimized C++ with
checked iterators/assertions retained. CPU was active, no assertion dialog.
Windows Debug mixer and Linux mixer/source contracts PASS. Catalog PASS:
428 top-level entries, 547 support scripts. Ordinary Windows/Linux and three-ABI
Android builds PASS.

Android's earlier 145-step named-audio owner PASS followed a corrected initial
wait for time_paused=false as well as in_game. Seismic exposed the same graphics
pause race; its wait is corrected without relaxing the 20s deadline/assertion.
The final combined run FAILS infrastructure: emulator/ADB disappeared during
stock transition. App-private files/xl-named-audio-runtime-backup may remain;
recover and verify before another run. Do not reuse a stale PASS result.json as
the final run's outcome. Recovery completed after resumption: all 26 original
files match before.sha256. The original archive is retained locally as
android-interrupted-original-state.tar; the device retains the archive and failed
state in files/xl-named-audio-recovered-20261114. Receipt: android-recovery.json.

Original xsound.hog remains pinned:
87faf44175cf565e857baee2edfafd10c7feed8b57cb4cda0258b85e3e9d44f9.
New cases use separate xambmiss. Synthetic probes/assisted replay are not readiness.
No new final real-capture/clean-playback qualification completed in this interrupted
window; old captures do not qualify the current candidate.

## Open work

Estimate: three further 12-hour tranches, range two to four, low confidence.
Full G2 still changes energy 91 to 100 before corrupt-trailer rejection. Android
startup FX has 1,500 extra draws; camera needs its pinned addon; Windows Debug
timeouts remain open. No G2-G7, 90% playability or unassisted campaign claims.
Untested native sound stash 9783012e2a77dfdd4680812ae72b852cfaca5e17 and all other
stashes remain untouched. Unrelated .codex/ stays untracked.

## Resumed validation and publication receipt

Published non-draft PR https://github.com/larsonm-personal/dxx-redux/pull/497
directly on #494 and linked native stack 471. Code candidate/head:
37276daac3630e8ef63780427992c7b998a9fb8d; synthetic merge:
c065987496dfa4a00ad2ddabf83beee3887aeb0f. No merge was performed.
Current trusted controller advanced to
72fdd76f5f8dc24a787b3a4858de1115ec091c19 during the interruption. Its readiness
runner/catalog/retail verifier compare byte-for-byte equal to this candidate.
Private qualification run 43627821931, attempt 1, targets this exact code head:
https://github.com/larsonm-personal/dxx-redux/actions/runs/43627821931
It is still pending/running at this receipt, not a pass. Request workflow
43627807247 succeeds only as a dispatcher.

After recovery, the final ordinary APK passes the complete Android owner:
xsound 145/145, run ff652cf7dc1f4279bd2c1ad1881ecc99; xseismic 64/64,
run 5dc4f8bec53646b29fe1d8f11593b122. Both before/after profile hashes match.
Exact receipts: android-named-audio-04.log and android-final-receipts.json.
Earlier race/transport failures remain retained, not replaced by these passes.

The committed ordinary headless cohort again measures 4/8. The first committed
receipt records a trailing carriage return in its SHA argument; it is retained
as a metadata defect. The same frozen cases completed again at 4/8 in
committed-readiness-02 with the exact clean candidate SHA, unchanged inputs,
assertions and deadlines.
All four successful sampled loads have XL extensions inactive. This is not
four qualified XL campaigns. A documentation-only follow-up records these
outcomes and receives its own CI cycle, which must be inspected separately.

### CI snapshot before documentation-only push

2026-11-14T18:43:35.396443+00:00; code head 37276daac3630e8ef63780427992c7b998a9fb8d. Pending is not pass.

| Check owner                                          | Outcome     | Receipt                                                                                        |
| ---------------------------------------------------- | ----------- | ---------------------------------------------------------------------------------------------- |
| Android tests / Run selected tests                   | IN_PROGRESS | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807206/job/132547753300) |
| JVM tests / Run selected tests                       | IN_PROGRESS | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807206/job/132547753227) |
| Linux tests                                          | IN_PROGRESS | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807206/job/132547753066) |
| execution-stack / check                              | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807206/job/132547713037) |
| Two-player lobby to game / Run selected tests        | IN_PROGRESS | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807210/job/132547674531) |
| scripts and ci tests (windows-2022)                  | IN_PROGRESS | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807268/job/132547582001) |
| scripts and ci tests (ubuntu-24.04)                  | IN_PROGRESS | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807268/job/132547581980) |
| Asset-free XL checks (ubuntu-24.04)                  | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807250/job/132547558303) |
| XL developer source compile (no game assets)         | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807250/job/132547558315) |
| Select tests                                         | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807206/job/132547559063) |
| XL developer source compile (Windows x86)            | IN_PROGRESS | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807250/job/132547558287) |
| Asset-free XL checks (windows-2022)                  | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807250/job/132547558339) |
| Linux d1 build and tests                             | IN_PROGRESS | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807273/job/132547551321) |
| Linux public build and save integrity                | IN_PROGRESS | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807214/job/132547552506) |
| Linux d2 build and tests                             | IN_PROGRESS | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807273/job/132547551347) |
| Linux public ASan runtime                            | IN_PROGRESS | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807273/job/132547551293) |
| Verify pinned Pumo staging                           | SKIPPED     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807206/job/132547560623) |
| XL developer Release source compile (no game assets) | SKIPPED     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807250/job/132547559457) |
| server                                               | SKIPPED     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807281/job/132547555351) |
| stack / check                                        | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807214/job/132547509976) |
| CI policy and runner checks                          | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807273/job/132547509929) |
| Select public LAN smoke                              | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807210/job/132547510025) |
| stack / check                                        | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807273/job/132547510142) |
| stack / check                                        | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807268/job/132547510144) |
| stack / check                                        | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807281/job/132547509864) |
| stack / check                                        | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807250/job/132547509983) |
| Signal exact PR head for trusted XL qualification    | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807247/job/132547509779) |
| Workflow syntax                                      | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807268/job/132547509927) |
| Automation catalogs                                  | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807268/job/132547510173) |
| stack / check                                        | SUCCESS     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807206/job/132547510838) |
| Inspect retained online receipt                      | SKIPPED     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807206/job/132547512083) |
| Export public timing evidence                        | SKIPPED     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627807210/job/132547511206) |
| XL asset smoke (private assets)                      | PENDING     | [job](https://github.com/larsonm-personal/dxx-redux/actions/runs/43627821931)                  |

All listed workflow runs are attempt 1: 43627807247, 43627807281, 43627807214, 43627807250, 43627807268, 43627807273, 43627807210, 43627807206.

Skip reasons: Release developer compile is restricted to two named Pumo branches;
Pumo staging requires explicit workflow_dispatch/campaign; online receipt inspection
and public timing export require an evidence_run_id; Rust gate omits unrelated
changes (its actual log says native non-tip or unrelated changes). These are not
passing tests. Dynamic downstream owners still pending are not silently omitted;
retain pr497-checks-handoff.json and pr497-private-jobs-02.json for the snapshot.
