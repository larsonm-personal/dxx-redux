# Mission asset isolation study

Date: 2026-09-15

## Conclusion

Enabled level packages currently participate in the global asset search path
before a mission is selected. The launcher knows which entries are levels, but
that distinction does not control runtime asset visibility. This is a real
architectural route for one package to affect Counterstrike or another package.

The paired phone runs now identify **`ewithin-rebirth.zip/ewithin.dxa` as the
source of the reported replacement robot sound during Counterstrike**. It
supplies both `descent2.s22` and `descent2.ham` in the affected run. Both locally
available Maximum distributions lack sound samples and replacement sound banks.
The runtime evidence and archive inventory are below.

The recommended model is:

- Enabling a level package makes its missions available in mission selection
- Selecting a mission activates only that mission's assets, its package's
  explicitly shared assets, and declared dependencies
- Global mods remain an independent, deliberate choice
- Mission changes rebuild a defined asset state inside the engine, including
  cached sounds, metadata, textures, palettes, models, and music
- Returning to the main menu restores the base game plus global mods

This requires both package ownership and asset lifecycle work. Filtering the
launcher's path list alone would break in-engine mission selection; unmounting
archives alone would leave some previously loaded assets alive.

This document records source inspection and archive analysis, not an implemented
fix or a reproduced audio bug. No game code was changed. `adb devices` returned
no connected devices, so the affected installation's enabled packages, loaded
sample, and mission history could not be inspected.

## 1. What the current system actually does

### Launcher classification and staging

[`ModManager.ModInfo.isLevel`](app/src/main/java/com/dxxredux/app/ModManager.kt)
already recognizes `mission_zip` entries and entries categorized as `levels`.
The runtime launch filter, `enabledForLaunch`, checks enabled state and game
compatibility, including optional D1 missions in D2. It does not check the
currently selected mission or restrict level packages to discovery.

`writeEnabledModPaths` sorts all enabled entries, adds generated patch overrides,
then adds each package's paths to `.active_mod_paths`. It runs before entering
the engine from [`SetupActivity`](app/src/main/java/com/dxxredux/app/SetupActivity.kt).
There is no per-mission ownership in that path-file format.

[`MissionZipExtractionStore.activePathLines`](app/src/main/java/com/dxxredux/app/MissionZipExtractionStore.kt)
returns the extracted package root and every extracted DXA archive. Mission
constituents are generally projected beneath `missions/`; other files retain
their paths. Consequently, an unrelated package containing a root
`descent2.s22`, `descent2.ham`, `descent.sng`, or a recognized replacement path
can provide that resource without its mission being selected. Extracted nested
DXAs are also activated as global archives.

There is a second content path:
[`FileSetContentManager.buildLaunchPaths`](app/src/main/java/com/dxxredux/app/FileSetContentManager.kt)
adds enabled DXAs and a combined projection of enabled content. An isolation
change must cover this path as well as `ModManager`, including loose mission
imports and content obtained from multiplayer transfers.

### Native mounts and precedence

[`physfsx_android_setup_search_paths`](app/src/main/cpp/shared/physfsx_android_setup.c)
mounts the selected file set and then prepends the supplied mod paths, processing
the list backwards. Within that supplied list, earlier entries therefore win.
These mounts are established at startup and are not owned by a particular
`Current_mission`.

PhysicsFS resolves ordinary reads from the first matching search-path entry;
mounting a directory exposes its contents, not just its mission descriptors.
See the [official PhysicsFS search-path documentation](https://www.icculus.org/physfs/docs/html/).

Both engines also call
[`PHYSFSX_addArchiveContent`](../d2/misc/physfsx.c), which discovers root DXA files
and prepends them. The Android design must account for this automatic discovery
instead of assuming `.active_mod_paths` is the only archive admission path.
Already-mounted archives are not repositioned merely by mounting them again,
while newly discovered archives can change the effective precedence.

Selecting an add-on normally prepends its HOG, and Android also mounts the loose
directory beside its descriptor. These are useful mission-scoped mechanisms,
but they sit above the still-mounted roots of *all* enabled packages. A missing
resource in the active mission can therefore fall through to an unrelated
package. The mechanism is not limited to sound.

### Current teardown is partial

[`free_mission`](../d2/main/mission.c) closes the selected mission's conventional
HOG and invokes
[`physfsx_android_unmount_mission_directory`](app/src/main/cpp/shared/physfsx_android_shared.c).
It does not remove the launch-time package mounts or reset all loaded assets.
D1 has the corresponding hooks in [`mission.c`](../d1/main/mission.c).

The Android directory helper also discards its remembered directory regardless
of whether `PHYSFS_unmount` succeeds. That matters for a stronger isolation
contract: an open file can prevent unmounting. The
[official unmount contract](https://wiki.icculus.org/PhysicsFS3/PHYSFS_unmount)
requires closing those files and using the exact mounted source path.

The new mount owner should retain failed cleanup records and report failure,
rather than treating a requested unmount as completed cleanup.

## 2. What can be said about the Maximum sound report

### Confirmed phone reproduction

Source: `C:/Users/first last/Downloads/debuglog_20260915_111816.txt`, supplied
by the user on 2026-09-15. The first run had all mods/levels enabled and the
incorrect sound; the second had all unchecked and the correct sound.

| Evidence | All enabled: incorrect | All unchecked: correct |
| --- | --- | --- |
| Counterstrike level 1 context starts | 11:18:38.917, line 546 | 11:19:20.374, line 1329 |
| First logged sample 53 playback | 11:18:44.843, 5.926 seconds later, lines 921-923 | 11:19:24.698, 4.324 seconds later, lines 1676-1678 |
| `descent2.s22` source | `mods/.extracted_mission_zips/ewithin-rebirth.zip/ewithin.dxa`, line 231 | Base file-set directory, line 1107 |
| Sample name / bytes | `snipe_1.` / 16,367 | `snipe_1` / 24,098 |
| Sample FNV-1a 64 hash | `446c95505b42af56` | `9a629f9713cdff17` |
| Robot 37 see / attack mapping | `59:53` / `60:54`, line 636 | `59:53` / `60:54`, line 1412 |
| Read and cache checks | `read_ok=1`, `bank_match=1`, `cache_input_match=1`, `cache_output_match=1` | Same |

The event timing matches the user's report. Robot 37's see-sound mapping is
unchanged, but the selected bank supplies different sample bytes. The clean
sample hash also matches the independently inspected stock GOG S22 bank in
`SOUND_TRACE.md`. The diagnostic checks show no read failure, mutation from the
loaded sample, stale mixer input, or mutation of the converted mixer buffer for
this playback. They do not validate the final device audio output, but the
wrong asset source and changed sample already explain the observed difference.

The same DXA supplies `descent2.ham` at lines 229 and 288, whereas the clean run
uses the base directory at lines 1105 and 1144. This establishes broader asset
leakage; it does not establish that every robot/gameplay field differs.

Immediate workaround: disable `ewithin-rebirth.zip` and relaunch D2. A run with
only that package disabled and the other packages enabled remains a useful
check for additional overrides. The paired runs identify the winning source,
but do not prove that every other enabled package is harmless. The lasting fix
is the mission-owned asset activation and cache lifecycle described below.

### Earlier local archive inspection

I inspected archive member lists and parsed the inner HOG directories, including
the small HXM files. This did not require launching or modifying the packages.

| Local distribution | Relevant contents | Sound evidence |
| --- | --- | --- |
| `game_data/mission_files/descent_maximum_fixed.zip` | Two MN2 descriptors, two HOGs, one readme | No external sound bank, sound sample, HAM, or DXA |
| Its `max_f.hog` | 30 RL2 levels, 16 PCX images, 2 TXB texts | No sound samples or robot replacements |
| Its `maxlnk_f.hog` | 6 RL2 levels | No sound assets |
| `game_data/mission_files/d2xxl_downloads/maximum.7z` | Campaign/anarchy descriptors and HOGs, readme, D2X-XL generated caches | No external sound bank, samples, HAM, or DXA |
| Its `maximum.hog` | 30 RL2 levels, 16 PCX images, 2 TXB texts, 4 HXM files | Each HXM is 32 bytes with version 1 and zero replacement counts; no robot sound remapping |
| Its `maxlink.hog` | 6 RL2 levels | No sound assets |

For identification, SHA-256 of the inspected campaign HOG bytes:

```text
max_f.hog
c14dc0cf1e60b7300c104b86afc9b5eaed0e541478a2d02c950258c3e8b04536

maximum.hog
7335f638084f41535e191c78ec1ce57b53459dffdd17f1f8177416c846b8c011
```

These results rule out a directly supplied replacement sound in these particular
archives. They do not rule out a differently packaged Maximum installation,
another enabled mod, a robot sound-table patch, or stale state from an earlier
mission. A changed sound can come from a changed sound ID as well as changed
sample bytes.

The repository contains a `d2-hires-sounds.dxa`, but its presence on the host does
not show that it is enabled on the affected device or that its loose samples
are used by this runtime. The ordinary D2 path inspected reads the S11/S22 bank;
the `Sounds/*.r22` loader in `bmread.c` is not evidence that every bank-backed
sound is automatically replaced. Do not substitute another unverified culprit.

[`CustomAudioSetManager`](app/src/main/java/com/dxxredux/app/CustomAudioSetManager.kt)
manages jukebox playlists, not robot effects. The open Maximum MP3 metadata file
does not itself explain a robot sample change.

### Targeted diagnosis when the affected runtime is available

Capture the active file set, package manifest, application revision, current
mission, and whether Maximum was previously played in the same process. Then:

1. Record `physfs.search_path`, including directory mounts. `mounted_mods` alone
   is insufficient: the current introspection field only lists `.dxa` paths
2. For the affected robot, record its numeric type and event: see, attack, claw,
   death, etc.; record the corresponding `Robot_info` field, `Sounds` mapping,
   final sample index/name, sample rate, length, and sample-byte hash
3. Record the actual loaded sound-bank origin and hash, HAM origin, applied HAM
   patch owners, HXM origin, and any D1 compatibility/custom sound state
4. Compare fresh Counterstrike runs with the same settings and Maximum enabled
   versus disabled; then compare Counterstrike -> Maximum -> Counterstrike in
   one process
5. A fresh-run difference points toward visibility/configuration; a difference
   only after switching points toward retained state. Matching sample bytes
   with changed IDs points toward metadata; matching IDs and bytes directs the
   investigation toward playback/event behavior

Add targeted `debug_log` diagnostics and introspection before speculative code
changes. Existing robot preview sound information is a useful starting point,
but the gameplay diagnostic needs *loaded* provenance: asking PhysicsFS where a
file resolves now cannot prove where an older cached sample came from.

## 3. Define scope independently of packaging

The hard question is more manageable if archive format, content type, and
activation scope are separate properties. ZIP, 7z, HOG, and DXA are containers;
none inherently means "global mod".

| Content | Default activation | Reason |
| --- | --- | --- |
| A package with playable mission descriptors | Its selected mission and declared shared resources | Installing campaigns should not alter other campaigns |
| DXA explicitly categorized as levels | Mission-scoped | Existing user-facing classification should have runtime meaning |
| A standalone replacement mod without missions | Global when enabled | This is the user's deliberate global modification |
| Optional add-on for a particular campaign | Explicit target mission/package IDs | It should follow that campaign automatically |
| Archive containing a campaign and optional global improvements | Separate components with separate enablement | An incidental bundled mod should not become global silently |
| Collection containing several campaigns | Selected campaign plus explicitly shared assets | Selecting one campaign must not activate sibling HOGs or sibling-specific assets |
| Jukebox/music collection | Explicit music preference | Selecting music is independent of granting gameplay asset overrides |

Use the existing importer/native mission inventory for detection. Do not build
a second MN2/MSN/HOG parser in Kotlin. Folder names, titles, archive extensions,
and robot counts are useful hints, not authority for global asset scope. The
existing single-player/anarchy mission-intent classification answers a different
question and should not determine whether assets are global.

Recommended stored concepts:

- Stable package/component ID and immutable content hash
- Game applicability, activation scope, explicit dependency IDs
- Mission identity: owner ID plus descriptor-relative path and game version
- Selected mission's constituent files/HOGs and optional per-mission overrides
- Explicit package-shared resource roots and nested archive ownership
- Classification origin: manifest, importer inference, or user override
- Resource capabilities needed for admission and reload planning

For legacy ambiguous collections, scope everything to the package first to
prevent leakage into other packages. Associate matched mission HOGs and known
sidecars only with their mission. Treat the remaining truly shared root as
package-shared, while surfacing unresolved collisions for review. This is an
explicit conservative fallback, not proof of author intent. If two sibling
campaigns require conflicting copies of the same unassignable resource, require
an association or keep that component unavailable; do not guess by filename
ordering. Explicit manifests can resolve the ambiguity.

The UI can remain simple: "Available missions" for level packages and "Apply to
all missions" for global mods, with an advanced scope override and an explanation
of what was inferred. Imported nested DXAs inherit their owner's scope unless
explicitly split into independently enabled components.

## 4. Separate mission discovery from asset resolution

### Recommended approach: a catalog plus one active asset context

At launch, publish an immutable catalog for all enabled missions and mount only
the base game, selected global mods, and discovery metadata. Keep level package
payloads on disk but outside the root asset search path.

The catalog tells the engine how to activate any mission the in-engine picker
offers. This removes the need for a launcher round trip when changing missions.

A descriptor-only projection is a practical way to preserve much of the existing
mission enumeration code. Give every entry a unique catalog path and carry its
owner separately into mission loading. When selected, open the real descriptor
and resources through that owner. Do not merely relocate the descriptor and
assume that its adjacent files will still resolve correctly.

This changes an important existing assumption:
[`mle`/`Mission`](../d2/main/mission.h) currently carry a relative path and filename,
but no package owner. Two archives can supply the same virtual path, and
`load_mission_by_name` compares only the filename. D2's loop can even attempt
multiple matching entries. Preserve owner identity across selection, launching,
save lookup, and multiplayer resolution. Titles and short filenames should
remain display/legacy lookup values, not the new unique key.

Where upstream save/demo/network formats must remain unchanged, keep the legacy
identifier and use launcher/native companion metadata or an explicit resolver.
Reject ambiguous legacy matches instead of silently selecting another package.
For Android's disposable launcher formats, replace the format directly; no
compatibility migration is needed under the repository instructions.

Avoid mounting every package under `missions/` as a shortcut: that still exposes
payloads, creates duplicate-name issues, and cannot express per-mission shared
asset ownership. Private namespace mounts are an alternative, but require
careful lookup/ownership plumbing. PhysicsFS also does not support mounting the
same source twice at different mount points through ordinary `PHYSFS_mount`;
see its [mount API contract](https://www.icculus.org/physfs/docs/html/physfs_8h.html).

### Alternatives and tradeoffs

| Approach | Assessment |
| --- | --- |
| Filter launch paths to one mission | Small change, but insufficient for in-engine switching and complete mission discovery |
| Rebuild global mounts whenever a mission is selected | Useful only when paired with ownership, cache reset, failure handling, and discovery separation |
| Catalog plus scoped PhysicsFS mounts | Recommended: reuses engine readers and existing mission HOG behavior with bounded hooks |
| Replace every resource lookup with a custom virtual filesystem | Maximum control, but broad D1/D2 churn and upstream maintenance cost |
| Restart the engine for every mission | Stronger cleanup boundary, but changes the in-engine experience and complicates saves/networking; reserve for explicitly unsupported startup-only content |

## 5. Precedence needs two rules

### File lookup precedence

For ordinary mission resources, use this highest-to-lowest order:

1. Explicit user overrides targeted at the active mission
2. Active mission's authored files and selected HOG
3. Its package-shared assets and explicitly ordered dependencies
4. Enabled global mods in the user's order
5. Base game assets

No inactive package participates, including as a fallback for missing files.
Within the authored mission layer, preserve the existing HOG-versus-loose-file
behavior unless the package declares otherwise. Report ambiguous dependency
collisions instead of depending on archive enumeration order.

Keep session configuration and engine UI assets in their own base/global
context. A campaign's generic `descent.txb` or font filename should not replace
the already-running engine's menus by accident. Mission briefing text/images,
endings, movie dependencies, and soundtrack files belong to the mission context.
Packages intentionally modifying startup/UI assets need explicit capability
support; an unknown startup-only asset must not be promoted to global visibility
as a workaround.

### Semantic replacement precedence

Search-path ordering is necessary but insufficient. A global high-resolution
PNG and an authored POG bitmap can represent the same texture while having
different filenames. A HAM patch can change a robot sound ID without supplying
the sample. Native replacement loaders must respect ownership at the resource
level too.

Default policy: preserve authored mission replacements. Allow compatible global
enhancements to fill untouched base resources; allow explicit targeted overrides
to replace authored resources. Track which bitmap/model/sound slots the mission
changes so a later global replacement path does not silently undo them. This
policy needs tests across formats, not just two files with the same name.

### Generated patches and music

`writeGeneratedPatchOverrides` currently composes patches across the enabled
list. Composition and conflict detection must instead operate on the effective
base/global/selected-mission dependency set. Otherwise an inactive patch can
survive as generated output after its source package is unmounted, or unrelated
packages can falsely conflict with each other.

Generate immutable patch outputs per effective context, preserving each patch's
base preconditions. Do not overwrite one shared patch directory underneath a
running engine. A mission containing a replacement HAM may require different
global-patch compatibility decisions from Counterstrike.

Music requires the same scope. `MissionZipExtractionStore` can promote a single
mission song list to root `descent.sng`; that generated file must inherit the
mission/package owner. [`songs_init`](../d2/main/songs.c) rereads song lists for
each song, so a globally visible inactive song list can win even without a stale
cache. Preserve the existing user choice of mission soundtrack versus jukebox,
but resolve "mission soundtrack" from the selected mission. Music decode caches
can remain on disk with content-based keys; playback handles cannot survive an
owner's teardown unnoticed.

## 6. Where and when to switch in the engine

### Lifecycle

```text
Base + global mods / main menu
    |
    | choose mission, load save, start demo, or accept multiplayer mission
    v
Resolve owner and dependencies; validate candidate
    |
    | close old consumers; release old assets; detach old mission mounts
    v
Mount candidate context; rebuild mission data and required caches
    |
    | publish successful mission context
    v
Briefing -> level -> secret levels -> ending
    |
    | campaign exit after consumers finish
    v
Release mission assets and mounts; restore base/global menu context
```

Browsing or highlighting a mission does not activate it. A dedicated preview
uses its own request/worker context. Opening the pause menu retains the active
mission because gameplay can resume. Leaving the campaign retains its context
until the final briefing/ending/movie consumer finishes, then returns to the
base/global menu context.

Selecting a mission begins activation before its HAM, sound bank, briefing, or
movie is read. Hook the common `load_mission` path and its cleanup boundary, not
only the mission-picker callback or `StartNewGame`. Audit built-in D1, shareware,
and OEM early returns; they bypass the normal tail of D2 `load_mission`.

Other real entry points include `load_mission_by_name` from
[`state.c`](../d2/main/state.c), [`newdemo.c`](../d2/main/newdemo.c), and
[`net_udp.c`](../d2/main/net_udp.c), current-directory loads, automation, and
native metadata requests. D1 needs equivalent coverage. Loading a save's level
state must happen after its mission asset context has been established.

For ordinary level/secret-level changes within one mission, keep the package
context and replace only level-specific assets. Rebuild the full context when
the owner, content revision, dependencies, global configuration, or game
compatibility mode changes.

### Transition ordering and failure

The current engines use mutable globals, and `load_mission` frees the previous
mission before validating the new descriptor. This is not already a transaction
that can restore an old playable game by swapping one pointer.

Use an explicit transition coordinator on the engine thread:

1. Resolve and preflight the candidate without exposing its assets globally
2. Stop gameplay/preview consumers, digital channels, and mission music; drain
   or cancel resource jobs and callbacks that retain old data
3. Undo D1/custom replacements while their saved backing data is still valid;
   free dependent model/texture/sample state and close persistent archive handles
4. Unmount exact owned paths in reverse dependency order; verify the result
5. Mount the candidate's resources and context-specific generated outputs
6. Rebuild base metadata under the new context, apply its patches/dependencies,
   initialize mission sounds/presentation, and validate successful loading
7. Publish the new context generation and allow briefing/gameplay consumers

Preflight errors leave the prior state untouched. A failure after teardown should
release the partial candidate and reconstruct a known base/global menu state.
Do not promise resumption of the old live level without a separately designed
checkpoint/restore mechanism. If an archive cannot be released or baseline
reconstruction fails, stop the transition and report the error; do not continue
with mixed owners. Normal users should stay inside the engine for supported
mission switches.

Carry exact mount ownership and reference counts for shared dependencies.
Re-resolving an old virtual HOG filename after changing mounts can identify the
wrong physical source. Preserve source identities acquired at mount time.

## 7. Asset reload audit

An asset-context generation is a monotonically changing runtime value identifying
the effective asset set. Initially, invalidate all affected caches when it
changes. Optimize by per-resource fingerprints only after correctness is proven.
Include misses as well as successful lookups in invalidation.

| Resource | Current behavior/evidence | Required transition behavior |
| --- | --- | --- |
| D2 robot/weapon/texture metadata | `load_mission` rereads HAM; `bm_read_all` applies DXA patches | Rebuild from the selected baseline, then only effective patches/dependencies; handle early-return paths |
| D2 sound-bank bytes | `load_mission_ham` reloads only when `sndfile_dir_changed` detects a new `PHYSFS_getRealDir` value | Compare context/source identity, filename, content revision, and sample format; directory equality alone is insufficient |
| Converted digital samples | `SoundChunks[]` is cached by sound index in SDL_mixer; D2 has `digi_free_cached_sounds` | Halt channels before freeing data; invalidate converted samples when bytes or mappings change |
| D1 sound/custom data | `custom_remove` restores saved samples; `load_custom_data` reads PG1/DTX/HX1; D1 mixer also caches by index | Restore custom state before releasing backing banks; add an explicit D1 converted-sample reset equivalent |
| D1-in-D2 assets | `d1_custom_remove` and `d1_in_d2_apply_*` run during level loading | Coordinate sound/bitmap/robot/effect restoration with bank teardown; test both directions independently |
| D2 PIG bitmaps | `piggy_new_pigfile` can return early when the filename matches and no POG replacement is present | Invalidate filename-only reuse when the source context changes; close `Piggy_fp` before removing its owner |
| POG/HXM replacements | Level loaders remove/reapply replacements; robot reset can reread mission HAM | Preserve reset ordering; do not reread the outgoing mission while establishing the incoming baseline |
| Palette and remapped UI | `load_palette` tracks palette/PIG filenames and skips matching names | Invalidate names, reload palette data, refresh dependent remaps and menu backgrounds |
| Hires textures and lookup caches | OGL has path/lookup caches and GPU texture cleanup | Invalidate native and JVM lookup caches, GPU resources, merged textures, and outstanding loads at context changes |
| External models | `xmodel_load_all` fills only empty entries; `xmodel_free_gl_all` releases only GPU state | Release/reload model objects as well as GPU data when their sources change |
| Music and movies | Song lists reread; playback/movie consumers can retain handles/state | Stop and close old streams, rebuild selected song names/list and scoped movie resources |
| Startup text/fonts/UI | Loaded before mission selection in `inferno.c` | Keep these pinned to session base/global scope unless a deliberate reload contract exists |
| Metadata/preview/route-derived caches | Work can outlive one file lookup | Key by effective asset fingerprint and reject results from an obsolete context |

Primary source locations:

- [`d2/main/piggy.c`](../d2/main/piggy.c): `sndfile_dir_changed`, `read_sndfile`,
  `piggy_read_sounds`, `piggy_new_pigfile`
- [`d2/arch/sdl/digi_mixer.c`](../d2/arch/sdl/digi_mixer.c) and
  [`d1/arch/sdl/digi_mixer.c`](../d1/arch/sdl/digi_mixer.c): `SoundChunks`
- [`d1/main/custom.c`](../d1/main/custom.c),
  [`d2/main/d1_custom.c`](../d2/main/d1_custom.c),
  [`d2/main/gameseq.c`](../d2/main/gameseq.c)
- [`d2/main/gamepal.c`](../d2/main/gamepal.c),
  [`d2/arch/ogl/ogl.c`](../d2/arch/ogl/ogl.c),
  [`d2/xmodel/xmodel.cpp`](../d2/xmodel/xmodel.cpp)
- [`d2/main/dxa_metadata_patch.cpp`](../d2/main/dxa_metadata_patch.cpp): patch
  application, sound/robot field changes, virtual bitmap bookkeeping

These are concrete audit targets, not claims that every row is independently
causing the reported sound. For example, the filename-only palette cache is
relevant to safe package switching, but does not diagnose the separate Maximum
blue-palette bug in the outstanding list.

## 8. Multiplayer, metadata, and diagnostics

Use one effective-context resolver for gameplay and metadata analysis, with
different policies about optional visual overrides where appropriate. Existing
[`LevelMetadataRequestMounts`](app/src/main/cpp/jni_level_metadata.cpp) already
tracks request-owned paths and checks cleanup. Reuse that ownership discipline;
it is not proof that all gameplay caches are restored. Avoid concurrent mutation
of the same process-global engine/PhysicsFS state by preview and gameplay jobs.

For multiplayer, establish mission ownership before native short-name lookup,
using the existing content-identity/transfer machinery. Required dependencies,
HAM/HXM changes, and gameplay-affecting sound mappings must be included in
compatibility decisions. Optional cosmetic/audio preferences should remain
separate where the engine permits them. Reevaluate conflicts for the selected
mission, so installing two unrelated campaigns does not make them incompatible.

Extend [`game_introspect.cpp`](app/src/main/cpp/shared/game_introspect.cpp) with:

- Active mission owner/path, context generation, effective content fingerprint
- Ordered mounts with owner, role, mount point, and dependency relationship
- Loaded HAM/sound-bank/PIG origins and content fingerprints
- Selected robot's sound mapping and loaded sample provenance
- Texture/model replacement owner, including why a candidate was masked
- Last transition result, outstanding consumers, and failed unmounts

This makes "why is this asset playing?" answerable without listening to samples
or guessing from package names.

## 9. Implementation sequence

### Selected product behavior and concrete regression case

The user has selected automatic isolation. A checked level pack is available
in mission selection; it is not an active global mod. Launch and main-menu
assets come from base/global scope. Starting a mission activates its owning
pack automatically, before any mission HAM, sound bank or briefing is read.
Counterstrike selects the base owner. Switching inside the engine requires no
launcher visit or manual checkbox changes.

Only one independent level pack is active at a time. The selected mission's
package-shared assets and explicitly declared resource dependencies belong to
that single context; a dependency does not implicitly activate another campaign
or all sibling HOGs. Standalone global mods remain independently enabled.
Extracted DXAs and generated resources cannot escape their owning pack's scope.

Use the phone example as the first end-to-end acceptance case, with the original
collection left enabled throughout:

| Step | Required result |
| --- | --- |
| Launch, browse missions, cancel selection | ewithin remains discoverable; its HAM/S22 never enter the base/menu context |
| Start or restore Counterstrike level 1 | Base HAM/S22; robot 37 see maps `59:53`; stock sample 53 hash `9a629f9713cdff17`, 24,098 bytes |
| Select ewithin in the same engine process | Its owner and nested DXA activate; authored HAM/S22 load successfully rather than being universally blocked |
| Return to Counterstrike in that process | Base HAM identity, metadata and S22 samples restored; no ewithin-owned cache, mount, patch or generated override remains |
| Switch ewithin -> Maximum -> ewithin | Maximum's missing HAM/S22 fall back to base/global assets, never the previously active ewithin pack; ewithin's resources return when selected again |

The bad Counterstrike sample is already known: `snipe_1.`, 16,367 bytes,
FNV-1a 64 `446c95505b42af56`, from `ewithin.dxa`. Use it as a positive provenance
control after validating the exact local fixture, not as a forbidden hash:
that sample is legitimate when its authored mission is in use. Capture the
package and HAM hashes during fixture setup; the phone trace records HAM origin
but does not supply a whole-HAM hash. Match baselines to the fixed base file set
and global-mod configuration rather than requiring stock hashes for every user.

Automate both source assertions and loaded-state checks. Identical robot sound
IDs did not prevent this bug, so checking mappings alone is insufficient. Fresh
process tests establish admission correctness; same-process switches establish
cleanup and reload correctness. Include a Counterstrike save restore because
the user's symptom also occurs on loaded levels. Preserve the engine process
identity in test output so a silent restart cannot make switching tests pass.

The actionable checklist is in the
[implementation plan](ai%20tool%20plans/2026-09-15-mission-asset-isolation-study.md).

### Phase 1: establish evidence and observable invariants

Sound provenance logging and the paired phone reproduction are complete.
Add owner/generation diagnostics for the new context and preserve this evidence
in reusable regression tests. Build deterministic small
fixtures containing deliberately colliding sound banks, textures, and song lists.
Do not use Maximum as the only sound fixture: the inspected packages do not
replace sounds.

### Phase 2: package catalog and scoped activation

Replace the launch contract with global mounts plus a mission catalog. Preserve
owner identity across the in-engine picker, current-directory loads, and legacy
name resolution. Stage mission payloads and nested DXAs by owner. Cover both
`ModManager` and `FileSetContentManager`, generated patch outputs, and automatic
DXA discovery. Enforce mount limits on the effective set, rather than charging
every installed campaign against the current 64-entry active path limit.

### Phase 3: complete D2 lifecycle, then D1 and D1-in-D2 parity

Implement the coordinator and explicit reload/reset operations. Prove sound-bank
and mixer reset first, then metadata/PIG/palette/POG/HXM/model/texture/music
behavior. Bring all mission entry/exit/error paths under it before declaring
isolation complete. Keep shared coordination in `android/` with narrow hooks in
both engines, preserving non-Android builds.

Phases 2 and 3 are one release boundary: shipping mount switching without cache
cleanup would introduce history-dependent behavior.

### Phase 4: semantic precedence and ambiguous collections

Apply ownership to cross-format replacement decisions, per-mission patch
composition, dependencies, and user scope overrides. Verify multiple missions
inside one archive and mixed campaign/global-mod bundles. Document any genuinely
unsupported startup-only modifications; do not conceal them through global
fallbacks.

### Phase 5: integration and optimization

Run the matrix below through the existing serial emulator automation runners,
then the relevant JVM/native checks and Windows build. Add reusable game scripts
under `android/game_scripts/` and use the scoped code-quality invocation for any
implementation. Measure transition time and memory after repeated switches.
Only then retain caches selectively based on effective content fingerprints.

## 10. Acceptance matrix

| Scenario | Required evidence |
| --- | --- |
| Fresh Counterstrike with A and B installed/enabled | Neither inactive package owns a loaded asset; enabling/disabling them changes mission availability only |
| Counterstrike -> A -> Counterstrike, one engine process | Final sound bytes/mappings, palette, textures, models, and song-list sources match the initial effective baseline |
| A -> B -> A with identical filenames | Each resolves its own content, including negative cache entries and same-named PIG/palette resources |
| Several missions within A | Sibling HOGs and sibling-specific overrides remain inactive; declared shared assets work |
| A containing a nested DXA and generated patch/song list | Generated and nested resources activate and deactivate with their owner |
| Mission-authored POG plus global hires replacement | Authored resource wins unless explicitly overridden by a targeted mod |
| Two inactive packages patch the same metadata field | They do not conflict until the effective dependency set actually combines them |
| Mission selection canceled; pause menu opened | Browsing does not change owners; pause preserves the playable mission context |
| Mission failure, missing dependency, failed unmount | No partially active context; verified previous state or reconstructed baseline, with a diagnostic |
| Same path with changed content or sample rate | Source revision/format changes invalidate data and converted caches |
| D1 native PG1/DTX/HX1 transitions | Original samples, converted samples, bitmaps, and metadata restored |
| D2 -> D1-in-D2 -> D2; D1 shareware/Mac/OEM variants where available | Compatibility backups and built-in early returns do not retain foreign assets |
| Save restore, classic demo, automation, multiplayer host/join | Correct owner activated before level state is consumed; ambiguous legacy names rejected |
| Metadata/preview A -> base -> B | Request mounts released and derived output keyed to its effective context |
| Repeated 20-cycle mission switching | Stable owned-mount counts, open-handle counts, and bounded memory; no stale asynchronous results |

Existing `test_mod_loading.jsonc` checks global hires texture loading; it does not
prove sound isolation or mission switching. Existing
`test_level_metadata_request_mount_scope.jsonc` covers sequential metadata
requests; retain it, but add live gameplay coverage. A fresh process for every
test would miss the central lifecycle problem.

## Decision

Proceed with **catalog-based mission discovery and mission-owned asset contexts**.
Use the existing level classification as the initial default, separate global
mods explicitly, and make mission activation the common engine boundary for
all launch paths. Treat loaded-state restoration and resource-level precedence
as part of the feature, not later cleanup.

The paired phone logs identify `ewithin-rebirth.zip/ewithin.dxa` supplying the
replacement sound bank and HAM during Counterstrike. The package-isolation
redesign now has a concrete reproduction fixture; preserve these source and
sample checks when validating the fix.
