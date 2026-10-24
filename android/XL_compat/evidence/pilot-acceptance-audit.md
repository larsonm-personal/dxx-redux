# Ahayweh Gate acceptance audit

Acceptance: 2026-10-24. Original Ahayweh Gate is verified playable in the opt-in
singleplayer pilot with the required authored XL extensions. The 19-category
reconciliation below leaves no required extension unimplemented or unverified
for these exact level bytes and the selected legacy rendering profile. Exact
images and incidental classic gameplay retuning were excluded by the user

This qualifies the bounded pilot, not ordinary builds, the other 29 campaign
levels, multiplayer, or optional XL rendering modes. The production browser's
campaign-wide unsupported status remains correct. The Android x86_64 APK is
`android/app/build/outputs/apk/debug/app-debug.apk`, SHA-256
`9a2ed2e6d48c6d43e97d532cfb5b130778e24b12cda27529aa1d18daf90b3f19`,
installed on emulator-5554. Build with `-PxlPilot=true`; ordinary builds stay off

This replaces pre-extraction APK
`0c34652882800e4593f8b57d70034e4afd8748f5ddb89f3b21ab2913fe33e977`.
The unchanged XL draw implementation now lives in `runtime/corona_renderer.c`.
Post-extraction Windows opt-in/default-off and Android builds pass, as does the
host lifecycle test. Android rendering passes 28/28 in 13017 ms, run
`aef424e243014005bab3d88b4d5a28a4`, with 25 wall-visible samples, 10 actual
object-occluded witnesses and zero GL errors. Evidence is retained under
`temp/xl-boundary-coronas-*`; earlier launcher failures and system-wide ANRs
are retained too. A full emulator restart resolved those preflight failures.
The broader qualification below remains prior evidence; it was not all rerun
for this source relocation

## Evidence checked

The current [19-category requirement reconciliation](pilot-requirements.md)
separates authored pilot requirements from optional renderer modes and incidental
classic behavior. It includes the legacy object-light consumer chain

The earlier full private CTest run covered 23 tests, all passing in 129.64 seconds
(`temp/xl-qualification-suite.log`). This includes the pinned-source lighting
and dynamic-color oracles, inventory contracts, lifecycle/isolation, normal-input
key/reactor/escape objectives, all 12 non-exit triggers, stock-loader/route probes,
and inventory of all 30 archive levels. The archive test still requires every
level to report inspection-only, not playable

The current full host suite passes 36/36 in 162.27 seconds
(`temp/xl-pilot-current-full-suite.log`). A fresh Android rendering run
`eb9cd0d5073142d8b9f7b56757c8fdc1` fails step 16/28: the center halo is visible
before objects but not fully hidden afterward, with no GL errors. The prior
object-depth pass is therefore insufficient evidence of a repeatable diagnostic.
The corrected diagnostic samples the projected body rather than assuming that
the bounding-sphere center is opaque. It still requires visibility before the
real object pass, occlusion afterward, and visibility with depth disabled, in
the same framebuffer. No production renderer or world state changed. Two fresh
28/28 Android runs pass: `4bf6a9725f774329bd2169bd2c6761d0` (26252 ms) and
`84c203681f214ac9ae9a21e4a99c1c92` (25349 ms). They retain 25 wall-visible samples
and find 9/10 object-occluded witnesses, respectively, with zero GL errors.
Logs/results/witness coordinates use `temp/xl-object-silhouette-*`; the earlier
failed center-only run remains preserved

| Requirement | Current evidence and scope | Result |
| --- | --- | --- |
| Original bytes and bounded admission | Archive test pins original HOG hash; runtime admits exact level hash, mission identity, singleplayer and level 1 | Verified boundary |
| Inventory, bounds and unsupported records | Native parser/contracts, preserved independent survey, all 30 archive inventories; no pilot object triggers, authored warps or speed boosts | Verified for selected bytes |
| Faithful lighting calculation | Functions extracted from hash-pinned reference source; numerical comparison tests pass | Verified arithmetic, not all renderer modes |
| Authored vertex, side and texture semantics | Source audit distinguishes consumed vertex/texture values from side registration; live state tests cover all 112 eligible emitters and their switching/removal | Verified scoped semantics; see authored-color audit |
| Pixel output and occlusion | Two current 28/28 Android runs: color/partial depth, real wall depth, before/after object-pass silhouette samples, effective depth pipeline | Verified tested emulator path; controlled poses/halos, not every silhouette |
| Wall opacity and persistence | Native material/collision/targeting checks, normal save/restore and imported Android persistence run | Verified tested states; no claim of every portal physically flown |
| Main completion route | Native normal-input key, reactor and timed escape; Android 22-step completion from earned exit approach includes score and menu cleanup | Verified composed route; not a continuous Android start-to-exit recording |
| Every trigger and linked target | 12 branch tests plus native exit; activation, target passability and save/restore are asserted | Verified actions/state |
| Secret-area entry | Nine normal-input entry cases reach capture segments 4, 205, 206, 17, 81, 225, 14, 227, 199 alive, without lost lives or paused countdown; segment 199 follows the earned reactor fight | Verified individual host entries, not an uninterrupted all-secrets run |
| Every linked-target portal | Prior gameplay logs cover 10/17 target records; four explicit adjacent-crossing cases cover the remaining seven records (four physical portals) from earned branch saves | Verified undirected physical portal coverage; not both directions or every shared-trigger sequence |
| Import and stock isolation | Original ZIP import test; imported runtime save/restore; native pilot -> stock -> pilot definitions/state comparison | Verified tested import and transitions |
| Reference resource availability | New isolated ar12 run loads level 1; read-only runtime inspection confirms enabled coronas and loaded corona/glare bitmaps | Verified prerequisite, not image parity |
| Ambient sound regions | Startup sampling finds texture 269 paged out without transparency before later becoming transparent; opaque-boundary control reproduces the exact ar12 flags, differing only at 81/82 | Classified classic bitmap-paging artifact; retain Redux's PIG flags while paged out; no audible-playback claim |
| Other levels, coop and expanded XL features | Explicitly rejected or unqualified; pilot has no evidence for those features | Outside this pilot claim |

## Reference run with resources

The isolated `temp/xl-reference-complete` workspace copies the old ar12 runtime,
original mission/base data and profile, then adds distribution resources extracted
from the existing 1.18.77 installer as an archive. Each resource copy is checked
against its extracted source hash. The executable remains ar12, SHA-256
`77db2a39a2293b10554986eeb0c6a4b664621cfc9839bf5686ad9bd402505e18`;
resources are explicitly from the different distribution documented in
[pilot-authored-colors.md](pilot-authored-colors.md)

The copied profile enables `render.coronas.bUse=1`, retaining method 0 and color
level 2. Launch uses the game directory as working directory, with explicit
`-datadir`, `-userdir`, `-cachedir`, `-player xlref`, `-auto_hogfile d2-xl.hog`,
`-auto_mission d2leva-1.rl2`, windowed mode and no sound/movies. The initial
runtime-directory launch stayed at the menu because the auto-HOG path is relative;
its log is preserved as `runtime-cwd-attempt.log`. Corrected load evidence is
`level1-resource-backed-load.log`

The log's asset-loading messages alone are insufficient. The adjacent
`read-runtime.ps1` checks the exact executable hash and process image, opens only
read/query access, and reads narrowly scoped globals. COFF symbols and retained
disassembly establish the addresses and field offsets: `CAddonBitmap::Load`
uses availability at 0x4050, bitmap pointer at 0x4048 and name at 0x405c;
`CGlareRenderer::Style` reads enabled/style at active options offsets 0x238/0x23c.
`runtime-state.json` records enabled=1, style=1, and both named corona/glare
assets with availability=1 and non-null bitmap pointers. It does not change
process memory or prove a draw occurred. The owned process was stopped after
capture. The original baseline profile hash remains
`702102254d6905a89b96e41a2bcf89af3327480dc841922cd99e87ff0247e514`

## Coverage reconciliation and follow-up scope

The new entry suite closes the nine-secret physical-entry gap. All 25 selected
navigation/lifecycle tests pass in 63.03 seconds; per-target frames, shields,
countdown and native logs are retained in `temp/xl-secret-entry-summary.json`.
Segment 199 finishes with 86.87 shields and 27.48 seconds on the earned timer.
Failed attempts are preserved in the pilot plan, including an initially omitted
reactor threat and subsequent wreck collision. The correction changes only the
test controller's route and objective order, not engine rules or resources

Linked-target reconciliation is retained in `temp/xl-portal-crossing-final.json`,
with source logs for all 17 target records. Explicit crossings are 176 -> 182,
193 -> 194, 136 -> 137 and 5 -> 185. The first attempt at 136 -> 137 died while
the controller stopped in a narrow passage to engage a distant robot. Continuing
forward while engaging distant defenders passes with earned resources; no engine
damage or collision rules changed. Failed logs remain available

The resulting navigation/lifecycle suite passes 29/29 in 74.64 seconds
(`temp/xl-portal-regression.log`). This extends the earlier 25-case run with
the four explicit crossing tests

The ambient-lava discrepancy is explained by reference bitmap paging and is not
a missing authored extension. See the retained startup samples and causal native
control in `pilot-ambient-flags.json`. No production change copies this artifact

The requirement reconciliation closes the remaining legacy object-light and
feature-scope questions. Required colors, opacity, interaction, persistence,
resource closure and normal-input progression have direct scoped evidence.
There is no remaining required work for this pilot under the clarified objective

Future work may qualify physical Android devices, uninterrupted Android full-run
recordings, the other campaign levels, multiplayer or broader browser admission.
Those are separate claims. The current evidence does not establish an exhaustive
visual comparison, audible-playback parity, every optional graphics mode, or a
single uninterrupted all-secrets playthrough; none substitutes for a missing
authored extension, and none is asserted here
