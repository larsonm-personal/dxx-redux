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
