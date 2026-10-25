# XL package support implementation plan

Source study: origin/xl-package-layout-study at d2a284497. The study and its
evidence manifest are copied unchanged alongside this plan. Implementation is
on xl_more; unrelated test-runner/browser changes from the study branch are not
merged. This goal replaces the previous broad feature-work priority, without
discarding its evidence or weakening its admission gates.

## Selected mission and qualification boundary

Target Dragon Pit Outpost, the original levelpack distribution, descriptor
missions/spotlight/drgpit.msn. It is one D1-format level, not a D2-only test.
Its package adds mods/drgpit/sounds/boss01.wav and boss02.wav. Both inspected
files are uncompressed stereo, 16-bit, 44100 Hz; 135128 and 120578 frames.
This is a bounded scope for mission-owned WAV resolution and sample conversion.
Name mapping, actual boss playback and natural death-WAV completion are now
verified in the controlled tests below; full mission qualification is separate.

Current qualification status (2026-10-25): original solid-7z extraction and scoped
WAV playback are exercised on Android, including save/restore and interrupted
playback. Classic LGT/CLR lighting calculations match the explicitly initialized
reference profile, and Android lifecycle tests pass. Ordinary controls have
verified the first door and one complete opening-route pass to segment22 with
four credited kills and84.7 shields remaining. Subsequent runs died near16.
A later staged first engagement now reaches160 through the first trigger-controlled
door, with seven kills and8.5 shields remaining; repeatable survival is unverified.
A controller-rate correction subsequently reached166 alive with eight kills and
27.9 shields in one84-step pass. Repeatability and full completion remain open.
A current-controller retry of the earlier simpler44-step route died in15;
removing the laser detour did not reproduce the earlier opening success.
The driver now handles zero-shield robots correctly and predicts constant-speed
laser interception; its survival benefit is not established. The authored
laser-upgrade detour13->311->588 has now been traversed with ordinary controls,
and the upgrade was collected. Actual muzzle diagnostics rule out blocked projectile creation
in the measured run; downstream hits and repeatable survival remain unresolved.
The current progression work is a gameplay/navigation investigation, not evidence
of a diagnosed engine defect. Native still-view comparisons now isolate the
reference's different projection and warped-face fan triangulation; five static
regions nearly match under those explicit diagnostic controls. Normal rendering
retains compiled face splits. Broader scene comparison, additional matched Android views,
normal-input full mission completion and separate native-D1 parity remain outstanding. Browser
support must stay gated; the opt-in imported-D1 path is still experimental.

The HOG contains drgpit.rdl (version 1, 106933 bytes), drgpit.lgt (3640 bytes)
and drgpit.clr (11830 bytes). The lighting companions must also be audited and
supported or shown to be semantically neutral. They must not be classified as
HAM data or silently ignored in a complete-support claim.

Existing checked-in metadata reports 632 segments, 134 walls, 11 triggers,
101 objects, 43 robots, no reactor, and partial routing: red key unreachable.
This is not proof of runtime incompatibility or completion. Inspect key
containment, trigger prerequisites, boss exit and original normal controls before
claiming that package additions are the remaining blockers. If another engine
feature is required, document and implement it or explicitly revisit pilot choice;
do not substitute a synthetic level or call a successful load mission completion.

Evidence hashes (original extracted local files):

- drgpit.msn: 44c5d33468e59b06c20ea15715928f1f3bba6351497b66f8db7de20fdf0f8df3
- drgpit.hog: a8eebb7e83faa55e143a48ec87c82407d8ffcaef3ad6b8f9329b84a6a42b262e
- boss01.wav: f448d3506def983bd2ba3e9d79a24386930ff7aeba956f2eaed666b443201480
- boss02.wav: 91fa0e3fb5ff3187929e6511ebfebff002b8ef47c3225d5b99be42fd67571f3d

Boiling Point remains a texture-resolution witness, not the playable pilot:
its seven-level d2x-name mission declares custom robots and textures and has
independent gameplay blockers. Enemy Within has 32 listed levels, so its cockpit
TGA files are useful case-sensitivity witnesses without being a compact campaign
qualification target. Neither choice should be portrayed as fixed by a decoder.

## Implementation sequence

1. Audit Dragon Pit's route and resource references. Verify original archive hash
   against the study manifest, D1 sound-bank names, boss sound use, LGT/CLR values
   and base comparisons. Obtain a normal-input completion baseline or identify
   concrete missing semantics. Preserve source bytes and distinguish source audit,
   metadata analysis, controlled setup, rendered execution and audible checks.
2. Extend the existing mission context with resolved mod root and signed level
   ordinal. Descriptor identity, selected HOG and resource root stay separate.
   Reject ambiguous wrapper/alias ownership. Do not flatten siblings into a global
   directory. Keep current classic/global-mod precedence outside scoped contexts.
3. Implement bounded mission WAV effects using the current sound replacement
   lifecycle and mixer. For the pilot, use canonical D1 bank names and explicit
   stereo-to-mono/rate conversion. Test malformed RIFF chunks, decoded-size limits,
   missing optional effects, bank-present suppression and source provenance.
   Mission-owned authored effects take priority over global cosmetic replacements
   inside the qualified scope; bank gating is applied first. Record fallback.
4. Implement the pilot's non-neutral lighting companions, if present, through
   XL_compat with minimal engine hooks. Source reference loads LGT as texture
   brightness and CLR as texture colors. Validate lengths, indices, finite/range
   constraints, D1 texture mapping and renderer use independently of level version.
5. Verify original mission gameplay and package behavior on Android: actual boss
   effect events, mixer output, lighting, earned keys/boss/exit, save/restore and
   A -> B -> stock -> A isolation. Use the preserved fixture pattern and only the
   designated emulator. Failed activation and resource revision changes must not
   leave stale decoded samples or resources. D1-on-D2 and native D1 are separate
   qualification targets; report whichever was actually tested.
6. Follow with scoped static TGA lookup using Boiling Point resources and synthetic
   A/B contexts. Then signed level/secret precedence, exact-frame before #0
   fallback, POG ordering, transparency/animation, music, model skins and finally
   individually audited configuration directives. OOF and PMH/PTI are distinct
   format projects; no extension renaming or arbitrary INI execution.

## Metadata browser

The native category source now distinguishes package identity, scoped textures,
WAV effects/banks, playlist/ordinal music, models/skins, LGT/CLR companions and
configuration/presentation. Existing category indices remain stable. Single-level
inspection cannot establish package contents, so these rows remain unknown;
regression checks enforce that boundary. No new playback or admission claim.

Next feed actual catalog resources into that same structured report, rather than
adding a Kotlin support table. Track detected, preserved, resolved, decoded and
qualified evidence separately, including owner, original path, hash, selected
candidate, scope, reason and build. Null means uninspected, not absent. Duplicate
files are not distinct successfully resolved resources. Resource fidelity and
gameplay admission remain separate. Browser detail views must not decode or run
package configuration merely to show support status.

Pre-integration audit (2026-10-25): category definitions exist, but the
external package inventory is not wired into these rows. `compatibility_json`
accepts only a source name and one byte buffer. `GameFileMetadata` follows a
descriptor's sibling HOG and inspects embedded lighting companions; it does not
provide the selected mission's external resource catalog to the native report.
`inspectXlCompatibilityTarget` visits HOG/descriptor/level paths, not the owned
external resource tree. Consequently Dragon Pit's verified external boss WAVs
still have no mission-specific browser evidence. This is remaining product work,
not merely an additional gameplay qualification test.

Implement the next browser slice in this order:

1. Separate informational package reports from level-analysis blockers.
   Implemented the `blocks_analysis` contract on2026-10-25: native detected
   reports explicitly retain their restriction; only Boolean false is
   informational. Missing/malformed values retain the restriction. Both the
   analysis entry point and whole-mission UI use the same blocker filter, while
   the display still includes all reports and retains their support statuses.
   The extracted-catalog producer described below now uses the informational path.
2. Feed an immutable owned-resource catalog through the native category source.
   Use the launch-publication ownership model and selected HOG identity, not the
   archive filename, mission title, or an unbounded scan of sibling missions.
   Record original path, owner, byte size and existing content identity. A wrapper
   ambiguity or incomplete catalog must yield unknown rather than zero resources.
3. Resolve WAV candidates with the same `resolve_scoped_sound` precedence used
   at runtime: rate-specific mission bank first, signed ordinal override next,
   then mission fallback. Report candidates and selected paths separately. A
   found unsupported bank must not be described as successful WAV fallback.
   Inspection must not mount resources, play audio or execute configuration.
4. Keep catalog detection, preservation, selection, decoding and qualification
   distinct. Runtime test evidence does not automatically qualify every file of
   the same extension. Report the original Dragon Pit boss WAVs as detected/owned
   when catalog evidence establishes that, while retaining its unqualified
   mission status and the independent LGT/CLR requirements.
5. Test original-package path evidence plus two missions sharing sample names,
   normal/secret ordinal precedence, bank suppression, ambiguous owners, missing
   catalog evidence and classic metadata-analysis availability. Verify actual
   browser rendering after native/JVM checks; no synthetic-only completion claim.

This browser work and remaining gameplay/fidelity qualification are independent
tracks. Further combat-driver experiments must not postpone the missing browser
integration indefinitely or be counted as package-feature implementation.

Validation for the report/analysis separation: scoped C++/Kotlin formatting
passed, all seven XlCompatibilityTest cases passed (zero skipped/failures), and
the Android D1/D2 pilot APK build passed in2m6s. Evidence is retained under
temp/xl-package-report-gate-20261025. Cases include informational blocked-status
reports, mixed informational/level reports, eligible-but-analysis-unsupported
levels, and missing/malformed flag values. This verifies model policy and app
compilation, not external-catalog wiring or rendered browser behavior.
Installed this successful APK on emulator-5554, replacing the withdrawn
close-target experiment. No gameplay rerun or browser-rendering claim is attached
to this installation.

### Extracted owned-resource reports connected (2026-10-25)

`MissionZipExtractionStore.extractedTarget` now constructs its package evidence
from the reusable verified extraction record, `missionLaunchPackage`, and
`MissionLaunchCatalog.resourcesFor`. This reuses existing owner filtering and
case-duplicate/conflict policy. The snapshot is attached to LevelMetadataTarget
and included by inspectXlCompatibilityTarget without enabling analysis or runtime
features. JNI passes the owned paths/hashes to the native category source; Kotlin
does not introduce a separate extension-support table.

The native report lists catalog candidate counts and original virtual paths with
SHA-256 evidence for identity, textures, sound folders/banks, music, models, loose
LGT/CLR and configuration/presentation. All rows remain unknown. Counts describe
candidate paths, not successfully selected or decoded resources. Embedded members
are outside this catalog; zero therefore cannot mean feature absence. Reports
are informational (`blocks_analysis:false`) and explicitly not playable.
Catalog construction/JNI failures leave package inspection unavailable rather
than synthesizing absence or support. No filesystem mounts or configuration
execution are performed for reporting.

Validation in temp/xl-owned-catalog-20261025: scoped formatting passed; native
MSVC xl_inventory_contract passed (0.59s total); seven compatibility-model tests,
four mission-catalog tests and eleven extraction-store tests passed, all without
failures or skips. Android D1/D2 pilot APK build passed in2m8s. Native cases check
WAV plus bank candidate evidence, hashes, unknown embedded lighting and no
eligibility promotion. These tests do not establish JNI/browser rendering with
the original archive; verify that on emulator-5554 next.

Remaining package-browser work: active-HOG/signed-ordinal resolution using the shared resolver, and evidence for
unextracted archive/loose-file targets. The current inventory does not select
WAV winners or claim that catalogued options are implemented. Full gameplay and
rendering qualification remains independently open.

### Original-package browser witness and dialog regression (2026-10-25)

Added an optional `-DragonPackageArchive` argument to the existing
android/tests/test_xl_feature_browser.ps1 runner. This uses an already imported
original archive; it neither downloads nor extracts a replacement. Its device
check hashes the entire archive against the audited original SHA-256 above, obtains a reusable
extraction record and the actual Dragon Pit metadata target, calls JNI, and
asserts exactly two sound candidates with the original boss01/boss02 paths and
SHA-256 values. All package rows remain unknown/informational. Embedded LGT/CLR
reports still block qualification and remain analysis blockers.

The original fixture exposed oversized evidence cells. The table now bounds
support/evidence previews to three lines with an ellipsis; the explanation
dialog exposes the full requirement and evidence. No provenance is discarded.
The UI test also exposed asynchronous accessibility transitions: scrolling now
uses advertised vertical actions with bounded retries, and explanation/Back
checks wait for unique dialog labels before observing text or comparing bounds.
The existing exact scroll-position equality assertion was retained.

Final original-package run passed on emulator-5554 (detail-run.log). Its captured
xl-feature-dragon-sounds.png was visually inspected: the explanation shows the
two-candidate requirement, unqualified sound support and original boss01 path.
Both full hashes are checked in the report assertion/evidence file; a single
screen does not show every line of the scrollable evidence. An earlier capture
showed only the table and was explicitly rejected as explanation-view evidence.
The existing light/dark dialog regression then passed (baseline-back.log),
including status colors, source handling, explanations and exact scroll restore.
Each restoring wrapper verified app state byte-for-byte and stopped the app.

Artifacts: temp/xl-original-package-browser-20261025, including failed attempts,
the successful original evidence/capture, baseline results and build logs.
Scoped formatting and app/instrumentation builds passed; git diff --check passed.
The optional fixture uses the existing registered test runner and its90-second
instrumentation limit. Reproduce with the unchanged imported archive's absolute
device path, for example:

```powershell
.\android\tests\test_xl_feature_browser.ps1 -Serial emulator-5554 `
  -DragonPackageArchive /data/user/0/com.dxxredux.app/files/imported/sets/default/.content/mods/XL_Dragon_package_investigation
```

This verifies the real extracted-target/report pipeline and production dialog
with original content. The test mounts the dialog in the launcher test activity;
it does not exercise every entry-screen navigation path. It does not prove
runtime selection, audio/render parity, full mission completion, unextracted
archive reporting or general package qualification.

### Actual scoped-sound selection evidence (2026-10-25)

Source audit confirms android_mod_sound_level_prepare derives identity from
mission_resource_archive, the actual mounted Current_mission_hog. It also binds
the incoming signed ordinal, mission key and generation. A browser descriptor
stem alone is therefore insufficient evidence of active selection.

Extended the existing logging-gated mod_usage snapshot with scoped_sound_context
and per-sound scoped_selection. Successful WAV installation records sample name,
resolver scope/path, mounted archive, identity, signed ordinal, key/generation
and bank-selection rate. Reloading an ordinary sound clears its selection record;
sound reset clears the whole overlay record. Context currentness compares the
actual current level, active archive, mission key and generation. This records
existing behavior; no resolution, conversion, playback or mission-admission rule
was changed. Logging disabled still suppresses this diagnostic evidence.

Scoped formatting and Android D1/D2 build passed (1m37s). On emulator-5554,
test_xl_classic_sound_lifecycle passed42/42 in92852ms, run
93eb239c8d84413392062bda400243d0. After restore both samples80/82 were resident,
selected at mission scope from mods/drgpit/sounds/boss01.wav and boss02.wav,
bound to the current mounted drgpit.hog, ordinal1 and generation3. Bank lookup
used22050 while installed WAV playback remained44100. The natural-completion
and interrupted-playback assertions also passed. Per-overlay played=false after
restore is not a contradiction: that field resets with installation, while the
sound_playback counters used by the test retain the measured playback events.

test_xl_classic_lighting_lifecycle passed35/35 in99588ms, run
47970fd0a7994c4491f09385bdda4b0c. Added assertions observed current=true in Dragon
Pit, current=false in stock Counterstrike, then current=true/identity drgpit/
ordinal1 after restoring the mission (generation5). Both wrappers restored app
state byte-for-byte and stopped the app. Compact records and logs are under
temp/xl-sound-selection-20261025; full test artifacts remain in their registered
Android test output directories.

These are actual runtime selection/lifecycle receipts, distinct from the browser's
owned-candidate inventory. They are not automatically merged into browser support
status: joining saved runtime evidence requires matching resource revision and
scope, not just matching filenames. Secret-ordinal runtime evidence, normal boss
combat, native-D1 parity and full mission completion remain separate open gates.

### Opt-in stand-off flight exercised; spur retreat fails progression (2026-10-25)

Added debug fly_to `combat_standoff`, default0 (unchanged existing behavior),
bounded0..60 and requiring combat=true when positive. For a visible live robot
inside the requested separation, the controller removes its waypoint-directed
approach component and requests retreat through ordinary mapped joystick axes.
It aims at that nearby robot. Actual position/segment arrival checks happen
before this steering adjustment; no physics, damage, inventory or waypoint
acceptance rule changes. Diagnostic stand_off samples record robot and distance.

The133-step continuous-arrival Vulcan probe requested40 units, retaining every
other step,15-second flight timeout, Hotshot difficulty and final Vulcan/health
assertion. Scoped formatting and Android D1/D2 build passed (1m14s). Run
59c3a4339db64ee4b19b890c02889cb2 failed47/133 after100385ms: return from the laser
spur timed out, ending back in588 with2.66379 shields, three credited kills,
laser1, energy89.5, two missiles and invulnerablefalse. State restored byte-for-byte.
Evidence: temp/xl-standoff-vulcan-20261025/combat-evidence.json and raw steps/logs.

Seventy-two stand_off samples exercise the option, first at robot70 distance39.665.
Final robot65 samples remain near40 units with aim cosine above0.999, firing
requested and target shields decreasing22.090->11.090. No robot-contact collision
events are logged, but repeated wall collisions occur in311 and588. These are
diagnostic event counts, not total collisions or proof of damage causality.
The route and survival margin are unacceptable despite the observed separation.
Do not call this improved mission compatibility or a passing progression run.

Retain the option as an explicitly requested experimental driver tactic only.
Next investigate engaging before entering the constrained upgrade spur, rather
than assuming backward retreat always has clearance. The40-unit trial is failed;
any changed distance/timing requires a new run and must not reinterpret its
timeout as success. Full normal-input completion and broader fidelity remain open.

### Earlier stand-off fails the short waypoint budget (2026-10-25)

Ran the same133-step route with only combat_standoff40->60 changed. Installed
and local APK SHA-256 matched
4da2214deb8493bb6dc62a4aef289d9eea900a36c61acafe27d0af556a168f21.
Runb71ead6ff24c4dc493fff2ad5c06a607 failed34/133 at89126ms, timing out on the
approach to13. It ended in0 with95.62535 shields, one credited kill, laser0,
energy97.5, nine missiles and invulnerablefalse. State restored byte-for-byte.
Evidence: temp/xl-early-standoff-20261025/trial.json and combat-evidence.json.

The66 stand_off samples start earlier, step34 at robot65 distance54.5582;
however the final robot64 distance is8.1857, so the requested60-unit separation
was not maintained. Repeated wall collisions and17 logged contacts with robot65
show retreat/navigation constraints. Robot64 remains80 shields at the final
samples with aim cosine0.958..0.966 and firing off. Do not infer a successful
combat tactic from the higher remaining health or call the route passable yet.

A separate follow-up was declared before running: retain60-unit separation and
all gameplay/route assertions, increase only each fly_to timeout15s->60s, retain
the600s overall bound. It tests whether the short navigation deadline truncates
an otherwise viable combat attempt; it does not retroactively change the failed
trial's acceptance. See temp/xl-standoff-combat-window-20261025/trial.json.

The follow-up run341638d031594b5981138821f9b5ba4e failed49/133 at107498ms,
dying in14 with shields-0.53340, three kills, laser1, energy87 and two missiles.
No sampled flight span exceeded15s, so this run did not exercise the extended
budget; the changed encounter timing cannot be attributed to that budget.
Its first stand_off event was later (step45, robot75 at55.9341), illustrating
the variability of these non-paired trials. Final robot74 range fell to10.5928;
wall collisions and35 logged contacts with64 again show failure to maintain
separation. State restoration passed byte-for-byte.

With no demonstrated progression benefit across these trials, removed the
distance-only stand-off experiment from the working source. Preserved the tested
game_automate.cpp beside the final trial artifacts, together with the APK hash
record and all failed results. The currently installed APK still contains the
opt-in experiment until rebuilt/reinstalled; default flight behavior is unchanged.
Next progression work needs route-aware engagement/retreat through actual open
space. Do not continue tuning a distance constant as though it solved navigation,
and do not relax mission-completion or resource-fidelity gates.

## Completion gates

Completion requires the selected original mission to work with its authored
package resources and normal progression, supported by recorded runtime evidence.
Category labels, source review, successful import, static routes, decoder tests
or a loaded level alone do not satisfy that gate. Track missing requirements and
failures explicitly. No mission-name/hash allowlist in runtime admission.

## Initial validation

The local original levelpack.7z SHA256 matches the study manifest:
b4b519dc5755b795fd49cc3786a0421b96cd099dcc8f002925ddfd7c038b2719.
The native metadata build and xl_inventory_contract pass (1.88s), including all
seven new package rows remaining unknown for level-only input. Scoped formatting
and git diff --check pass. This turn has not built an Android APK, exercised the
new rows in the actual dialog, decoded samples through the game mixer, or played
Dragon Pit. Existing metadata is historical route-analysis evidence only.

## Native pilot audit checkpoint

Added a reusable `-package-audit <json>` mode to the private native lifecycle
runner. It uses the real mission loader and StartNewGame, exports startup objects,
wall/trigger references, segment adjacency and texture references, and distinguishes
native D1 assets from D2 fallback. It does not advance gameplay or certify visuals.

Original Dragon Pit loads with native D1 assets. The red key is a placed powerup
in segment 350 (object 24), not a robot-contained pickup. The boss is robot 17,
object 93, segment 493. Ordinary robot see/attack sounds are robot01/robot02;
the separate D1 boss AI calls SOUND_BOSS_SHARE_SEE and SOUND_BOSS_SHARE_DIE.
Native translation maps those to samples 80/82, and the active sound registry
maps boss01/boss02 to exactly 80/82. This verifies the external WAV name mapping,
not actual external-sample playback. Source: d1_in_d2_ai.c boss loop/death paths.

The historical Guide-Bot run stopped after blue/gold with no actionable goal;
it did not prove player death or an impossible mission. An independent optimistic
adjacency check over the native dump, honoring key locks and opening both sides
of reachable open-door trigger targets, reaches trigger 10 at segment 630 after
blue/gold progression. Trigger 10 targets eight locked doors around segments
338-345, exposing the red key; the red key then exposes the boss region. Reachable
open-door trigger groups are 0/1, 2/3, 8, then 10. Generator triggers are excluded
from door opening. This check ignores timing, collision clearance and combat and
is only a dependency hypothesis for a normal-input run, not completion evidence.

LGT contains 910 signed brightness entries, 69 nonzero, range 0..131072. CLR
contains 910 records of one index byte and three signed int32 color channels;
103 are chromatic. The pinned XL ReadColor(..., false, false) divides channels
by 0x7fffffff; these are not IEEE float values. Native D1 texture-index mapping,
used-texture intersection and comparison against XL defaults remain necessary
before deciding which entries affect this mission. Do not label them neutral
merely because many records are zero or resemble a default editor table.

Evidence: temp/xl-package-dragon/startup.json and startup.log. Reproduce with:

```powershell
& temp/xl-runtime-engine/main/xl-preview-lifecycle.exe `
  -hogdir temp/xl-reference-ar12/game/data `
  -extra-dir temp/xl-package-dragon `
  -userdir temp/xl-package-dragon/preview-user `
  -mission drgpit -package-audit temp/xl-package-dragon/startup.json
```

The private fixture contains only copies of the original descriptor/HOG under
missions; external resources have not been installed or claimed active. Native
build and startup audit passed. This narrows the next implementation to scoped
WAV resolution/conversion, with lighting and normal progression still mandatory.

## Bounded PCM WAV decoder

Added XL_compat/wave.cpp and wave.h as a reusable native decoder. The accepted
profile is little-endian PCM RIFF/WAVE, mono/stereo, 8/16 bit, 8000..192000 Hz.
Input and decoded PCM each have a 16 MiB bound; chunk count is limited to 1024.
RIFF length, chunk headers/payloads/padding, unique fmt/data, PCM extensions,
frame alignment and byte rate are validated before samples are published.
Unknown chunks are skipped within bounds; data-before-format is accepted.
Float, compressed and extensible WAV formats are explicitly rejected.

Output is native-endian signed 16-bit mono at the original rate and frame count.
Unsigned 8-bit PCM is unbiased; stereo is averaged in a wider integer with
rounding toward zero. No implicit resampling or sample normalization. Runtime
integration must honor this representation in the mixer and explicitly convert
for a backend requiring another format/rate; it must not reuse the raw-8-bit path
with incorrect metadata.

The synthetic contract tests signed extrema, silence, stereo cancellation,
unsigned bias, odd chunk padding, data-before-format, every truncated prefix
including repaired RIFF lengths, duplicate format/data, excessive chunk count,
invalid codec/channel/bit/alignment/rate/frame data and allocation bounds.
An optional private CTest pins both original Dragon Pit WAV SHA256 values and
checks source format/duration and the complete converted PCM against independent
Python wave/struct golden hashes. Output frames: 135128 and 120578; FNV-1a over
little-endian mono samples: 6c6729efe4dd2fa6 and 3c272da347f23ae0.

This is a decoded-evidence milestone only. The decoder is not yet wired into
Android lookup or playback, and metadata support remains unknown/unqualified.
Next connect it to mission-owned resolution with bank gating, then verify actual
boss-loop/death playback, timing, teardown and save/mission transitions.

Final native build, scoped formatting and all three focused tests pass
(xl_wave_contract, xl_dragon_wave_decode, xl_inventory_contract; 2.76s total).
Private originals are enabled with XL_DRAGON_SOUNDS pointing to the audited
mods/drgpit/sounds directory. No proprietary samples are committed.

## Scoped selection and ownership verification

Added `scoped_sound.cpp` to select the rate-appropriate mission bank first,
then signed-ordinal level/secret-level WAV, then mission WAV. A bank result
means presence only: it must suppress the WAV path but does not establish that
the bank is decoded or supported. Identity and sample components reject path
traversal and separators. The caller must supply the resolved mod identity;
neither the package ZIP name nor display title is suitable.

`android_mission_assets_find_owned_file` resolves only regular files whose
PhysFS origin belongs to the active selected context. It rejects unsafe paths,
global fallback, and lookups during a transition. Case correction is validated
by opening the selected bytes, not by requiring canonical capitalization on a
case-insensitive filesystem. A foreign file shadowing an owned path currently
produces no owned match; runtime admission must not mistake that for proof that
the package has no resource requirement.

The real catalog/mount integration test now exercises mission, level and secret
selection, bank gating, archive-member ownership, and disappearance of the
previous mission's sound after switching contexts. Synthetic resolver tests
also distinguish s11/s22 selection at the 22050 Hz boundary.

Verified October 24: native runtime build completed; all four focused CTests
passed (`xl_wave_contract`, `xl_dragon_wave_decode`, `xl_inventory_contract`,
`xl_observatory_descriptor`). The descriptor test invokes the real catalog/mount
test, not only the simulated resolver. Android x86_64 opt-in XL debug assembly
also passed. Evidence: `temp/xl-scoped-sound-native-build.log`,
`temp/xl-stack-review/Testing/Temporary/LastTest.log`, and
`temp/xl-scoped-sound-android-build.log`. This is build and selection evidence,
not Android playback evidence.

### Runtime integration constraints confirmed from source

- `d2/main/gameseq.c::LoadLevel` invokes `d1_in_d2_prepare_level_assets` before
  assigning `Current_level_num`. Pass the incoming signed ordinal explicitly
  into sound preparation; reading the current global during asset loading can
  select the previous level's folder. Include secret levels, reloads and restore
  in the lifecycle checks.
- Pinned XL `gameio/loadgame.cpp` calls `MakeModFolders` with the active HOG
  identity; `main/gamefolders.cpp` splits that identity to obtain the mod stem.
  Establish and retain descriptor, selected HOG and mod identities separately
  before activating scoped sounds. A descriptor stem alone is insufficient for
  aliases and explicit HOG selection.
- `android_mod_assets.cpp` currently installs unsigned 8-bit raw/r22 overlays.
  The new WAV decoder returns signed 16-bit mono; integration must preserve its
  rate/format for SDL_mixer or explicitly convert for the fixed-rate backend.
  Apply authored duration overrides as well as playback bytes, and preserve
  the existing original-buffer restoration contract.

At the selection milestone, no production caller connected the scoped resolver and WAV decoder to
`android_mod_sound_apply`. Dragon Pit remains unqualified. Next implement that
connection with explicit identity/ordinal and observable selection/rejection
reasons, then verify actual boss-loop/death playback and teardown. Lighting and
normal-input mission completion remain required; these passing tests do not
remove either gate.

## Initial runtime connection (not yet playback-qualified)

The opt-in Android D2 runtime now connects scoped WAV selection to imported D1
sample installation. `mission_resource_archive` exposes the actually mounted
mission archive; the mod stem is derived from its filename. `LoadLevel` supplies
the incoming signed ordinal before D1 asset preparation. The selection context
also records the immutable package key, activation generation and archive path;
a mismatch disables use of the stale context. Unpackaged/global missions do not
enter this path. This initial integration does not yet implement native D1 or
D2-content scoped playback.

For an owned match, the decoder installs explicit unsigned 8-bit source PCM
because Redux's existing mixer conversion accepts that format. Quantization is
`(signed_sample + 32768) / 256`; SDL_mixer retains the authored source rate, and
the legacy backend uses explicit nearest-frame conversion to its fixed rate.
The shared bounded conversion uses 64-bit frame arithmetic on 32-bit hosts.
Authored duration uses the original frame count/rate. The existing overlay
storage and reset path owns the installed bytes, and existing mod-use evidence
records their path, hash, rate and played flag. An XL log also records selected
scope and incoming ordinal. The imported D1 registry is rebuilt per level,
including stopping sounds and clearing mixer caches before replacement.

An owned scoped bank currently produces a helpful unsupported-bank error;
malformed or unreadable selected WAVs also reject instead of silently falling
back. Level-custom D1 bank samples retain their existing precedence. Full
package admission and explicit diagnostics for shadowed resources remain open.

Verification: native x86 runtime build and Android x86_64 XL debug assembly
passed. Three focused CTests passed in 7.96s, including the real mission mount
test. PCM conversion checks cover signed extrema, silence, downsampling and
upsampling; the original Dragon Pit decoder golden checks still pass. Logs:
`temp/xl-wave-runtime-native-build.log`,
`temp/xl-wave-runtime-policy-build.log`,
`temp/xl-wave-runtime-android-build.log`. No new warnings were emitted from the
changed sound/conversion files. The Android build retains unrelated existing
engine warnings.

Next verify this exact APK with the imported Dragon Pit package on emulator-5554:
confirm package-owned boss01/boss02 installation and actual boss playback,
duration, restart/restore and mission-exit cleanup. This runtime connection is
not a successful playback claim. Native D1 parity, companion lighting and
normal-input completion are still outstanding, and browser qualification must
remain unchanged until those requirements have evidence.

## Original package import exposed a decoder gap

The first on-device attempt used the unmodified 102161128-byte `levelpack.7z`
on emulator-5554, with the new WAV runtime APK. Import failed before mission
selection with Commons Compress `SevenZFile.buildDecoderStack` reporting
`Multi input/output stream coders are not yet supported`. The runner recorded
FAIL at import step 3/13 after 58501 ms. No sound or gameplay result was reached.
Evidence: `temp/xl-dragon-android/startup.log`, `import-result.json` and
`import-logcat.txt`. The original archive listing confirms solid compression,
eight blocks, and BCJ2 with LZMA streams. Repacking as ZIP would conceal this
actual distribution-package requirement and is not a fix.

`ArchiveFiles.kt` now catches only this exact unsupported-decoder IOException
when opening a 7z member and delegates to the existing bounded native 7-Zip
extraction path. Subsequent reads use that extracted catalog, matching normalized
member paths and declared sizes. CRC, memory/byte limits, truncation and other
errors do not trigger the fallback. The existing output-path/collision and
actual-byte checks still apply; the fallback does not provide Commons' decoder
memory cap and must not be described as doing so. Native extraction remains
subject to device memory and practical timing qualification.

The test emulator's pilot, configuration, file-set selection and preferences
were backed up to `files/xl-dragon-runtime-backup/state.tar` and restored after
the failed run. The original staged package and backup remain available for
the retry. The fallback build/regression test and a successful original-package
retry are required before claiming this distribution is importable.

Fallback-policy regression result: 10 tests, zero failures/errors, including the
new 7z unsupported-decoder discrimination check. The updated APK assembled and
installed; the original-package retry is running with run ID
`f848fd28973147d8b6088b85b93c4547`. Capture its terminal result before any rerun;
log: `temp/xl-dragon-android/startup-native-decoder.log`. The Gradle invocation
also remains pending after producing its unit-test XML and APK; observe its
terminal status rather than launching another build. After the emulator run,
capture import/usage/introspection evidence and restore the saved state again.

### Retry outcome and solid-block extraction

The fallback build completed successfully (3m13s). The original-package retry
passed the initial Commons inspection and reached extraction of 917/918 files,
then entered the native fallback for the remaining unsupported stream. The
existing native extractor used `extractSlow` per entry. Observed native output
counts rose only 292 -> 309 -> 338 while the solid archive was repeatedly decoded.
Stopped that run deliberately before its timeout; the runner recorded failure
after the application was force-stopped. This is incomplete import evidence.
Saved emulator state was restored again. The old native temporary directory
`cache/dxx_archive_6400120506823840381_7z` contains partial test output, not an
admitted mission package.

The 7z fallback now requests the admitted catalog in one native `extract` call.
Its callbacks retain path containment, catalog membership, byte accounting,
CRC success and per-entry declared-length checks, and require every admitted
entry to complete. RAR keeps its previous path. Added callback-contract tests
for complete extraction and short, oversized, corrupt and missing output;
these require a passing build and another original-package device run.

The metadata report now appends `distribution_archive`, distinct from the inner
HOG/descriptor category. Level-only scans report it unknown. The updated native
inventory contract passes (2.98s). The scoped-sounds description now distinguishes
the implemented opt-in imported-D1 path from unverified playback and unsupported
scoped sound banks.

The pinned XL texture conversion returns native D1 indices unchanged when
`bHaveD1Data` is true. Intersecting original Dragon Pit sidecars with the native
startup audit's wall/boundary textures finds 108 used texture IDs, 28 with nonzero
LGT brightness and 24 with chromatic CLR channels. Comparison with XL defaults
and renderer behavior remains open; these counts do not establish rendered parity.

### Original-package import passed; runtime title defect found

Single-pass build and tests passed: 12 tests (two solid-extraction contracts and
ten fallback-policy tests), zero failures/errors; Android build completed in
1m59s. The original-package run `f0afc1d9f3d1400286ff3c6a0e5d7e24`
reported successful import at 18:43:28, about 120s after starting import. All 918
files extracted. The published Dragon Pit descriptor, HOG and both WAVs compare
byte-for-byte with the audited originals. Its selected context key is
`92ed5b1f2d050d4e13586016cd2afd20a064d6028f29a72085b7bfd0c3a75a3e`,
owned by `mod/XL_Dragon_package_investigation`, with the original archive hash as
revision and no activation error. Evidence: `import-bulk-result.json` and
`startup-bulk-decoder.log` under `temp/xl-dragon-android`.

The same run selected Dragon Pit and Hotshot but aborted during startup. JNI's
`NewStringUTF` rejected the level overlay's bytes `ff ff ff ff 05`. Those bytes
are present at the title position in the original RDL (wrapper v1, game data v25,
game-data offset 85723, declared game header size 119). The pinned XL reader uses
the same title position. This was not a sound-conversion success or evidence of
full serialization compatibility. Evidence: `startup-bulk-logcat.txt` records
SIGABRT and the JNI stack.

The Android overlay and introspection now reuse the metadata display-name chooser,
which already rejects unusable raw titles and selects `drgpit` from the filename.
No gameplay bytes or read offsets were changed. The native audit now exports raw
title bytes separately from the chosen display title; the original audit passed
with exactly `[255,255,255,255,5]` and `drgpit` (`title-audit.json`/`.log`). Android
validation of the display fix and WAV installation remains pending. The saved
emulator state was restored after the abort. Earlier restoration was also checked
byte-for-byte across all 43 backed-up files.

The next run uses `temp/xl-dragon-android/launch.jsonc` to reuse the imported package
and enable sound diagnostics. `verify-sound.py` independently converts the original
stereo WAVs and compares full installed unsigned-PCM hashes, lengths, rates and
resident flags from introspection. It deliberately does not equate those checks
with audible output, boss AI playback, lighting parity or mission completion.

### Android startup and controlled boss playback

The corrected Android build completed successfully in 1m45s. Startup run
`2ec21126e00e4b32b4910d4a23d96f46` passed 13/13 steps and reached original
Dragon Pit gameplay. Evidence under `temp/xl-dragon-android`:
`launch-fixed-title.log`, `startup-result.json` and `startup-state.json`.
The original title remains invalid in engine storage; the display fallback fixes
the overlay/JNI boundary, not the raw title or every engine menu. The completion
score menu still exposes the unusable raw title and needs a display-boundary fix.

Both owned WAVs were installed at their original 44100 Hz rate. Independent
stereo-to-mono and unsigned-8 conversion verifies full installed PCM hashes:
sample 80 (`boss01`), 135128 bytes, `ed2701734f96d185`; sample 82 (`boss02`),
120578 bytes, `fec9c210cc291d90`. Ownership points to the selected package context.
`verify-sound.py death-state.json` passes both checks and reports both played.

The controlled loop probe `xl-dragon-boss-loop-20261024` passed 8/8 steps;
the death probe `xl-dragon-boss-death-20261024` passed 11/11. Their state, result,
logcat and captured audio files use `boss-` and `death-` prefixes in that evidence
directory. Mixer traces report matching input/cache/output fingerprints. The
loop probe teleported to the boss; the death probe used scripted lethal damage.
The player died during both probes and the reactor sequence advanced after the
boss death. These establish sample selection and playback, not normal-input
progression, combat difficulty, audible timing parity or mission completion.

The subsequent finish probe did not produce a terminal automation result.
`finish-logcat.txt` shows `main_menu: ACTIVATED`, then `skip_briefing` dispatching
Escape to that menu, followed by `jni startup main returned` and the app's normal
process termination. This was a probe error, not evidence of a second JNI crash.
A null-free warning appeared during exit and remains untriaged. The same log's
launcher ANR is earlier (18:55:13, setup-command broadcast), not the 19:03:39
engine exit. Replace the unconditional briefing skip with a main-menu assertion
before attempting same-process stock-mission cleanup verification.

Afterward, all 43 files in `state.tar` and all three files in `audio.tar` were
compared byte-for-byte with their restored emulator-5554 copies and matched.

### Remaining sound timing gate

Imported-D1 `boss_dying_frame` still subtracts the native fixed
`D1_BOSS_DEATH_SOUND_DURATION` (0x2ae14) from the death-roll duration. It does not
consume the duration override installed for a scoped WAV. The original death
sample lasts 120578/44100 seconds, approximately 2.7342 seconds. Pinned XL's
`DoRobotDyingFrame` instead derives duration from its selected sample length and
configured audio sample rate. Trace XL's high-resolution sample length/rate
semantics before changing the scheduler; a source-duration comparison alone is
not proof of equivalent scheduling. Any scoped-resource correction must preserve
native D1 timing for missions without that resource. Add a timing assertion
alongside playback, then verify save/restore and same-process mission switching.
Lighting companions and normal-input mission completion remain separate gates.

Further source tracing exposes an ambiguity in that reference: `LoadHiresSound`
(`gameio/loadsounds.cpp`) stores the entire WAV file length in `nLength[0]` and
sets `bHires = 1`, whereas `DoRobotDyingFrame` indexes `nLength[bHires]`.
`SetupSounds` resets `bCustom = 0`, and SDL playback selects the WAV buffer using
`bCustom`, not `bHires`. Slot 1 is populated by the separate custom-bank loader.
Consequently the reference expression cannot yet be treated as decoded WAV
frame-count/rate timing. Establish the actual selected slot and effective timing
in the reference before claiming parity or copying its indexing into this port.

### Completion-menu display follow-up

The Android completion menu now uses the existing display-name chooser, matching
the overlay and introspection fallback for unusable titles. Raw engine title
storage and non-Android title selection are unchanged. Scoped code-quality
reported no new files in scope; `git diff --check` passed. Android build output
is `temp/xl-dragon-android/completion-title-build.log`; the build completed with
exit code 0 (`BUILD SUCCESSFUL in 1m41s`). Device validation is still required. Use the existing
`trigger_levelcomplete` automation action to reach this menu directly, assert
its `menu_title` contains the filename fallback, then advance once and assert
the main menu. Do not use `skip_briefing` after reaching the main menu. This
scripted completion is a display/lifecycle probe, not gameplay qualification.

Device follow-up passed on emulator-5554. Startup run
`766b8e1fb3a84839963408db3657e864` passed 13/13 steps. The first completion probe
timed out because it checked `menu_title`; this screen stores its completion
text in `menu.subtitle`. Corrected run `xl-completion-assert-20261024` passed
2/2 steps, asserting the exact subtitle `\n\n\nLevel 1 Complete\ndrgpit Destroyed!`.
`completion-menu-state.json` and `completion-assert-result.json` preserve this
evidence. Run `xl-completion-advance-20261024` passed 4/4 steps and returned to
the main menu without closing the game process.

The stock-switch probe needed two script corrections: numeric array paths are
not supported by the assertion resolver, and this existing pilot has unlocked
levels, so Counterstrike presents a starting-level dialog before difficulty.
After accepting level 1, `xl-stock-level-20261024` passed 5/5 steps and reached
Ahayweh Gate in the same process (PID 31353). `stock-level-state.json` reports
asset owner `base`, generation 3, and an empty `mod_usage.sounds` array. This
proves installed-override cleanup for this Dragon Pit-to-stock transition, not
all save/restore paths or stale mixer-cache behavior under active playback.
`stock-level-result.json` and `completion-switch-logcat.txt` retain the evidence.
The 43 backed-up settings/player files and three audio files were restored and
compared byte-for-byte afterward; all matched. No normal-input mission
completion claim follows from `trigger_levelcomplete`.

### Bounded lighting companion readers

Added `companion_lighting.h/.cpp` under XL compatibility. Pinned XL
`LoadTextureBrightness` reads 910 signed little-endian 32-bit values; its
`LoadTextureColors` reads 910 records of one byte and three signed 32-bit
channels via `ReadColor`, then resets each runtime color index to zero. The new
readers preserve those serialized fields without normalizing, clamping or
reinterpreting signed data. They require exact lengths (3640 and 11830 bytes),
reject null/truncated/oversized buffers, and return complete tables only.
This is the pinned layout, not a claim that other table lengths are compatible.

`xl_companion_lighting_contract` and `xl_dragon_lighting_decode` passed (2/2,
0.20s). The private original-HOG test first checks SHA-256, reads the actual HOG
members, then verifies 69 nonzero LGT entries, maximum 131072, and 103 chromatic
CLR records. Synthetic cases cover signed extremes, little-endian decoding,
packed CLR offsets, the last record, invalid lengths and null input. Configure
`XL_DRAGON_HOG` to opt into that private fixture; no mission bytes were added to
the repository. The native build passed with warnings treated as errors;
scoped formatting/lint and `git diff --check` passed.

These readers are not yet wired to the renderer or mission admission. Next,
resolve companions through the selected mission HOG/context with explicit
ownership, compare used texture entries against reference D1 defaults, and
connect brightness and color to their actual rendering consumers. Verify
rendered effects and restoration before promoting the browser category. Do not
apply a parsed brightness table to HAM gameplay fields or describe the parser
test as lighting parity.

### Default comparison: companions are not neutral

Compared the original mission tables with `descent.lgt` and `descent.clr`
inside the previously audited 1.18.77 resource HOG. Its SHA-256 is
`df69c62aad949f2d92bc1bbea4b597623cd29d0a0f594cdcc3d66568d6ce482a`.
This is a resource baseline, not an ar12 executable observation. Default table
hashes are `a4e177d61533f6e4bfe72fa2ce347b5b2caf6651ef24edd0ae64e7c893b84839`
(LGT) and `eae3956288f2ba839b6a6b3eebc784ba4064bce32a005c89c801cc6dcec9957c`
(CLR). Mission hashes are
`82282958084b55c2d4aba4b8007937e08d49a34065c91edb742109ec970d6529` and
`2ec52d9bb08980fc6224e3529d25e4265150b97f346171b7d6376b4c768a7604`.

Every changed LGT entry occurs on an authored boundary/wall face:

| Texture | Default brightness | Mission brightness | Face uses |
| --- | ---: | ---: | ---: |
| 9 | 0 | 39300 | 16 |
| 10 | 0 | 39300 | 4 |
| 55 | 0 | 39300 | 7 |
| 286 | 58950 | 85150 | 6 |
| 322 | 19650 | 6550 | 12 |
| 333 | 65500 | 131072 | 17 |

Eleven CLR records change RGB, not merely their discarded index byte. Changed
textures 9, 10, 55, 281, 285 and 286 occur on 16, 4, 7, 14, 2 and 6 authored
faces respectively. The other changed entries (212, 213, 214, 221 and 222) have
no boundary/wall uses in this initial audit. Overlay orientation bits were
masked and open connections without walls excluded. These are dependencies,
not counts of visible or enabled emitters. Scratch reproduction and exact face
lists: `temp/xl-dragon-lighting-audit.py` and `.json`.

Pinned XL loads D1 defaults and then level companions before geometry loading
(`gameio/loadgame.cpp`). Embedded texture colors replace those values only for
`bD2XLevel` (`LoadTexColorsCompiled`); Dragon Pit's classic RDL does not supply
that replacement. With native D1 data, `ConvertD1Texture` retains D1 indices.
`CLightManager::AddGeometryLights` uses both base and overlay `IsLight` brightness plus
texture RGB to construct geometry lights (`lighting/dynlight.cpp`). Coronas
are another consumer, not a replacement for that lighting. The existing Redux
XL preview emitter uses a D2 default brightness table and requires an extended
source inventory, so merely enabling it for this classic D1 mission would not
implement these semantics. Add a classic companion resource profile and its
geometry-light consumers with D1 indexing, and qualify the resulting light
contribution and restoration. The neutral-companion shortcut is ruled out.

### Browser companion evidence wiring

`GameFileMetadata` now sends embedded LGT/CLR members to native compatibility
inspection rather than skipping them. Local and archived D1 MSN descriptors
also follow their sibling HOG, and archived metadata targets include MSN paths.
The native report validates exact table layouts, reports nonzero/chromatic
counts, and keeps `playable: false` and `status: blocked` while rendering is
unqualified. Malformed companions report the decoder's size/layout diagnostic.
Presence alone does not prove level association or equivalence to defaults.
Level-only reports still leave package requirements unknown. These reports also
feed the existing XL metadata-analysis guard; companion-bearing targets must
not be presented as fully analyzed by a backend that ignores those resources.

Native companion, original-HOG and inventory contracts passed 3/3 (3.15s),
including uppercase extension handling, malformed input diagnostics and original
Dragon Pit counts in browser JSON. Scoped formatting/lint passed. Android build
log: `temp/xl-dragon-android/companion-browser-build.log`; Android assembly
completed successfully with exit code 0. Device browser rendering still requires
verification.

### Classic lighting mode dependency trace

The pinned reference does not bypass geometry lighting in classic mode.
`CLightManager::Setup` calls `AddGeometryLights`, nearest-light computation and
`GatherStaticLights`. In classic mode, the latter gathers vertex lighting when
ambient color is enabled and embedded colored lighting is absent.
`HasColoredLight` returns false for a classic D1 level, so Dragon Pit needs this
generated vertex-color path for that configuration. Selecting classic mode
therefore does not make its LGT/CLR requirements disappear.

The geometry emitter construction has an ordering constraint: base texture is
processed first, then overlay. `Find` keys geometry lights by segment and side;
`Update` replaces the existing light's brightness/color instead of adding a
second light. Creation metadata (including the original texture and ambient
flag) is retained on an update. A new black light falls back to white for this
classic level, but update occurs before that fallback. These details differ
from the corona texture-selection helper and must not be shared by assumption.

Extended the static audit with this classic face-material selection. Among
boundary/wall faces there are 225 default candidates and 252 mission candidates;
27 are newly lit and 77 face material records differ. Both profiles have one
face with a positive base and overlay brightness. Exact changed face records
are in `temp/xl-dragon-lighting-audit.json`, generated by
`temp/xl-dragon-light-material-audit.py` using the prior audit script. These
counts still exclude XL face-compilation, enabled-state and visibility checks.
For concrete rendering probes, segment 2 side 4 uses texture 286 (brightness
58950 -> 85150 and gray -> orange), and segment 9 side 3 uses texture 333
(65500 -> 131072 with the same RGB). They are candidate test locations, not
verified visible camera positions.

Renderer implementation must cover the actual chain: geometry-light creation
and update ordering; nearest/visible light selection; face-distance, normal and
attenuation calculations; generated ambient vertex RGB; existing vertex-color
composition; and rebuild/reset on level and mission transitions. Relevant
pinned paths are `lighting/dynlight.cpp` (`GatherStaticVertexLights`,
`GatherStaticLights`, `Find`, `Update`, `Add`) and `ogl/ogl_color.cpp`
(`GetVertexColor`, `ComputeVertexColor`). A colored glow drawn on the emitting
face alone would not satisfy this dependency. Keep the browser blocked until
this chain has meaningful runtime evidence.

The first implementation stage is now `ClassicFaceLightMaterials` in
`companion_lighting.h/.cpp`. Its explicit scope is classic D1, lighting method
0, full level colors, no nostalgia override. It converts signed CLR channels
using the reference float divisor and builds one material per unique compiled
face: positive overlay brightness updates the base material while retaining
creation metadata. Nonpositive sources do not create lights; invalid base IDs
skip the face. Texture index bytes do not select colors. The object owns its
per-level normalized palette, including the reference's shared black-to-white
mutation on creation. Constructing a fresh profile restores original colors.

The native tests cover base/overlay replacement and retained ambient metadata,
black creation versus black update, palette mutation across faces, fresh-profile
reset, invalid IDs, unlit faces and negative brightness. The original-HOG test
also checks texture 286's orange material and 85150 fixed brightness, and
texture 333's brightness of 2. Native build and all three companion/original-HOG/
inventory tests passed (2.45s); scoped formatting/lint and diff checks passed.
This component is not yet called by the game renderer. It does not implement
face compilation, nearest/visible selection, attenuation or generated vertex
colors, and does not qualify lighting or a renderer mode by itself.

Added `geometry_attenuation` to the XL lighting module for the pinned
`ATTENUATION_TYPE=0` arithmetic after light selection and geometry measurements.
It applies range scaling, back-facing angular distance decay, cutoff and linear/
quadratic attenuation divided by source brightness. Nonpositive brightness is
rejected before division. It returns the modified distance and normal dot value
because subsequent diffuse calculations consume those values too. This is not
nearest-light selection, occlusion, face-distance calculation or final RGB.

The cutoff is an explicit input. Pinned `ogl_color.cpp` casts `MAX_LIGHT_RANGE`
(defined as fixed-point `I2X(222)`) directly to float in this expression. Do not
silently replace that with 222 world units; trace the effective runtime settings
and limits before connecting the helper to the renderer.

`xl_geometry_attenuation_reference` extracts and compiles the exact attenuation
fragment from `ogl/ogl_color.cpp`, SHA-256
`2f4e1ac609fa0043027d989b64b27c520de1fc25df9be0cab03b6268b258ab43`.
Configure `XL_REFERENCE_OGL_COLOR` to enable this source oracle. All 3240 cases
matched exactly across distance, normal, angle, range, cutoff and brightness
combinations. The ordinary lighting test also covers analytical distance decay,
self-light normalization, cutoff and nonpositive brightness. Both tests passed
(0.10s), and native compilation, scoped formatting/lint and diff checks passed.
No new Android rendering qualification follows from these arithmetic results.

### Nearest-light selection and visibility boundary

Added `nearest_geometry_lights` with the pinned distance-sort ordering and
32-light cap. It accepts candidates already qualified by visibility and range,
in reference light-table order; it does not invent visibility results. Signed
distance values remain intact because the reference subtracts source radius.
Invalid/out-of-range signed-16-bit light indices and excessive candidate counts
are rejected before narrowing. Equal-distance ordering matches XL's quicksort,
including swaps across the 32-entry boundary, rather than assuming stable sort.

`xl_nearest_lights_reference` extracts `QSortLightDist` from pinned
`lighting/lightprecalc.cpp`, SHA-256
`5f4f66408dd530f0e6282baeaf60e4f4b985deef35f4de5bbe9e32585cedd70e`.
Configure `XL_REFERENCE_LIGHT_PRECALC` to enable it. All 40 candidate lists
matched index and distance ordering, covering empty lists, 31/32/33 boundaries,
252 and 1024 entries, equal/ascending/descending/mixed/negative distances.
Nearest-light, attenuation-reference and ordinary lighting tests passed 3/3
(0.19s). Native build, scoped formatting/lint and diff checks passed.

The reference candidate stage is more than distance sorting: source-face
vertices receive distance zero; other vertices require the precomputed
segment-to-vertex visibility bit and a fixed-point range test before sorting.
`ComputeSingleSegmentVisibility` builds portal visibility from multiple views
via `BuildSegList` and `SegmentIsVisible`; light visibility has side-center and
corner views. This cannot be replaced by the existing corona single-ray check
without evidence of equivalence. The fixed-point 222-unit range test here also
precedes the separately audited float attenuation cutoff.

For static vertex gathering, `SetNearestToVertex` receives destination segment
and side -1; `Contribute` starts diffuse-enabled and returns after its range
calculation for that destination mode. Consequently its earlier precomputed
visibility input is essential. Runtime variable-light selection additionally
checks enabled state and other visibility/path rules. The new selection helper
is not yet a game-renderer hook; implement and verify those inputs before using
it to generate ambient vertex RGB.

### Selected-HOG companion resolution

Added `runtime/companion_resources.h/.cpp`, linked into opt-in D2 targets and
called by the native package audit. The resolver validates a flat RDL/RL2
identity, confirms that the level belongs to the selected mission HOG, performs
case-correct lookup, and reads only companions whose PhysFS origin matches that
same HOG. Foreign shadows and malformed present resources throw with a reason;
absent members remain absent. Reads have exact bounded lengths and return
complete decoded tables without global cached state. Loose/context-root
companions remain outside this initial resolver's qualified scope.

The original mission audit reported the selected `drgpit.hog`, both companion
tables, 69 nonzero brightness entries and 103 chromatic colors. Its ownership
probe passed uppercase level lookup, a deliberately prepended foreign LGT
shadow (ownership diagnostic required), recovery after unmounting that shadow,
and empty results after releasing the mission. Reusable CTest
`xl_dragon_companion_ownership` passed in 3.22s; it verifies original descriptor
and HOG hashes, stages them in a private fixture, invokes the real engine loader
and checks the report. Enable it with `XL_DRAGON_HOG`, `XL_PREVIEW_LIFECYCLE`
and `XL_RETAIL_DATA`. Evidence: `temp/xl-stack-review/dragon-companion-ownership`.
Native engine compilation, scoped formatting/lint and diff checks passed.

The companion interfaces explicitly preserve 8-byte MSVC packing across engine
headers. The resolver does not yet run as a level-render initialization hook;
the audit keeps `rendering_qualified: false`. Android execution of this new
resolver, loose-file precedence, geometry visibility and renderer integration
still require verification. Resource ownership is now exercised through the
real loader without claiming a rendered result.

### Emitting-face hit point

Added `GeometryLightFace` and `geometry_light_point` to the XL lighting module.
The caller supplies compiled side corners, triangle order and normals. The
implementation follows the hit-point branch of pinned `DistToFace` in
`physics/sphere_collision.cpp`: try each triangle's plane projection with the
reference barycentric tolerance, keep the first containing projection, retain
the receiver within 0.001 units of that plane, otherwise find the closest point
on the perimeter edges. It skips negligible normals and validates array bounds.
This deliberately does not substitute distance to the face center or select
the mathematically nearest plane when multiple triangles contain projections.

Analytical tests cover both halves of a quad, front/back projection, edge and
corner fallback, near-plane tolerance, nonplanar triangle-order behavior and
zero-normal fallback. Native compilation and three lighting/nearest/attenuation
tests passed (0.14s), with scoped formatting/lint and diff checks passing. The
new hit-point function has analytical coverage only; the existing extracted
source oracles verify nearest ordering and attenuation, not this function.
An independent source comparison and original-mine compiled-face coverage are
still needed before renderer qualification. No runtime shading hook was added.

The independent hit-point comparison is now implemented as
`xl_face_point_reference` (enable with `XL_REFERENCE_GEOMETRY_ROOT`). CMake
checks hashes and extracts the original float point-to-line intersection,
barycentric containment and `DistToFace` functions. Reference source hashes:
`physics/sphere_collision.cpp`
`499873929ce6606f00b504e59624595e6bee57f9f4e225f53c85ebd0c1a80b60`;
`maths/vecmat.cpp`
`4115afe844ad77a81e9f0de57ae1bbf577b06d5f49aed8b02d19002ff2021335`.
The harness supplies scalar vector operations and side storage, so this is not
a comparison against the reference executable's SIMD path or face compiler.

All 1008 cases passed within 1e-6 per coordinate (0.05s), covering planar,
warped, reversed-triangle and zero-normal faces with interior, exterior,
negative-side and near-tolerance receiver positions. An initial harness build
failure was a name collision between its vertex-storage macro and the extracted
function's local index pointer; renamed the harness storage and rebuilt
successfully. Scoped formatting/lint and diff checks passed. Original-mine
compiled-face coverage, portal visibility and runtime shading remain open.

### Loaded side geometry audit

The native package audit now exports each loaded side's four corners, compiled
triangle indices/count, fixed-point-derived normals and the light hit point
for its segment center. It calls the engine's `create_all_vertnum_lists` and
checks finite results across all sides. The original mine has 632 segments:
2884 quad sides, 430 TRI_02 sides and 478 TRI_13 sides (3792 total). The original
mission CTest now checks these counts in addition to companion ownership.
Evidence remains in `temp/xl-stack-review/dragon-companion-ownership/audit.json`.

`CSide::SetupVertexList` assigns a quad one face, and float `DistToFace` tests
only its first three vertices before perimeter fallback. The audit preserves
that behavior for DXX's loaded quads. The extracted-source oracle includes this
one-face quad case: 1260 cases across five shapes. However, subsequent inspection
of the caller's active preprocessor branch found that XL's `CSide::Setup`
triangulates every four-corner side. Its quad/de-triangulation setup is under
an inactive `#else`. The one-face test remains valid function coverage, but is
NOT the expected XL geometry for this classic mine. Feeding DXX's 2884 quads
directly to the lighting helper would therefore be incorrect.

Native engine compilation and both the source-oracle and original-mission
ownership/geometry tests passed (5.54s combined). This is coverage of DXX's
loaded geometry and the reference hit-point arithmetic, not proof that DXX and
XL compile identical normals or side types. No visibility, emitting-face
selection, generated RGB or rendered-lighting claim follows from finite hit
points. Next integration work must verify that compiler boundary, then connect
owned materials to eligible surfaces and the reference visibility/shading path.

The audit now also probes the inherited triangle-selection rule on a copy of
each segment with `add_side_as_2_triangles`, leaving gameplay collision geometry
untouched. It exports each candidate split and aggregate counts. This identifies
the separate lighting representation needed for planar sides; it is not yet a
port of XL's complete compiler. XL separately constructs float normals, computes
its side center, and applies `FixNormals`; converting DXX fixed normals to float
does not establish parity. Those operations must be accounted for before the
candidate representation can drive shading.

The original-mine probe produced 2022 TRI_02 and 1770 TRI_13 candidates, with
zero changes to the 908 already-triangulated sides. The reusable mission test
checks these counts. Native compilation and the mission probe passed; this
confirms the planar-side representation mismatch without changing collision
behavior or claiming reference floating-normal parity.

### Float triangle normals

Added `geometry_triangle_normal`, following scalar `CFloatVector::Normal` in
the pinned `include/vecmat.h`: cross `(p1-p0)` with `(p2-p1)`, then divide by
the magnitude when nonzero. Analytical coverage checks a slanted triangle and
coincident-point degeneracy. The original-mine audit evaluates all 7584 candidate
triangle normals, checks finite coordinates, and records them separately as
`raw_float_triangle_normals`. These are winding-order normals before portal
vertex sorting and `FixNormals`, not final XL side normals.

Against the corresponding fixed normals generated on copied engine segments,
the largest component difference was 0.00012671947479248047. Thus converting
fixed normals to float is measurably different even in this original mine.
Native compilation and lighting/mission tests passed (5.14s). A Windows `max`
macro collision in the audit was fixed with the macro-safe standard-library
call. The mission regression checks coverage of all 7584 triangles. Remaining
work includes reference-source normal comparison, portal ordering, side-center
and orientation correction, and use in actual visibility/shading. No renderer
qualification follows from this diagnostic.

### Portal normal ordering

Added `geometry_portal_normal` for the connected-side path. It sorts the three
absolute vertex IDs with XL's comparison order, preserves the reference winding
flip rule (including the fourth 0xffff sentinel), then computes the float normal.
The native audit invokes it for connected sides and exports
`ordered_float_triangle_normals_before_fix` separately from raw winding normals.
Solid-side normals retain their specified winding order. Collision geometry is
unchanged.

The optional pinned-source oracle now extracts `SortVertsForNormal` from
`main/segmath.cpp`, SHA256
`65949a1ce9a0a001ccb4a474b7b009d41e9cf0ea6975376350418d85ed1b6f4c`.
All six triangle-ID permutations match its ordering and flip behavior. This
comparison shares the ported normal arithmetic; it verifies ordering, not an
independent reference implementation of the cross product/normalization.
Native builds, the reference test, lighting test and original-mission audit
passed (three tests, 5.63s). Scoped formatting/lint and diff checks passed.
Side-center calculation and the two-stage `FixNormals` correction remain to be
ported before these normals can be considered complete XL side geometry.

The original-mine audit contains 4208 connected-side triangle normals. Sorting
changes 592 normals numerically, with a largest component delta of approximately
1.78066e-7 versus raw winding order. This small but observable difference is
why the port preserves the reference ordering rather than assuming all cyclic
normal calculations are numerically identical.

### Projected side-center candidate

Added `geometry_side_center` following `CSide::ComputeCenter` and the active
fixed `FindPlaneLineIntersection` branch. It averages float corners, converts
to fixed, uses the average supplied fixed normals as the line direction, and
projects onto the first triangle plane. Parallel lines and fractions greater
than one retain the original center; negative fractions remain allowed.
The active XL `maths.h` macros use double arithmetic with truncation toward
zero for FixMul/FixDiv, not DXX's arithmetic-shift multiplication. The helper
preserves those operations and rejects out-of-range intermediate values.

The native audit exports `lighting_center_candidate_fixed` for all original
mine sides using fixed normals from copied engine segments. Analytical tests
cover projection, parallel fallback, negative fractions and fractions above
one. Native compilation and lighting/mission tests passed. These centers remain
candidates: independent extracted-source comparison and XL fixed-normal parity
are pending. The engine's intersection helper was not substituted because its
overflow/ratio checks and scaling differ. `FixNormals`, visibility, and rendered
lighting still remain unfinished.

The side-center source comparison is now implemented as
`xl_side_center_reference`, enabled by the existing
`XL_REFERENCE_GEOMETRY_ROOT` option. It extracts pinned `CSide::ComputeCenter`
and fixed `FindPlaneLineIntersection`, using a scalar vector/storage adapter
with the active XL arithmetic macros. `main/side.cpp` is hash checked as
`5f8552e6fb177b24a624c47c9c644029acf95eda9f7001624c7f28e7972b4989`;
the existing sphere-collision hash check also applies.

All 700 combinations matched exactly in fixed coordinates: four translations,
seven corner heights and 25 pairs of supplied normals, including zero, opposed,
axis-aligned and slanted normals. The center, face-point and analytical lighting
tests passed together (0.10s), with scoped formatting/lint and diff checks
passing. This closes the independent center/control-flow comparison gap for
these inputs. It does not certify the fixed-normal compiler, SIMD/executable
behavior, overflow cases outside the helper's checked range, or final rendered
lighting. The next geometry dependency remains `FixNormals` and its input
fixed-normal/reference-direction arithmetic.

### Two-stage normal orientation

Added `geometry_orient_normals`, taking the ordered float normals, supplied
fixed normals, and independently normalized float/fixed directions toward the
segment center. It preserves the pinned `FixNormals` behavior: the float pass
flips negative-facing normals and stores their average; the fixed pass tests
the original fixed normals but flips the float normals a second time. It does
not recompute the float average after that second pass. Fixed dot truncation
occurs before the sign test, so a negative fraction smaller than one fixed unit
does not cause a flip. Tests explicitly cover double flips, the retained average,
zero direction and fractional fixed-dot truncation.

The original-mission audit now computes candidate directions from projected
side centers and exports `oriented_float_normals_candidate` (two normals plus
the first-pass average). All 3792 sides were covered; no final triangle normal
changed relative to the ordered input in this mine with the current supplied
fixed normals. Native compilation, the center-source comparison, analytical
lighting tests and original-mission audit passed (three tests, 7.12s). Scoped
formatting/lint and diff checks passed.

The audit currently uses DXX's fixed normals and `vm_vec_normalize` for the
fixed direction. That compiler/normalization boundary remains unqualified;
the orientation helper is not itself a complete side compiler. An extracted
`FixNormals` comparison is also still pending. No renderer hook or broader
playability claim was added.

### Separate XL fixed-normal calculation

Source inspection found a concrete reason not to reuse DXX fixed normals for
all geometry: XL's small-edge
`CFixVector::Check` scales upward with a left shift, whereas DXX's `check_vec`
small-edge branch shifts downward. Added opt-in runtime `geometry.cpp` with
XL edge scaling, signed cross adjustment, and double/truncating normalization.
It leaves collision normals unchanged. Integer magnitude still uses the
inherited engine square-root routine; reference validation of that dependency
remains necessary before claiming a complete compiler port.

The audit evaluates the new calculation in reference portal vertex order and
exports `xl_fixed_triangle_normals_candidate`. After correcting the overload
selection described below, all 7584 triangle normals match their copied-engine
counterparts in this original mine. Native compilation and the mission audit
passed; the regression checks zero differences
and analytical tiny-edge, reversed-winding and degenerate-normal cases. Scoped
formatting/lint and diff checks passed. Existing center/orientation candidates
still use the older supplied normals, so their earlier numerical results must
not be mistaken for the final XL pipeline. Next work must validate the new
fixed calculation, use it for split selection and feed it through centers and
orientation before connecting materials, visibility and the renderer.

**Correction to the initial fixed-normal audit:** the original 1767-difference
result came from an incorrect implementation and is withdrawn. XL has two
`Cross` overloads. The destination-reference overload divides by 65536 with
truncation, but `Normal -> Perp` calls the returning overload, which uses
`FixQuadAdjust` and arithmetic-shift rounding. The port now follows that actual
call chain. `xl_fixed_cross_reference` extracts the returning overload and
`FixQuadAdjust` from the already hash-pinned `maths/vecmat.cpp`; all 10000
deterministic signed scaled-edge comparisons match exactly. The subsequent
original-mission audit reported zero differences, invalidating the old regression
expectation; it was updated on that evidence. Tiny-edge upward scaling remains
a real difference from DXX and is covered by the native audit assertions.
This correction removes the claimed fixed-normal mismatch for Dragon Pit; it
does not remove the float-normal or all-sides-triangulated lighting requirements.

### Assembled classic lighting side compiler

Added `compile_classic_lighting_side` under the opt-in XL runtime. It reads a
classic side without changing engine collision data and returns one consistent
lighting representation: two ordered triangles, fixed and float normals,
projected side center, segment center, corrected normals and the reference's
first-pass average. Solid-side diagonal selection uses the XL fixed normal
and double/truncating fixed dot; connected sides use minimum absolute vertex
ID. Normal creation and triangle containment retain their different reference
vertex orders. Segment centers now follow XL's float averaging of eight
vertices before conversion to fixed, rather than assuming DXX's center routine
is equivalent. The public runtime structure preserves 8-byte MSVC packing.

The original-mission audit calls this compiler for all 3792 sides, exports
`xl_lighting_geometry`, and checks finite center hit points. It reports 2068
TRI_02 and 1724 TRI_13 sides: 46 diagonals differ from the earlier copied-engine
probe (2022/1770). This is a new assembled-path result, not a claim that the
older probe represented XL compilation. The reusable regression checks the
new totals and 46 changes. Native compilation and the mission audit passed
(7.64s); an initially missing `gameseg.h` declaration include was fixed.
Scoped formatting/lint and diff checks passed.

The assembled compiler now provides the geometry input needed for light-source
material selection. Its inherited square-root dependency and full compiler
comparison against the reference executable remain unqualified. Source-function
oracles and finite hit points do not establish visibility, rendered RGB or
normal-input playability. No renderer hook was added in this step.

### Owned companion materials on compiled geometry

Added `compile_classic_geometry_lights` under the XL runtime. Given decoded
brightness/color tables for a loaded D1 mission, it visits segments and sides
in compiled-face order, selects textured faces, applies the existing classic
material rules, and attaches the assembled lighting geometry. It returns a
fresh source list without global cached state. The audit supplies the owned
HOG companion tables and exports `geometry_light_sources`.

Face eligibility follows classic `createmesh.cpp` and `CWall::IsInvisible`:
unwalled sides need child -1; open or fully cloaked walls are omitted only when
neither they nor their existing opposite wall are trigger targets. The XL cloak
threshold is 31, independent of DXX's fade-table size. Classic D1 has no XL
colored-segment/outdoors surface path in this profile. Base/overlay processing
uses one material result per face, retaining creation metadata when the overlay
updates it; it does not sum duplicate lights.

The original mine produces 252 geometry light sources, including six using
texture 286, seventeen using 333, and one base-to-overlay material update.
Segment 2 side 4 carries the expected orange RGB and brightness 1.299285888671875;
segment 9 side 3 carries brightness 2. The native build and original-mission
audit passed (9.03s); the reusable regression now checks the source and material
counts. Scoped formatting/lint and diff checks passed.

This joins resources, face eligibility, material selection and geometry into
an actual runtime source list, still called only by the audit. Light-table
sorting, variable/enabled state, source size/range, visibility, generated vertex
RGB and renderer integration remain required. It does not certify lighting or
full playability, and the classic D1 profile must not be applied to XL extended
colored/outdoors geometry without its separate support.

### Geometry-light emission direction

Reviewing `CLightManager::Add` before adding variable state exposed a source
representation correction: `info.vDirf` is normalized from the side's fixed
normal average, not taken from `m_fNormals[2]`. Added
`geometry_emission_direction`, preserving fixed integer averaging before float
conversion and normalization. Each `ClassicGeometryLight` now carries this
direction explicitly. The audit replaces its ambiguous source `normal` field
with `emission_direction`; compiled geometry still retains its distinct float
average for other consumers.

Analytical checks cover a non-unit average and truncation of odd negative fixed
components. In the original mine, 198 of 252 emission directions differ from
the earlier float-average value, with maximum component difference
0.01422196626663208. Native compilation, analytical lighting and the original
mission audit passed (9.60s); scoped formatting/lint and diff checks passed.
The material counts remain unchanged. Variable/destructible/triggered light
classification and table sorting are still pending; the direction correction
must precede visibility/shading to avoid propagating the wrong source vector.

### Variable-source classification and ordering

The classic source builder now records destructible, flickering, triggered and
volatile-wall reasons using the creation texture (not a later overlay update).
It sorts static sources before variable sources, then by segment/side. Since
each face has exactly one source, those keys uniquely order this table without
needing the reference sort's equal-key behavior. The native audit verifies
strict identity ordering within each class and no static sources after variable
ones. This builder explicitly supports single-player classic D1; multiplayer
indestructible-light settings are not silently assumed.

Native D1 trigger records are treated as XL's `TT_DESCENT1` while classifying
light state. This follows `gameio/loadobjects.cpp` and avoids using DXX's D2
adapter type as if it were XL's loaded type. Wall target eligibility still checks
all native trigger links independently of type. The volatile-wall check
preserves the pinned source's either-coordinate match predicate; correcting
that reference behavior would require a separately justified compatibility
decision.

Dragon Pit has 249 static and 3 variable sources. All variable sources are
destructible: segment 559 side 4 texture 343, segment 560 side 5 texture 345,
and segment 586 side 4 texture 351. No sources classify as flickering, triggered
or volatile-wall in this original mission. The regression checks the three
variable sources occupy the tail of the 252-source list and are destructible.
Native compilation and the original-mission audit passed (11.37s); scoped
formatting/lint and diff checks passed. This tests the original mission's
classification, not every unexercised trigger/flicker/wall branch.

Classification is not enabled-state lifecycle support. Shooting a destructible
light, toggling or flickering a source, save/restore, source radius/range and
visibility/shading remain to be connected and verified. No static-light bake or
renderer qualification is claimed by the new ordering.

### Radius and visibility-gated nearest-vertex prefilter

The assembled side compiler now computes the maximum fixed distance from the
projected side center to its four corners. The source builder converts that
outer radius to float and halves it, following `CSide::ComputeRads`, mesh face
radius assignment and `CLightManager::Add`. The audit exports each source's
radius; all 252 original sources have positive radii (approximately 2.71 to
42.72 world units). The reusable mission regression checks positive radii.

Added `geometry_vertex_candidate` for the pinned geometry-source range scale
of 1. A source's own vertex receives distance zero regardless of range or
visibility. Other vertices require an explicit positive visibility result,
then use fixed center distance minus `int(radius * 6553.6f)`, with the inclusive
`222 * 65536` cutoff. Negative radius-adjusted distances are preserved for the
reference ordering. Analytical tests cover hidden-nearby rejection, own-vertex
bypass, the cutoff, radius adjustment and negative distances. Native compilation
and lighting/original-mission tests passed (13.05s), with scoped formatting/lint
and diff checks passing.

This helper does not produce a visibility table or claim complete candidate
lists. Source inspection confirms that `ComputeNearestVertexLights` consumes
segment-to-vertex visibility (`VERTVIS`). Correction after tracing its writers:
`SetLightVis` populates this table from visible mesh faces, grouped by the
light's source segment. Six segment-center views populate only the separate
segment visibility table. The portal traversal/projection boundary remains the
next required integration. Treating all range-qualified sources as visible
would be incorrect and is not enabled by this change.

### Portal eligibility before projection

Added a separate classic D1 portal predicate following the pinned reference's
`CWall::IsPassable(..., true)` and `CSide::CheckTransparency`. Precomputation
ignores closed doors. Overlay transparency can expose the base texture;
super-transparent overlays pass directly, while opaque overlays hide a
transparent base. These rules differ from DXX's existing transparency helper.
Malformed child, wall and texture references fail explicitly. This predicate
does not change collision or claim projected visibility.

The native engine build and lighting/companion-ownership tests passed (16.58s).
The original mission audit reports 2,104 eligible directed portals and 1,688
ineligible outer sides. Every connected side in this mission is eligible, so
the original fixture does not exercise opaque connected-side rejection.
Analytical texture tests cover opaque-overlay rejection, transparent-overlay
base lookup and super-transparent textures separately. Wall loading/default
cloak values still need reference parity evidence; these observations use the
engine's loaded wall state.

Next, implement reference projection and portal rectangle traversal before
building segment-to-vertex visibility. Connectivity alone cannot substitute
for visibility. Six segment-center views and source-specific face views remain
separate requirements, followed by shading, renderer integration and gameplay
qualification. The browser's unsupported status remains appropriate.

### Screen projection arithmetic

Added `geometry_screen_point` for the projection stage after camera and
projection matrix multiplication. It preserves the pinned reference's absolute
view-depth division, float arithmetic and truncation to integer screen
coordinates. The result records behind-camera state separately: source
inspection shows `ProjectAndEncode` overwrites `Encode`'s screen-edge bits with
only the behind bit. Off-screen coordinates are retained for portal bounds.

The new optional `xl_projection_reference` test extracts `CRenderPoint::Project`
from hash-pinned `3d/points.cpp`. An adapter supplies the matrix product, then
compares 5,202 projections across positive/negative depths, three viewport
sizes and off-screen positions. All match exactly. Zero/nonfinite depth and
screen-coordinate overflow are rejected explicitly rather than relying on
undefined float-to-integer conversion. These rejections are deliberate limits,
not claims of reference parity at those inputs. A future complete visibility
run must surface such a failure instead of silently treating it as hidden.

The projection and analytical lighting tests pass, as do scoped formatting and
lint checks. This oracle does not verify camera construction, matrix products,
`ProjectAndEncode` state/caching, portal rectangles, near-plane frustum tests,
or visibility tables. Those remain required before renderer qualification.

### Precomputation portal rectangles

Added `geometry_light_portal` for the four-corner classic precomputation path.
It preserves the reference's finite initial rectangle bounds, screen clipping,
fully-behind rejection, strict rectangle-disjoint comparisons (edge contact
survives), and mixed-depth inheritance of the current portal. Analytical tests
cover intersection, edge contact, disjoint rejection, mixed-depth inheritance,
fully-behind rejection and off-screen mixed-depth rejection. Both targeted
tests and x64 unit/x86 engine builds pass; scoped formatting/lint and diff
checks pass. These are boundary tests, not an extracted full-traversal oracle.

Source inspection refines the earlier frustum requirement: `SetupProjection`
in `lighting/lightprecalc.cpp` sets `nShadowMap=0`, while the extra mixed-depth
frustum test in `BuildSegList` runs only for negative `nShadowMap`. The helper
therefore implements the zero setting specifically; it is not a general
renderer portal implementation. Setup also restores the previous screen
dimensions after constructing its 1024-square projection. Viewport dimensions
are explicit inputs here; their complete lifecycle must be reproduced and
verified before assuming every visibility computation clips to 1024 square.

Portal revisit/expansion and traversal depth, camera transforms, final segment
visibility tests and runtime integration remain pending. No visibility table
or mission support promotion follows from these helper tests.

### Prepared-view traversal and revisits

Added `geometry_portal_traversal`, consuming six ordered sides per segment with
explicit passable children and already-projected corners. It preserves the
reference's per-pass list boundary, discovery order, rectangle union and
reprocessing of previously visited segments when another route expands their
portal. The caller supplies the pass limit corresponding to
`min(4 * (detail depth + 1), segment count)`; the helper does not substitute an
unbounded reachability search. Returned segments are traversal candidates,
still subject to `SegmentIsVisible` before setting segment/vertex visibility.

A four-segment regression exercises a narrow first route and a second route
that expands an already-processed segment, revealing a previously hidden exit.
It checks discovery order, union bounds, pass cutoff, cycles without duplicate
entries, and rejection of the exit when the expanding route is removed. The
lighting and projection tests pass, as do x64 unit and x86 engine builds,
scoped formatting/lint and diff checks. This is synthetic graph coverage;
full extracted traversal parity and original-mission camera integration are
not yet established. The next required connection is projected loaded-mission
vertices, followed by the final per-segment visibility test and six-view union.

### Loaded-mine adapter and corrected vertex-visibility provenance

Added `classic_visible_segments` to assemble the prepared-view portal graph
from loaded D1 segment children, wall eligibility and absolute side vertex IDs.
It applies the reference detail-depth cap and final `SegmentIsVisible` corner
filter. Because `ProjectAndEncode` retains only the behind code, this final
filter accepts a candidate if any of its eight classic corners is in front.
The adapter rejects incomplete projected-vertex tables and invalid vertex IDs.
It does not construct a camera or claim the input projections are qualified.

The native mission audit now exercises the adapter with explicitly synthetic
screen coordinates: an entirely behind-camera mine must yield no candidates,
front-facing points must retain the starting segment, and an incomplete vertex
table must fail. The report labels this evidence `synthetic_visibility_adapter`
and keeps `camera_qualified=false`. These are loaded-mesh integration checks,
not real camera or lighting evidence.

Tracing `SetSegVis` through `include/descent.h` corrected an earlier assumption:
it writes only the segment visibility bit. `SetVertVis` is called by
`SetLightVis`, which marks vertices belonging to visible mesh faces and indexes
the table by the light's source segment. Thus nearest-vertex selection needs
the union from light-source views, not the union of six segment-center views.
`ComputeSingleLightVisibility` performs five source views and subsequent
`CheckLightVisibility`; these dependencies remain required. Earlier wording
in the radius/prefilter section has been corrected to prevent implementing the
wrong table. Camera construction remains the next integration boundary.

The native build and original-mission audit pass (14.37s), with scoped lint and
diff checks passing. At detail depth 10 (44 traversal passes), the synthetic
all-front case yields 454 candidates and the all-behind case yields none.
The 454 count is a bounded traversal observation under artificial coordinates,
not a real-camera visible-segment count or a mission reachability verdict.

### Fixed camera basis

Added `geometry_camera_basis`, following pinned `CFixMatrix::CreateF` with the
existing fixed normalization and returning-cross helpers. It returns right,
up and forward axes before aspect/zoom scaling and rejects a zero direction.
The vertical special case deliberately preserves the reference's test of the
forward z component, which is zero in that branch: both upward and downward
views select up=(0,0,-1). Substituting a conventional float look-at matrix would
erase this behavior and fixed-point rounding.

The native audit checks forward, upward and downward basis construction.
These axis cases do not establish arbitrary-direction or complete camera
parity. The integer square-root dependency still needs reference validation.
Next, reproduce `ScaleTransformation`'s fixed aspect/zoom multiplication and
the float view transform, keeping projection matrix construction and restored
screen dimensions explicit. No actual mission camera view is yet qualified.

### Camera scale and world-to-view transform

Added `geometry_scaled_camera` for non-stereo `ScaleTransformation` with
`bOglScale=-1`. Aspect is an explicit fixed vector; zoom below one scales the
forward row, while zoom above one scales right/up by a fixed reciprocal. Each
multiplication follows the pinned double-arithmetic `FixMul` with truncation
toward zero, including negative fractions. Invalid scales and out-of-range
products fail explicitly. `geometry_camera_view` converts absolute fixed world
and camera positions separately to float, subtracts them, then dots against
the converted scaled right/up/forward rows in reference order.

Analytical tests cover both zoom branches, aspect scaling, negative fractional
truncation, translation/axis ordering and a large-coordinate case where doing
fixed subtraction before float conversion gives a different result. Targeted
lighting/projection tests, x64 unit and x86 engine builds, scoped lint and diff
checks pass. This is not a full extracted camera oracle. Aspect construction,
projection matrix setup and the actual viewport lifecycle still need to be
connected and verified before feeding real mission views to traversal.

### Assembled controlled camera on original geometry

Added `ClassicVisibilityCamera` and `classic_camera_visible_segments`, joining
fixed camera basis, fixed aspect/zoom scaling, float world-to-view transform,
perspective screen projection, loaded portal traversal and final segment
filtering. Inputs explicitly specify position/direction, aspect, zoom,
projection XY factors, canvas half-extents, screen bounds and detail depth.
The projection profile is diagonal non-stereo perspective only; no reference
global defaults or viewport lifetime are silently inferred.

The original mission audit now runs a controlled camera from segment 0's XL
center, forward=(65536,131072,196608), unity aspect/zoom, square 1024 viewport,
105-degree perspective and detail depth 10. It reports the complete inputs and
result under `controlled_visibility_camera`, with `reference_qualified=false`.
It returns 14 segments, in order:
`0,2,4,7,3,8,6,5,9,10,11,13,311,588`. This differs from the synthetic all-front
454-candidate case and exercises actual loaded vertex positions. The native
build and original-mission audit pass (13.66s); scoped lint and diff checks pass.

This establishes an assembled experimental path, not reference-camera parity.
In particular, the current adapter eagerly projects all loaded vertices; XL
projects lazily as traversal visits them. A zero-depth or overflowing vertex
outside the visited area can therefore reject our whole view unnecessarily.
That behavior needs refinement before production use. Projection matrix
readback rounding, reference viewport state, arbitrary camera basis/root math,
source-view unions and vertex-visibility population remain unqualified. No
browser support promotion or renderer hookup is made on this evidence.

### Lazy camera projection

Replaced eager whole-mine projection with a cached vertex callback and lazy
segment preparation during traversal. As in `BuildSegList`, processing a
segment with an eligible child projects its eight vertices before inspecting
portal corners. Projection results persist across revisits within that view.
The final `SegmentIsVisible` equivalent now also stops at the first front
corner instead of unnecessarily projecting remaining corners. Unvisited
geometry no longer causes projection errors merely by existing elsewhere in
the mine; zero depth encountered on the actual visited path still fails
explicitly pending a defined compatibility treatment.

The prepared-vector API delegates to the same traversal implementation.
A regression callback throws if the disconnected segment is requested, while
the existing expansion/revisit/cycle coverage remains in place. Native builds,
projection/lighting/original-mission tests, scoped lint and diff checks pass.
The controlled original-mission camera retains the same 14 segments in the
same order. This resolves the eager-projection limitation above, but does not
establish full reference camera parity or source-view visibility tables.

### Five source-view configurations

Added `classic_light_views` following `ComputeSingleSegmentVisibility` for
light views 1 through 5. The four corner views use the exact loaded vertex
positions, normalize corner-to-center direction, average it with the fixed
side normal and normalize again. The fifth uses side center plus the fixed
average normal and leaves normalization to camera-basis construction. A zero
side normal skips the source's views; coordinate arithmetic is range checked.
The fixed average is deliberately distinct from the normalized emission
direction used by light attenuation.

The original mission audit exports all five configurations for each of its
252 sources: 1,260 views, all with nonzero forward vectors. Native build and
mission audit pass (18.06s), as do scoped lint and diff checks. Configuration
coverage does not establish visibility correctness or complete source parity.

The next concrete compatibility issue is unavoidable zero depth in the four
corner views: the viewer is exactly at a face vertex, which is projected when
the source segment is traversed. The current projector rejects zero depth.
The reference divides by zero and converts nonfinite screen coordinates to
integers; that conversion has no portable C++ result. Establish target behavior
with reference evidence before choosing a deterministic implementation. Do not
silently skip the four views, shift their origins, or mark their visibility
complete from the center view alone. No source-view visibility union is yet
published or used by nearest-light selection.

### Singular projection host evidence

Added an explicitly diagnostic `--probe-singular` mode to the extracted
projection test. It runs the hash-pinned `CRenderPoint::Project` body with
runtime positive/negative zero depths and minimum positive normal float depth,
each with projected XY values -1, 0 and +1. This intentionally measures host
behavior of the source's undefined conversions; it is not a portable oracle.

MSVC 19.44.35225 x64 Debug, x64 Release and x86 Release agree in all nine cases:
zero depth yields (-2147483648,-2147483648) for all three XY inputs; tiny depth
yields that same integer-indefinite result for nonzero XY and (512,512) for
zero XY. The ordinary 5,202-case projection comparison passes in all three
builds. The diagnostic is reproducible with `xl_projection_test.exe
--probe-singular`; independent release builds are under
`temp/xl-projection-release` and `temp/xl-projection-x86-release`. Scoped lint
and diff checks pass.

This supports explicitly emulating the observed Windows integer-indefinite
conversion instead of retaining undefined casts or dropping corner views.
It does not prove historical XL executable or Android compiler behavior.
The production helper still rejects these inputs in this revision; the next
step is a defined conversion policy with dedicated regression coverage, then
running the actual corner views and inspecting their portal consequences.

### Defined singular projection and first assembled source views

The production projection helper now explicitly maps zero depth and
unrepresentable computed screen coordinates to INT32_MIN, matching the measured
MSVC x86/x64 integer-indefinite behavior. It checks before conversion and avoids
division entirely for zero depth. Nonfinite input coordinates/depth and invalid
viewport extents still fail. This is a deliberate portable emulation of the
observed Windows profile, not reliance on another platform's undefined cast.
Nine policy cases cover positive/negative zero depth and tiny-depth overflow;
the existing 5,202 ordinary projection comparisons still pass.

The loaded mission audit now runs all five source views for the actual emitter
at segment 0 side 0, using the same explicitly experimental optics as the
controlled camera. The four corner views and center view return respectively
8, 8, 13, 14 and 13 segments, with a union of 14. Full ordered lists are emitted
under `controlled_light_visibility_views` with reference qualification false.
All retain their containing segment. This removes the zero-depth rejection
that previously prevented corner views from running; it does not validate the
lists against a complete reference run. Native builds, three targeted tests
(17.48s total), scoped lint and diff checks pass.

Next: extend source-view execution across the original source table, implement
the segment-distance qualification and visible-mesh-face vertex marking, and
compare the resulting candidate tables before feeding nearest-light selection.
Renderer integration and normal-input mission completion remain pending.

### Complete original source-table view execution

Added `classic_light_view_candidates` to execute five source views with
explicit caller optics, retain per-view results and form a sorted deduplicated
segment union. It includes the source segment before view testing, as the
reference does. The API deliberately names these candidates: it does not
perform SegDist qualification, `CheckLightVisibility` or mesh-face vertex
marking, and it is not consumed by nearest-light selection.

The original mission audit now executes all 1,260 views for all 252 sources
using the controlled 105-degree, unity-zoom, 1024-square, depth-10 profile.
Every source retains its own segment. The 252 candidate unions contain 15,696
source/segment pairs, ranging from 6 to 182 segments per source. Each source
exports five view counts and the full union under
`experimental_view_candidates`, explicitly `qualified=false`. These are
observed experimental outputs, not expected reference visibility values.

Native compilation and the original-mission audit pass (21.61s), with scoped
lint and diff checks passing. The next required filter is the reference
segment-distance qualification before `SetLightVis`, followed by the separate
range/self-light/ray checks and visible-mesh-face vertex marking. Whole-table
execution removes the single-emitter coverage limit but does not establish
rendered lighting fidelity or mission playability.

### Router topology differs from view traversal

Tracing `ComputeSingleSegmentDistance` and `CDACSUniDirRouter::FindPath` shows
that the distance router uses `IsPassable(..., false)` with transparent OR
physically passable flags. It therefore does not ignore closed doors, while
opaque illusion walls remain eligible through physical passability. This is
distinct from the view traversal's transparent-only, ignore-doors predicate.
Added a shared wall evaluator with explicit router/view modes and
`classic_light_route_reachable` for directed topological reachability.

The audit compares those reachable segments with every source's experimental
view union. This changes 185 of 252 source unions and reduces candidate pairs
from 15,696 to 10,730. Filtered union sizes range from 2 to 168. Full topology
reachable counts range from 2 to 169. These observations use initial loaded
wall state; they are neither player progression reachability nor a claim of
verified SegDist behavior. Reports retain `segdist_qualified=false`.

The native build and original-mission audit pass (24.58s), with scoped lint
and diff checks passing. Do not replace SegDist with this breadth-first walk:
the reference chooses weighted routes, shortens them using segment visibility,
compresses lengths and endpoint IDs into its distance table, then reconstructs
distance for the zero-vector query. Those arithmetic/overflow details and the
subsequent range/self-light/ray checks still require implementation or evidence
that a narrower sign-only equivalence is valid. None of these experimental
candidate lists is yet supplied to nearest-light selection.

### Distance row encoding boundary

Added the distance-row scale, pack and unpack arithmetic from
`ComputeSingleSegmentDistance` and `CSegDistList`: scale is one through 65534,
otherwise float(maximum)/65534; negative distances encode as 65535; reachable
distances divide by the row scale and truncate to uint16; decoding multiplies
in float and truncates to fixed. The helper explicitly rejects invalid scales,
sentinel collisions and out-of-range decoded values instead of invoking an
undefined conversion. These are checked limits, not claims that such inputs
match historical undefined reference behavior.

Tests cover the scale threshold, maximum reachable code, unreachable sentinel,
small positive distances quantizing to reachable zero, sentinel collision and
decode overflow. Native unit/engine builds, the lighting calculation test,
scoped lint and diff checks pass. Weighted routing, visibility-based path
shortening and endpoint reconstruction remain unimplemented. In particular,
safe row encoding alone does not prove that reconstructed distance stays
nonnegative or that topology reachability is a valid substitute for SegDist.

### Loaded route edge costs

Added `classic_light_route_graph`, using the loaded mine's vertex bounding-box
diagonal to compute the reference integer `xDistScale` (fixed distance divided
by 65536, minimum one). It records XL-compiled segment centers, each connected
side's fixed center distance (minimum one), integer-scaled edge cost (minimum
one), and the separate router passability result. Costs outside the Dial heap's
uint16 range fail explicitly. Initial wall state remains distinct from geometry.

The original mission has scale 1559, 2,104 directed geometric edges and 2,048
router-passable edges. Scaled costs range from 78 to 3,172. The audit exports
the entire edge list under `experimental_route_graph` with `qualified=false`;
these are observed assembled values, not a comparison against a full reference
graph. The native build passes after correcting Windows min/max macro
interference, and scoped formatting/lint and diff checks pass.

This supplies the actual weighted graph for the next routing step. Preserve
reference side order and equal-cost predecessor behavior when implementing
the Dial queue; a shortest-path result with different tie ordering can choose
a different path for the subsequent visibility-based shortening. Integer-root
parity and the downstream path/distance-table stages remain unqualified.

### Weighted Dial traversal

Added `geometry_weighted_routes` with 65,536 circular buckets, six-side input
order, strict cost improvement, unlink/reinsert on decreased cost and LIFO
ordering for equal-cost bucket entries. It returns cost, predecessor and
incoming side for each node, preserving unreachable nodes separately. Positive
uint16 costs and the reference signed-node limit are enforced; arithmetic and
queue invariants fail explicitly rather than publishing corrupt routes.

Regression graphs cover equal-cost predecessor selection, decrease-key,
cycles, unreachable nodes and crossing the bucket wrap boundary. The original
mission audit runs weighted traversal for all 252 sources and verifies its
reachable set equals the independent topology walk. Maximum route costs per
source range from 525 to 29,404 scaled units. Native builds, lighting and mission
tests (24.85s total), scoped lint and diff checks pass. This confirms assembled
execution and these invariants, not extracted full-router parity.

The next step is reconstructing predecessor routes and applying the reference
visibility-based shortening before row encoding and endpoint reconstruction.
The weighted cost itself must not be used as the final fixed lighting distance.
Source candidate reports remain explicitly unqualified and disconnected from
nearest-light selection and rendering.

### Route reconstruction and interior shortening

Added checked predecessor-chain reconstruction and `geometry_shorten_route`
for the all-destinations `BuildPath` mode. The shortening helper takes explicit
segment visibility and fixed center-distance callbacks, excludes both endpoint
legs, advances across visible route segments, and retains the reference's
unconditional adjacent step. Routes shorter than three nodes have no interior
endpoints; a three-node route has one shared interior endpoint and zero
interior length. Overflow and invalid predecessor chains fail explicitly.

Tests cover reconstructed equal-cost routes, unreachable destinations, full
visibility shortening, no shortening, partially visible paths and the
three-node endpoint case. The original mission audit reconstructs all routes
for all source rows and checks their start/destination endpoints. Maximum
route node counts per source range from 2 to 38. Native builds, lighting and
mission tests, scoped lint and diff checks pass.

The mission does not yet use the shortening helper: its segment visibility
matrix must first be built from the six center views and combined symmetrically
as in the reference. Supplying an always-visible callback or reusing the
source-view table would be incorrect. Endpoint reconstruction and complete
distance-row integration follow that prerequisite; rendering remains gated.

### Assembled six-view segment matrix and route shortening

Added `classic_segment_visibility` using six outward fixed-normal center views,
skipping degenerate directions, seeding each diagonal and OR-ing both matrix
directions to reproduce the reference triangular SegVis semantics. It uses
explicit caller optics and remains separate from source-view vertex visibility.
The original audit checks symmetry/diagonals and supplies the assembled matrix
plus loaded center distances to route shortening for every source/destination.

With the existing experimental optics, the 632-segment matrix contains 37,100
unordered visible pairs including the diagonal. Row counts range from 9 to
234. The maximum shortened interior distance is 40,424,605 fixed units; every
route retained its reachable/unreachable classification after shortening.
Reports remain `qualified=false`: these values have not been compared against
a complete reference run or an actual reference viewport configuration.

Native build and mission audit pass (26.22s), with scoped lint and diff checks
passing. A newly introduced bool/int counting warning was corrected with an
explicit bool argument and the native target rebuilt. The next integration is
per-source row scaling/encoding plus endpoint-distance reconstruction, followed
by actual SegDist sign filtering. This is still precomputation work; no rendered
lighting or normal-input mission completion is claimed.

### Assembled distance rows and zero-vector qualification

Added `classic_distance_row` to join weighted routing, predecessor recovery,
segment-visibility shortening, row scaling and uint16 encoding. Dense rows
retain unreachable sentinels rather than trimming unused offsets; query
semantics are unchanged. `classic_segment_distance` decodes the entry and
applies reference direct, single-endpoint or two-endpoint reconstruction with
checked fixed-point sums. Native exact-value checks cover all three cases,
unreachable sentinels and out-of-row queries.

The mission audit now builds rows for all 252 source entries and compares the
sign of each reconstructed zero-vector query against weighted reachability.
All agree. The maximum reconstructed query is 90,291,110 fixed units; maximum
row scale is approximately 616.8493. View candidates are now filtered using
the assembled distance query, retaining the previous 10,730 pairs. This
agreement is evidence for this assembled original-mission run, not a proof
that arbitrary topology walks can replace reference distances.

Native compilation, endpoint checks and original-mission audit pass, as do
scoped lint and diff checks. Row/visibility reports remain unqualified because
full reference parity and viewport configuration are unresolved. Subsequent
range/self-light/ray checks, visible-mesh-face vertex marking, nearest-light
integration, renderer behavior and normal-input completion remain required.

### Mesh vertex visibility and assembled nearest-vertex selection

The pinned `lightprecalc.cpp` static `SetLightVis` wrapper marks vertices of
destination mesh faces in a row indexed by the light's source segment. Its
five-view/SegDist stage runs before `CheckLightVisibility`. The latter calls
the separate `gameData.segData.SetLightVis` method directly: clearing a light
flag for range or adding one by ray checks does not revise vertex visibility.
This corrects the implied ordering in the preceding next-steps list. The two
tables must remain distinct rather than rebuilding VERTVIS from final flags.

Added classic method-0 mesh-face vertex enumeration using the same textured
face eligibility as source creation. This profile has four original vertices
per face, no colored-volume faces and no generated triangle-mesh vertices.
The experimental source-view stage accumulates 21,569 source-segment/vertex
pairs across 150 nonempty rows (maximum 374 vertices in a row).

Added runtime `classic_nearest_vertex_lights`, joining loaded original vertex
positions and source face identities to the existing visibility/radius/range
prefilter and pinned nearest-light sorting helper. Own-face vertices bypass
visibility and distance with a zero candidate distance. Static and variable
sources both participate; selection preserves source indices and caps at 32.
The supplied visibility table is validated, not inferred from distance.

The original-mission audit now exports all 1,737 nearest-vertex lists. With
the existing experimental optics, 1,598 lists are nonempty, 489 contain 32
entries, and 29,120 entries are retained. It checks source indices, ordering
and caps for every vertex, plus own-face admission with all visibility flags
cleared for each of the 252 sources, including the three variable lights.
Native build, scalar lighting test and mission audit pass (26.49s for the
mission), with scoped formatting/lint and diff checks passing.

These assembled tables remain `qualified=false`. The inherited fixed-distance
routine and complete reference camera/table parity still need qualification;
the numbers above are observations, not golden reference values. No renderer
consumes these tables yet. Next work includes reference comparison of assembled
selection, nearest-segment/final light flags where consumed, ambient vertex
shading and renderer integration, variable-light lifecycle, then actual visual
and normal-input progression verification. Browser admission remains blocked.

### Corrected setup versus lighting magnitude modes

The pinned `CFixVector::Mag` has two distinct paths. `SetupSegments` forces
math format 0 while compiling side normals, then restores `nDefMathFormat`.
Both default option initializers select format 2. The subsequent side-radius,
child-distance, camera and light calculations therefore use double-precision
square root truncated to fixed-point integer, rather than the inherited
integer square-root routine. The prior blanket use of `vm_vec_mag` for both
phases was incorrect for this default profile.

Separated setup normalization from post-setup normalization and distance.
Normal construction and orientation retain the legacy routine; camera/view
directions, radii, routing and nearest-vertex distances use explicit default
math-format helpers. Values outside representable fixed-point magnitude are
rejected rather than relying on an undefined cast. This implements the pinned
default profile, not every user-selectable reference math mode.

Added an optional source oracle extracting `CFixVector::Mag` and `Normalize`
from the pinned files, with the existing vecmat.cpp hash and an additional
vecmat.h SHA-256 of
`83d7f73f89a2f588a0b56a08fb9e293b747cc65a5ed22bad6771d64319bdca0f`.
Its adapter fixes math format 2 and throws if the legacy branch is reached;
it does not claim to qualify legacy square root. All 20,344 magnitude and
normalization cases pass, including zero, signed tiny vectors, large vectors
and the largest representable axial magnitude. Overflow rejection is checked
separately without evaluating undefined reference conversions.

Native build, scalar lighting and original-mission audit pass (27.61s for
the mission), along with formatting/lint and diff checks. The corrected run
retains the preceding visibility/selection cardinalities: 37,100 symmetric
segment pairs, 21,569 vertex flags and 29,120 nearest-light entries. This does
not imply identical distances or tie ordering. Setup normals still have zero
differences from engine normals on the selected triangles. Complete reference
table comparison, actual viewport qualification, rendering and playable
mission verification remain outstanding; admission stays blocked.

### Shared vertex normals for ambient gathering

Traced `GatherStaticVertexLights` through `SetNearestToVertex` and
`GetVertexColor`. The ambient pass uses the normal stored on each shared
render point, not the receiving face normal. Added
`compile_classic_vertex_normals`: visit every segment/side/corner in setup
order, average that side's two fixed normals before conversion to float,
accumulate, then normalize. Portal sides participate even when they have no
textured mesh face or light source.

The pinned `AddToVertexNormal` calls both `CRenderNormal::operator+=` and
postincrement, each of which increments its uint8 face count. The new builder
preserves that doubled wrapping counter and the divide-before-normalize
ordering, including the zero-counter and zero-vector cases. It reports actual
contribution counts separately; it does not invent normals for cancellation.

The native audit exports all 1,737 normals with `qualified=false` and checks
finite components and the complete 15,168 side-corner contribution count.
Maximum contributions at one vertex is 36, so this mission does not exercise
counter wrapping. There are 55 exactly zero normals; the maximum unit-length
error among the remainder is approximately 1.10e-7. These are observations
from the assembled compiler, not a whole-reference normal-table comparison.
Native build and mission audit pass (26.01s), as do scoped formatting/lint
and diff checks.

The next ambient stage cannot simply sum the 32 nearest candidates.
`GatherStaticVertexLights` resets active/used state and requests static lights
plus variable lights when method 0 colorization is enabled. `SetNearestToVertex`
checks enabled state and calls `Contribute` with destination segment -1 and
distance modifier minus the source segment's average radius. That path uses
center distance and source radius, then returns before later ray/LightVis
logic. `SetActive` places accepted lights into distance buckets with forward
collision probing; `ComputeVertexColor` consumes that order. These filters,
segment average radius and bucket ordering remain necessary before ambient
color generation and renderer hookup. Nearest-vertex tables alone do not
establish either correct shading or mission support.

### Ambient contribution filter, radii and active ordering

Added the classic segment average radius used by ambient `Contribute`: minimum
center-to-side distance from the two fixed side planes, with the reference
65-unit tolerance and diagonal minimum-vertex anchor, combined with maximum
center-to-corner distance. The original mission's average radii range from
188,950 to 3,644,574 fixed units. This is an assembled runtime implementation;
whole-reference radius-table parity remains unqualified.

Added the default-range ambient contribution calculation, preserving the
float conversion of center distance, source radius and segment-radius
modifier. Added the 4,096-slot `SetActive` bucket ordering with negative-distance
clamping, forward collision probing, duplicate suppression and exhaustion.
Analytical tests cover collisions that differ from sorting, duplicates,
out-of-table distances, radius boundary admission and float rounding.

Further source tracing corrects an important assumption in the preceding
section: for destination segment -1, `Contribute` returns before assigning
`render.xDistance`. Its range-check distance is therefore NOT the bucket
distance. `CLightRenderData` initializes stored distances to zero in its
constructor (`main/gamedata.cpp`), while later `CDynLight::Init` and `Transform`
do not reset them. Preserve stored distances across subsequent passes rather
than recomputing them or always forcing zero in production.

The audit explicitly models fresh source records with zero stored distances
and enabled method-0 geometry lights. In that profile all 29,120 nearest-light
entries survive ambient selection, covering 1,598 nonempty vertex lists.
Reports remain `qualified=false` and state these assumptions. This does not
qualify enabled-state transitions, repeated gathering, or final colors.
Native build, lighting analytical checks and original mission audit pass
(26.89s), along with scoped formatting/lint and diff checks.

Next integration must supply actual persistent render-distance/enabled state,
then apply `ComputeVertexColor` source evaluation, accumulation and final
ambient clamping before hooking the method-0 vertex-color renderer. The
constructor-only audit profile is not a substitute for that lifecycle.

### Assembled ambient color evaluation

Added `geometry_source_color` for the ambient gather's diffuse geometry-source
path: closest face point, the better of center/nearest incidence angles,
near-planar compensation, emission angle, existing ATTENUATION_TYPE=0 math,
and the nearby-point diffuse adjustment. Material ambient/diffuse, range,
attenuation coefficients, cutoff and source brightness remain explicit inputs.
This path excludes spotlights, specular materials and lightmaps. Those are not
silently treated as supported by this helper.

Added separate ordinary/self accumulation. Self-source colors are averaged
by the eligible self-source count before being added to ordinary colors; the
combined RGB is divided by its highest channel when that exceeds one. The
self denominator includes eligible sources even if attenuation rejects their
sample, matching the location of `nSelf++` in `ComputeVertexColor`. Analytical
checks cover self brightness, averaging, denominator retention and peak-channel
normalization.

The mission audit joins the existing geometry, normals, nearest selection,
ambient admission and active ordering to this color stage. Its explicitly
experimental profile uses fresh enabled sources, zero stored distances,
renormalized world normals, no material specular, range scale 0.5, ambient 0.3,
diffuse 0.7, attenuation 0.05/0.005, brightness 1, and the reference's literal
fixed-valued cutoff. It excludes self sources at vertices outside their own
face and skips black source colors before accumulation.

The original mission produces 1,737 finite RGB values in [0,1]: 139 are black,
813 have a peak channel of one, and 1,512 are chromatic at a 1e-6 threshold.
These counts are observations, not reference targets or proof of visual
correctness. Full `ComputeVertexColor` parity, profile configuration and
reference screenshots remain unverified. Native build, analytical tests and
mission audit pass (29.40s), with scoped formatting/lint and diff checks passing.

The next step is to move assembled state out of the audit into level-owned
runtime lighting, preserving enabled/stored-distance state and release/reload
behavior, then connect the existing method-0 vertex shading helper to actual
rendered faces. Source destruction and restored state must update the relevant
colors. Admission remains blocked until that pipeline and actual progression
are verified; an exported ambient array alone is not playable support.

### Owned ambient state and source-state transitions

Added `ClassicAmbientLighting`, which captures one loaded classic D1 level's
source geometry, face vertex identities, segment radii, original vertex
positions, compiled normals and nearest-light lists. Gathering then reads
only this owned snapshot and explicit profile/source state; it retains no
engine pointers. Construction rejects non-D1 or multiplayer use. This is a
runtime component, but the engine's level-load/release hooks do not own an
instance yet.

Each source has an explicit enabled flag and stored bucket distance. Gathering
does not reset either. The profile carries material values, range/attenuation,
normal normalization choice and final brightness. Black colors are skipped
after active insertion to preserve bucket collision ordering. `clear` removes
all captured level entries, and destruction releases the owning containers.
Actual engine light flags must still be mapped to the enabled state: the
method is not itself a destruction, flicker or save/restore hook.

The original mission audit checks exact initial agreement with the separate
assembled color path and exact repeatability without mutation. Disabling all
three variable/destructible sources changes 160 vertices. Disabling all
sources produces black; assigning out-of-table stored distances also produces
black, proving that gathering does not overwrite them with range-check
distances. Restoring enabled flags and zero distances restores every initial
RGB exactly. Clearing yields an empty gather. These are controlled component
transitions, not in-game destruction or save/restore evidence.

Native build and mission audit pass (24.88s), with scoped formatting/lint and
diff checks passing. Next work is engine ownership at load/release, generation
of visibility inputs outside the audit, source lifecycle wiring and actual
renderer consumption. The compatibility decision remains unsupported until
reference visuals and ordinary mission progression are verified.

### Runtime visibility construction before load-hook integration

Moved the assembled source-view vertex-visibility pipeline into
`compile_classic_vertex_visibility`. It builds the symmetric segment matrix,
route graph and decoded-distance rows, unions source views, filters them by
SegDist sign and marks destination mesh-face vertices in source-segment rows.
Distance rows are cached per source segment, retaining the same values for
multiple emitting sides. Optics remain explicit; no reference viewport is
silently assumed.

`ClassicAmbientLighting` now has a loaded-level constructor taking sources
and optics, so it no longer requires an audit-produced visibility table.
The original mission audit compares every runtime vertex-visibility bit with
the separate assembled path, constructs ambient state through the new path,
then reruns exact color and enabled/stored-distance transition checks. Native
build and audit pass (26.92s), as do formatting/lint and diff checks.

Load-hook inspection found that `xl_preview_load_level` delegates classic
levels directly to the stock loader, before `LoadLevel` subsequently loads
palette/PIG metadata, bitmap replacements and level robots. Ambient creation
must occur after those asset changes, not inside that early geometry wrapper:
the visibility decisions consume bitmap transparency and source creation
consumes texture/effect metadata. The wrapper can clear previous state before
replacement, while `free_mission` provides the mission-release boundary.
These hooks are identified but not wired yet. The next integration should
initialize after asset loading under an explicit experimental preview option,
keep ordinary admission blocked, and verify both failed-load and mission-switch
cleanup before renderer consumption.

### Experimental level lifecycle hooks

Added the explicit `-xl-classic-light-preview` option. With this option,
single-player classic D1 levels with both selected-HOG LGT and CLR companions
create the owned ambient snapshot after palette, bitmap replacements and
level robots load. Levels without either companion remain inactive; partial,
malformed or shadowed companion sets fail with an experimental-lighting error.
Optics and material defaults are the same unqualified audit profile documented
above. Ordinary admission has not been widened.

The loader wrapper releases previous classic lighting before validation,
including failed replacement attempts. `free_mission` also releases it.
Preparation computes into temporary ownership and publishes only after the
snapshot and colors succeed; failure leaves no active snapshot. A read-only
color accessor and stored vertex count expose the state for integration and
testing. Rendering does not consume this accessor yet.

The original mission integration test now enables the option and verifies that
normal `StartNewGame` initializes all 1,737 colors exactly as the independent
audit path. It exercises a missing-level replacement, verifies the stored
vertex count becomes zero, restores the unchanged precommit geometry's lighting,
rejects a mounted foreign companion shadow, verifies zero retained state,
recovers after unmounting, and checks actual storage is empty after mission
release. Checking storage avoids mistaking a getter's null-mission guard for
real cleanup. Native build and mission integration pass (21.90s), with scoped
checks and `git diff --check` passing.

The guarded core changes are four lines after `load_level_robots` in LoadLevel
and a mission-release call/include. Android/device behavior has not yet been
revalidated. Next is actual vertex-renderer consumption plus destruction,
flicker and restored-state refresh; initialization alone does not qualify the
mission as playable or the lighting as reference-correct.

### Classic ambient colors reach the vertex renderer

The existing textured-face hook now accepts active classic companion lighting
as well as translated XL inventory colors. It feeds the computed ambient RGB
through `shade_vertex` with the original face UVL light, flash state and XL
dynamic-light inputs. Initialization collects each shared vertex's maximum
loaded UVL brightness and allocates its dynamic-light entry. Classic release
also clears those inputs and muzzle snapshots.

The existing XL dynamic-frame and muzzle-light paths now run for classic
preview state. This is reuse of an experimental path, not a claim that every
D1 object/emitter or dynamic-light arithmetic matches the reference. Existing
unresolved-emitter diagnostics still apply. Actual destruction/flicker and
restored-state refresh of geometry-source ambient colors remain missing.

The XL menu-active predicate includes classic ambient state. Automatic native
preview startup accepts `-xl-classic-light-preview` and requires an explicit
`-mission`, avoiding a mission-name-specific default. `-xl-preview-start`
can therefore use the existing 30-rendered-frame screenshot mechanism for
the next visual investigation.

Both native headless and graphical targets build. The graphical rebuild also
reported pre-existing return-path warnings in untouched `weapon.c` functions
POrderList/SOrderList; no new warning was reported in changed files. The
mission audit calls the actual renderer hook for every original vertex with
unit static light and verifies exact fixed RGB conversion of the generated
ambient colors. Lifecycle and source-state tests still pass (22.05s), with
scoped formatting/lint and diff checks passing.

This is renderer-path evidence, not a GPU capture or visual parity result.
Next steps are real native/Android preview captures, source lifecycle wiring,
reference comparison and ordinary-input mission completion. Unsupported
admission remains necessary until those checks succeed.

### First native GPU capture of classic companion lighting

Launched the graphical target from an isolated copied executable/DLL/mission
directory at `temp/xl-classic-render-20261024-initial`, using the original
Dragon Pit HOG and descriptor, retail data from `temp/xl-reference-ar12/game/data`,
and `-window -nosound -xl-preview-start -xl-classic-light-preview -mission drgpit`.
The existing preview capture path saved `screenshots/scrn0000.png` after
30 rendered ticks with the XL vertex hook active. Viewed the actual 640x480
image: textured mine geometry and the cockpit render, with visible colored
lighting. Foreground sprites obscure part of the spawn view, so this is a
rendering smoke test rather than a clean material comparison.

The process was confirmed live during capture, then the exact isolated
executable/PID was checked and stopped. Its launching tool session completed.
`evidence.json` records executable, mission HOG and screenshot hashes and
explicitly keeps rendering qualification and normal completion false. Output
logs were empty; no runtime diagnostic conclusion is inferred from them.
Audio was disabled for this visual run, so it supplies no audio evidence.

This is the first real GPU image of the assembled classic companion pipeline,
not reference visual parity. A controlled unobstructed view, source
destruction/flicker/restore behavior, Android rendering and ordinary-input
completion remain required. The screenshot must not be used to promote the
metadata support category.

### Real destruction-path probe and method-0 lifecycle correction

Added a native probe through `check_effect_blowup` for each detected original
destructible source. It supplies a synthetic player-owned impact and forces
the texture hit, so this is real engine effect handling but explicitly not a
normal-input shooting test. All three overlays change: 559/4 from 343 to 358,
560/5 from 345 to 363, and 586/4 from 351 to 360. Each source sets its
`Light_subtracted` bit. The probe observes zero changes to face UVLs, segment
static light and cached ambient RGB. Starting the level again restores the
initial RGB exactly and clears all subtraction bits. Native build and mission
integration pass (23.39s), with scoped formatting/lint and diff checks passing.

Source tracing changes the earlier proposed lifecycle work: do not equate
the controlled component test's 160 changed vertices with the expected result
of shooting these monitors. The pinned `GatherStaticLights` call occurs in
`CLightManager::Setup`; `ChangeLight`, `Toggle` and `Delete` do not call it.
Method-0 rendering consumes the setup ambient table together with current
UVL/dynamic light. `LoadTextureBrightness` writes the separate brightness
table, while `ChangeSegmentLight` reads texture-info lighting. Automatically
regathering ambient colors on every subtraction would introduce behavior
that this source trace does not support.

Keep destruction qualification open until reference runtime behavior is
compared, including the delta-index availability branch and save-load order.
The current probe demonstrates texture replacement, subtraction flags and
fresh-level recovery, not save/restore or full visual parity. Next work should
trace and test actual save restoration before adding any ambient-refresh hook;
earlier statements that such a hook is necessarily required are superseded
by this finding. Normal-input shooting remains a separate gameplay gate.

### Native save/restore after real monitor destruction

Extended the destruction probe to write a real imported-D1 save through
`state_save_all_sub`, start a clean level, change player energy, and restore
through `state_restore_all_sub`. The save is isolated at
`temp/xl-stack-review/dragon-companion-ownership/preview-user/xl-classic-destruction.sav`
(337,641 bytes in this run).

The restored state matches the destroyed state for all segment subtraction
flags, every base/overlay texture, every face-corner UVL light, and every
segment static-light value. Mission identity/level and player energy are
also restored. All 1,737 ambient RGB values match the setup colors exactly,
consistent with the current method-0 initialization order rather than a
destruction-triggered regather. A final new-game reload returns the fixture
to its original state before the independent ownership-failure checks.

Native compilation and the expanded mission integration test pass (30.04s),
with scoped formatting/lint and diff checks passing. The report now records
`save_restore_verified=true` inside the still-unqualified destruction evidence.
This verifies the targeted native checkpoint path; it does not qualify
Android save loading, all object/effect timing, reference visual parity, or
ordinary-input shooting/completion. No gameplay-support category was promoted.

Next priority is Android build/runtime verification of this assembled path,
followed by controlled reference comparisons and actual mission progression.
Further lifecycle changes should be driven by mismatches from those runs,
not by assuming method-0 ambient colors must be recomputed on every toggle.

### Android preview instrumentation

The assembled classic-lighting pipeline builds in the Android x86_64 pilot
APK (`:app:assembleDebug -PciTestAbi=x86_64 -PxlPilot=true`, successful in
8m 56s). The long post-packaging delay was traced to Kotlin's build-finish
statistics collection, not a native compilation failure.

Android does not consume the desktop ini argument path, and its startup
profile selects a game variant. Added the explicit automation field
`xl_classic_lighting` with `enable`/`disable` values, rejected during gameplay.
Disabling outside gameplay releases the cached lighting. Introspection now
separates `xl_pilot.classic_lighting_requested` from
`xl_pilot.classic_lighting_vertices`; these allow an Android test to prove
that a requested preview actually initialized the companion lighting.
These instrumentation additions still need their incremental Android build
and on-device verification. No device files have been changed for this run.

Use emulator 5554 with a fresh state backup, enable the profile before mission
selection, and verify the original mission initializes 1,737 vertices and
uses the renderer hook. Follow with save/load and a stock mission transition
to check cleanup. These are controlled runtime gates, not reference visual
parity or ordinary-input completion; support categories remain unpromoted.

### Android launch exposed repeated solid-archive inspection

The instrumentation APK build passed in 4m 26s. The first device run did not
reach the game: launcher preparation hit its 120-second limit while
`ModManager.buildMissionLaunchCatalog` called `MissionZip.inspect` on the
original archive. The cancellation stack passes through
`ExtractionBudget.registerEntry`, `MissionZip.inspect`, and
`ModManager.writeEnabledModPaths`. The automation runner subsequently timed
out at 240 seconds. No native automation result was created; existing older
introspection files are not evidence for this run.

Evidence is in `temp/xl-classic-android-20261024/launch.log` and `logcat.txt`.
The fresh backup covered pilot/save files, game configuration, mod usage,
file-set selection and shared preferences. Cleanup stopped the app and
restored those paths; `before.sha256` and `after.sha256` match byte-for-byte,
with `restoration.txt` recording verification. Emulator 5556 was untouched.

Changed catalog construction to use `reusableRecord` and `inspectExtracted`
before inspecting the source archive, following the existing package-details
path. Cache reuse still checks owner and extracted-file identities; missing
or changed files take the existing source-inspection/extraction path.
Extended the cache reload test to compare mission entries and resource
revision. This fix needs focused JVM tests and another original-package
device run. Do not extend the launch timeout or stage a reduced mission as
a substitute for proving the installed package can launch.

### Cache fix passes tests; Android lighting initializes, but the screenshot exposes an ANR

All 37 focused JVM tests passed (33 `ModManagerMissionZipTest`, four
`MissionLaunchCatalogTest`, zero skips). The combined test/build completed
successfully in 5m 8s. The original installed package then reached gameplay
within the unchanged launcher preparation limit. Native automation passed
18/18 steps and reported 1,737 classic-lighting vertices, `runtime_active=true`
and 1,400 shaded vertex calls. Disabled/requested-but-not-loaded states were
both checked before selecting the mission.

Do not report this as a clean end-to-end pass. Inspection of `render.png`
shows Android's not-responding dialog over the rendered mine. Logcat records
an ANR in the launcher process for `com.dxxredux.SETUP_COMMAND`, originating
during the test's `pause_route_metadata_precompute` command, before the game
process starts. The command waits for cancelled discovery work via
`stopAndAwait` and keeps its asynchronous broadcast open too long. The
native automation PASS does not detect that system dialog.

Evidence: `temp/xl-classic-android-20261024-cache/{result.json,state.json,steps.jsonl,logcat.txt,render.png}`.
The native script reports 93,533ms; total launcher-to-test time was longer,
and the existing harness applied its progress-based extension after 240s.
No launcher timeout setting was changed. State restoration again matched
the fresh backup byte-for-byte and left the app stopped.

Next run removes the test-only pause broadcast and uses the regular launch
handoff, which already handles background work. It adds a logcat ANR/fatal
gate alongside runtime assertions and exercises quick-save/restore,
transition to stock Counterstrike with zero classic vertices, and restoring
the Dragon Pit save. Prepared under
`temp/xl-classic-android-20261024-lifecycle`; results are pending. Reference
visual parity and ordinary-input mission completion remain separate gates.

### Android lifecycle verification without the pause-broadcast ANR

The normal-launch rerun passed all 35 automation steps (88,732ms native
script time), and its wrapper exited successfully. It explicitly rejected
logcat containing an app ANR or fatal exception; neither occurred. Visual
inspection of `render.png` confirms an unobstructed rendered mine and D1
cockpit rather than the previous system dialog.

Verified on emulator 5554 with the original installed package:

- Preview starts disabled with zero stored vertices; requesting it alone
  still leaves zero vertices before loading
- Dragon Pit initializes all 1,737 classic-lighting vertices and uses the
  renderer hook
- Quick-save, deliberate difficulty change from 2 to 3, and quick-load
  restore difficulty 2 and the initialized lighting
- Stock Counterstrike runs with zero classic vertices, inactive XL runtime
  and zero shaded vertex calls, even though the preview remains requested
- Restoring the Dragon Pit save from stock Counterstrike reinitializes all
  1,737 vertices and resumes shading; final introspection reports 980 shaded
  vertex calls and five menu-indicator draws

Evidence directory: `temp/xl-classic-android-20261024-lifecycle`, including
`result.json`, `steps.jsonl`, `state.json`, `logcat.txt`, and `render.png`.
The wrapper stopped the app and verified backed-up pilot/save/configuration
and launcher preference files byte-for-byte after restoring them. Emulator
5556 was untouched. The earlier pause-command ANR remains a separate
automation cancellation issue; removing that command exercises the regular
launch handoff and is not a fix to the pause command itself.

This closes the controlled Android initialization/save/switch/restore gate,
not visual fidelity or mission playability. Remaining work includes a
registered reusable Android runner for this scenario, reference comparisons
for the assembled lighting (including its experimental optics profile),
and ordinary-input mission progression/completion. The pinned reference
runtime and read-only introspection scaffolding are available under
`temp/xl-reference-complete`; those existing results concern the earlier
228-segment pilot and must not be reused as evidence for Dragon Pit.

### First Dragon Pit reference runtime ambient-table comparison

Created an isolated reference data/profile/cache tree at
`temp/xl-dragon-reference-20261024`. Its HOG matches the original audited
SHA-256 `a8eebb7e83faa55e143a48ec87c82407d8ffcaef3ad6b8f9329b84a6a42b262e`.
The pinned ar12 executable hash is
`77db2a39a2293b10554986eeb0c6a4b664621cfc9839bf5686ad9bd402505e18`.
Reference startup logs confirm imported-D1 data and the drgpit mod directory.
Read-only process inspection confirms 632 segments, 1,737 vertices, method
0, per-pixel lighting off, ambient-color enabled, no authored vertex colors,
and 252 static lights. No process-memory writes were used.

Captured all ambient RGB values and nearest-light index lists using offsets
derived from this executable's COFF symbols and `GatherStaticLights` /
`ComputeNearestVertexLights` disassembly, retained alongside the reader.
Ambient storage is a 20-byte `CFaceColor` stride; nearest lists contain 32
signed-short indices per vertex. `read-lighting.ps1` rejects a different
executable or geometry count. `compare.py` records input hashes and explicit
qualification limits in `comparison.json`.

The assembled native preview does not yet match this reference run:

- 1,398 vertex RGB triples differ by more than 0.00001
- Maximum component difference is 0.702082; mean per-vertex maximum is 0.081325
- Native/reference black-vertex counts are 139/157
- Nearest-light lists differ at 1,493 vertices, with 29,120 native entries
  versus 18,851 reference entries

The comparison remains unqualified: projection/zoom/detail/material options
are not fully matched, and source-by-source light index identity still needs
verification. Do not tune colors to these numbers. First capture the actual
precomputation projection, zoom and aspect, verify source identities, then
compare visibility and nearest lists before investigating color accumulation.
The reference does explicitly build a 1024-square projection and copies that
transformation into each light view; the normal gameplay screen dimensions
alone do not establish the precomputation optics. Square dimensions are not
by themselves evidence of the mismatch's cause.

The owned reference process was stopped after capture. This finding is why
the successful Android lifecycle checks cannot justify promoting lighting
support or claiming the mission is fully playable.

### Reference zoom identified and corrected

A fresh-cache reference run exposes the static precomputation projection:
1024-square viewport, diagonal XY terms 1.7652469, zoom `0x9000` (0.5625),
and screen aspect 65536. This matches `DEFAULT_ZOOM` and the pinned
`COGL::SetupProjection` formula, which multiplies 105 degrees by zoom.
Each light view then calls `ComputeAspect(1024,1024)` and applies that zoom
again through `ScaleTransformation`. The static projection object's zero
aspect fields must not be mistaken for the per-view aspect: the latter is
explicitly computed after copying it.

The prior preview assumed zoom 1 and therefore used the wrong projection
and camera scale. Added `classic_default_lighting_camera()` shared by the
runtime and audit to use the source-defined default zoom in both places.
Scoped formatting/lint and native compilation passed. The full native
mission ownership/destruction/save-restore test passed in 34.27s.

Reference light-table inspection also verifies all 252 segment/side identities
in their original order. Brightness and radii agree within float serialization
precision. Twelve fixed-point source centers differ by at most a few units;
those small geometry differences remain unresolved.

After the zoom correction, against `reference-detail.json`:

- 27 of 1,737 vertex colors differ by more than 0.00001
- Mean per-vertex maximum component difference falls to 0.000398374
- 33 nearest-light lists differ; native has 18,811 entries versus 18,853
- Vertex 8 remains a material failure: native black/no nearest sources,
  reference RGB 0.43988633 with sources 0 and 1

Keep the comparison unqualified. Two fresh-cache reference captures themselves
differ at nine nearest lists/colors (maximum RGB delta 0.019196209), for an
unresolved reason. Explicit single-thread settings and repeat captures should
be checked before assigning small residual differences to the port; the
source's default is already single-threaded, so concurrency is not yet an
established explanation. Next prioritize the missing visibility at vertex 8,
verify remaining canvas/detail settings, and investigate source-center math.

Snapshots `native-before-zoom.json`, `native-after-zoom.json`,
`comparison-before-zoom.json`, `comparison-after-zoom.json`, and the extended
read-only reader/disassembly are retained in `temp/xl-dragon-reference-20261024`.
Both owned reference processes were stopped after capture. Android lifecycle
evidence predates this zoom correction and needs refreshing after residual
lighting issues are resolved; no support category has been promoted.

### Explicit single-thread reference capture and vertex 8 localization

Captured the live pinned reference as `reference-single-thread-a.json` using
read-only process introspection. Runtime flags confirm `multithreaded=0` and
`threads=1`; projection/zoom still match the preceding capture. Against
`reference-detail.json`, nearest-light lists change at vertices 81, 82, 85,
87, 93, 95, 97, 99, 100, 103, 104, and 105. Single-thread operation therefore
does not establish repeat stability; do not attribute these changes to
concurrent lighting workers without further evidence.

Vertex 8 remains RGB (0.43988633, 0.43988633, 0.43988633) with nearest sources
0 and 1. Captured segment vertex arrays establish that it belongs only to
segment 1 (vertices 11, 10, 9, 8, 0, 1, 2, 3). The native audit marks both
0/side5 -> 1 and 1/side4 -> 0 as non-passable for routing. Segment 1 appears
in both source view unions but is removed by the subsequent route filter.
This localizes the largest discrepancy to route admission rather than color
accumulation; it does not yet prove which wall predicate or asset differs.

Pinned `CDACSUniDirRouter::FindPath` calls `CSegment::IsPassable` with the
transparent/passable mask and the default `bIgnoreDoors=false`, so globally
allowing closed doors would be an unjustified fix. Compare the specific wall,
texture identities and bitmap flags next. `CSide::CheckTransparency` also
uses bitmap overrides and the auto-transparency option; our current predicate
only tests imported bitmap flags. Establish whether either difference applies
to this connection before modifying it. No runtime behavior or support
category was changed by this investigation.

### Portal state agrees; route-filter defect is not established

Extended the native package audit to record texture IDs, bitmap flags and
wall type/flags/state/cloak for every connected side. Scoped format/lint,
native build and the full ownership test passed (35.68s). This adds diagnostic
evidence only, with no admission or rendering behavior changes.

Read-only pinned-binary captures now include portal sides, wall records,
effective bitmap flags (including override/frame resolution), and distance
rows 0 and 1. Offsets were checked against CSide::Wall,
CSide::CheckTransparency, CWall::IsPassable and CSegmentData::SegDist
disassembly. Evidence is `reference-route-rows.json` under the existing
`temp/xl-dragon-reference-20261024` directory; the extended reader is retained.

Both engines have identical affected walls: type 2, flags 24, state 0,
cloak 0, base texture 32, overlay 399. Reference bitmap transparency flags
are base 0 and overlay 1, with no overrides; native flags 8 and 9 differ
only by its RLE storage bit. Auto-transparency does not apply: pinned
IsTransparentTexture explicitly excludes D1 missions.

Crucially, the reference distance row 0 also marks destination 1 as 65535
(unreachable); row 1 contains only segment 1. The previous localization
identified where our visibility is removed but did NOT prove a route-filter
defect. Do not relax closed-door routing to fit vertex 8. Its reference nearest
sources remain 0 and 1 despite this route rejection. Next trace reference
vertex-visibility initialization/population, mesh face indices, and nearest
candidate ownership to explain that inconsistency. Reference repeat variation
remains unresolved; all qualification gates remain open.

### Reference vertex-visibility initialization defect

Captured source face ownership in `reference-source-faces.json`. The reference
uses triangle_mesh=0; sources 0 and 1 each have four vertices, respectively
[7,6,2,3] and [0,1,5,4]. Neither owns vertex 8. This rules out an ownership
shortcut or an oversized face-index scan for that mismatch.

Pinned source exposes a concrete initialization defect: ComputeNearestLights
calls InitLightVisibility(0), which clears the existing bVertVis buffer, then
calls InitLightVisibility(1), which allocates it with CArray<uint8_t>::Create.
Create uses `new uint8_t[length]` without value initialization; no subsequent
clear occurs before SetVertVis ORs bits into it. ComputeNearestLights destroys
the buffer after precomputation. Thus a cold allocation need not start at zero.

The pinned executable confirms this sequence: InitLightVisibility stage 0
clears at VA 0x14015b369 only if the existing pointer is non-null. Stage 1
allocates at 0x14015b795, stores pointer and length, and jumps directly to the
success path at 0x14015b7a7, with no data-buffer clear. The long zero stores
before allocation are the array name/header, not the newly allocated bytes.
ComputeNearestVertexLights checks this buffer at gameData+0x1b78 when a vertex
is not owned by the source. The buffer is already destroyed in a completed
run, so the post-load reader cannot recover its original contents.

This is a verified initialization defect in the reference, and a plausible
explanation for repeat-varying extra light candidates. Causality for vertex 8
and the full residual set still needs a controlled run that captures the
newly allocated bytes and clears them before visibility construction. Preserve
the unmodified pinned binary and its evidence. Any diagnostic reference with
this one initialization correction must be separately hashed/labeled and
compared against both the original and native output; it must not silently
replace the baseline. Do not reproduce uninitialized memory in the port or
waive all remaining RGB differences on this evidence alone.

### Controlled reference initialization experiment: stable residuals

Created a separate diagnostic executable alongside the unchanged pinned binary.
`diagnostic-clear/patch.py` verifies the original SHA and nine original bytes,
then changes only the post-allocation jump/padding at RVA 0x15b7a7. The new
instructions set nStage=0 and re-enter the existing initialization path at
0x15b34c, which reloads and clears the newly allocated bVertVis buffer. It also
redundantly clears bLightVis, already cleared before any visibility is built.
Disassembly verifies the new control flow. This is a diagnostic binary, not a
new claim of equivalence to the unmodified reference.

Original SHA remains
77db2a39a2293b10554986eeb0c6a4b664621cfc9839bf5686ad9bd402505e18.
Diagnostic SHA is
24e5b2a202ffb1b1020b19802df00b63dd9b47377d4b206ac8d30ce89d82c325.
Manifest records before/after bytes and file offset. Launcher/reader require an
explicit DiagnosticClear switch and verify the diagnostic hash; captures are
labeled diagnostic_clear=true and qualified=false.

Two fresh-cache, explicitly single-threaded runs A/B produced exactly equal
source records, all nearest-light lists, and all 1,737 RGB triples. Vertex 8
now has no nearest lights and RGB zero, matching native output. This controlled
change confirms the initialization defect caused that largest discrepancy.
It does not justify emulating the original uninitialized bytes.

Against the diagnostic reference, native has only four RGB differences above
0.00001: vertices 44, 45, 48, 49. Maximum component delta is 0.0342071864;
mean per-vertex maximum is 0.0000388956. All four native lists contain extra
source 12 (segment 73, side 1). Vertex 44 belongs to segments 10 and 11;
source 12's native view/route union includes segment 11. Next trace that
visibility path rather than adjusting color accumulation.

Ten nearest lists differ in total. Several are ordering differences; vertex
808 also differs in retained membership at the 32-light cap. These remain
explicit unresolved evidence even where the resulting RGB agrees. Twelve
source-center rounding differences also remain to be resolved.

Artifacts: reference-diagnostic-clear-a.json, reference-diagnostic-clear-b.json,
reference-source-faces.json (original), and diagnostic-clear/{manifest.json,
patch.py,compare.py,comparison.json} under temp/xl-dragon-reference-20261024.
Comparison records input hashes. Both diagnostic processes and the earlier
owned original reference process were stopped after capture. No engine code
or support category was changed by this controlled experiment. Android and
ordinary mission progression verification are still required.

### Preserve original fixed coordinates for side-center projection

Corrected geometry_side_center to accept the original fixed plane vertex.
The reference averages float corners for the initial center but intersects
that center against VERTICES[m_corners[0]], not a float-to-fixed round trip.
Our previous reconstruction lost low bits at large coordinates. Runtime and
audit callers now pass the original fixed coordinate explicitly.

Extended the extracted-reference side-center test with large fixed coordinates
whose low bits cannot survive float conversion. Scoped formatting/lint and
both native builds passed. CTest passed xl_side_center_reference,
xl_lighting_calculation and xl_dragon_companion_ownership (29.07s total).

New native-after-plane-point.json and comparison-after-plane-point.json show
all 252 source centers now exactly match the diagnostic reference (previously
12 differed). Nearest-list mismatches shrink from ten to six: vertices 44,
45, 48, 49, 807, 808. The four RGB discrepancies and maximum delta 0.0342071864
are unchanged; source 12's visibility into segment 11 remains unresolved.
Sources 12 and 13 belong to different segments (73 and 74), so their shared
appearance in nearby candidate lists does not prove equal visibility rows.
No support promotion; Android refresh and full progression gates remain open.

### Remaining source-12 mismatch: routes and face membership checked

Fresh diagnostic run captured as reference-diagnostic-visibility.json and
reference-diagnostic-mesh.json. Extended the reader with distance rows 73/74,
packed light visibility, and compiled segment-face corner indices, using
SetLightVis disassembly for table offsets and strides. Row 73 admits both
segments 10 and 11; segment 11 has stored distance 55126 and intermediate
endpoints 99/12. Thus the remaining four colors are not explained by reference
route rejection. Reference segment 11 faces include [48,51,47,44],
[45,46,50,49], and [44,45,49,48], confirming the affected vertices are present
in its textured mesh.

The final bLightVis row includes segment 11, but this is not proof that the
five camera views admitted it: CheckLightVisibility subsequently updates the
same light-to-segment table using range/path/ray checks without populating
bVertVis. Keep these evidence stages distinct. Native candidate distance for
source 12 at vertex 44 is 8967185 fixed units, comfortably within the
222-unit cutoff; source radius/range and center match the reference.

Next compare camera-stage visibility into segment 11, including actual
precomputation canvas/detail values and projected portal boundaries. The
remaining 807/808 nearest-list differences are also still open. No engine
behavior was changed during this localization step.

### Actual pre-selection canvas captured

Created another separately hashed diagnostic binary, derived from the cleared
reference, that pauses at ComputeNearestVertexLights entry (RVA 0x15d350,
original bytes 4157 replaced with ebfe). It intentionally cannot complete a
level load; its sole purpose is to retain the camera-stage bVertVis table and
active canvas globals before candidate selection. Manifest is under
`diagnostic-snapshot`; SHA is
8cd246e41fb6dc29a2538f670037b3876f31747646e9a4b6e572e6ba4cef18cb.
The launch log reached `computing nearest vertex lights`, and the reader
validated the complete allocation size before capture. Owned process was
terminated immediately afterward.

`reference-preselection-snapshot.json` captures all source-segment vertex
visibility rows. Row 73 excludes vertex 44, directly proving the discrepancy
exists before candidate sorting. More importantly, actual canvas half-width
and half-height at this stage are 1440 and 900, not the assumed 512 and 512.
Static projection is still the previously captured 1024-square projection.
SetupProjection restores screen dimensions and deactivates its local canvas;
Project uses CCanvas::fCanvW2/fCanvH2 and portal bounds use the restored screen.
Our square-canvas audit therefore does not yet match the reference settings.

Do not change the production canvas constants merely to fit one desktop run.
Next add explicit audit viewport inputs (or set and verify an equivalent
reference viewport), compare the same geometry with matching canvas/screen
bounds, and decide/document the intended deterministic runtime profile.
Integer pixel quantization can affect edge-touching portal visibility even
where width/height scaling otherwise cancels. Remaining four RGB differences
must be reassessed after matching these settings; no portal fix is justified
by the unmatched comparison alone.

### Matched-canvas native calculation comparison passes

Added paired audit-only canvas width/height arguments, strict dimension parsing,
and an explicit report of the chosen canvas. Custom canvases reject combination
with ownership certification, which compares against the default runtime profile.
Production remains deterministic 1024-square; no desktop dimensions were copied
into runtime defaults.

At reference-matched 2880x1800, native-matched-canvas.json has all 1,737 ordered
nearest lists exactly equal to reference-diagnostic-clear-a.json. No RGB triple
differs above 0.00001; maximum component difference is 0.0000001909240722.
All source centers already match. The four prior RGB/six nearest differences
were therefore artifacts of unmatched viewport settings, not established portal
defects. Scoped format/lint, native build and default ownership integration pass
(the integration took 28.40s). The initial direct audit invocation rejected a
non-fixture userdir; it was rerun successfully in an isolated fixture/preview-user.

Durable summary added at XL_compat/evidence/dragon-lighting-comparison.md and
EXTENSIONS.md updated to remove the stale claim that companion tables are only
decoded and not rendered. Remaining gates: matched square runtime-profile
reference, refreshed Android lifecycle/render checks, rendered-scene comparison,
and normal-input mission completion. No browser category was promoted.

### Default-square reference and refreshed Android lifecycle pass

A separate reference user profile sets custom display width/height to 1024 and
nDefaultDisplayMode=-1. A pre-selection snapshot verifies half-extents 512/512.
The completed initialized reference matches native-after-plane-point.json at
every nearest list and every vertex RGB within 0.0000001909240722. This closes
the unmatched-default-canvas calculation question without changing production
settings. Evidence: reference-square-preselection.json,
reference-square-complete.json and comparison-square.json. Owned reference
processes are stopped.

Android x86_64 pilot build completed successfully in 1m28s. Fresh lifecycle run
in temp/xl-classic-android-20261024-geometry passed 35/35 steps in 90.468s, wrapper
exit 0. It exercises disabled/enabled state, original mission load, difficulty
save/restore, stock Counterstrike with zero classic vertices/runtime inactive,
and return to Dragon Pit with 1737 vertices/runtime active. Final introspection
reports 2940 shaded vertices and four menu indicator draws. Screenshot was
visually inspected and shows the mine/D1 cockpit without an error dialog.
Captured logcat has zero ANR/fatal matches. Restoration hash comparison passes
byte-for-byte, and emulator-5554 app is stopped; emulator-5556 was untouched.

APK SHA256 is 9a675f1ecf0eb65501e65d902b546aa55720d7787ba51e62b250b568d2cfd86e.
EXTENSIONS.md and durable lighting evidence now reflect the refreshed checks.
Remaining work includes reusable lifecycle-runner registration, rendered scene
comparison and normal-input mission completion, plus the separate boss timing
and native-D1 parity gates. An inspected rendered frame is not a reference image
comparison or proof of normal combat/progression. Browser support stays gated.

### Reusable Android lifecycle test registered and verified

Promoted the 35-step lifecycle flow to
android/game_scripts/test_xl_classic_lighting_lifecycle.jsonc with explicit
_standalone=false and owner test_xl_classic_lighting_lifecycle. Added the owning
PowerShell runner, explicit test-suite registration and 360-second master timeout.
The runner uses shared device/physical-isolation helpers, requires an explicit
serial and pre-provisioned original mission/pilot APK, retains artifacts, backs up
state exclusively, rejects incomplete results and Android ANR/fatal errors, and
verifies byte-exact restoration before removing its backup.

Scoped formatting/lint passed. Both catalog validators passed: automation catalog
99 standalone JSON/433 support/267 standalone PowerShell, runner catalog 380
top-level entries. The registered runner itself passed on emulator-5554, 35/35
steps in 99.298s, wrapper exit 0. Restoration is byte-exact, logs have zero
ANR/fatal matches, and the captured rendered frame was visually inspected.
Artifacts are android/temp/xl-classic-lighting-lifecycle and
 temp/xl-classic-registered-run.log. App is stopped; emulator-5556 untouched.
The earlier requirement to promote the scratch lifecycle runner is now satisfied.
Rendered reference comparison, normal mission progression/completion and the
separate boss timing/native-D1 gates remain outstanding.

### Boss timing: reference dispatch and sound-slot distinction verified

Source inspection at reference commit
2576a98b5de310e3ac7c9777df2677d1627b8bae establishes that the D1 AI dying branch
in ai/d1_ai.cpp:1522 calls DoBossDyingFrame. That function in ai/aiboss.cpp:354
calls the shared DoRobotDyingFrame in ai/aianimate.cpp:211. Therefore the old
constant present in the reference D1 source does not establish its death-sound
scheduling behavior. The non-D1 AI path also calls the shared boss function.

The shared scheduler reads nLength[bHires] divided by audioSampleRate, with a
one-second fallback for zero duration. LoadHiresSound in gameio/loadsounds.cpp
fills nLength[0] with the WAV file byte count and sets bHires=1. SetupSounds
sets bCustom=0; audio playback indexes by bCustom instead. CSoundSample's
constructor initializes both lengths to zero. LoadSoundReplacements can populate
slot 1 from a separate DTX bank, so a fresh constructor plus WAV loading alone
would yield the one-second scheduling fallback, but a live mission's selected
slot and prior loading history still require observation. This is a conditional
source-derived prediction, not a measured reference timing result.

Our imported-D1 scheduler still uses the native fixed 0x2ae14 duration, while
the D2 scheduler calls piggy_sound_duration. Do not replace the D1 constant
merely because the WAV plays correctly, or copy the reference's apparent slot
mismatch as intended behavior. Next evidence needed: audio-enabled reference
run capturing the translated death sound, bHires/bCustom, both lengths and
configured sample rate; then observed sound onset and boss removal times.
Existing no-sound lighting captures do not establish those facts. Any scoped
duration correction must retain stock D1 timing without a mission override and
verify death playback, save/restore and mission-switch ownership. The support
gate remains closed pending this and the remaining gameplay/render checks.

### Audio-enabled reference capture resolves selected sound slot

Launched the original, hash-checked reference executable with -nosound 0 and a
separate copied square profile in temp/xl-dragon-reference-20261024/audio-timing.
Read-only process introspection verified the original mission's 1737 vertices
and 632 segments. Captured state.json reports D1 sound bank 1, low-memory mode 0,
logical sound 185 translated to sample 82 named boss02, bHires=1, bCustom=0,
nLength={482356,0}, and configured audioSampleRate=22050.

The compiled DoRobotDyingFrame confirms sample stride 592, bHires offset 9,
length array offset 12, selected sound pointer at gameData+0x43190e0, and sample
rate at gameOpts+0x634. Its duration branch at RVA 0x48bd0 reads the zero slot 1
and selects the 65536 fixed-point (one-second) fallback. This now resolves the
previous uncertainty about which length is selected for this fresh mission.
The 482356-byte slot 0 agrees with the original WAV file size, not PCM duration.
The capture script, state, private profile and reference log are retained in the
audio-timing directory. The owned process was stopped and its launch session
returned exit 0.

This is measured loaded state plus verified compiled scheduling logic; no boss
death event or audible onset was observed. It must not be described as verified
death-sequence parity. In particular, substituting the actual 120578/44100-second
WAV duration in our D1 scheduler would be an intentional correction relative to
this reference path, not an exact port of its current behavior. Next verify the
death sequence and object-linked playback lifetime before deciding the scoped
correction; keep native timing untouched for missions without an override.

### Boss one-shot lifetime traced; historical mixed audio is inconclusive

The actual dispatch path rules out a direct boss-removal cutoff: our
digi_link_sound_to_object3 takes its !forever branch and calls
digi_play_sample_3d before allocating any SoundObject. Reference
CAudio::CreateObjectSound likewise takes !bForever and calls PlaySound before
allocating a CSoundObject. Both boss death calls pass zero for that flag.
Object-linked cleanup is therefore not the lifetime owner of this one-shot.
The general cleanup paths do differ (plain SDL detaches a one-shot, SDL_mixer
stops it, and reference DeleteSoundObject stops its channel), but those paths
cannot establish truncation of this particular boss sound. Channel contention,
global audio stops and actual playback completion remain separate questions.

Analyzed the existing controlled death capture rather than treating its played
counter as completion evidence. temp/xl-dragon-android/correlate-death.py rebuilds
the exact unsigned-mono/sample-and-hold conversion from the original WAV and
matches the logged converted PCM FNV hash 8f74c6cf7077c67d. It subtracts the
captured music stem and searches waveform alignment against the mixed capture.
The resulting death-correlation.json has weak quarter-second correlations and
does not establish onset or complete playback; its offset is explicitly only
a candidate. This inconclusive analysis neither proves truncation nor closes
the audio gate. The old run also includes player death and other effects.

No timing correction is justified by the object-removal hypothesis. Preserve
native timing for now. A fresh isolated capture or channel-completion telemetry
must distinguish actual sample completion from successful playback submission;
measure game-time boss events independently of wall-clock capture timestamps.

### Fresh invulnerable death probe collected

Ran a fresh controlled probe on emulator-5554 with the current pilot APK
(SHA256 9a675f1ecf0eb65501e65d902b546aa55720d7787ba51e62b250b568d2cfd86e).
Artifacts: temp/xl-death-isolated-20261024. Automation passed 33/33 steps in
57111 ms, run ID 345e027fa3f74eedbda8d55cd3033548. The probe loads original
Dragon Pit with classic lighting enabled, asserts invulnerability after the
almighty cheat, stages the player near the boss, applies lethal scripted damage,
captures ten seconds, and verifies the player remains alive and invulnerable.
This is controlled audio evidence, explicitly not normal combat completion.

The death sound is submitted as sample 82 on channel 6 with matching installed
PCM and converted-buffer fingerprints. Capture contains 489984 stereo frames
at 48000 Hz with no overflow. Mixed-output SHA256:
e6696126071ca9c4e6271c7b321ef3f45a6d7bb17926d15b469adba0490e4e61.
The waveform search is still inconclusive; no full-playback claim follows.
Mixed output is non-silent and unclipped, but other effects remain present.

Automation passed, but the scratch wrapper's post-test export failed because
run-as could not write into /data/local/tmp. Its finally block stopped the app
and verified byte-exact restoration. Recovered capture files through binary
adb exec-out without rerunning the game. Corrected the scratch wrapper to use
that export path and the actual store-music.wav filename; syntax check passes,
but the corrected wrapper has not been rerun. Emulator-5556 was untouched.
Next use direct channel-completion telemetry or a validated full-waveform
alignment method; repeating an inconclusive sparse search is not verification.

### Full correlation locates actual mixed boss audio

Replaced sparse candidate selection with a full normalized FFT correlation over
every possible complete-sample offset. The synthetic validation injects a scaled
random template into independent noise at offset 157; the search recovers 157
with correlation 0.997044. The exact converted PCM fingerprint still matches
the engine's logged buffer. Scripts and results are in the same isolated-probe
directory: full-correlate.py, full-correlation.json, control-correlate.py and
full-correlation-controls.json.

The real capture's strongest full-sample alignment is frame 162304 (3.381333
seconds), exactly an audio-buffer boundary, consistent with the logged sound
submission about 3.37 seconds after capture began. Full-sample correlation is
0.278435 in the noisy mix. Quarter-second windows from 0.5 through 2.25 seconds
of the sample have correlations 0.163 through 0.612, with no higher correlation
among 300 randomly shifted controls per window. The 2.25-2.5-second window has
correlation 0.179 and one higher shifted control. Fitted gain is consistently
about 0.076-0.087 through the main body, supporting actual playback in mixed
output rather than mere submission or a coincidental isolated match.

The last 0.234 seconds fade to template RMS 538 (versus peak-window RMS 9411).
Their correlation is only 0.048, with 38/300 shifted controls exceeding it.
Thus the main body and late sound are evidenced, but the final fade remains
below this method's useful discrimination in the combat mix. Do not report
the candidate alignment or passing synthetic test as proof of complete playback.
This replaces the earlier sparse-search conclusion that no convincing waveform
was found, while retaining the distinction between measured mixed playback and
an observed natural channel completion. No game behavior changed.

### Mixer completion diagnostics distinguish a full WAV from interruption

Added debug-only D2 Android SDL_mixer completion tracking in the existing sound
trace module, exposed as sound_playback in introspection. It counts attempts,
natural completions, stop-request callbacks and failed starts per sample for the
process lifetime. Last callback elapsed milliseconds and converted byte count
are diagnostic evidence. Counters do not imply mission qualification, reset on
save/restore, or identify asset ownership by themselves; combine them with the
existing context/PCM fingerprints. Engine accesses use the SDL audio lock; the
completion callback performs no logging, allocation or engine-object access.
Stop and close paths are marked before the mixer halt, conservatively classifying
a completion racing with a stop request as stopped. Registration precedes play.

Android x86_64 pilot APK built successfully in 1m54s with no compiler warning/error
matches. SHA256: 279322eb2393651eda3afc72ddbdfaea22834c1affa8232e119c63711d08c368.

Controlled test in temp/xl-death-completion-20261024 passed 42/42 steps in
85050 ms, wrapper exit 0, run ID 297f30ccb23146cb90568768a128ebc7. First boss
death reached one natural completion with no stop request or failed start and
524964 converted bytes. Restoring a pre-death save and triggering it again,
then restoring during playback, resulted in attempts=2, completed=1,
stop_requested=1, failed=0; the stopped callback occurred after 397 ms.
App state restoration passed byte-for-byte and the app was stopped.

An earlier attempt timed out because field/value wait_for does not resolve the
new nested diagnostic path. Its failure state actually showed two natural
completions, the last taking 2739 ms. Changed the test to the supported expect
form; this was a test wait bug, not a missing replay after restore.

Promoted the corrected flow to test_xl_classic_sound_lifecycle.ps1/jsonc, with
support-script ownership, explicit suite registration and a 360-second master
timeout. Its wrapper exports binary captures through bounded base64 ADB calls,
requires an explicit serial and provisioned original package, and backs up and
verifies app state. Scoped checks pass after normalizing mixed PowerShell line
endings. The registered runner also passed 42/42 steps in 76998 ms, wrapper
exit 0, run ID 7669a69840174e25818fcb10c2ef50f7. Final sample 82 counters were
attempts=2, completed=1, stop_requested=1, failed=0; the intentional stop took
424 ms. Its 484352-frame stereo capture has no overflow, logcat has no ANR/fatal
matches, and restoration is byte-exact. Artifacts are in
android/temp/xl-classic-sound-lifecycle; emulator-5554 app is stopped and
emulator-5556 was untouched. Both catalog validators passed: automation catalog
99 standalone JSON/434 support/268 standalone PowerShell, integration catalog
381 top-level entries/434 support scripts.

This closes the missing evidence of full death-WAV consumption in the tested
Android mixer path, including restore and an intentional-stop negative control.
Preserve native D1 scheduling rather than copy the pinned reference's empty-slot
fallback. Exact reference death-event timing is not claimed. Normal-input mission
completion, rendered-scene comparison and native-D1 parity remain unverified.

### Ordinary-control entry flights verified

Two fresh original-mission runs use production joystick-axis injection only for
movement after normal new-game startup, with the classic-lighting profile enabled.
They contain no invulnerability cheat, pose/face teleport, forced door/trigger,
scripted damage or level-completion command. Both assert no invulnerability and
player survival, and finish at 100 shields.

temp/xl-flight-entry-20261024 passed 26/26 steps in 39648 ms: fly forward from
spawn segment 0 into segment 2, then strafe through 4 and 6 into 235. Final
position is approximately (-337.832,-63.523,20.411). This left corridor ends at
the gold-key door and is not the initial progression branch.

temp/xl-flight-door-20261024 passed 26/26 steps in 40153 ms: from a fresh spawn,
fly into 2 and strafe right through 3/5, the ordinary door into 10, and into 11.
Final position is approximately (-223.737,-63.926,20.554). Passage through the
door is driven by normal movement/collision, not a door-opening test hook.
Both wrappers exited 0, stopped the app, and verified byte-exact state restoration.
Emulator-5556 was untouched.

Next progression witness is the open-door trigger at segment 69 side 2. Static
key/lock-aware adjacency suggests 11->13->15->16->46->22->27->40->71->41->69;
crossing into 70 should open the door from 70 into 160. This remains a flight
hypothesis until verified physically, including collision clearance, trigger
activation and survival. Neither entry flight is full mission completion.

### Extended ordinary-control flight failed before trigger qualification

The scratch route extension in temp/xl-flight-trigger-20261025 first failed to
load because its PowerShell generator serialized a negative axis value as text.
Corrected those fields to numeric JSON and reran after verified state restoration.
The corrected run progressed through the entry door and several coordinate
waypoints, then timed out at step 40/46 waiting for position.z<29. The log records
"Ship destroyed!" at 00:04:25.760. Failure introspection reports player_dead=true,
control_type=0 and a changed forward vector in segment 142; that is not a valid
live-ship endpoint for evaluating the planned route through segment 41.

The run therefore does not prove trigger 0 activation, a blocked passage or an
engine compatibility defect. Coordinate-only waypoint checks were too weak to
separate navigation drift, combat and death-camera movement. Before extending
the flight, add live segment/orientation/survival checkpoints at each turn and
inspect the first divergence. Do not resolve this by restoring invulnerability,
teleporting across the passage or opening its door with a debug command. The
failed wrapper still stopped the app and verified byte-exact restoration.

### Live checkpoints separate incoming damage from a poor approach

The corrected checkpoint probe in temp/xl-flight-checkpoints-20261025 records
the live ship in segment 13 at 100 shields, segment 16 at 99.6 shields, and
segment 46 at 72 shields. At all three stops its forward Z component is 1 and
player_dead is false. It then dies before the next coordinate target. These
observations establish normal-control access through segment 46 and incoming
damage during the route, not a compatibility failure or trigger activation.

A faster approach probe in temp/xl-flight-approach-20261025 instead stopped
alive in segment 15 at (-176.738,-67.359,29.443), still facing +Z, with 15.46
shields. The axis probe shows a nonzero processed slide command, so this is not
evidence that the lower approach input was swallowed by a deadzone. The planned
trajectory is close to the narrow portal edges: segment 13's outgoing opening
spans Z=21.025..31.025. The earlier zero-thrust coast left Z around 29, poorly
centered for the ship's collision sphere. This motivates centering the approach
with normal thrust, not changing collision geometry or bypassing the passage.

Both failed wrappers stopped the app and restored state byte-for-byte. A first
checkpoint attempt also exposed an unsupported ge assertion operator; the
corrected scripts use gte and validate operator names/numeric axis values before
launch. No failed setup run is counted as gameplay evidence.

The follow-up in temp/xl-flight-centered-20261025 did not achieve a centered
crossing either. Its staged throttle reduction still coasted to Z=27.6 before
the lateral leg. It finished alive in segment 15 at
(-173.206,-66.930,31.183), forward vector (0,0,1), 39.18 shields, and nonzero
processed slide input, after timing out before X=-160. State restoration was
again byte-exact. Do not call this a centered-ship collision regression: the
actual trajectory differs from the intended centerline.

The next route driver should correct lateral and vertical error using live
position and velocity, through ordinary controller inputs, rather than chain
one-coordinate thresholds with uncontrolled coasting. Keep the native collision,
door, trigger, damage and resource paths active. Existing route_confirmation is
not a substitute for this evidence: it drives a companion, protects the player,
and has direct damage/interaction hooks. Its outcomes remain route-analysis
evidence, not ordinary-player mission completion. No mission support promotion
follows from these flights.

## Ordinary-input feedback steering (2026-10-25, implementation pending runtime validation)

The timed-axis trials above do not establish a collision defect: their measured
trajectories drifted toward the narrow portal edges. Added debug-only `fly_to`
to the Android D2 automation dispatcher to make the next experiment reproducible.
It accepts a target segment, x/y/z and bounded timeout (1..60000 ms). Every frame
it reads position, velocity and orientation, projects error into ship-local axes,
and publishes bounded translation input through the production joystick mailbox.
The mapping and inversion come from the current controller configuration. It
requires three distinct translation axes and foreground, live single-player
play without replay. The target must be inside its declared segment. Arrival
requires the correct segment, distance below 0.5 units and speed below 0.5 units/s.
Success, death/state failure, timeout and script replacement release owned axes.
It does not write ship position, orientation, velocity, shields, walls or triggers.

Scratch opening-route experiment: `temp/xl-flight-feedback-20261025/probe.jsonc`
and its state-preserving `run.ps1`, on emulator-5554 only. The initial build found
C++ list-initializer narrowing from the engine f2fl macro; calculations were
changed to double. Scoped formatting passes. Build retry and actual gameplay
validation remain pending; this action is not yet qualification evidence.
The prior branch-origin question was read-only, not progress toward this gate.

### First feedback run result

The corrected Android build passed in 1m23s (both D1 and D2), with no compiler
warnings/errors in build-fixed.log; the scoped formatter passed. Installed on
emulator-5554. The 34-step opening-route experiment failed honestly at step 19,
the first fly_to, after its 15-second deadline. Run ID:
bfdc36f53e714f50be019346ef2e0445. Final live state: segment 2, position
(-282.59454, -61.62102, 20.49640), speed 2.54802, shields 100, forward +Z.
Target was (-282.59, -62.16, 21.03). Thus normal input reached the intended
segment but did not satisfy both 0.5-unit / 0.5-unit-per-second settling limits.
The last input sample was processed; this is controller calibration evidence,
not a collision or mission compatibility failure. The current gains need live
trajectory sampling and calibration against the configured axis response before
trying the narrow corridor. Do not relax qualification based on this result.
The wrapper exited 1, verified byte-for-byte state restoration, and force-stopped
the app. No build or test session remains active. Logs and result are in the
scratch folder above. The action remains experimental and is not a reusable
registered integration test until its positive and negative controls pass.

### Trajectory diagnosis and controller deadzone (2026-10-25)

Added debug trajectory samples at 100 ms cadence to fly_to. The unchanged-gain
rerun `temp/xl-flight-trajectory-20261025` built successfully (2m02s, no compiler
warnings/errors), then failed at the same first waypoint: run
144cde373afa4cacb8b2843906fe1d1d, step 19/20, elapsed 51405 ms. Restoration was
byte-exact and the app stopped. Its 128 samples establish a specific input-driver
problem: during the final five seconds, all 43 samples had z=20.4939 and zero
forward thrust, 0.5361 units short of the requested z=21.03. Vertical position
ranged -62.3563..-61.9497 and speed 0.0215..1.3234. The stationary forward offset
is consistent with the configured throttle deadzone, not a collision obstacle.
Normal PF_WIGGLE adds vertical velocity in d2/main/controls.c; do not turn that
off or classify all vertical velocity as unwanted navigation drift.

The next driver revision converts desired command magnitude into actual stick
deflection using the current JoystickDead setting and the integer quantization
in joy.c / kconfig.c. It preserves those settings and still uses the production
joystick path. This is test-driver calibration, not added mission support. The
0.5-unit distance and 0.5-unit/s acceptance limits are unchanged. All usable
quantized inverse commands were checked against the deadzone arithmetic; live
validation is in `temp/xl-flight-deadzone-20261025`. Its outcome is pending.

### Deadzone correction and original narrow passage results

The calibrated build passed in 1m42s; formatting and diff checks passed.
`temp/xl-flight-deadzone-20261025`, run c066ecbcaa694ecf84e74f671e9f2531,
completed the first three waypoints (segments 2, 3, 11), including the ordinary
door, then timed out at step 25/34. That waypoint (-222.59,-62.16,26.03)
lay approximately on the sloping segment 11/13 shared boundary. This was a
waypoint-design failure, not evidence that the corridor cannot be crossed.

Retried the same APK using segment 13's interior center x=-207.59, with the
other coordinates and every arrival limit unchanged. Scratch:
`temp/xl-flight-interior-20261025`, run 88bef2304e1f4e039e78bc778de08036.
Waypoints into 2, 3, 11 and 13 passed at elapsed 37150, 41115, 46647 and 50155 ms.
The next waypoint toward segment 15 timed out at step 27/34, elapsed 65234 ms.
Final live ship: segment 15, (-192.00299,-62.41061,25.64795), shields 56.79053,
speed 2.14520, forward +Z, physics flags 75. It entered the target segment near
the portal but did not reach the requested interior (-182.59,-62.16,26.03).
This is intentionally a failed waypoint, not a partial-position pass. No direct
movement, invulnerability, combat disablement, door opening or damage hook was
used. Both runs restored state byte-for-byte and stopped the app.

Next investigation: measure actual player collision radius and contact geometry
at the 10-unit-deep opening (z=21.02518..31.02518), alongside the live trajectory
and steering commands. The observed z~25.65 remains below its midline; distinguish
insufficient centering and collision response from missing D1 semantics before
changing engine collision or accepting this path. The existing normal-input
trace through segment 46 remains separate evidence, not proof that this new
controller can negotiate the opening consistently. No build/test session remains
active. Full original mission completion, rendered comparison and the browser
qualification gate remain outstanding.

### Measured ship clearance and possible robot contact (2026-10-25)

Added read-only position.radius and velocity_x/y/z introspection fields, available
in both engines. Scoped formatting passed; Android D1/D2 build passed in 1m40s
without compiler warnings/errors. Repeated the unchanged interior-waypoint route
on emulator-5554: `temp/xl-flight-clearance-20261025`, run
f626540761e9486a9fa806e0619c99e3. It again timed out at waypoint step 27/34
(elapsed 73140 ms). App state restored byte-for-byte and app stopped.

Actual player radius is 4.7351837158203125. The original portal's Z endpoints
21.025177001953125 and 31.025177001953125 leave nominal sphere-center clearance
25.760360717773438..26.2899932861328125, only 0.52963 units wide. A 0.5-unit
arrival sphere at the preceding waypoint does not guarantee safe Z centering.
The earlier stalled z~25.65 was outside this nominal range. This calculation
alone does not prove a valid swept-sphere path through neighboring geometry.

This repeat ended at (-197.52831,-62.78029,25.89987), segment 13, shields66.29906.
Robot64/id1 was at (-191.57402,-60.89029,17.91937), segment14, radius5.62935.
Euclidean center distance is10.13479, versus full radii sum10.36453. The previous
introspection distance is a quick magnitude approximation, so it must not be used
as an exact overlap test. Contact is now a plausible competing explanation.
`check_vector_to_object` in d2/main/fvi.c reduces robot radius for attack_type
robots; that effective value and actual FVI contact were not captured, so do not
claim this snapshot proves robot collision. Saved `clearance-evidence.json` has
the full measured state and bounds. No collision engine changes were made.

Next: capture the contact kind/object/side during ordinary flight, and handle any
obstructing robot with ordinary aim/fire input. Do not disable it, delete it or
use scripted damage to make the route pass. Tighten pre-portal alignment where
required by the actual clearance. The earlier passage failures remain navigation
and combat investigations, not diagnosed mission incompatibility. Full mission
completion and rendered-scene qualification remain open; browser support gated.
No build/test sessions remain active.

### Confirmed robot obstruction and first ordinary combat (2026-10-25)

Added game_automate_observe_flight_collision, called by a 10-line guarded Android
INTROSPECT_ON hook in D2 physics. It is read-only and logs only while fly_to runs;
repeated identical contacts are sampled at 100 ms, changes immediately. The
fly_to action is D2-only; the shared implementation also compiles in D1. Scoped
formatting and both Android engine builds passed (1m40s, no compiler diagnostics).

Unchanged-route probe `temp/xl-flight-contact-20261025`, run
f7f6d9abae214909831d438ee7772202, failed step27/34 after74246ms. During the stalled
waypoint it recorded43 contacts with robot65 and35 with robot64, both type2/id1,
and no wall contacts. These are sampled observations, not total contact counts.
The final stationary ship was inside segment15 at approximately
(-188.899,-61.9999,26.1038), still commanding lateral thrust. This directly
identifies robot obstruction in this run; do not change wall collision to fix it.

Then tested ordinary yaw input and primary fire (mapped button100) before and
through the passage. No new engine change or special weapon/damage setup.
`temp/xl-flight-combat-20261025`, run215e76d62130494f9f5c318330c4c113, passed
waypoints into segments2,3,11,13,15,16,46, with3 actual robot kills, then died while
moving toward22. Failure at step42/45 after82838ms; final player shields-5.06830,
energy59.5, invulnerablefalse, player_deadtrue. The death-camera ConsoleObject
reported shields100: use player.shields and player_dead for survival conclusions,
not the death-camera object's shields. Both wrappers restored app state exactly
and stopped the app; no build/test session remains active. Machine-readable
contact-summary.json and combat-summary.json are preserved in their run folders.

This resolves a concrete mistaken navigation assumption: the passage need not be
made passable by a compatibility patch. It is physically traversable using normal
controls and combat in the imported original mission. It does not qualify the
whole mission. Next work should improve ordinary aiming/combat against remaining
robots before extending toward trigger0, keeping Hotshot difficulty and normal
survival rules. Generic controller-based targeting may replace the fixed yaw
sweep; it must never set orientation, delete robots or apply scripted damage.
Full completion and the separate rendering/qualification gates remain open.

### Ordinary aim tracking trials (2026-10-25)

Added debug-only Android D2 attack_object: initially validates a live robot slot,
tracks its signature, reads current direction/rotation velocity, and injects
mapped yaw/pitch axes and primary fire. Fire requires cosine>0.99. No direct
orientation, position, damage or enemy changes. Timeout/death/state failure and
completion release owned axes/fire. Removal/destruction ends the action; separate
kill-count/identity evidence is still required. It is experimental automation,
not a mission compatibility implementation or registered passing integration test.
Scoped format/diff checks and Android D1/D2 build passed (1m33s, no diagnostics).

First test at segment15: temp/xl-flight-tracking-20261025,
run839ec488ac6a4df2b4a1a31c884a69b7. Failed step39/50 at77562ms with player death.
Tracking started at73235ms. Robot75 already had15 shields at its FIRST sample,
and every tracking sample retained15. There were7 firing samples among28.
The prior fixed sweep, not this tracker, had damaged it from65. An intermediate
commentary incorrectly attributed that damage to tracking; corrected immediately
on reading the first sample. No tracker hit or kill was demonstrated.

Moved the same action earlier, to segment13, using the same APK:
temp/xl-flight-early-tracking-20261025, run5fcb8869ba4f43a3bed9cbd72718ceb0.
Failed step37/50 at80222ms, target still alive after15s. Here robot75 was about119
units away late in the action, rather than near the corridor. Alignment and firing
were sustained, but its shields remained65. A fixed robot slot is therefore not
a reliable nearby threat selection strategy. This run does not prove whether
misses were due to occlusion, motion or shot geometry: no firing-path probe was
captured. Both tests restored state byte-for-byte and stopped the app. Their
tracking-summary.json files preserve first/last samples and shield/fire ranges.

Next: select a nearby live robot with a clear actual firing path, and record that
path test with aiming/shot evidence. object_to_object_visibility exists in
laser.c, but inspect its flags before using it: homing's FQ_TRANSWALL visibility
must not be mistaken for a laser-passable opening. Keep target selection and
normal input distinct from gameplay mutation. Retain the current survival and
full-completion gates; no browser promotion. No build/test sessions remain active.

### First verified controller-tracked combat (2026-10-25)

Extended attack_object with object=-1 selection of the nearest live non-companion
robot within distance (default80, bounded300) whose center ray is unobstructed.
Uses find_vector_intersection with no transparent-wall bypass, no object checks,
and zero ray radius. This is conservative center-line geometry for selection,
not proof of either gun-muzzle clearance or projectile impact. Each firing sample
also checks that ray; success still needs separate actual-damage/kill evidence.
Legacy FVI p1 is mutable, so the probe passes a local endpoint copy after the
first build flagged a const-pointer mismatch. Corrected build passed in1m19s;
scoped formatting/diff checks passed without compiler diagnostics.

Fresh original-mission probe temp/xl-visible-target-20261025 passed30/30 in66563ms,
run d977f3fcd9f94e05895b7f7d0d2a247a. It first asserted zero robot kills, then
selected object64/signature65 in segment13's vicinity. First tracked sample:
distance14.0878, target shields80, player shields100, center_ray_open1. Last
sample before destruction: distance9.0372, target shields10, cosine0.99825,
center_ray_open1, fire1. Final state has robot64 absent, robot65 still alive,
robots_killed_level1, player shields87.62846, energy97.5, invulnerablefalse and
player_deadfalse. Thus this run demonstrates an actual normal-controller kill,
not just aim alignment or a removed target slot. combat-evidence.json preserves
the correlated records. Wrapper passed, restored state byte-for-byte and stopped
the app. Full original-mission qualification is still separate.

A longer follow-up, temp/xl-visible-route-20261025, uses two nearby-target fights
before the segment15 waypoint and one after it, then continues toward22. Its
outcome is pending; do not extrapolate the single-kill proof to route completion.

### Longer route exposes fixed combat-stop assumption

The longer visible-route run did not reach its first combat action. It failed at
fly_to step25/39 (toward segment13), elapsed66497ms. Collision observations at
that waypoint identify robot64 intercepting the moving ship before its scheduled
combat stop. See temp/xl-visible-route-20261025/result.json and contact-summary.json
for run identity and sampled contact counts. The wrapper failed as expected,
restored state exactly, and stopped the app. No build/test sessions remain active.

This is a scheduling limitation of the normal-input test driver, not a diagnosed
mission feature failure. Moving a fixed combat stop repeatedly is not a robust
solution: robots approach during travel. Next implementation should share the
verified controller aim/fire logic with fly_to so it can react to nearby visible
threats while retaining normal position steering, collision, energy, damage and
survival. Keep completion dependent on waypoint arrival and explicit gameplay
assertions. The successful single-target test remains valid evidence; the longer
route still has no passing result. Full mission and rendered-scene gates stay open.

### Combined ordinary flight and combat (2026-10-25)

Extracted center-ray selection and normal yaw/pitch/fire driving into shared
helpers used by attack_object and optional fly_to combat:true. Translation and
aiming must have distinct mapped axes. All owned axes and primary fire are
released on completion/failure. Combat acquires a nearby visible non-companion
robot during movement, retains it within a waypoint until death/removal or loss
of the conservative center ray, and uses unchanged normal inputs. No direct
orientation/movement/damage edits, changed difficulty, invulnerability or
collision bypass. Scoped format/diff checks passed; Android D1/D2 build passed
in1m35s without compiler diagnostics.

Original-route probe temp/xl-reactive-flight-20261025, run
0a40b2f17f8145b5a46065ec39b3baa6, failed step27/35 at74030ms with player death.
It entered segment15 but did not meet the settled-arrival requirement. Final
player shields-1.77045, one credited kill, invulnerablefalse. Sampled target
shields reached20 for robot64,20 for65,35 for75. Robot75's first sampled shields
were65 and later35, so this trace demonstrates damage during combined steering;
only one kill was credited overall. Do not infer three kills from three targets.
combat-summary.json preserves per-target first/last/shield/fire observations.
Restoration was byte-exact and the app stopped. No build/test sessions remain.

Remaining driver issues visible here: aim lag on moving target75 (last sampled
cosine0.95255 below0.99 firing threshold), and target retention resets at each
waypoint, potentially leaving a wounded robot alive while selecting another.
Next inspect relative target/ship angular velocity and preserve valid target
continuity across waypoint transitions before extending the route. Any tracking
compensation must still generate normal input, not set orientation or velocity.
The observed lag is a test-driver finding, not a missing XL mission feature.
The successful standalone tracked kill remains a bounded positive control;
combined-flight survival and full mission completion remain unverified.

### Relative-motion aiming and cross-waypoint retention (2026-10-25)

The input controller now derives pitch/yaw direction rates from target minus
ship translational velocity in the ship basis, expressed in turns/second. Adds
those rates to the velocity-error damping term, capped at1 turn/s before the
existing0.6 stick-command cap. Physics.c confirms rotvel is in turns/second
(tangles=rotvel*FrameTime). No game velocity/orientation values are written.
1000 independent finite-difference checks of the derivative equations passed,
maximum error9.13414e-7 turns/s. A valid target now survives waypoint transitions
within the same script/level; signature, liveness and ray checks remain in place.
Scoped formatting/diff checks passed; Android D1/D2 build passed in1m44s without
compiler diagnostics.

Same-route run temp/xl-reactive-rates-20261025,
1fbe7da5fdfb47ae8aa8d8664feec017, failed at step29/35 after69104ms with death in
segment16 before settled arrival. Final player shields-0.03596, four credited
kills, invulnerablefalse. Compared with the previous run's one credited kill,
this is improved observed progress, not a controlled proof of either change's
individual effect. Sampled targets64,65,74 reached shields20,10,5 respectively;
actual credited kills were4, so do not infer exact kill identity solely from
sampled shield minima. Target75 retained65 shields across its final6 samples,
with no fire. It was acquired when player health was already nearly exhausted.
That short last engagement is not sufficient evidence of persistent tracking lag
or a pitch sign/gain defect. An initial commentary focused on upward aiming;
corrected after inspecting the full target timeline. combat-summary.json records
the full first/last/rate/fire observations.

Wrapper restored app state byte-for-byte and force-stopped the app. No build or
test sessions remain active. Next investigate survival tactics and ordinary
pickup opportunities along this route, rather than blindly retuning rotation
from the final dying frames. Evasive movement and shield/energy collection must
still use normal collision/input; no resource grants or physics changes. Full
mission completion, rendering comparison and browser admission remain open.

### Authored starting supplies and item-identification correction (2026-10-25)

Added read-only level_scene.powerups (object/id/segment/position/radius) so actual
pickup locations/removal can be checked. Formatting passed; D1/D2 APK build passed
in1m37s. Original static audit puts shield object16 in segment7 and item26/id11
in235, both reachable off the first junction. I initially misidentified id11 as
a Vulcan cannon in commentary. The runtime inventory assertion rejected that
assumption, and direct inspection of both engines' powerup.h corrects it:
id11=POW_MISSILE_4; id13=POW_VULCAN_WEAPON. The actual Vulcan is object31 in589,
position(-44.51149,-62.15762,-120.97519). Do not treat the failed weapon assertion
as an engine incompatibility or claim a starting Vulcan pickup.

Probe temp/xl-authored-supplies-20261025, run0467f54a2a584522b10cc1b5b2575d0e,
failed step28/45 at63319ms because primary_weapon was0 instead of the erroneous
expected1. Actual pickups succeeded: player.shields112 (from100), concussion
ammo9 (from5), powerup objects16 and26 absent; primary flags remained1 and Vulcan
ammo0. pickup-evidence.json captures these facts and the source-ID correction.
The wrapper restored state exactly and stopped the app.

Next probe temp/xl-authored-missiles-20261025 corrects inventory expectations and
uses the nine owned concussion missiles through normal secondary-fire input.
Optional fly_to secondary_fire enables the configured secondary button only
when center ray is open, cosine>0.995 and target distance30..80; the engine still
owns ammo/cooldown, collision and damage. Both fire buttons are released with
owned axes on completion/failure. No weapons/health/ammo granted. This is normal
use of authored equipment, not a new compatibility exception. Outcome pending.

### Zero-shield robot liveness correction (2026-10-25)

The authored-supplies retry first rejected a numeric assertion because plain
112 compared against serialized 112.0; exact numeric range [112,112] fixes the
probe without relaxing the expected value. The subsequent numeric retry,
temp/xl-authored-missiles-numeric-20261025, run006a02b2ca4f4664872bada3df83d28a,
passed the pickups (112 shields, nine concussion missiles), then timed out on
step34/44 approaching segment13 after96437ms. Robot64 remained collidable at
exactly zero shields; credited kills remained0. Player shields99.37529, ammo9.
No secondary-fire samples were issued: do not claim missile use from this run.
State restoration was byte-exact and the app stopped.

Both engines' collide.c kill robots only below zero shields. The new driver
incorrectly excluded zero-shield targets, and level_scene.robots.alive repeated
that error. Corrected target acquisition, retention, attack completion, and the
read-only alive diagnostic to match the existing engine rule. Game damage rules
are unchanged. Scoped formatting and git diff --check passed. Build and normal
route rerun evidence will be recorded below; this correction alone does not
qualify the mission or demonstrate completion.

Corrected D1/D2 APK build passed in1m15s. Ordinary-input route retry
(temp/xl-zero-shield-combat-20261025,
run d14bae3501414b6daca242171007ed4b) PASSED44/44 in102675ms.
Robot64 remained targeted and firing continued at exactly0.000 shields in two
samples (85576/85680ms); unlike the previous driver, it was not abandoned.
Final state: segment22, four credited kills, shields84.74063, energy91,
concussion ammo3 (nine after pickup), invulnerablefalse, player alive.
Objects64/65/74/75 were absent from the final robot list. Secondary-fire samples
and ammo consumption establish use of normal missiles; do not attribute each
kill to a specific weapon from this evidence. combat-evidence.json records the
bounded result and zero-shield samples. State restored byte-for-byte, runner
exited0 and force-stopped the app. No test/build session remains active.

This clears the previously failing opening route, not full mission support.
Next extend ordinary flight through22/27/40/71/41/69/70 toward160, observing
trigger0 and its authored doors. Keep the original mission, Hotshot difficulty,
normal collisions, actual inventories and unmodified damage. Full normal-input
completion, rendered lighting comparison and separate native-D1 parity remain
open; browser admission is unchanged.

### First-trigger extension exposed survival variability (2026-10-25)

Extended the same ordinary-input probe through27/40/71/41/69/70/160, including
a segment70 side2 door-state assertion after crossing trigger0. Static audit
links trigger0 to70/2 and68/2; wall12 at69/2 carries it. This is a test of the
authored route, not a direct trigger invocation or wall-state edit.

Run41e4a1208feb4c3cac8e9aa23c7b23cb in
temp/xl-first-trigger-flight-20261025 failed38/61 after99119ms: player died
approaching16, before any extension steps. Final shields-5.34364, three credited
kills, concussion ammo7, energy91, invulnerablefalse. Wrapper restored state
byte-for-byte and exited1; app stopped. No trigger/door claim follows from this
run. The prior44/44 pass remains a bounded success, not repeatable survival.

Comparison in combat-summary.json: first target64 took about7.65s of sampled
tracking versus4.50s in the pass, with player health112->63.656 versus112->108.863.
The pass then gained24 health during target65 engagement (ordinary pickup;
precise dropped-item identity not established here), while this run lost health.
Target65 had seven secondary-fire samples in the pass and none in this run;
final ammo confirms six versus two missiles consumed overall. These observations
do not isolate an engine defect or any single cause of the survival difference.

Driver currently aims at target position with angular-rate compensation, not
projectile interception. Center-ray openness does not guarantee a clear muzzle
path or a projectile hit. Next isolate early engagement tactics/projectile
tracking before changing gains or claiming the new corridor tested. Any aiming
prediction must use actual weapon speed and normal stick/fire input; no changes
to mission geometry, health, enemies, damage, or difficulty. Full playability,
rendered comparison and native-D1 parity remain open; browser gate unchanged.

### Projectile-aware ordinary aiming (2026-10-25)

The debug-only combat helper now predicts a constant-velocity laser intercept.
The weapon is selected with do_laser_firing's laser-level mapping and its actual
difficulty speed; Laser_create_new confirms lasers do not inherit ship velocity.
Prediction applies only to constant-speed, non-thrust lasers, target speed below
projectile speed, positive intercept time below2s. Other cases keep direct aim.
Implicit differentiation supplies the predicted aim-point velocity for existing
stick damping. The helper writes only normal input, not projectile/ship/robot
state. Missiles still share the aim direction; this does not model their separate
flight speed. Current-center ray visibility still does not prove muzzle clearance.

1000 seeded numerical interception/derivative checks passed; maximum finite-
difference vector-derivative error5.92508e-6. Formatting and diff checks passed;
D1/D2 APK built in1m31s. temp/xl-projectile-aim-20261025 holds math evidence and
the unchanged61-step first-trigger route. Gameplay outcome pending below.

Prediction run24efecff7a3546c28065ed49818509a9 failed38/61 after101795ms,
with death approaching16. Final shields-0.57864, credited kills5, ammo6, energy88.
The live logs confirm nonzero lead_seconds and actual target shield reduction,
but this single run does not establish prediction improves survival. Target64
sample interval4.13s, player112->105.970; target65 player105.730->96.892;
target75 player96.545->62.844; target74 player62.844->2.421. Only four target
identities were sampled, so do not assign all five credited kills from these
samples. No trigger-extension steps executed. State restored byte-for-byte,
runner exited1 and app stopped. combat-summary.json preserves the observations.

A topology/powerup follow-up found a useful authored alternative: laser upgrade
object22/id3 at segment588 (-212.59431,-62.15767,61.02518), reachable via
13->311->588 before15. Segment311 center(-212.59431,-62.15765,41.02518);
connecting doors101/102 are not locked in the original audit. A conservative BFS
excluding locked/keyed doors found this as the sole remaining reachable shield,
laser/quad or primary-weapon pickup in the final snapshot. This is topology,
not verified navigation or pickup. Next add this detour after13, assert actual
player.laser_level increases, return through311/13, then retry the trigger route.
This tests use of the authored arsenal rather than granting weapons or weakening
enemies. Existing speed/position arrival tolerances remain unchanged; brief
settling near16 was observed but is not established as the survival cause.

### Authored laser detour verified, progression still incomplete (2026-10-25)

Probe temp/xl-authored-laser-20261025,
run d172fc85a9984d07ad4dc44dfe08de9d, verifies ordinary flight from13 through311
and door101/102 into588, pickup of laser object22, and return through311/13.
Explicit inventory assertions passed: laser_level0 before detour and1 after
pickup. Object22 is absent from the final powerup list; no inventory grants or
wall edits were used. pickup-evidence.json preserves that bounded evidence.

The full72-step route failed51/72 at116222ms while climbing from16 toward46.
Final player shields-0.51883, laser_level1, two credited kills, concussion ammo4,
energy86.5, invulnerablefalse. State restoration passed byte-for-byte; runner
exited1 and stopped the app. The pickup is verified; a survival benefit and
trigger0 behavior are not established by this run. Normal mission completion,
rendered lighting comparison and native-D1 parity remain outstanding.

Keep the verified pickup detour available for subsequent ordinary-input route
work. Do not relax arrival/survival assertions to convert this failure to a pass.
Next inspect engagement timing and projectile/cover geometry around15/16 with
the earned weapon: current-center ray visibility remains weaker than gun-muzzle
clearance. Any tactic changes must preserve original geometry, difficulty,
resources and enemy behavior. The plan's opening status has been refreshed to
include both the previous successful segment22 run and later survival failures.

### Actual muzzle/creation observations (2026-10-25)

Added read-only flight_shot events around the existing D2 player muzzle FVI and
Laser_create_new calls, only for ConsoleObject and an active fly_to/attack_object
script. Records weapon, gun, actual FVI fate/segment and created object slot.
Core change is12 guarded lines (Android INTROSPECT_ON); shared diagnostics remain
in android/. D1 has its own equivalent firing routine, inspected for separation;
these automation actions currently run only in D2, so no D1 hook was added.
No shot acceptance, collision, timing, damage or input decisions are changed.
Formatting and diff checks passed. The same72-step authored-laser route will be
used to test the muzzle-obstruction hypothesis. Creation proves launch only,
not that the projectile later reaches or damages the target.

D1/D2 APK with muzzle observations built successfully in1m46s. Run
1ec7da086ef248799fe954e7cc5be1d4 (temp/xl-muzzle-evidence-20261025) failed53/72
at119773ms, after entering22 but before settled arrival. Player shields-0.61581,
laser_level1, credited kills5, concussion ammo3, energy87.5, invulnerablefalse.
Wrapper restored state byte-for-byte and exited1; app stopped.

All56 observed muzzle checks returned HIT_NONE; each was followed by successful
projectile creation (56/56). No blocked muzzle or allocation failure occurred.
muzzle-evidence.json records counts by step, actual result and final state.
This rejects muzzle obstruction as the cause for this run. It does not prove
clear subsequent projectile paths or hits, and does not establish repeatable
survival, trigger0 activation or full completion. Next investigate actual
projectile impacts/engagement geometry beyond the muzzle, rather than changing
muzzle acceptance or attributing the failure to unsupported mission semantics.

### Visible reference capture with explicit presentation diagnostics (2026-10-25)

Work resumed the independent rendered-scene gate; ordinary-input completion is
still outstanding. temp/xl-rendered-reference-20261025 contains an isolated copy
of the square1024 profile and hashed diagnostic manifests. The pinned original
reference executable remains unchanged. First diagnostic inherits the documented
visibility-clear correction and changes only AutoScreenshot's interval load to
index1 (one second). SHA256 d12f3c22c227308df979cb0fe3630ee367648ff2bd3f3e8e23bdc5f9a3ddc1d6.
It generated16 correctly sized1024-square TGA files, all entirely black. These
are rejected as rendering evidence despite the loaded geometry and screenshot
files. The built-in path reads GL_FRONT after SDL_GL_SwapBuffers in a hidden
window; source inspection identified presentation readback as a hypothesis.

A separate backbuffer diagnostic retains those changes, skips the two compiled
swap calls in COGL::SwapBuffers and reads GL_BACK in SaveScreenShot. Exact RVAs,
before/after bytes and executable hashes are in backbuffer-manifest.json.
This captures the composed frame without presenting it; it is not a claim of
unaltered final presentation behavior.20 captures were produced, with visible
nonblack imagery. Visually inspected back-scrn0019.png: original mine geometry,
red/brown colored textured surfaces, cockpit, reticle and blue powerup are present.
The black first diagnostic is therefore not evidence of absent scene rendering.

Both exact owned executable/PIDs were verified and stopped; launching sessions
exited0. capture-evidence.json preserves counts, black-frame checks and hashes.
No gameplay progress or audio claim follows (reference audio disabled). The old
640x480 port smoke capture has a different viewport and obstructing foreground
sprite, and predates current fixes; it is unsuitable for pixel parity. Next build
and capture the current port with matched camera, viewport, lighting and texture
filter settings, then compare scene/material regions. Rendered parity remains open.

### Current native port square capture (2026-10-25)

Rebuilt xl-render-preview.exe from the current worktree in xl-runtime-engine;
all28 incremental build steps passed. Isolated run temp/xl-current-render-20261025
uses the original mission/HOG and retail data, a1024x1024 window, square aspect,
TexFilt1 and port gamma0. Reference gamma3 is neutral (Brightness=0.25+gamma/4);
do not match raw gamma setting numbers across engines. Capture hashes, executable
hash, settings and limitations are in capture-evidence.json. The exact owned
preview process was checked and stopped; launch session exited0.

Visually inspected current screenshots/scrn0000.png alongside diagnostic
reference back-scrn0019.png. Both show the same spawn geometry, colored textured
surfaces and blue powerup; the old foreground obstruction is absent in this
capture. There is a small visible vertical scene offset and different cockpit/HUD
assets. This is a usable current comparison pair, not proof of pixel parity.
Camera pose/frame timing (including normal ship wiggle), texture filter equivalence
and viewport/cockpit mapping must be measured before attributing per-pixel errors
to lighting. Next establish those controlled view parameters, then compare
material/geometry regions independently of HUD and animated sprites. Full scene
and gameplay qualification remain open; browser gate unchanged.

### Camera telemetry identifies unmatched view parameters (2026-10-25)

Developer preview now writes xl-preview-camera.json immediately before its
30-frame screenshot, recording fixed camera/player position, orientation, zoom,
canvas/screen dimensions and gamma/filter options. Read-only; no rendering or
physics inputs changed. Scoped formatting/diff checks and the native preview
rebuild passed. Capture artifacts are in temp/xl-camera-profile-20261025.

Current port: screen1024x1024, gameplay canvas1024x682, identity orientation,
zoom36864, matrix scale[43648,65536,36864], position
[-18520117,-4028797,-1243530]. These are fixed-point values, not rounded pixels.

Reference live transformation symbol RVA0x4a8520, read with existing validated
class layout: identity orientation, zoom36864, scale[43712,65536,36864], viewport
[0,341,1024,683], position[-18520117,-3761853,-1243530]. The read is explicitly
not synchronized with a screenshot frame. The initial accidental read of the
precomputation projection at RVA0x1db72e0 is retained as
precompute-camera-rejected.json, not used as gameplay-camera evidence.

Thus camera Y and gameplay viewport height differ; identical square output sizes
are insufficient for pixel comparison. The reference profile exposes
extraGameInfo[0].bWiggle=1. Next use an explicitly still-camera visual fixture,
verify exact pose and canvas, then assess scene/material regions. Disabling
wiggle for a controlled visual fixture must never be counted as ordinary-input
mission completion; gameplay probes retain original physics. Both exact owned
port/reference processes were checked and stopped after capture.

### Still-view matched-camera pixel measurements (2026-10-25)

Added explicit -xl-preview-still to developer preview only: disables the player's
PF_WIGGLE and selects full-screen cockpit mode. Sidecar labels
still_visual_fixture=true. This changes a visual fixture's physics and must never
be counted as normal gameplay. Formatting and native build passed. Port captured
1024-square canvas, identity orientation, zoom36864, scale[65536,65536,36864],
authored position[-18520117,-4073563,-1243530].

Reference profile bWiggle=0 did not stop live camera motion; two reads differed.
Do not infer the setting was effective. A separate reference diagnostic disables
only CObject::Wiggle (RVA0x2d8b50 prologue55->c3), inheriting the documented
visibility/readback diagnostics. reference-still-manifest.json records hashes
and all provenance. Two subsequent live reads show the same authored position,
identity orientation, zoom and full viewport[0,0,1024,1024] as the port. The
reference read itself is not synchronized to a screenshot; the still pose was
verified unchanged. All owned port/reference processes were checked and stopped.

Source verification: port TexFilt1 selects GL_LINEAR magnification and
GL_LINEAR_MIPMAP_NEAREST minification. Reference renderQualities index2 matches;
its earlier index3 was trilinear. A final isolated profile uses nImageQuality2.
Both final frames were inspected: aligned spawn geometry, red/brown surfaces,
blue animated powerup; HUD/reticle and powerup rendering differ.

In temp/xl-still-view-20261025/region-comparison.json, direct unregistered pixels
(no resizing, color fitting or translation) for five static surface rectangles:

| Region | Pixels | Mean absolute channel error /255 | 95th percentile | Maximum |
|---|---:|---:|---:|---:|
| Rear red wall |28800|0.6540|4|17|
| Left red wall |14000|1.1755|7|30|
| Upper-left rock |10000|3.5851|9|46|
| Lower-left floor |16800|8.6991|25|50|
| Middle grate |16000|3.9486|16|58|

Average regional RGB differs by at most0.548 per channel, but local pixel errors
are appreciable, especially the floor. These are observations, not a retroactive
pass tolerance or complete lighting qualification. Regions exclude HUD, reticle
and animated powerup; other scenes and dynamic lights remain untested. Next
separate texture/mipmap/raster sampling from lighting interpolation using this
fixed view; do not erase the errors with fitted color or image alignment.

### Nearest-filter control exposes projection mismatch (2026-10-25)

Added a reusable isolated scratch capture runner and same-region comparison
script under temp/xl-still-view-20261025. First attempt stopped after camera JSON
but before PNG completion; rejected as incomplete capture. Runner now requires
the PNG IEND trailer before stopping the owned process. Corrected capture in
temp/xl-nearest-complete-20261025 completed and all owned processes stopped.
Both engines used GL_NEAREST with no mipmaps and identical pose/legacy parameters.

Same-region mean channel errors: rear wall0.79594, left wall1.39867, upper rock
5.31597, floor9.83879, grate4.05381. Removing mipmaps did not remove the difference;
it is not sufficient to blame mipmap generation alone.

Source inspection identifies a further unmatched parameter. Reference
ogl_lib.cpp SetProjection uses gluPerspective(105 degrees * fixed zoom, aspect),
while the port uses90-degree perspective and scales view depth by fixed zoom.
At zoom0x9000, reference XY factor is1/tan(105*0.5625/2 degrees), approximately
1.76525; the port's effective factor is1/0.5625=1.77778. Thus equal fixed zoom,
pose and viewport do NOT establish matching effective projection. The previous
'matched camera' wording only proved pose and legacy parameters; correct the
interpretation before assigning pixel differences to lighting or textures.
region-comparison.json now labels effective_projection_match=false and retains
all measured errors. No image warping or fitted alignment was used.

Next verify the live reference projection matrix for the still capture and use
an explicitly scoped preview projection control to match effective FOV. Preserve
ordinary game FOV/physics; no mission-specific camera exception is warranted.
After that control, rerun the same regions rather than changing thresholds.

### Effective projection verified and isolated residual floor difference (2026-10-25)

Added bounded -xl-preview-zoom-fixed for explicit -xl-preview-still only
(16384..262144). Invalid/missing values reject startup. This is a developer
visual control; ordinary game FOV remains unchanged. Formatting, diff check and
native preview build passed. Sidecar reports actual zoom and still-fixture status.

Live reference GL projection now read from CTransformation::m_info.projection:
XY=1.765246868133545, confirming source derivation. Port capture zoom37126 yields
effective XY1.765231912945106, relative error-0.0008472 percent. The small residual
is fixed-point quantization, not an image fit. Pose/orientation/full viewport
match. temp/xl-projection-matched-20261025 contains nearest-filter captures,
read-only camera records and exact-region metrics; all owned processes stopped.

Mean absolute channel errors after projection correction: rear wall0.02108,
left wall0.02305, upper rock0.06513, middle grate0.03790, floor9.40780. Four regions
now nearly agree; floor remains materially different. Do not average that region
away or call full rendering parity. No pixels were shifted, resized or color-fit.
The floor reference crop is byte-identical across captures1..4; in the earlier
25-frame sequence there are only two variants (initial frame versus settled
frames), so arbitrary animation timing is not established as the explanation.

Original spawn segment0 side3 has base texture22, no overlay, floor center
(-282.59454,-72.15764,-18.97476). This is a candidate surface for the lower-left
crop, not yet verified by ray intersection. Next identify the exact hit surface
and compare its texture identity/UV interpolation and light inputs. Other views,
dynamic lights, Android rendered comparison and normal mission completion remain
open. These near-zero static-region measurements do not promote browser support.

### Residual crop traced to warped-face triangulation (2026-10-25)

Static geometry ray investigation corrects the earlier spawn-floor hypothesis:
four crop samples, including center(110,760), hit segment4 side3, texture22;
the lower-right sample(179,819) hits segment8 side0, texture149. Initial generic
0-2 ray triangles were inappropriate for nonplanar sides; retained floor-rays.json
is superseded by floor-rays-compiled.json and trace-floor.py, which use compiled
split triangles (true quads still divided0-2). Wall opacity assumes initial state.
No runtime hit or complete pixel mask is claimed from these five rays.

Segment4 side3 is nonplanar, compiled SIDE_IS_TRI_13, triangles[3,0,1] and[1,2,3].
The port render_side path preserves those triangle/UV groups. Reference profile
has triangle_mesh0; CQuadMeshBuilder::SetupFace emits four corners in order and
DrawFace uses GL_TRIANGLE_FAN. Retained reference mesh record for this floor is
[22,21,14,6], matching the ordinary side corner order, yielding the0-2 diagonal.
Source paths: reference gameio/createmesh.cpp SetupFace/Create, render/fastrender.cpp
DrawFace; port main/render.c render_side. The different diagonal changes the
surface plane and texture interpolation on a warped face, unlike planar walls.

This is a source/runtime-mesh-supported hypothesis for the remaining9.4 error,
not yet an isolated rendered proof. Next run an explicitly diagnostic fan-vs-split
comparison on the same fixed camera and unchanged pixel regions. Do not change
collision geometry, invent mission-specific exceptions, or label full lighting
parity until the residual is explained and the intended rendering contract chosen.
No runtime process or build is active from this investigation.

### Controlled fan comparison confirms the floor discrepancy (2026-10-25)

The next controlled capture completed in temp/xl-reference-fans-20261025.
Only the explicit still-fixture draw path changes to four-corner fans; camera,
effective projection, nearest filtering and the five pixel rectangles are held
fixed. Floor mean absolute channel error drops from 9.407797619 to 0.055873016;
rear wall0.021076389, left wall0.023047619, upper rock0.065133333 and middle
grate0.037895833 are unchanged. Sidecar records reference_fan_diagnostic=true.
This confirms the triangulation explanation for the measured crop, without
image fitting or inventing an acceptance threshold.

Normal rendering retains compiled triangle splits. Document the reference's
non-mesh fan-mode difference on warped faces rather than changing collision
geometry to match pixels. The explicit diagnostic is not a gameplay mode or
mission-specific exception. Native build passed and both owned capture processes
stopped. CMake inspection confirms start.cpp, which implements the diagnostic
hook, is included in Android targets as well as the native preview.

Remaining qualification work includes additional views and dynamic lights,
Android rendered comparison, normal-input mission completion and native-D1
parity. Browser qualification stays unchanged. The ordinary-input route probes
still do not establish repeatable survival or mission completion.

Integration follow-up: the headless lifecycle target excludes start.cpp and
supplies its own preview stubs. Added an always-disabled reference-fan stub to
that harness so the new render hook links there as well. Scoped code quality
passed. Rebuilt xl-preview-lifecycle.exe successfully; the original Dragon Pit
companion-ownership CMake integration check passed using that executable, with
artifacts in temp/xl-fans-link-validation-20261025. Android x86_64 XL-pilot
assembleDebug also passed (1m51s, 49 tasks); log is
temp/xl-current-render-20261025/android-fans-build.log. This proves build/link
integration, not on-device rendered parity. git diff --check passed, with only
line-ending normalization notices. No capture or build process remains active.

### Rebuilt Android lighting lifecycle and capture (2026-10-25)

Installed the rebuilt x86_64 pilot APK on emulator-5554 and ran the durable
test_xl_classic_lighting_lifecycle.ps1 wrapper. Run
4ec850d7d2b6456fb9262f34a242b023 passed35/35 in95676ms. Stock isolation and return
to1737 Dragon Pit companion-light vertices passed; the final frame was inspected
and shows textured, colored mine surfaces. State restoration was verified
byte-for-byte and the app was stopped. Exact APK/frame hashes and limitations
are recorded in evidence/dragon-lighting-comparison.md.

No Android pixel-parity claim: final cockpit0 and moving spawn camera differ
from the native square still fixture. Main-view capture telemetry must be tied
to the rendered frame, since renderer globals can be reused by other views.

Launch log also contains308 warnings attempting to read DXA compatibility
metadata from XL_Dragon_package_investigation as a ZIP. The run still passes;
this is a separate package-dispatch issue to investigate. checkModSetCompatibility
calls checkModCompatibility for enabled files, and that helper unconditionally
opens ZipFile, unlike collectModPatchDocuments' mission-archive format guard.
Confirm the installed owner's format and manifest before changing dispatch;
preserve actual DXA base-requirement checks and nested-resource handling.

### Extensionless non-ZIP owner compatibility dispatch (2026-10-25)

Installed default-set manifest confirms XL_Dragon_package_investigation is an
extensionless mission_zip owner with extracted_bundle mode, size102161128 and
the original levelpack.7z SHA256 b4b519dc5755b795fd49cc3786a0421b96cd099dcc8f002925ddfd7c038b2719.
It is not a directory or a DXA. checkModCompatibility now consults the validated
reusable extraction record for extracted mission packages and skips the ZIP-only
root-manifest reader when the recorded format is7z orrar. Actual ZIP/DXA reading
is unchanged; the decision uses package format, not mission identity or filename.

Regression coverage imports an extensionless7z owner, checks compatibility and
verifies its embedded Seven.dxa remains mounted. The existing named7z import
test also checks compatibility. Scoped formatting passed; all48 targeted JVM
tests passed (34 ModManagerMissionZipTest,14 ModManagerDetailsTest), Gradle
exit0 in1m28s. Log: temp/xl-current-render-20261025/package-format-tests.log.
This does not add new nested-DXA base-requirement validation or promote mission
support. Device verification of warning removal requires a rebuilt APK; the
previous Android lifecycle run predates this Kotlin fix.

Device follow-up of the extraction-record version: run
7078bb2a30e746bc9fc301ceb3199948 passed35/35 in116605ms and restored app state
byte-for-byte. The complete log contains zero owner-as-DXA ZIP warnings, versus
308 before the fix. Retained result and log:
temp/xl-current-render-20261025/package-format-record-{result.json,logcat.txt}.
However, enter_game to surface creation took82.179s, and source inspection shows
the new compatibility query repeatedly reparses the large extraction manifest.
Concurrent build load also differed, so this is not a controlled timing benchmark.

Replaced that lookup with ArchiveFiles.isNonZipDistribution: at most8 header
bytes identify7z, RAR4 orRAR5 before the ZIP-only root-manifest check. This is
format dispatch, not archive validation; extraction retains its existing full
checks. Extensionless owner regression now explicitly tests signature dispatch.
Actual ZIP/DXA readers and nested mount selection remain unchanged. The48
targeted JVM tests pass on this replacement; its on-device verification remains
separate from the extraction-record run above.

Final signature-based APK built successfully (55 tasks,1m45s) and was installed
on emulator-5554. SHA256:
7b8e9a1dcfd22e500b675632b67eb131205d0ad60aca21c445db4341fabb4375.
Device run9c1ae5afaebb415bab7737f7b74c27f7 passed35/35 in108885ms, with zero
owner-as-DXA ZIP warnings and no ANR/fatal exception. State restoration was
byte-for-byte and pidof confirmed the app stopped. Retained evidence:
temp/xl-current-render-20261025/package-signature-{result.json,logcat.txt},
package-signature-device.log and package-signature-verified-build.log.
Enter_game to surface creation was45.078s, versus82.179s for the record-reading
version; this is observational timing, not an isolated benchmark. The bounded
signature path no longer reparses the extraction catalog on each query.

Next Android visual work can reuse game_introspect_sample_framebuffer and its
request-generation handshake in game_introspect_check_and_dump. Capture the
main-view camera before auxiliary/gameplay views can overwrite renderer globals,
then associate that record with the sampled framebuffer. Do not infer a rendered
camera from later player state or count a stale menu-frame record. This is a
planned evidence mechanism, not implemented or passing visual qualification.

### Staged opening combat investigation (2026-10-25)

Previous muzzle-evidence samples show successive targets64/75/65/74/69 while
the ship advances. Target69 took damage25->14->3 before the player died;
projectile creation and damage are present, so this does not diagnose a broken
weapon. Tested three ordinary-input attack stops: first at13 before the earned
laser detour, second after returning to13, third at15. No mission, difficulty,
health, damage, robot or movement-state changes were made.

Run d6c1e834b9f648f5899ba1c9641f50ab in temp/xl-staged-combat-20261025 stopped
50/81 after97402ms because the second attack had no nearby live robot with an
open center ray. This is a probe precondition failure, not a player death or
mission incompatibility. Final player: segment13, shields104.82735, energy90,
three credited kills, laser_level1, invulnerablefalse. The first attack tracked
robot64 through zero shields until destruction. Original laser upgrade was
collected. Wrapper restored state byte-for-byte and stopped the app.

This is a promising bounded tactic result, not repeatability or completion.
Follow-up retains only the first staged engagement and the original full route
through the trigger-controlled door to160; later fights remain ordinary input
while flying. No optional-success attack semantics or game-rule changes added.

Follow-up run b1678443b0d44fac9ea06857ebd1196c in
temp/xl-first-engagement-20261025 passed75/75 in147235ms. It retained only the
first attack at13 before the upgrade detour, then used ordinary combat/movement
through27/40/71/41/69/70/160. Segment70 side2 wall_state assertion passed with2
after crossing the authored trigger; player then crossed the doorway and reached160.
No trigger invocation, wall edit, pose edit, resource grant or invulnerability.
Final difficulty2 (Hotshot), shields8.49681, seven credited kills, energy84,
laser_level1, concussion ammo0. Exact result and assertion are recorded in
progression-evidence.json. Wrapper restored state byte-for-byte and stopped app.

This proves one normal-input traversal of the first trigger-controlled door,
not repeatability, all triggers, boss/exit or mission completion. The near-empty
shields and exhausted missiles require attention before extending the route:
inspect authored supplies and upcoming encounters, and preserve this successful
route as evidence without promoting browser support. Remaining rendering and
native-D1 qualification gates are unchanged.

### Beyond-trigger supply audit and repeatability failure (2026-10-25)

The successful160 snapshot contains dropped shield objects103 at166 and108
at352. Conservative topology excluding original locked/keyed walls except the
already crossed trigger0 pair finds routes160/163/165/166 and onward167/352.
These drops are runtime-dependent, not fixed authored supplies. Authored Vulcan
object31/id13 at589 is farther along160/163/165/164/168/174/175/176/177/178/183/
186/310/589. Graph routes are not flight or pickup proof. Candidate snapshot
routes are saved in temp/xl-beyond-trigger-20261025/supply-audit.json.

Extended the unchanged successful75-step probe with ordinary flight to the
original centers of163/165/166, making84 steps. Run
f30b3d64b7764c808d1e2a6adf67e1d8 failed73/84 after150400ms, dying while entering160
before any extension step. Final shields-2.98788, six credited kills, energy83.5,
laser_level1, concussion ammo4, invulnerablefalse. State restored byte-for-byte;
app stopped. combat-evidence.json retains per-engagement samples. No traversal
or resupply claim follows for163/165/166, and the prior75/75 run is not repeatable
survival evidence.

First stationary engagement with64 lost shields112->85. Robot71 beyond the
trigger door then reduced21.012->0.012 during approach70; player died at160
while engaging80. Source inspection shows attack_object calls drive_combat_input
without its optional secondary-fire argument, whereas fly_to passes the parsed
combat_secondary option. Next evaluate using the same ordinary missile-button
path in the stationary engagement. Four missiles remained at death in this run;
this supports a specific tactic experiment, not an engine-damage change or a
claim that spending them earlier will guarantee survival.

### Optional stationary missile input: first trial not exercised (2026-10-25)

attack_object now passes the already parsed secondary_fire option to the same
drive_combat_input helper used by fly_to. Default remains false; ordinary mapped
button input, engine ammo/cooldown and existing cosine/range/visibility conditions
are unchanged. Scoped formatter and Android x86_64 D1/D2 APK build passed
(49 tasks,1m38s). No mission or weapon behavior was modified.

Probe temp/xl-stationary-missiles-20261025 keeps the previous84-step route and
changes only the existing stationary attack to secondary_fire=true. Run
aa8f3cf870f343c08cabff6f3d5cd0e0 failed73/84 after151582ms, dying upon entering160.
Final shields-0.64783, five credited kills, energy84, laser_level1, one concussion
missile remaining, invulnerablefalse. State restored byte-for-byte; app stopped.

Stationary target64 closed from31.494 to9.412 units; sampled aim never met the
missile condition while farther than30 units. Zero stationary secondary-fire
samples and ten actual weapon0 projectile creations were recorded. Thus the
new optional branch has build coverage but no positive runtime missile evidence
from this run. Stationary shields112->108.798; later death does not establish
that optional stationary missiles help or hurt. combat-evidence.json preserves
these limits. Next test engaging farther back at11 with a longer acquisition
distance so normal aim can settle before the robot enters the splash exclusion
range; do not weaken the range check or grant ammunition to force a pass.

### Earlier engagement verifies normal stationary missile fire (2026-10-25)

Moved the same attack block from13 to11 and changed acquisition distance80->120;
all84 route steps, Hotshot difficulty, normal input and missile firing checks
otherwise remain unchanged. Run df6b6fbea921486ca8b6409a397d7947 in
temp/xl-early-engagement-20261025 failed52/84 after116758ms, dying near16.
Final shields-2, four kills, energy88, laser_level1, concussion ammo3,
invulnerablefalse. State restored byte-for-byte and app stopped.

This time the new branch was exercised: stationary step34 reports three sampled
secondary_fire=1 frames and two actual weapon8 (CONCUSSION_ID) creations, alongside
24 weapon0 laser creations. Target64 tracked from107.349 to67.040 units; player
shields stayed112 across the engagement. This establishes actual normal missile
input/projectile creation, not a causal survival improvement or overall success.
combat-evidence.json retains exact result and samples.

Later engagement samples show shields88->40 while advancing toward15 and40->4
while advancing toward16. Targets69 and74 required large direction changes;
many samples have open center rays but aim below the primary-fire cosine gate.
No trigger-extension step ran. Further investigation should focus on these
moving engagements and the controller's aim/route behavior, rather than infer
a missing mission feature or repeat the earlier missile hypothesis unchanged.

Separately, the original object audit shows robot80 at167 contains_count2,
contains_type7 (powerup), contains_id2 (shield). This is an authored drop source
beyond the first door. It does not prove that any particular later snapshot
object came from80 or that its dropped positions will repeat across runs.

### Read-only aim-rate audit identifies a controller tracking error (2026-10-25)

Added requested pitch/yaw stick, raw ship angular velocity, mass, drag, maximum
rotational thrust, sensitivity/undercalibration and frame duration to the existing
debug combat samples. Steering, targets, firing conditions and game rules remain
unchanged. Scoped formatting and Android D1/D2 pilot build passed (1m16s).
Unchanged earlier-engagement route run4206738c96d8437f96846f1146394ddb in
temp/xl-aim-rate-audit-20261025 failed52/84 after117791ms near16; state restored
byte-for-byte and app stopped. This run supplies99 controller samples.

Measured ship mass4, drag0.03299, maximum rotational thrust0.14, pitch/yaw
sensitivity8 and undercalibration0. The full physics-substep recurrence in
physics.c and d1_in_d2_semantics.c is v_next=(v+thrust/mass)*(1-rotational_drag),
where rotational_drag=2.5*drag. Its unsaturated steady-rate gain is about0.38937
turns/second per normalized axis. Partial substeps, rounding, bank and collisions
are excluded from this bounded model; it is not an exact simulation replay.

Current helper command=4*angle_error+1.5*(required_rate-actual_rate). At steady
tracking the relative-rate term approaches zero, leaving an angular error needed
to sustain the turn. Predicted yaw error at required_rate0.04 is0.02568 turns,
larger than the primary firing limit acos(0.99)/(2*pi)=0.02253 turns.
One sampled moving engagement at step48 directly illustrates the issue:
required yaw rate-0.07530, actual-0.07622, yaw error-0.05020 turns (18.072 degrees),
open center ray and fire0. Model error at that rate is-0.04835 turns (17.405 degrees).
rate-evidence.json records the selection rule and all model limitations. This
single close match supports a feedforward-rate correction experiment; it does
not explain every missed shot or prove the cause of death.

Next add a bounded ordinary-stick feedforward term from required angular rate
and measured ship/controller response, preserving feedback, saturation and fire
thresholds. Verify actual tracking and gameplay afterward, without changing
mission/weapon/physics behavior or treating controller improvement as XL parity.

### Rate feedforward applied; first normal-input pass to166 (2026-10-25)

The debug controller now adds required angular rate divided by the estimated
steady ship/controller response to its existing feedback command. Response uses
actual ship mass, drag and rotational thrust, and joystick sensitivity including
undercalibration. Invalid/degenerate response parameters fall back to feedback
only. The existing0.6 command clamp, deadzone inversion, ordinary axis injection,
target selection, fire thresholds and engine simulation remain unchanged.
Added ff_pitch/ff_yaw to the existing diagnostic record.

Scoped formatting passed. A bounded full-substep yaw model checked15 cases over
three sensitivity scales and positive/negative/zero target rates: the added term
eliminates steady tracking error in that model. model-check.json is model-only
evidence, not a physics replay or gameplay assertion. Android D1/D2 APK build
passed in1m32s; installed on emulator-5554.

Unchanged84-step route run9bb50cbf183644aa875eebd0f4f0d02a in
temp/xl-rate-feedforward-20261025 passed84/84 in167588ms. Segment70 side2 door
assertion again read state2; ordinary traversal reached160/163/165/166 alive.
Final shields27.90706, eight credited kills, energy73, laser_level1, ammo0,
invulnerablefalse. The trace contains163 nonzero feedforward samples and maximum
raw requested stick magnitude0.6367 after the existing deadzone conversion.
State restored byte-for-byte and app stopped. progression-evidence.json records
the result, descriptive tracking samples and surviving nearby shield pickup.

This is one successful progression run, not an isolated causal A/B comparison,
repeatability proof or complete mission qualification. Some moving-target samples
still show material error; do not claim exact tracking. Robot80 is absent from
the final live-robot list; one shield pickup remains at352 near
(71.86958,-68.47983,31.44519). Its particular object number/position is not stable
across runs. Next extend the earned route toward resupply and the authored
Vulcan/lower corridor, while retaining all remaining compatibility gates.

### Vulcan extension exposed stationary targeting and waypoint delays (2026-10-25)

Audited166/167/352 resupply-area traversal, then return167/166 and lower route
164/168/174/175/176/177/178/183/186/310/589. The doors175/176 and310/589 are
ordinary automatic doors with KEY_NONE, not locked. At352 the route does not
cross its red-key door. Added a bounded interior sweep near(72.62148,-66,29)
for possible dropped shields; no fixed drop presence asserted. Final acceptance
requires live arrival589, positive primary_ammo[1] and actual HAS_VULCAN_FLAG
bit2 checked by the restoring wrapper. Topology is not runtime proof.

Run07ec3b654a8e4e19b62d4ae583101cdd in temp/xl-vulcan-route-20261025 failed34/136
after96925ms at the early stationary attack: target64 remained80 shields and
lost center-ray visibility. Player ended displaced to3 with93.26390 shields,
zero kills and laser0. Thus loss of sight is verified, but robot retreat alone
is not established. The fixed-target helper correctly timed out instead of
claiming a kill. State restored byte-for-byte; combat-evidence.json retained.

Removed only that three-step stationary attack block, retaining moving combat's
existing visibility-based reselection and the full Vulcan extension. Run
d2f59018c2ef4424ae0990883c9dfc17 in temp/xl-moving-vulcan-20261025 failed73/133
after150782ms, dying upon entering163. Final shields-0.52415, seven kills,
laser1, ammo3, invulnerablefalse. State restored byte-for-byte; app stopped.
Neither run verifies the added resupply/lower corridor or Vulcan acquisition.

Arrival trace audit estimates39.598s within2 units of waypoint centers out of
101.886s of sampled flight spans. Current fly_to requires both distance<0.5
and speed<0.5 at every waypoint, imposing repeated stops. These are sampled
interval estimates, not exact continuous timing or proof of damage causality;
arrival-evidence.json records per-step intervals and limitations.
Next evaluate an explicit continuous-arrival option for ordinary flight while
preserving segment/position checks, actual pickups, door assertions and final
mission-completion requirements. This changes driver tactics, not game physics
or compatibility acceptance. Default precise-settling behavior should remain
available for existing positioning tests.

### Continuous arrival exercised; close-contact combat remains unresolved (2026-10-25)

Added opt-in `fly_to` JSON `settle:false`: arrival requires the requested segment
and distance below two units, without the default speed/half-unit settling gate.
The default remains precise settling. Both paths release owned inputs at arrival;
ordinary momentum, physics, pickups and gameplay assertions remain unchanged.
Scoped formatting and the Android APK build passed (1m38s). The 133-step moving
Vulcan probe requested continuous arrival at intermediate waypoints only; its
final589 waypoint retained precise settling.

Run51b617a202f241c99a6f59a39816fe61 in temp/xl-flow-vulcan-20261025 failed49/133
after91310ms. Fifteen continuous-arrival completions were logged, but the player
died in16 with shields-1.21713, two credited kills, laser1, energy96, nine
concussions and no invulnerability. State restoration was verified byte-for-byte.
This exercises the option but provides no evidence of improved survival. The
trigger door, resupply extension and Vulcan acquisition were not reached.

The final trace records repeated object collisions with robots64 and65 while
position stalls about11.65 units from the waypoint. Robot64 remains at80 shields
in its final six combat samples, at distance about8.95; all six request fire0
while aim cosine rises from0.52274 to0.90254, below the existing0.99 threshold.
Earlier in the same flight step the selected target is75. Current flight combat
retains a live, visible target until it dies or loses visibility; a newly close
robot does not itself cause reselection. This is a concrete driver limitation
to investigate, not proof that it alone caused the death or that enemy behavior
matches the reference engine.

encounter-comparison.json compares the preserved successful166 run and the two
moving-route failures. The successful run logged no collisions with64/65; the
precise moving failure logged22/2 and the continuous failure16/14 respectively.
These are diagnostic event counts, not total impacts or attributed damage. The
runs are observational, with differing tactics and timing, not paired causal
trials. Do not use their pass/fail differences to declare a physics regression
or a compatibility improvement.

Next isolate the opening encounter: evaluate bounded close-contact target
reselection or an ordinary-input retreat before extending the route again.
Preserve weapon rules, difficulty, actual pickups and survival assertions. A
repeatable opening and earned save checkpoint would allow later progression
work without repeatedly conflating opening-driver failure with package support.
Full mission completion, rendered fidelity and native-D1 parity remain open;
none of these automation experiments changes browser qualification.

### Close-target reselection tested and withdrawn (2026-10-25)

Tested an ordinary-input driver change that reselected a visible robot within20
units when it was at least25 percent closer than the retained target. Scoped
formatting passed; APK build passed in1m43s. The unchanged continuous-arrival
133-step route ran on emulator-5554 as527e7ceec629456491440c4c50d6976f.
Evidence is in temp/xl-close-target-20261025/combat-evidence.json and steps.jsonl.

The new branch executed twice in step49:75->70 at distances19.1773/13.0327,
then70->65 at14.1428/9.0661. Nevertheless the player died in16 at90167ms,
49/133 steps, shields-2.81380, one credited kill, laser1, energy97.5, nine
concussions and invulnerablefalse. Robot65 still had80 shields in the final
samples, with aim cosine0.67..0.79 and no firing request. State restoration
passed byte-for-byte. This rejects close-distance reselection alone as a
demonstrated solution; it neither proves an engine defect nor isolates the
cause of death. The experimental reselection code was removed after this run.
The installed APK still contains that experiment until the next build/install;
do not mistake it for the restored source behavior.

Next investigate maintaining separation during the opening engagement rather
than making further nearest-target threshold changes. Keep this bounded to
ordinary inputs and independently track mission-support verification: the
ability of an automation driver to win is not evidence of reference gameplay
parity, and its inability to win is not by itself evidence of unsupported data.


### Opening-route geometry audit and current-controller recheck (2026-10-25)

Re-read the source study from origin/xl-package-layout-study and inspected the
current worktree before continuing. The local study branch name is absent;
the remote-tracking ref remains available. Package/browser qualification remains
separate from this ordinary-control gameplay investigation.

Compared the original successful44-step route with the later84-step pass.
The earlier route had neither the stationary attack at11 nor the laser detour
13->311->588->311->13. Native geometry audit confirms588 has only one portal,
back to311, which otherwise connects to13. Segment11's actual center is
(-222.59428,-62.15764,18.52518), whereas its scripted waypoint uses z21.03;
segment15's center uses z27.27518 rather than the scripted26.03. These are
waypoints, not exact centers. No claim that these offsets cause failures.
The geometry and unchanged probe hash are recorded in
`temp/xl-opening-route-recheck-20261025/trial.json`.

Retested the exact original44-step script against the current installed APK,
SHA256 4da2214deb8493bb6dc62a4aef289d9eea900a36c61acafe27d0af556a168f21.
This binary retains the withdrawn opt-in stand-off experiment, but the script
never enables it. Exact tested controller source remains in
`temp/xl-standoff-combat-window-20261025/game_automate-tested.cpp`.
No new controller or game-rule changes were made for this trial.

Run ba926fcec18740c2b9dd64da78669c41 FAILED36/44 at100325ms with player death
in segment15. Actual player shields-0.27719, four credited kills, energy93,
laser0, six concussion missiles and invulnerablefalse. The position object's
shields100 is not player health. Final target74 remained at65 shields,
range13.1293, cosine0.88857 and fire0. This does not establish whether target
selection, orientation, collisions or prior exposure caused the death.
Sampled frame-time median was0.07401s, versus0.069s in the earlier44-step pass
and0.07001s in the84-step pass; these are sampled diagnostics, not complete
frame distributions or a controlled performance comparison.

The wrapper terminated with failure, restored saved state byte-for-byte and
force-stopped the app. No live run remains. Raw steps, logcat, result and
`combat-evidence.json` retain the evidence. Skipping the upgrade detour alone
is not sufficient to reproduce survival. Do not infer a controller regression
from different builds and uncontrolled run timing, or an engine incompatibility
from automated-player death.

Next progression work should inspect the target transitions and damage timeline
at13/15 and use explicit traversable retreat waypoints through the audited
portals, rather than restoring distance-only repulsion or relaxing gameplay
assertions. Keep route experiments distinct from resource-fidelity qualification;
full mission completion, matched Android rendering and native-D1 parity remain
open and the browser stays unqualified.


### Explicit corridor retreat trial (2026-10-25)

The prior opening retry's target timeline is now retained in
`temp/xl-opening-route-recheck-20261025/target-timeline.json`. During step36,
target70 was retained while its sampled range changed78.73->43.81 and player
health95.12->38.12. These are net sampled health changes, not attribution to
that robot. Earlier successful routes killed the first robot before advancing
or used a separate opening engagement; the retry advanced with it still close.

New independent trial `temp/xl-portal-retreat-20261025` inserts six explicit
flight legs13->11->10->5->10->11->13 before continuing toward15/16/22.
Each inserted leg has an alive, positive-shields, correct-segment and no-invulnerability
assertion. Flight deadlines remain15s with default settling and normal primary/
secondary controls. Overall timeout360s accounts for the additional18 runtime
steps (62 total; the JSON has63 entries including its metadata header).
Walls4/5 between5 and10 are type2, flags16, keys1: an unkeyed door that closes
automatically. Geometry alone did not establish a successful return crossing.

Run4104c440faca456c9edbcf38f022d77d completed every retreat/return waypoint and
its alive assertion, including both door crossings, then FAILED56/62 at115377ms
with death approaching16. Final actual player shields-0.10130, four kills,
energy95, laser0, seven missiles, invulnerablefalse. Sampled health during the
retreat was112->105.543. After the return, the next combat sample in the advance
to15 showed90.543 shields, decreasing to32.899 before that waypoint completed.
Target75 remained alive at62.093 shields on the final sample, range17.0428,
cosine0.83624 and fire0. This is a usable retreat route, not reliable combat
progression or proof that the retreat improves survival across runs.

Same installed APK as the preceding trial; no source, game-rule, resource or
admission changes. Raw result/steps/logcat and combat-evidence.json are retained.
The wrapper terminated, verified byte-for-byte state restoration and stopped
the app. No process remains live. git diff --check passes apart from line-ending
notices. The next investigation should address engagement completion before
advancing beyond13; repeatedly extending the path with uncleared close enemies
has not produced reliable progression. Do not compensate with granted health,
weapons, altered difficulty, or reduced compatibility requirements.


### Retreat plus completed engagement reaches22 (2026-10-25)

Previous turn was progress: a completed independent trial verified both retreat
door crossings and exposed failure on the following advance. Current worktree
and restoration marker were checked before the next trial.

`temp/xl-retreat-engagement-20261025` retains the six-leg corridor retreat,
then adds one nearest-visible attack at the return to13, with range120 and a
30s attack deadline. Existing flight deadlines and gameplay assertions remain.
The action fails if it cannot acquire a visible live robot, if that target
survives the deadline, or if the player dies. It does not assert that the whole
area is clear. No controller, game-rule or resource changes; same installed APK
SHA as the preceding two trials. Runtime65steps, overall360s limit.

Run1c67ff98e4ad4371840c0b02863376fd PASSED65/65 in139130ms. Final segment22,
player alive, shields15.52647, energy76.5, six credited kills, laser0,
concussion ammo3, invulnerablefalse. The inserted attack selected robot69 at
84.5459 units with25 shields and completed at115039ms after starting111383ms.
Its last sample showed target5 shields at109.0655 units, cosine0.99980 and
primary fire held; player health fell112->79 during sampled attack observations.
The action's removal/destroyed event is not itself a credited-kill assertion;
the final six-kill total covers the whole route. Raw steps and final state are
preserved alongside combat-evidence.json.

This establishes one current-controller opening pass after a successful retreat
and explicit engagement, not repeatability or a causal improvement. Different
runs encountered different targets and timing. Saved state was restored
byte-for-byte and the app force-stopped; the wrapper exited0. No live process.
No earned checkpoint was exported by this wrapper, so later routes must start
fresh unless a future run explicitly saves and preserves normally earned state.

Next gameplay extension can reuse this route toward the first trigger and the
authored shield/Vulcan supply route, retaining all positive-health/no-cheat
assertions. Consider preserving a normally earned save with resource/build/run
provenance to investigate later sections without replaying every opening; never
substitute a granted or teleported checkpoint for normal progression evidence.
Matched Android rendering, native-D1 parity and full normal-input completion
remain open. Browser admission and package qualification are unchanged.


### Supply extension fails before new route; separate fidelity work next (2026-10-25)

`temp/xl-retreat-supplies-20261025` preserves the successful65-step opening
and appends the previous supply route from27 through the first trigger, shield
sweep and Vulcan589. Prospective143-step script, unchanged15s flight deadlines,
30s fixed-target attack and600s overall limit. The extension retains door-state,
alive, no-invulnerability and actual Vulcan-ammo assertions; no laser upgrade
is assumed. Same installed APK, no gameplay or controller edits.

Run3ff4611f31944b6aac515f6163dbfa84 FAILED54/143 at138442ms, before the new
route. The fixed-target attack chose70, not69 as in the preceding pass. Initial
sample: range86.5438, target25 shields, player112 shields. Target70 later became
occluded; the driver correctly stopped firing but continued tracking that same
robot. Final samples show target15 shields, range107, center_ray_open0 and fire0.
Player died in13 with-2 shields, two kills, energy91 and three missiles. Failure
was death before the30s deadline, not attack timeout. combat-evidence.json,
result/steps/logcat retain this outcome. State restoration was byte-for-byte and
the wrapper exited1 after stopping the app. No live process remains.

The preceding opening pass is not repeatable under the present driver. Neither
this death nor a fixed-target action waiting on occluded geometry diagnoses an
engine incompatibility. Do not weaken its destroyed-target postcondition and
then count an occluded target as defeated. A future tactical action would need
explicit distinct outcomes for defeated, lost visibility and retreat; it must
not silently redefine attack_object success.

Next implementation priority is the independent Android rendering evidence gap,
while normal-input completion stays open. Current source inspection confirms
that game_introspect_sample_framebuffer stores the current introspection request
generation and game_introspect_poll consumes that generation. The main-view
render wrapper in d2/main/render.c performs separate CPU/visual passes and
restores render state; later auxiliary views can overwrite camera globals.
Capture main-view pose/projection at the actual rendered view and associate it
with the framebuffer sample's frame/generation, rejecting stale or absent main
views. Reuse the existing capture path, preserve D1/desktop guards, and validate
against the controlled original-mission fixture before comparing pixels. This
is a required fidelity gate, not a replacement for full gameplay completion.


### Frame-linked Android main-view evidence (2026-10-25)

Added debug-only main-view recording immediately after render_setup_view for
D2 window0. It snapshots actual g3 View_position, Unscaled_matrix, Matrix_scale,
View_zoom and GL viewport before auxiliary views/gameplay collection can change
them. The shared introspection path associates that record with the sampled
framebuffer request generation. Every D2 flip clears pending view evidence,
including unsampled frames; absent or differently requested main views yield
valid:false. The snapshot also records a flip sequence. D1 does not advertise
main-view capture; non-introspection builds have no new runtime hook.
No camera, projection, lighting or admission behavior is changed.

The existing lighting lifecycle runner now explicitly requests a startup-menu
sample and waits for main_view.valid:false, then requests gameplay evidence
before checking valid:true. Its final restored sample must have matching camera/
framebuffer request IDs, positive viewport size and valid projection scale.
Initial trial4dd0e0c2ffec4799a0eca4a83c537b41 failed6/35 because the first new
assertion preceded any framebuffer request. Corrected test sequencing, retaining
the checks; the updated script is36steps. Initial failed state restored exactly.

D1/D2 x86_64 build passed1m42s. Installed/tested APK SHA256
328bdfaf7df41e5261fc872c92f4427c7622d51b326ccf89c1656a12a3881c43.
Run243e009380a8492096973ab7b915cc79 PASSED36/36 in119126ms with the final
wrapper checks and byte-for-byte restoration. App stopped, no live sessions.
`temp/xl-camera-frame-20261025/camera-evidence.json` records final viewport
[0,360,1920,720] inside1920x1080, request IDs5/5, flip sequence240, segment0,
zoom36864 and scale[24576,65536,36864], GL error0. Startup menu evidence has
valid:false. Scoped format/lint passed. A final INTROSPECT_ON guard around the
flip hook preserves the same tested debug code and excludes the release call.

This synchronizes camera metadata with framebuffer metrics, not the separately
captured ADB screenshot. The existing screenshot remains smoke evidence. Next
export raw pixels from the same on-demand sample with its camera/provenance,
exercise post-game menu invalidation and auxiliary-view cases, then match pose/
viewport/projection to the reference. No matched Android pixel claim yet.
Full normal-input completion and separate native-D1 parity remain open.


### Exact sampled-pixel export verified on Android (2026-10-25)

Added opt-in set_debug capture_framebuffer=once. It requests the existing
introspection framebuffer sample and exports only if the sample has a valid
matching main view and GL readback succeeded. No camera/lighting manipulation.
`introspect.json.frame` is one atomic publication: a compact JSON header line
followed by bottom-up RGBA8 bytes. Header version1 records dimensions, byte
count, SHA256, actual camera/projection/viewport, request ID, mission key and
activation generation, and level. Reused the existing SHA256 helper. Failed
captures report failed rather than presenting an old file as a new success.
The fixed output path is derived from the configured introspection path; no
script-provided output path. This remains debug-only, one-shot evidence output.

Extended the existing lifecycle test from36 to38steps: capture after restoring
Dragon Pit, wait for saved status, retrieve the binary export and validate its
header, byte length, SHA256 and matching camera/capture request IDs. The later
introspection sample may have a newer request ID; the pixel_capture receipt and
export header identify the exported frame, not that later sample.

Scoped format/lint and git diff --check passed. D1/D2 x86_64 build passed1m33s.
Installed APK SHA256 d78384231ced3ef442d8b260688e8b6bf8d637b5000d603fbabd55c29d073e4e.
Run9b87f1f5971043deafd201da060eddaa PASSED38/38 in117684ms, including host-side
export validation; saved state restored byte-for-byte and app stopped.
`temp/xl-frame-export-20261025/export-evidence.json` records full provenance.

An independent Python decoder rechecked all8294400pixel bytes and SHA256
8b1c463220a96e324d3e1a0e2f48e86b43e9b545da74bbc17d653bd8f6237597.
Decoded captured-frame.png was visually inspected: correct mine/cockpit,
textured surfaces and colored lighting, no blank/corrupt export. Header request4
matches camera request4, flip222, mission generation5 and original Dragon Pit
context key. Viewport[0,360,1920,720] inside1920x1080; pose fixed
[-18520117,-3915860,-1243530], identity orientation, zoom36864,
scale[24576,65536,36864]. These differ from the square reference fixture, so no
numeric visual-parity conclusion follows. Pixel bytes and camera are now from
one sample; the separate ADB screenshot is still a different artifact.

Next: exercise post-game stale-view rejection/auxiliary views and construct a
matched reference pose, viewport and projection using this export, with clear
normal-render versus diagnostic-fan labels. Full gameplay completion and
native-D1 parity remain independent open gates; browser qualification unchanged.


### Post-game camera invalidation and rear-view isolation (2026-10-25)

Extended the same lifecycle runner to43steps on the unchanged pixel-export APK.
It now exports gameplay before the save, aborts to the main menu, requests a new
export and requires status failed plus framebuffer main_view.valid:false. After
stock isolation and restoring Dragon Pit, it enables the left rear-view window,
exports again, and requires saved status, valid main view, forward z65536 and
cockpit_views[0]=2. This is one auxiliary camera configuration, not coverage of
all guided/missile/subview combinations.

First runc99038f0492348ed86fa9ecb259ee219 passed all43 engine steps but the wrapper
failed: it checked a later introspection sample instead of the exported frame's
receipt. Export camera/pixels matched request6, while the later probe8 correctly
reported invalid. Preserved first-state.json, first-result.json and first.frame
in temp/xl-frame-invalidation-20261025. Independent SHA validation passed, and
first.png was visually inspected: the rear-view window is rendered while the
recorded main view remains forward. Both failed-wrapper and subsequent successful
runs restored saved state byte-for-byte.

Corrected wrapper to validate pixel_capture, not the unrelated newest probe;
all validity, viewport, generation, length and hash checks remain. Scoped
format/lint passed. Full rerun3eadc5febf10469c8205384bfc9782ba PASSED43/43 in112088ms
and completed host validation/restoration with exit0. Export request6 matches
camera6. verified.frame and invalidation-evidence.json retain the verified
payload and receipts. App stopped; no live test or formatter remains.

This proves rejection after a rendered mission and main-view isolation with an
actual rear subwindow in these runs. It does not qualify visual fidelity.
Next build a labeled still visual fixture matching the reference's square
viewport, exact camera and effective projection. Existing native fixture uses
PF_WIGGLE off, full-screen cockpit mode and explicit zoom; Android should record
those controls and restore device/app state. Do not resize/color-fit output to
hide a mismatch. Keep normal compiled face splits separate from reference-fan
diagnostics, and retain normal-input completion/native-D1 parity as open gates.


### Matched Android still-view regions (2026-10-25)

Added temporary debug still_view_zoom_fixed control (16384..262144 or restore)
for live single-player D2. It saves zoom/wiggle, disables ship wiggle and applies
explicit zoom. Script success, failure and replacement restore the saved zoom;
wiggle restoration checks player signature, level and mission generation.
Captures label diagnostic_still_view explicitly. Ordinary gameplay is unchanged
unless this diagnostic is requested. pose_view also accepts position_fixed as
three bounded integers, avoiding its existing float-coordinate rounding;
existing float scripts keep their behavior.

First fixture65c20d1614644f3dba87f0c59359dd31 failed25/28: Android display override
alone did not change native EGL buffer size, which remained1920x1080. Float pose
also rounded X from-18520117 to-18520116. The exact assertions rejected both;
no comparison was accepted. Initial capture/result evidence is retained. Failure
cleanup restored wiggle (flags75), saved app state and physical display size.

Corrected setup using existing write_graphics_settings for ResolutionX/Y1024,
TexFilt0, AspectX/Y1, and exact fixed-point pose. Backup now also covers
files/graphics_safety.json and its lock, in addition to engine configs, player
files and preferences. The wrapper owns only5554 and restores the prior wm size.
No test tolerance changed. The explicit projection37126 was derived from the
reference, not fitted after observing pixels. Normal compiled face splits remain
active; no reference-fan override.

D1/D2 build passed1m15s after the exact-coordinate addition; scoped formatting
and diff checks passed. APK479ae821ac04d36dacd00504de73a23878f7a9623439e804e7d371c5d62d06ab.
Run4c5760833fec4105aeed65b5d7398064 PASSED29/29 in51713ms, including capture
viewport/pose/scale and explicit zoom/wiggle restoration assertions. Host payload
checks passed; saved state restored byte-for-byte and display restored to its
original physical1080x1920/no override. App stopped; no active sessions.

`temp/xl-android-still-20261025/comparison.json` uses the existing five reference
rectangles, original hashed reference TGA, exact1024x1024 camera and no image
rescaling or color fitting. Pixel SHA256:
4ddcf5a00abb4ebc71eddda73d1745fad4b70d008e38b6e4f6a7a76bd064ea33.
Effective projection relative difference is-8.472e-6, the previously calculated
fixed-point rounding limit. Android.png was visually inspected. Mean absolute
RGB channel errors (0..255): rear red wall0.01034, left red wall0.01745,
upper-left rock0.03500, middle grate0.01804, lower-left warped floor9.40272.
The four close regions each have95th-percentile error0; sparse edge errors
remain (max14..32). The floor hasp95=30 andmax54. This reproduces the bounded
native comparison's normal-split discrepancy; it does not erase or qualify it.
The independent decoder uses Pillow plus standard-library statistics; NumPy was
unavailable, so the same metric was implemented without that dependency.

This is the first matched Android static-region witness, not full scene/mission
qualification. Broader/material/dynamic views, the floor rendering policy,
normal-input completion and native-D1 parity remain open. Next inspect additional
authored light/material views and resolve the triangulation fidelity requirement
explicitly rather than promoting the mission from four close regions.


### Floor policy: profile-dependent drawing, independent collision (2026-10-25)

Previous turn made progress with matched Android static-region evidence. This
turn revalidated current source and audited the remaining floor difference
against clean files at the pinned reference2576a98. No renderer changes or
runtime tests were made. Source/equations are retained in
`temp/xl-floor-policy-20261025/geometry-evidence.json` and summarized in the
lighting comparison document.

Reference lighting method0 selects four-corner fans; nonzero methods select a
triangle mesh that preserves split side topology (with additional eligible quad
subdivision). The measured no-mesh snapshot is one profile, not a universal XL
geometry contract. Segment4 side3's two diagonal surfaces differ by2.000002 world
units at their x/z intersection. This is not just channel rounding, nor a
runtime collision measurement. Reference collision uses side faces/normals
independently of the OpenGL draw primitive.

Refined implementation strategy: explicitly model a legacy-fan visual profile,
keep compiled collision geometry unchanged, and validate backface/grazing views
before enabling it outside still diagnostics. Do not infer all classic warped
faces require XL admission restrictions, or silently label a fan capture as the
normal renderer. Preserve the measured four-close/one-different result until an
actual profile implementation and its tests justify another claim. Subsequent
work should add an off-angle material witness for the same warped face and a
profile-scoped draw/culling contract, then broaden beyond the starting room.
Normal-input completion and native-D1 parity remain required and unproven.

### Face-culling contract refinement (2026-10-25)

The preceding branch-answer turn did not advance the pilot. Resumed from the
current worktree and inspected clean pinned reference sources. New arithmetic
evidence in `temp/xl-face-culling-20261025/evidence.json` establishes a grazing
submission difference between the reference raw-displacement test and the
existing normalized fan diagnostic. This is not a runtime rendering failure:
both engines also enable hardware triangle culling, which may mask it.

The lighting evidence document records exact camera coordinates, source
locations, arithmetic conventions and limits. Next implementation must scope
raw-displacement submission to the named visual profile and preserve GPU
culling. Runtime witnesses must include mixed-facing fan triangles, not only
front-facing screenshots. No normal renderer change or support promotion was
made. Gameplay completion and broader visual coverage remain open.

### Oblique Android floor fixture (2026-10-25)

Previous turn progressed by finding a reproducible grazing submission
difference. This turn produced an actual Android floor view from inside
segment 4, pitch 8192, using the original mission and existing still-camera
controls. Run `063b9a901791499895251c9c607d5f47` passed 29/29 with exact
position, segment, camera direction, resolution and export validation.
The decoded floor frame was inspected; app and display restoration passed.
Evidence and scripts: `temp/xl-android-oblique-20261025`.

The existing reference capture helper only produces the mission-start pose.
Next work must establish the matching controlled reference pose and capture;
do not compare the new Android view against the old reference start image.
This turn neither proves oblique parity nor exercises mixed-facing fan
triangles. The lighting evidence document records these limits and the
frame hash. No support classification or runtime rendering policy changed.

### Paired oblique comparison and split control (2026-10-25)

Previous turn progressed by capturing Android's oblique floor view. This turn
completed its reference half using an isolated HOG with only player-start
fields modified and byte-audited. The reference camera matches position and
orientation; two successive reference captures agree within all measured
crops. The reference camera read remains separate from screenshot sampling.

The same host executable was then captured with and without its existing fan
diagnostic. Left/right floor mean errors fall from 10.048/7.994 to 0.094/0.080;
Android compiled gives 10.050/8.009. The red-wall control stays close. This
provides a second angle and a same-executable control supporting triangulation
as the primary discrepancy. Full scripts, manifest, frames and comparisons:
`temp/xl-reference-oblique-20261025`; durable details in the lighting evidence
document. Processes stopped; original mission assets untouched.

Still required: mixed-facing triangle/culling witness, scoped visual-profile
implementation with reference submission semantics, Android verification,
broader authored visual features, normal-control completion, native-D1 parity.
No production rendering or support classification changed from this evidence.

### Mixed-facing visibility coverage (2026-10-25)

Previous turn progressed with paired oblique captures and a same-host split
control. This turn tested mixed-facing fan geometry. The initial reference
pose moved during startup, failing exact camera validation. A higher-clearance
pose stayed fixed but its downward view excluded the back-facing triangle.
Both inadequate fixtures are retained and explicitly excluded as culling
qualification evidence.

The final horizontal view at fixed position `[-19503156,-4368693,214644]`
stays fixed in both engines and includes the back-facing triangle's projected
area. Opposite plane signs are computed from original corners. In the
2675-pixel footprint outside HUD exclusions, host fan versus reference mean
RGB error is 0.06704 (maximum 12). The captures were visually inspected;
camera/hash checks and exploratory region metrics are reproducible under
`temp/xl-reference-mixed-visible-20261025`. This remains a diagnostic modified
player-start fixture, not original mission completion or GPU instrumentation.

Next implementation step: explicit raw-displacement submission semantics for
the scoped legacy-fan visual profile, then Android captures at the established
start, oblique and mixed-facing poses. Preserve compiled collision geometry
and GPU culling. No normal renderer or browser classification changed here.

### Android fan diagnostic enabled and measured (2026-10-25)

Implemented a dedicated raw-displacement, truncating split-face submission
rule for the existing fan diagnostic. Engine compiled normals remain the
inputs; collision, GPU culling and normal rendering are unchanged. Android
automation can enable fans only in the classic D1 lighting still fixture,
with mission/level/player identity checks and frame-linked diagnostic metadata.
Still-view restoration clears the option.

Scoped formatter, Android D1/D2 build and Windows renderer/headless build
passed. Android run `ca5e0506bc3a4aa3a9e65bc9e71d353c` passed 30/30 plus
frame validation and byte-for-byte state restoration. Oblique left/right floor
mean errors fell from 10.050/8.009 to 0.043/0.026; red wall unchanged at
0.00652. The decoded frame was inspected. Evidence and exact hashes are in
`temp/xl-android-fan-profile-20261025` and the lighting evidence document.

Next: validate start and mixed-facing Android views and cleanup behavior, then
decide normal-use profile selection from detected package requirements. Do not
turn this diagnostic success into a supported mission classification. Normal
mission completion, broader fidelity and native-D1 parity are still unproven.

### Android start/mixed views and failure cleanup (2026-10-25)

Previous turn implemented the Android fan diagnostic and verified the oblique
view. This turn ran the remaining established camera fixtures on the same APK:
start run `5e135941912b4eb09dfd29202e84d5ad` and mixed-facing run
`f88ff38c9b964a5aab6faffd408c6a38`, each 30/30. Exact camera and frame receipt
checks passed; both decoded images were inspected. Start floor mean error is
0.03024 (previously 9.40272); other four regions unchanged. The established
2675-pixel mixed-facing footprint gives mean 0.03464, maximum 12.

An additional expected-failure fixture (`c77c662da5624569822805968fbf8532`)
enabled fans, captured, then supplied an invalid fan value. After the expected
engine FAIL at step 28, a later frame from the same process proved both
diagnostic flags cleared, zoom 36864 and physics flags 75 restored. The wrapper
passed. All three runs restored saved state byte-for-byte, restored the display
and stopped the app. Scripts, receipts and comparison results are under
`temp/xl-android-fan-views-20261025`. EXTENSIONS.md now summarizes the bounded
three-view evidence; mission support classification remains unchanged.

Next work can move beyond still-only drawing: associate a named legacy-fan
visual profile with the successfully prepared classic companion-lighting
runtime, not with mission identity or arbitrary warped sides. Review lifecycle
and stock-mission isolation before enabling it for normal-control pilot runs.
Keep diagnostic overrides and captured effective-profile metadata distinct.
This does not remove the remaining gameplay-completion, authored-feature and
native-D1 parity requirements.

### Prepared-lighting visual profile and lifecycle (2026-10-25)

Previous turn verified three Android diagnostic poses and failure cleanup.
This turn associates `legacy_fan` with successfully prepared classic companion
lighting during live single-player D1 gameplay. It does not use mission names,
change collision or automatically admit unsupported missions. Lighting remains
the existing experimental opt-in. Effective-profile receipts distinguish normal
profile activation from the still-only diagnostic override, including host
capture receipts so an override-off capture is not mislabeled as compiled.

Android D1/D2 and Windows renderer/headless builds passed, as did scoped
formatting. Extended durable lifecycle run
`9f44518731784d458597f73260ed6ec6` passed 43/43 with save/load, stock isolation
and restored pilot captures reporting fans while both diagnostic flags are off.
The final frame was decoded and inspected; wrapper state restoration passed.
Evidence and exact APK/pixel hashes are in
`temp/xl-classic-visual-profile-20261025` and the lighting comparison document.

Next: resume normal-control mission progress with this effective profile,
continue authored-feature/dynamic-light coverage, and resolve remaining native-D1
parity and admission requirements. A successful profile lifecycle must not
substitute for full pilot completion. Browser status remains unqualified.

### Bounded engagement trial design (2026-10-25)

After the visual-profile lifecycle passed, resumed normal-input progression.
The prior supply run died while strict `attack_object` tracked an occluded
live robot. Preserve that action's completion contract. Added a separate
Android D2 `engage_object` action using the same ordinary aim/fire controls:
it records `no_visible_target`, `lost_visibility`, `duration_elapsed`, or
`target_removed` explicitly, clears held inputs, then allows the next tactical
step. None of these outcomes certifies a kill; target removal still requires
an independent kill-count check. Player death and invalid gameplay context
remain failures. No robot, player, weapon, collision or damage rules changed.

Prospective trial: original supply route, two engagements bounded to 4000 ms
with the existing six-leg corridor retreat repeated between them. Existing
15-second flight deadlines, survival, no-invulnerability, actual kill-counter,
door-state and earned Vulcan-ammo assertions remain. Total 163 steps, 600-second
runner limit. No teleports, grants or checkpoint injection. Artifacts and
summary tool: `temp/xl-bounded-engagement-20261025`.
Build/format passed; APK
`068d87bac6034dae935a2a9aea4270bffad86de88f92764260d1dfa39f825a8f`.
Record the actual result below; this design is not evidence of route success.

The first launch was interrupted by an Android system restart before automation
began; cleanup initially failed while the activity service was unavailable.
The retained backup was recovered and checked byte-for-byte using `recover.ps1`.
The next launch timed out in SetupActivity while Android reported an ANR in
Settings. Its wrapper restored saved state successfully. Neither attempt
exercised gameplay or the new action. Sandbox visibility also hid the existing
AVD from automatic recovery, which then failed on the SDK writer lock.
An elevated read-only inventory confirmed Nexus5X_Light_1 exists on 5554;
targeted recovery uses that existing AVD and the original host/headless options,
without changing 5556 or provisioning new AVDs. Recovery log is retained with
the trial. Retry gameplay only after that recovery completes.

Targeted recovery completed and reported `RESTARTED_AND_HEALTHY`; the unchanged
trial then ran under `temp/xl-bounded-engagement-20261025/recovered`.
Run `3d0738b0cf4f4a53a5cfefb4d866f5e7` FAILED at 83/163 (129387 ms),
while flying from segment 46 into 22. The player died with shields -2.18512,
four actual kills, energy 85.5, laser level 0 and five concussion missiles.
Vulcan ammo remained zero. No invulnerability; prepared legacy-fan profile
was active. Wrapper restored state byte-for-byte and stopped the app.

The new action was exercised without treating disengagement as a kill:
step 54 recorded `lost_visibility; target still alive` after about 2.94 seconds;
the subsequent six retreat legs completed. Step 73 recorded `no_visible_target`
and the script advanced. Duration-expiry and target-removal outcomes were not
exercised in this run. The strict `attack_object` contract remains unchanged.
`summarize.py` preserves the engine FAIL and distinguishes those outcomes from
the actual kill counter in `combat-evidence.json`.

The health timeline changes the next tactical focus: the first engagement
began at 108.815 shields, ended near 102.815, and the player still had 102.815
when advancing beyond the return to segment 13. Subsequent flight samples
show 78.815 on the way into 16, 30.815 before the next leg, 15.815 before 22,
then death. See `engagement-health-timeline.json` for source samples; these do
not attribute damage to individual enemies. Repeating more corridor retreat
loops is not supported by this evidence. Next investigate the exposed 15/16
engagement and actual shot/visibility observations before another supply-route
attempt. The supply extension never ran, no checkpoint was earned, and this
trial provides no mission-completion claim.

### Combat aim geometry observation (2026-10-25)

Reviewed the preceding failed run's steps 77/79/81/83: all 111 sampled
center rays were open, but only 8 samples held primary fire. All 12 logged
muzzle checks had fate 0 and produced projectile objects. This excludes
muzzle-wall rejection for those recorded shots, not downstream obstruction
or missed targets. The existing controller requires cosine > 0.99 regardless
of target size and range. Ordinary lasers use gun points 0/1 and ship forward
direction (`laser.c`); the fixed cone is an automation choice, not a game rule.

Added observational `aim_geometry` records beside combat samples: predicted
target center in ship coordinates, target radius and identity, selected primary,
and both laser muzzle offsets. No firing threshold, input strategy, mission
asset or game rule changed. Scoped formatter passed; Android D1/D2 x86_64
pilot APK build passed. APK SHA-256:
`f909719a04a28177c50376e653582ef7b9502bd9c1673cb90028500a780e951d`.

Unchanged 163-step trial, artifacts `temp/xl-combat-geometry-20261025`, run
`ab1199e60847448ab8bbb6ff7d69160a`: FAIL at step 77, 119207 ms. Player died
with shields -2.21133, energy 87.5, three actual kills, laser level 0, five
concussions, no Vulcan ammo and no invulnerability. Saved state restored
byte-for-byte; wrapper stopped the app. Step 54 exercised the bounded action's
`duration_elapsed; target still alive` outcome; step 73 had `no_visible_target`.
Target-removal outcome remains untested. No supply-route completion/checkpoint.

`analyze_geometry.py` pairs adjacent geometry/control records with matching
step and target signature and writes `aim-geometry-evidence.json`. At step 77,
29 samples had clear center rays, 7 held fire, and 13 withheld fire while at
least one muzzle's forward ray overlapped the projected predicted target
sphere. Example at 116720 ms: target 69 radius 4.83565, muzzle transverse
errors 6.82228/3.59245, predicted target forward distance 25.59115. The
measurements support investigating size-aware firing, not declaring those
samples hits. They omit muzzle-to-target wall clearance, muzzle-specific
flight time, target acceleration, projectile radius and cooldown availability.
The player entered this leg with 81.793 shields and died while the last target
was behind the ship; a firing change alone is not proven to solve survival.

Next: evaluate constant-velocity closest approach from each real laser muzzle,
including forward flight time and wall clearance, before changing the firing
decision. Keep other weapons and concussion safety separately constrained.
Validate actual damage/kills and survival on ordinary controls; projected
intersection is a controller estimate, never a compatibility acceptance gate.
Full pilot completion and the previously listed fidelity/admission gates remain
open. Browser support remains unqualified.

### Muzzle-aware controller trial (2026-10-25)

Based on the recorded suppressed-shot geometry, the Android D2 automation
controller now estimates a laser/target-sphere intersection from each of the
two actual muzzle positions. It solves forward intersection time using current
target velocity and the selected laser's speed, bounded to the existing
two-second prediction horizon. It requires clear ship-to-muzzle and
muzzle-to-intersection wall queries, plus the existing clear center ray. No
transparent-wall bypass. A qualifying muzzle holds ordinary primary input;
the engine still owns cooldown, ammo, projectile creation, collision and damage.
Other primary weapons retain the fixed cone; concussion range/alignment
constraints remain. No mission-name special case or gameplay rule change.

The estimate deliberately does not certify hits: robots may accelerate, other
objects may intervene, engine projectile placement advances the bolt by half
its length, and the estimate uses the target sphere without adding projectile
radius. `aim_geometry.laser_muzzle_mask` records 0/1/2/3 for the qualifying
muzzles, or -1 when this estimator is inapplicable. The actual run result and
damage/kill counters must decide whether the changed input strategy helps.
Trial artifacts: `temp/xl-muzzle-fire-20261025`; unchanged 163-step route and
acceptance assertions. Initial build caught a const-pointer mismatch in the
wall query, corrected using a local position copy; record final build and
runtime outcomes below.

Corrected build and scoped formatting passed. APK SHA-256:
`1bb21f4c3e9c2c29a4d298c8c7d298a48a478732a98a850ec168005b73027b00`.
Run `3667202a806c482d8452a40782b780af` FAILED at 96/163 after 157186 ms,
with player death near segment 70. Seven actual kills, shields -2.33730,
energy 83.5, laser level 0, three remaining concussions, zero Vulcan ammo,
no invulnerability. Wrapper stopped the app and verified restored state
byte-for-byte. Both bounded engagement outcomes were again duration-expiry
and no-visible-target. No checkpoint or supply-route success.

The player entered step 77 with 81.838 shields. Target 75 was down to 8.227
shields at the end of that leg, while primary fire was held at cosine 0.91059
and distance 14.3331. Step 79's target 74 fell from 65 to 9.739 shields in
the sampled interval. The player reached segment 46 alive with 18.663 shields,
continued through segment 41, then died on step 96. That leg acquired target
71 at distance 47.5439 and later target 80 at distance 29.6674. It held primary
fire in 16 of 18 combat samples; lack of firing opportunities is therefore
not established as the remaining failure on that leg. Samples do not attribute
individual damage events or prove all predicted shots hit.

This one run progressed further than the immediately preceding fixed-cone
trial (step 77, three kills), but gameplay timing/AI differ between runs, so
it is not a controlled proof of repeatable improvement. Retain the geometric
input rule for further evaluation; do not tune its acceptance from this result.
Next inspect the approach to segment 70 and opportunities to avoid entering
its combat with critically low shields, while preserving actual authored
resources, difficulty and normal controls. Full mission completion, dynamic
fidelity, native-D1 parity and browser admission remain open.

### Authored upgrade with muzzle-aware input (2026-10-25)

Rechecked initial topology from segment 13, allowing audited passable portals
and unlocked unkeyed doors. The reachable static shield is object 16 in
segment 7, already collected by the opening route. Laser object 22 in 588
is the nearest unused combat pickup; the current retreat route had omitted
the previously verified detour. Other reachable pickups are energy and
concussion packs beyond the exposed opening. This is a topology/resource
inventory, not a proof of collision clearance or safe combat navigation.
`temp/xl-muzzle-laser-route-20261025/reachable-resources.json` preserves it.

Restored the prior 11-step detour 13->311->588->311->13 immediately after
the first arrival at 13. Preserved its before/after laser-level assertions,
all original route assertions, difficulty and timeouts. No controller or
game-rule edits; used APK `1bb21f4c3e9c2c29a4d298c8c7d298a48a478732a98a850ec168005b73027b00`.
`trial.json` records the exact 174-step probe and hash.

Run `b59b59e3f7a648058b98bcdf01c24df6` FAILED at 153/174, 217323 ms,
with death in segment 176. Laser upgrade acquisition and return passed with
112 shields. Actual laser level changed 0->1, and later shot records use
weapon 1. Progression reached segment 70, passed the existing door-state
assertion, crossed into 160/163/165/166/167, and traversed the shield sweep
through 352. Final counters: nine kills, laser level 1, shields -2.99606,
energy 75, no concussion or Vulcan ammo, no invulnerability. Wrapper restored
saved state byte-for-byte and stopped the app. No earned checkpoint or Vulcan
pickup; the route and full mission remain incomplete.

The shield evidence changes the next action. Step 107 began at 24.004 shields
and ended at 6.004; later the player briefly survived at about 0.004 (rounded
to 0.0 in assertion logs). Pickup traversal recovered first to 12, then 24.004
before step 153. The final powerup list contains only the eight distant static
shields, with no nearby dropped shields remaining, and robot 80 is absent.
Do not repeat the pickup sweep on the assumption that one drop was missed.
Step 153 targeted robot 76; the final sample has target shields 43, distance
16.5093, both estimated muzzle paths qualifying and primary fire held. The
player died while settling near the segment-176 waypoint. Next inspect that
door approach, incoming damage and ordinary combat movement rather than
loosening arrival tolerances or granting resources. This single longer run
does not prove repeatability or reference gameplay parity. Browser support
remains unqualified pending the full existing acceptance requirements.

### Continuous corridor transit trial (2026-10-25)

The failed segment-176 flight spent about 0.6 seconds against the opening
door, then decelerated from roughly 8 to 1.6 units/second over the following
1.5 seconds while exposed. Audited portals 174->175->176->177->178 share
the scripted centerline. Their minimum transverse centerline clearance is
about 10 units (20 at the last portal), versus player radius 4.73518. The
175/176 door is ordinary, unkeyed and unlocked. This is static geometry
evidence, not a swept-sphere collision or survival guarantee.

Prepared `temp/xl-door-transit-20261025`: one ordinary `fly_to` from 174 to
178 replaces separate stops/assertions at 175, 176 and 177. Endpoint 178
still requires the existing arrival tolerance, living player, positive shields
and no invulnerability. The wrapper additionally requires movement samples
in all three intermediate segments; live-player validation still runs during
flight. All earlier authored pickups and first progression-door checks remain.
No engine changes or arrival-tolerance relaxation. Exact removed steps,
script hash, APK hash and portal geometry are retained in trial artifacts.

The route remains a supply objective, not full mission completion. Only after
Vulcan acquisition may it quick-save, quick-load and assert the earned
position, inventory, difficulty and active lighting profile. Only after all
those checks pass may the wrapper export a hashed player-save archive for
continued ordinary-control investigation. The user's original player state
must still be restored byte-for-byte. Total 169 steps, existing 600-second
runner timeout. Record actual outcome below; no checkpoint currently claimed.

Run `f9157df0c13d41e98d597bf16ca6454d` FAILED at 111/169, 160784 ms,
while flying into segment 160. Final player shields -1.66573, six actual kills,
laser level 1, energy 86.5, two concussions, zero Vulcan ammo, no invulnerability.
Both bounded engagements recorded no-visible-target. The modified corridor
leg at step 150 never executed; this run provides neither positive nor negative
runtime evidence about that route change. No checkpoint archive was produced.
The wrapper restored original saved state byte-for-byte and stopped the app.

Repeated openings now obscure later-route experiments: the preceding run
reached 176 but this one died before the modified portion. Next establish an
earned ordinary save/reload checkpoint at the settled segment-41 waypoint,
before crossing the first trigger corridor. Preserve the full original-input
prefix, explicit alive/inventory/difficulty assertions, native save/reload
verification and an exported archive hash. Export must be conditional on that
checkpoint's own verified success, even if a later route fails, and must never
turn the later FAIL into a PASS. Compare save files with the initial backup so
an old user save cannot be mistaken for a newly earned checkpoint. Then use
that exact checkpoint for later-route experiments without grants or pose edits.
This does not replace full mission completion or any fidelity/admission gate.

### Earned opening checkpoint verification (2026-10-25)

Prepared `temp/xl-earned-opening-20261025`, keeping the ordinary-input prefix
through segment 41. At that point require survival, positive shields, laser
level 1, actual kills, Hotshot difficulty and prepared classic lighting. Then
quick-save, fly back to 71 with ordinary input, quick-load and assert return
to 41 with those conditions. This tests restoration rather than merely staying
in place. No grants, pose edits or difficulty changes. The 112-step checkpoint
test is a prefix verification only, not a narrowed mission-completion target.

Run `4860fe795eef4ff5b61229533743aea9` PASSED 112/112 in 168182 ms.
Final state was alive in 41, shields 40.91264, energy 86, six kills, laser 1,
zero concussions/Vulcan ammo and no invulnerability. Logcat confirms quick-save
completion and restoration from `Players/save_sets/single/player/drgpit/player.sg3`.
However, the wrapper FAILED export validation: its hash parser treated the
multi-line adb output as one item. It restored original state byte-for-byte,
so this run's earned save was not retained and cannot be used as a checkpoint.
Do not conflate the engine PASS with a successful checkpoint export.

Corrected parsing to split both initial and checkpoint manifests into lines
before matching changed quick-save hashes. Replayed the parser against the
captured manifests: exactly one newly written `drgpit/player.sg3`, SHA-256
`eb70557e377a9b66f32988262b536bcc47d9319482da9a06fc7dfc75a217a04f`.
Retest uses the unchanged gameplay script in the `retry` subdirectory; only
the wrapper parser changed. Successful retention still requires script PASS,
post-reload state checks, exactly one changed quick-save, exported archive
hash and verified restoration of original state.

Retry `b247f844217a435c9b776d7a2ee9a0c1` PASSED 112/112 in 168413 ms,
and the corrected wrapper completed export and restoration successfully.
Final post-reload state: segment 41, alive, 76 shields, energy 87, six kills,
laser level 1, zero concussions/Vulcan ammo, no invulnerability, Hotshot and
prepared legacy-fan profile. The movement to 71 before reload and subsequent
return to 41 passed. This state was earned by the retained original-input
prefix, not injected.

Retained evidence under `temp/xl-earned-opening-20261025/retry`:

- `earned-checkpoint.tar`, SHA-256
  `8fb0ccce41ff1da5ce6cc193aaf59aed62dd989359fd1313f18140ea4f5f3200`.
- Newly written `files/d2x-redux/Players/save_sets/single/player/drgpit/player.sg3`,
  SHA-256 `8925391a26f0f2ee45fa9571583218f6df559f437e8d200e41c3edf059ae2212`.
- `checkpoint-verification.json`: all 24 regular archive files hash-match the
  captured manifest; path traversal check passed; quick-save magic DGSS,
  version 48. Original and final restoration manifests match byte-for-byte.
- Script, run/result/step logs, post-reload introspection and changed-save
  manifest establish the provenance. Wrapper exited 0 and app is stopped.

The archive also contains pre-existing user files. For continuation, install
only the verified Dragon quick-save into an isolated backed-up test session;
do not replace unrelated saves from the archive. Reuse the same APK and original
mission package, enable the same prepared lighting profile, load through the
ordinary save path and verify checkpoint inventory/health/difficulty before
driving the remaining route. Keep each continuation result separate from this
opening-prefix PASS. Next exercise the previously untested continuous corridor
route from this checkpoint and reach the authored Vulcan supplies. Full mission
completion and all fidelity/admission gates remain open; browser support is
still unqualified.
