# XL gameplay qualification, November 15, 2026

This explicitly invoked tranche started November 14 at 22:37:54 PST, after the
user merged the preceding stack. Its hard deadline is November 15 at 10:00 PST
(18:00 UTC); 06:00-10:00 PST remains reserved for validation, CI repair and handoff.
Work is sequential, without agents. This report is published while candidate CI
is completing; pending jobs are not passes. No merge, auto-merge or enqueue is
authorized or performed.

The final runtime implementation is PR 512 at e086949010e07773c2c2e531485b507698a537a2.
The terrain follow-up section records its qualification and the currently restored
ordinary APK. Earlier sections retain the separate starfield checkpoint evidence.

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

## Rendered terrain follow-up

[PR 512](https://github.com/larsonm-personal/dxx-redux/pull/512) extends
[native stack 511](https://github.com/larsonm-personal/dxx-redux/stack/511)
after immediate parent PR 510. Its implementation head is
e086949010e07773c2c2e531485b507698a537a2, tree
0f8e1be52d51d19679acd4c16cb9633ca58ea04f, initial merge candidate
a909462c431a9437defed29d58f453ee24ca5453. The preceding qualification head is
6800111f97f16d1617bee8bfebda691e2b7ee4f6, tree
4557a5a068159b152ba966a08b0cbf96e7c1ebce, merge candidate
383189ce0d0c4767f9fa97fbdf3af98c20a970fa. Historical merged stack 471 remains
linked; no stack is merged or queued by this tranche.

The additional ordinary Android flyout owner exposed two failures. In
android-flyout-01, the fixture requested Resume while graphics validation held
pause reason 8 alongside the overlay. The native guard correctly rejected it.
The existing paused-state wait now also requires pause.can_resume within its
unchanged 20-second limit; no guard or assertion was weakened.

That correction exposed a real native exterior crash in android-flyout-02.
The x86_64 backtrace identifies render_terrain at PC 0x94891e, terrain.c:217,
build ID fccc9e0abbbf7aeab6d8780303896d1704dd9102. The renderer used unclamped
camera grid coordinates as painter traversal limits. Off-grid writes could
overrun the 64-entry save_row buffer and corrupt the immediately adjacent
height_array pointer. Both games now bound only the traversal split to valid
grid vertices; camera position, terrain data and draw ordering remain intact.
The crashed owner reached its existing timeout and restored protected state.

All four Debug rendered owners pass three complete flyouts with four off-grid
camera quadrants each. They verify terrain pointers/dimensions and traversal of
the four mine tiles, restore the diagnostic camera position, then finish normal
campaign advancement. The additional camera poses are test assists. Evidence:

| Owner                 |     Time | Binary SHA256                                                    |
| --------------------- | -------: | ---------------------------------------------------------------- |
| terrain-windows-d1-01 |  5.906 s | 4bd5ccbd9d1e8cbcf2ff743f5f8e40219869818962770ab692c00093a5fbada6 |
| terrain-windows-d2-01 |  8.125 s | b7db2031fde0403a6010813adaa5d44b8d4cc51b5e7c395b1f5dc26ed70269eb |
| terrain-linux-d1-01   | 15.051 s | df8ecdef546e69ca222100aa97e1c4d84bae4636874aabc9678b5708dda653c1 |
| terrain-linux-d2-01   | 25.076 s | 40dc2b6d89b206707ef34984058dae34cce91f8d7a1c6e326592215883de642b |

Original fan-mission owners also pass: Windows 13.297 s, Linux 22.071 s, using
the corresponding D2 binaries. All four upstream contracts pass again in
terrain-native-contracts-01: Windows D1/D2 10.89/65.86 s and Linux D1/D2
2.29/23.71 s. Scoped formatting and both automation catalogs pass unchanged
counts. The new test-source SHA256 is
fac1d13dcc894e2ff668558ed992f21fb21929642de84c89fd29d7f1025a5d21.
qualified-terrain-source-01.json pins ten source files, and
terrain-commit-source-identity.json verifies that the pre-commit build sources
match the published implementation without changes.

The ordinary all-ABI/both-game APK is now frozen separately at
apk-ordinary-02/app-debug.apk, SHA256
504848a52babb1c5c286e1c013d52f436cf6d1ffeaddf725444d93a41172dbb5.
Its installed hash is verified. android-flyout-03 passes all 19 steps in
37.142 s: real rendered exterior, score advancement and cached return to level 2.
Its three Pumo before/after manifest pairs each match SHA256
9c3b5a0240e73c80ad477c2f0671301e5764912e37ea276beddef2569dcabb66.
The derived fixture adds scenery to original Pumo geometry and uses controlled
exit pose/player energy, presentation dismissal and a derived return trigger.
It does not establish unassisted campaign or frozen readiness qualification.
Both failed runs and the earlier APK remain retained.

readiness-terrain-01 again reports the same frozen 4/8 on the exact ordinary
headless route, binary SHA256
5440f93822b1a07db651c928b72d6ea6dd362cbf5e4782cbe6c3dbd1080c64c4.
The rebuilt ordinary Windows/Linux engines both match the retained capture's
1235 object frames, 1239 world records, 196 SIM events, 1808 FX events and exact
trailer in replay-terrain-windows-01 and replay-terrain-linux-01. This reuses the
original capture without rerolling or filtering it. A failed build invocation
using the output filename d2x-redux as a CMake target is retained; the actual
dxx-redux-d2 target builds successfully.

The ordinary replay binaries are Windows
23496c04af71bf3437f90cc95e82fe853574ace95b7abd44754efef09ba483c5
(14.266 s playback) and Linux
422a28bacc8625f3e840d7efabcacdf476f841782e7c4e006c68cef7349817ca
(19.160 s playback). Detailed comparisons finish separately after playback.
android-water-ordinary-02 passes 59/59 steps in 26.391 s on ordinary APK 504848a5.
The matching x86_64 developer APK, apk-developer-02/app-debug.apk, hashes to
10c99a60de601c40e0313fe7cc5c1d8970ee865e519c6d1946cb008232100219.
android-water-developer-02 matches the embedded 1235-frame result;
android-water-comparison-02.json independently confirms all 1235 object frames,
1239 world records, 196 SIM and 1808 FX events with no filtering.

The ordinary 504848a5 APK is restored and hash-verified. Its pre-commit version
code 30600 is lower than the source-equivalent developer build's 30610, so the
initial install -r refusal is retained and restoration uses install -r -d for
the debuggable APK, preserving app data. android-ordinary-guard-02 failed on an
ADB connection error before backup/test evidence; it is not a guard pass.
After connectivity recovered and absent backup/stopped game were verified,
android-ordinary-guard-03 records the actual native XL timeline rejection,
owner exit 1, no replay result and exact restoration. All three final water
manifest pairs match 2f1ef35b4d183307e257eeb3b1c351d7b2821011f28c9d80e6b82558023bed15.
The raw Gradle APK remains the developer artifact. The receipt verifier
verify-terrain-handoff-01.log passes source, six flyout owners, four upstream
contracts, full traces, APK hashes, readiness and state restoration checks.

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

## Candidate CI and final-push verification

The committed xl_gameplay_ci_20261115.json receipt records exact PR heads, bases,
merge candidates, workflow/controller heads, requested candidates, URLs, attempt
numbers and every returned job outcome. All initial runs are attempt 1. Its
observed_utc field defines the snapshot; pending entries are not passes.

All nine workflows for PR 509 and all nine for PR 510 completed successfully.
PR 509's engine artifacts record 95 D1 and 104 D2 CTest passes. The private
readiness artifacts for both heads reproduce 4/8 with the pinned cohort.
Important limits remain despite workflow success:

- PR 509 extended sample 43690324711 skipped execution by eligibility. PR 512's
  initial extended sample 43696337139 likewise skipped expensive coverage with
  the native non-tip/unrelated-changes eligibility message before stack linking.
- Server execution is skipped without a server change. LAN timing export and
  retained-online inspection are skipped; the two-player LAN owner executes.
  Separate native-save/replay and headed-parity jobs can be skipped by their
  replay eligibility; the public build/save-integrity and admission owners pass.
- Private controllers 43692210110 and 43694256733 use controller
  5f8ab3535bec69b86a287f6516a5bddd6b81c383 and candidates def09a9e9 and 6800111f9
  respectively. Their host-profile:retail-xl-fast owners report PASS but
  coverage_complete=false, zero counted leaf cases and aggregate incomplete.
  Android, pinned-Pumo staging and nightly Release compile are skipped by
  selection. No full private gameplay acceptance is claimed.
- PR 510 extended sample 43694244952 executes Linux, JVM and Android jobs,
  reports 159 passing leaves and four fully covered owners, but aggregate
  incomplete with seven incomplete owners. Extractor coverage excludes one
  explicitly skipped registration and three missing real-media fixture cases.
  Gradle coverage lacks optional Mac/GOG, Anniversary, Pumo, Vertigo, RAR,
  replacement-bank/static-texture/distribution and real-server fixtures. Other
  incomplete aggregate owners lack complete counted leaf evidence. Exact names,
  reasons and exclusions are preserved in the structured receipt.
- PR 512's implementation-head scripts, XL contracts, LAN, server and request
  workflows have completed successfully at this snapshot. Engine, public
  shareware and private controller 43696348519 remain in progress. The request
  workflow is only a signal. Controller 5f8ab3535bec69b86a287f6516a5bddd6b81c383
  requests e086949010e07773c2c2e531485b507698a537a2.

The handoff documentation push creates a new candidate identity. Inspect that
push separately and retain final-handoff-receipt.json under the evidence root;
PR 512's handoff body records its exact final head/merge/controller/candidate,
workflow URLs, attempts, outcomes and skips. This committed snapshot does not
claim that subsequent jobs passed, and old green runs are not substitutes.

## Revised estimate

The procedural FX defect is repaired and its detailed Android developer replay
is qualified in this tranche. The off-grid terrain crash is also repaired and
qualified by host quadrant renders and the ordinary Android flyout/cache owner.
These fixes do not reduce the external readiness
dependencies: no finite unconditional 8/8 estimate is supported with the frozen
inputs. The earlier two-to-four-tranche range remains conditional and low
confidence, not a delivery commitment. Broader G2-G7 completion cannot be inferred
from this slice. The next window must be explicitly invoked with new bounds.
