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
It is not proof that the names map to sounds actually used by its boss.

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
