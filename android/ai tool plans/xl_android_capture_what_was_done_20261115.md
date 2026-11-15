# Android XL live capture tranche - November 15, 2026

The explicit next-tranche invocation began around 10:12 PST. The user answered
"no bound", overriding the expired kickoff deadline. No agents were used. This
report covers one capture/lifecycle slice and does not schedule another tranche.

## Delivered behavior

The existing native recorder now opens the existing detailed state observer at
the completed Android quick-record startup boundary in explicit XL developer
builds. Automatic recording is labeled new_level; checkpoint recording remains
save_checkpoint. Trace filenames do not overwrite retained files after a process
restart. No second parser, replay engine or comparator was introduced. Ordinary
Android and true-headless XL timeline admission guards remain unchanged.

The protected water owner supports controlled-portal and automatic level-start
captures. The existing original-addon owner records slow motion, bullet time and
a normal toggle after fresh-process save restore, then replays all three through
the shared native Android helper before restoring state. Failed captures survive.

Published non-draft immediate-parent checkpoints in native stack 511:

- [PR 513](https://github.com/larsonm-personal/dxx-redux/pull/513), head
  51edd57ede67a8e70fd8b06c344dd17b36431a5e, parent PR 512 at
  ac628f537a3a6c208da34fe3607f1346b9b28681, initial merge candidate
  57a61918ff8a1a622a11db8f765527e0c0c24226.
- [PR 514](https://github.com/larsonm-personal/dxx-redux/pull/514), implementation
  head 67c8dcf083757d37c4ff53074864a33040021dda, parent PR 513, initial merge
  candidate 88edad187b6510d17e92371deb28b6acd81154a9. The documentation push
  creates a later candidate, audited separately in the final receipt and PR body.

Nothing was merged, auto-merged, queued or published upstream. Historical stack
471 was not reused. Four stashes and unrelated .codex/ remain untouched. The
sound stash was neither applied nor tested.

## Complete comparison matrix

The unchanged d1_replay_parity.py comparators check every frame diagnostic,
objects, complete world/XL state, SIM RNG, FX RNG and the terminal result. No
differing fields or events are removed.

| Android-origin recording  | Frames / world records | SIM / FX events | Android developer | Ordinary Windows | Ordinary Linux |
| ------------------------- | ---------------------- | --------------- | ----------------- | ---------------- | -------------- |
| Controlled water portal   | 44 / 48                | 64 / 87         | PASS              | PASS             | PASS           |
| Automatic new-level start | 129 / 133              | 0 / 128         | PASS              | World FAIL       | World FAIL     |
| Slow-motion control       | 85 / 89                | 1172 / 0        | PASS              | PASS             | PASS           |
| Bullet-time control       | 85 / 89                | 15 / 0          | PASS              | PASS             | PASS           |
| Cold-restored toggle      | 85 / 89                | 75 / 0          | PASS              | PASS             | PASS           |

The final time recordings are byte-identical inputs on all three platforms:

- Slow: 102efaff20b18c52bde78c801b90f5f9a30fab2c7eb2a7dd309d0cd0655f7024.
- Bullet: 375172aab821a522191e276abac575eb89d321faeef3eae96ca7560f3143ba1e.
- Cold: 9e3142cbe80eac3313ae18970c8f884a2c03d7afc5c16d4507b1b7db0e0b978f.

Final ordinary Windows binary:
75ba3ab9e08c36c0b6d08fb63241160b8d7cd7ffc5f9e41ea7729133c248276a.
Final ordinary Linux binary:
54e88ff85a1424f8444875df6f2eb0729cfccebb3c9bda068e5fcf16910b99cc.
Both are rendered playback; Xvfb is not true-headless qualification. Zero event
counts limit coverage rather than proving stress coverage of that RNG stream.

Both hosts retain 207 automatic-start world-field differences in canonical route
metadata/plan summaries, beginning at the restored boundary at frame 0 and
persisting through the terminal boundary. Every other comparator passes.
Android's deferred planning and host-populated canonical plan lack a demonstrated
common startup-state contract. Checkpoint restore carries this state; new-level
startup does not establish equivalent ownership. This admission gap stays open.

## Other validation and assists

Four native Debug/assertion contracts pass: Windows D2/D1 in 89.93/18.25 seconds,
Linux D2/D1 in 30.00/2.73 seconds. All 61 public XL reference/source/GPU/audio
contracts pass. Both catalogs pass: 99 standalone JSON, 549 support scripts,
315 standalone PowerShell tests and 428 master entries. Scoped formatting passes.

The final time owner passes warm 39/39 and cold 14/14, distinct process/run IDs,
64 original resource hashes, unchanged saves, three replay results and exact
protected-profile restoration. Ordinary Android water passes 59/59 in 40.307 s.
Ordinary Android replay rejects with the unchanged not-yet-qualified error and
produces no replay result. Ordinary Windows also rejects the low-memory profile
before playback. These expected-negative owners exit 1, not positive replay PASS.
All protected water before/after manifests match exactly.

Assists: one initial water portal pose; five original time-equipment poses outside
recordings; a native-written pilot with Warp Shock level 6 unlocked; scripted
launcher/menu/graphics confirmation; normal axis/meta-action controls. Pickups
use native collision. Automatic-start capture has no pose assist. No fixture
position writes occur during recording. These short cases do not prove campaigns,
actual multi-level progression, physical devices, full G2-G7 or 90% playability.

## Frozen readiness and retained failures

The unchanged eight-case cohort at asset revision
9dad21bd65ba05665ac6fe315f37fe7461e85725 again yields 4/8 on the exact ordinary
D2 headless-route target, SHA256
6ab97fdcea670b0950adc8ddc0a27f8e0c09e9c597d365849bed011d98f2bdd4.
The criterion remains the requested original level plus 120 unassisted frames.
No rerolls, injected definitions, deadline/assertion/catalog relaxation or
rendered substitution occurred. Time-equipment fixtures are a separate case.

Retained failed receipts include: initial SDL timer/header compile errors;
shared-storage readiness timeout and repository-helper recovery of the same AVD;
unaccepted graphics confirmation and viewport rejection; stale companion-APK
instrumentation crash; host viewport, installation-shadowing, Vertigo-location
and disabled-audio preparation failures; a Linux filename typo; and six Python
NameError failures before engine launch. Corrected attempts use fresh outputs.
The full startup-world mismatch remains FAIL, not a corrected preparation error.

Older Windows Debug water timeout (1145/1235 frames at the unchanged 60 seconds),
D1 software-fixture GL compilation limitation, missing HXM definitions/owned
camera semantics and corrupt-trailer energy 91-to-100 remain open.

## Source, input and device receipts

xl_android_capture_receipts_20261115.json records source hashes, APK/library/cache
identities, original input/resource hashes, captures, comparison counts, binary
hashes and restoration. The three compiled observer source hashes match committed
source. Private originals, full comparisons and failures remain under
temp/xl-android-capture-20261115; no original asset bytes were published.

Developer APK: 46dd38c6ed7df81d36265f9d5f244a2928f79a51d40a8c3ce9cfbd26f830dc97,
x86_64, both games, developer ON. New ordinary APK:
91121130dc0167d969a9ddb8d093daf84d39384a198e97905250ebb010818639,
both games, arm64-v8a/armeabi-v7a/x86_64, developer OFF. Installed hashes match.
Existing warnings remain in unchanged native sources, not the observer hook.

The original frozen ordinary APK was restored with install -r -d, without
uninstalling or clearing data. Verified installed SHA256:
504848a52babb1c5c286e1c013d52f436cf6d1ffeaddf725444d93a41172dbb5.
The game is stopped; both exclusive backups and the disposable imported set are
absent. Authentic saf_provider_source fixtures remain retained. No workspace
cleanup was needed or performed.

## CI snapshot and final-push verification

xl_android_capture_ci_20261115.json records observed heads, merge candidates,
workflow/controller/candidate IDs, URLs, attempts and job outcomes. Pending jobs
at this documentation snapshot are not passes. Request workflow success is only
a signal to the private controller. Skips and incomplete coverage stay explicit.

After the documentation push, inspect its exact nine candidate workflows, private
controller, meaningful skips and public replay negative controls. Retain the
final-handoff-receipt.json locally and update PR 514's body with final identities
and outcomes. The committed snapshot does not claim subsequent jobs passed.
Completed old watches are not restarted.

## Revised estimate

Android-origin checkpoint/time-equipment evidence is established on this emulator
and both ordinary hosts. Android admission still needs owned startup metadata,
actual level-handoff coverage and broader platform evidence. No unconditional
8/8 estimate is supported with frozen missing inputs/camera semantics. The former
two-to-four tranche range remains conditional and low confidence. Re-estimate the
startup-ownership slice after concrete design/reproduction; these short passing
recordings do not imply whole G2-G7 completion.
