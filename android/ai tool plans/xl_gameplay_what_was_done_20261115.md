# XL gameplay qualification, November 15, 2026

This explicitly invoked tranche started November 14 at 22:37:54 PST, after the
user merged the preceding stack. Its hard deadline is November 15 at 10:00 PST
(18:00 UTC); 06:00-10:00 PST remains reserved for validation, CI repair and handoff.
Work is sequential, without agents. This report is published while candidate CI
is completing; pending jobs are not passes. No merge, auto-merge or enqueue is
authorized or performed.

## Delivered behavior

[PR 509](https://github.com/larsonm-personal/dxx-redux/pull/509) preserves the native
500-star endlevel presentation owner across mission-resource reloads in both
games. Engine shutdown retires that owner through close_game -> close_endlevel.
Stars have no mission, palette or GPU ownership dependency. The generation
algorithm is unchanged. There is no RNG rewind, trace trimming, event filtering
or replay compensation.

The new native owner test fails on the baseline's redundant generation, then
passes on the repair: first initialization, three reloads in each of two engine
sessions, identical visible pixels and unchanged SIM/FX state, and exactly 1500
fresh FX draws after genuine engine teardown/reentry. Full flyout and original
fan-mission controls also pass. The active-flyout save-rejection fixture now
supplies the caller pause consumed by the native save subroutine; its assertion
and rejection behavior remain intact.

The Android developer replay now matches all 1808 FX events instead of producing 3308. It also matches all 1235 object frames, 1239 world/XL records, 196 SIM RNG
events and the embedded result. Ordinary Windows and Linux clean replays match
the same complete capture. This resolves the observed extra-starfield-draw
defect for this capture, not full Android replay admission or campaign playability.

Ordinary Android water runtime passes 59/59 steps. The ordinary APK is restored
and its native XL timeline guard is verified: it rejects the capture with
"XL clock/input timeline recording and playback on Android are not yet qualified".
That negative owner exits 1 as expected, produces no replay result and restores
protected state. It is not counted as successful ordinary Android playback.

## Source and authentic inputs

- Fresh base: cmake/origin/cmake 5f8ab3535bec69b86a287f6516a5bddd6b81c383,
  tree d59e394f6413066103c9305c9c0e6e2a7d16dd8c, following merged historical stack 471.
- Implementation: def09a9e98f209088ad0c0de9b60d9739f5583fe,
  tree 631e4d6dcf07b83d693be09c1338587e695aa92d, immediate parent cmake.
- PR 509 merge candidate: 16014a1d1413a915cef225046728046f0b380df1.
- Shared owner test SHA256:
  65d715b5e36c82935f96f39d22579496742cb55713f30061900edde089766cb4.
  qualified-source-02.json pins all seven changed native/test files. The six
  final flyout receipts identify the precommit base plus these exact source
  hashes. Subsequent ordinary/probe builds identify implementation commit def09a9e9.
- Frozen readiness assets: 9dad21bd65ba05665ac6fe315f37fe7461e85725, unchanged
  eight-case catalog. No HXM, Vertigo or camera-addon injection.
- Original Robotech HOG:
  adbfea019c1ac29ac922e15876c22ca6ce30eb02d598a98e6ef7dfdce2b2dd3e.
  Descriptor: 14150755f14e210b284aa03c45d75e1b2f96be7b567bbc324bf4986b96928a71.
- Shared new capture:
  d68ce6e430b7738b5f3d41f3170547055b3f9a55a4ace51acfcdca9590b7db12.
  Native D1 and fan inputs have independent source/staged hash receipts. The
  original fan archive remains unchanged. Neither is a substitute readiness input.
- Public XL reference source: 2576a98b5de310e3ac7c9777df2677d1627b8bae;
  consumed source adapters enforce their existing SHA pins.

Private evidence is retained under temp/xl-gameplay-20261115. Reports commit
identities and outcomes, not proprietary resource bytes or private trace payloads.

## Qualification results

| Owner                                             | Result and scope                                                                                             | Receipt under evidence root                                                      |
| ------------------------------------------------- | ------------------------------------------------------------------------------------------------------------ | -------------------------------------------------------------------------------- |
| Frozen ordinary readiness, baseline and candidate | FAIL 4/8 both times; four original levels tick 120 frames with XL inactive; same four load failures          | baseline-readiness-01; readiness-candidate-01                                    |
| Windows/Linux Debug D1 and D2 endlevel            | PASS six reloads/two sessions, visible star pixels, three real flyouts per engine                            | starfield-final-{windows,linux}-{d1,d2}                                          |
| Windows/Linux original fan controls               | PASS both cases on both platforms                                                                            | starfield-final-{windows,linux}-fan                                              |
| Native upstream contracts                         | PASS all four host/game combinations                                                                         | native-final-contracts-01                                                        |
| Software D1                                       | Production Debug target builds; shared test fixture does not compile because of older unguarded GL observers | software-native-build-01.log; software-build-01.log                              |
| Scoped format and catalogs                        | PASS; final formatting completed before exact-source builds                                                  | starfield-format-03.log; catalog-01.log; master-catalog-01.log                   |
| Pinned public XL references                       | PASS 61/61, including GPU and audio controls                                                                 | public-reference-01/public-status.json and public-ctest.xml                      |
| Linux Debug water owner                           | PASS 29.032 s; assertions, original 90 s capture/60 s replay limits                                          | water-debug-linux-01.receipt.json                                                |
| Windows Debug water owner                         | FAIL paired replay timeout at frame 1145/1235; capture completed; unchanged limits                           | water-debug-windows-01.receipt.json; water-debug-windows-timeout-diagnostic.json |
| Ordinary Windows clean replay                     | PASS; clean exit 13.672 s; complete object/world/SIM/FX/result equality                                      | replay-clean-windows-02                                                          |
| Ordinary Linux clean replay                       | PASS; clean exit 25.784 s; same complete comparisons                                                         | replay-clean-linux-01                                                            |
| Ordinary Android water runtime                    | PASS 59/59, 30791 ms scripted portion; exact protected-state restoration                                     | android-water-ordinary-01                                                        |
| Explicit Android developer replay                 | PASS 1235-frame result and all four detailed comparisons                                                     | android-water-developer-01; android-water-comparison-01.json                     |
| Restored ordinary Android guard                   | Expected native rejection; no actual replay result; exact restoration                                        | android-ordinary-guard-01                                                        |

The native flyout controls assist exit positioning, reactor state and presentation
dismissal. The water fixture uses developer arena controls, an experimental
water-physics profile and a canonical viewport. Ordinary Android water controls
also stage a pose before native thrust. Linux ordinary replay uses a real window
under Xvfb; it is not true-headless XL qualification. Android ran on an emulator,
not a physical device. Android D1 is compiled in the APK; this tranche's on-device
runtime owner exercises D2. No unassisted campaign completion or visual/audio
parity claim follows from these results.

## Binary receipts (SHA256)

| Artifact                                | SHA256                                                           |
| --------------------------------------- | ---------------------------------------------------------------- |
| Windows D1 Debug flyout                 | be2d672a518281b97199485e6d872a5c70c5e5a335cfbce530fa296d1e595e49 |
| Windows D2 Debug flyout/fan             | 6630116ea69af1e35436eee774ffb465d6a78ce4a614599a2495b0ba720b981a |
| Linux D1 Debug flyout                   | 916190fccea6ae3ac9881a4ac90984fee87024d774490cccd60a5dd0c24200ed |
| Linux D2 Debug flyout/fan               | 6ce376be68926027042f5ee38a5bc9c48adffc70eca2fae3f7be35360ae445b0 |
| Windows ordinary D2                     | e72ad5c445ebe3fcf0286caef150a2a6b8b83b90be2ac6801ed90651f07985d1 |
| Windows ordinary headless route         | ca4a0a1fd2371d794fd41cc564b3e91977991df51236cd1aef44361bf74532ec |
| Linux ordinary D2                       | 2a2d469bbc8f8b4b36d0d52c3e878d8be8271004c1d793fe9c429bcc45ee5f2d |
| Linux ordinary headless route           | 42d1b830a2692c63a54d13960cefe576e9ca136f46bd38b6b7e939cfb5307f11 |
| Windows Debug water probe               | d0a2a74011cb82f0bf513836d8d9dbeda3f0268f3a929f63e72e261e6565aaa8 |
| Linux Debug water probe                 | 908f28a61f8dca35bba122a00ed2c0990c4ac4300c9b7af47ab1e785de37c455 |
| Ordinary APK, all three ABIs/both games | d41857cb8d0d762eed54bab3444794c9ae80d8f967a9462de126855b7db878f5 |
| Developer probe APK, x86_64/both games  | c8ebaa62f7d2b83cd8e753a1d3c816075f91412830e2237f2da64ec2111cf92e |

The ordinary APK is frozen at apk-ordinary-01/app-debug.apk; its installed hash
is verified after restoration. The raw Gradle app-debug.apk remains the developer
artifact and must not be mistaken for the ordinary build. Each frozen APK receipt
hashes packaged libraries and records the selected CMake cache flags. All three
Android protected manifests hash to
2f1ef35b4d183307e257eeb3b1c351d7b2821011f28c9d80e6b82558023bed15.
The game is stopped and the fixed water backup directory is absent.

## Retained failures and open limits

All failed native starfield setup attempts and the genuine baseline reload
regression remain retained. The first candidate also exposed the existing
save-fixture pause error; that fixture is corrected without weakening its native
assertion. Software test compilation remains blocked by pre-existing GL calls.
The Windows Debug water timeout remains an open performance/qualification limit;
neither the Linux Debug pass nor release replay is substituted for that owner.
The first ordinary Windows harness attempt failed mission discovery because it
used the metadata-only extra-dir option. Its modal error and stopped-process
receipt are retained; the corrected run stages hash-verified original resources
under the ordinary executable's supported hogdir.

Readiness still lacks three genuine HXM definition dependencies and owned camera
semantics/addon coverage. G2 incoming ownership and corrupt-trailer energy 91 to
100 remain open. Sound stash 9783012e2a77dfdd4680812ae72b852cfaca5e17 is untouched,
unapplied and untested. Ordinary Android and true-headless XL timeline admission,
Android live capture-to-host, physical-device evidence and broader G2-G7 campaign
acceptance remain incomplete. There is no 90% playability claim.

## Candidate CI (qualification in progress)

All listed runs are attempt 1 for implementation head def09a9e9, with the exact
merge candidate above. As of 08:12 UTC, eligibility/request successes are not
native gameplay qualification:

- [Extended sample 43690324711](https://github.com/larsonm-personal/dxx-redux/actions/runs/43690324711):
  workflow success; execution owners skipped by the standalone extended-path
  eligibility filter. No extended gameplay pass is claimed.
- [Server 43690324759](https://github.com/larsonm-personal/dxx-redux/actions/runs/43690324759):
  eligibility success; server job skipped because there is no server change.
- [XL request 43690324787](https://github.com/larsonm-personal/dxx-redux/actions/runs/43690324787):
  signal success only.
- [Private XL controller 43692210110](https://github.com/larsonm-personal/dxx-redux/actions/runs/43692210110):
  queued, controller 5f8ab3535bec69b86a287f6516a5bddd6b81c383, requested candidate
  def09a9e98f209088ad0c0de9b60d9739f5583fe. Admission and owner results are pending.
- [XL contracts 43690324761](https://github.com/larsonm-personal/dxx-redux/actions/runs/43690324761),
  [scripts 43690324717](https://github.com/larsonm-personal/dxx-redux/actions/runs/43690324717),
  [engine 43690324782](https://github.com/larsonm-personal/dxx-redux/actions/runs/43690324782),
  [shareware 43690324751](https://github.com/larsonm-personal/dxx-redux/actions/runs/43690324751)
  and [LAN 43690324786](https://github.com/larsonm-personal/dxx-redux/actions/runs/43690324786): pending.

The qualification child will form the native published stack. Inspect its exact
tip and final-push CI before handoff; do not recycle old green runs. Source and
runtime bytes remain those tested above. Final candidate receipts will be retained
under this tranche's evidence root and reflected in the handoff update.

## Revised estimate

The procedural FX defect is repaired and its detailed Android developer replay
is qualified in this tranche. This does not reduce the external readiness
dependencies: no finite unconditional 8/8 estimate is supported with the frozen
inputs. The earlier two-to-four-tranche range remains conditional and low
confidence, not a delivery commitment. Broader G2-G7 completion cannot be inferred
from this slice. The next window must be explicitly invoked with new bounds.
