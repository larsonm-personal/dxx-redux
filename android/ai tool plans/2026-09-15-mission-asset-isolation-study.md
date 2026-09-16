# Mission asset isolation study

## Request

Study how enabled level packages can override assets in unrelated missions,
and design automatic isolation that supports switching missions inside the engine

The 2026-09-15 phone reproduction identifies `ewithin-rebirth.zip/ewithin.dxa`
supplying Counterstrike's replacement sound bank and HAM. The user has selected
automatic isolation: enabled level packs remain available, but only the pack
owning the mission in use supplies campaign assets

## Plan

- [x] Trace launcher package classification, staging, and native search path setup
- [x] Inspect available Maximum package evidence and asset override behavior
- [x] Trace D1/D2 mission selection, asset caches, and failure/cleanup paths
- [x] Recommend package scope, precedence, mission lifecycle, and verification
- [x] Write a source-linked study with confirmed findings separated from hypotheses

## Scope

Implementation started at the user's request. Preserve the user's outstanding
bug edits. Native activation remains pending until scoped mounting and cache
teardown can be connected together

## Result

[Detailed study](../MISSION_ASSET_ISOLATION_STUDY.md)

- Confirmed that level classification does not scope launch-time asset mounts
- Inspected both local Maximum distributions and their HOG contents; neither
  supplies replacement sounds, and the four HXM files in maximum.7z have zero
  replacement counts
- Recommended catalog-based discovery, explicit resource ownership, and a
  complete native mission transition lifecycle
- Verified all 27 local study links and ASCII/whitespace formatting
- Initial study had no connected-device reproduction; the subsequent paired
  phone log `debuglog_20260915_111816.txt` confirms the winning package and sample
- No builds run because only documentation was added
- Repository-wide diff checking found pre-existing trailing whitespace in the
  user's outstanding_bugs.md edit; that file was left untouched

## Agreed behavior

- A level-pack checkbox controls mission availability, not global asset mounting
- Launch and the main menu use base assets plus explicitly enabled standalone mods
- Selecting a mission automatically activates its owner; the player does not
  manually uncheck other packs or return to the launcher
- At most one independent level pack supplies gameplay assets at a time. Its
  selected mission, package-shared resources, and explicit resource dependencies
  form one context; dependencies must not activate unrelated sibling campaigns
- Built-in Counterstrike has no external level-pack owner
- Nested DXAs, generated patches, song lists, and projected files inherit their
  package owner. Archive extension never grants global scope
- Normal level/secret-level changes and pause retain the owner. Campaign exit
  releases it after ending/media consumers finish
- Saves, demos, multiplayer and automation use the same activation boundary

## Implementation checklist

### 1. Turn the phone example into a regression fixture

- [x] Record source, timing, mappings and sample hashes in the detailed study
- [x] Locate/import the exact `ewithin-rebirth` package and record content hashes
  for the ZIP, nested DXA, HAM and S22; do not assume another release is identical
- [ ] Establish a clean Counterstrike baseline with a fixed base file set and
  fixed global mods. Preserve sample 53 hash `9a629f9713cdff17` for the inspected
  stock bank; measure baseline HAM identity and relevant metadata separately
- [ ] Add a reusable serial emulator runner and game script that enables both
  packs, selects missions through the native picker, and records context/source
  diagnostics. Assert that the same engine process survives mission switching
- [ ] Add tiny synthetic colliding packages for deterministic tests of sound,
  metadata, images and music without redistributing the real campaign payload

### 2. Catalog and owner resolution

- [ ] Reuse native/importer mission inventory to classify packs automatically
  and associate each mission with an owner ID, descriptor path and content revision
- [ ] Replace the launch contract with base/global mounts and an immutable catalog
  of enabled missions. Keep inactive payloads outside the root asset search path
- [ ] Carry owner identity through picker entries and all mission lookup paths;
  reject ambiguous legacy short names instead of choosing search-path order
- [ ] Scope extracted nested DXAs and loose roots in `ModManager`, file-set
  projections in `FileSetContentManager`, and native DXA autodiscovery together
- [ ] Generate patches/song lists for the effective owner set; retain ownership
  through derived files so unmounting the original ZIP cannot leave overrides behind
- [ ] Associate sibling HOGs/sidecars individually in multi-mission collections;
  keep unresolvable conflicting components unavailable with a concrete diagnostic

### 3. One engine-thread transition boundary

- [ ] Add shared Android coordination with narrow D1/D2 `load_mission` and cleanup
  hooks, covering built-in/shareware/OEM early returns as well as normal missions
- [ ] Resolve and preflight the candidate before changing the current context
- [ ] Stop channels/music, close streams, drain resource jobs and release caches
  before unmounting their owners. Restore custom-data backups while still valid
- [ ] Detach exact owned paths, check unmount results, mount the selected owner,
  rebuild HAM/banks/patches and presentation state, then publish a new generation
- [ ] Reset all affected caches initially, including filename-only reuse and
  negative lookups; cover the complete asset audit in study section 7
- [ ] Preserve package state across ordinary level changes, but reset level-owned
  POG/HXM/custom assets correctly
- [ ] On preflight failure retain the prior state; after teardown failure rebuild
  a known base/global menu or stop safely if that cannot be verified

### 4. Prove both absence and activation

- [ ] All packs enabled -> fresh Counterstrike: base HAM/S22 selected; no inactive
  pack-owned mounts, patches, generated files or loaded resources
- [ ] Same setup -> load Counterstrike save: identical asset baseline before
  restoring level state; the first robot still uses the correct sample
- [ ] Counterstrike -> ewithin -> Counterstrike in one process: the nested DXA
  contributes while ewithin is active, and is fully absent on return
- [ ] ewithin -> Maximum -> ewithin in one process: Maximum uses its own authored
  resources plus baseline fallbacks, never ewithin's HAM/S22; ewithin reactivates
- [ ] Select/highlight/cancel without starting: active owner remains unchanged
- [ ] Ordinary and secret levels, ending, main menu, save/demo and multiplayer
  paths obey the same lifecycle; test D1 and D1-in-D2 restoration separately
- [ ] Missing dependency, failed activation and forced unmount failure leave no
  mixed owner state; repeated 20-cycle switching has bounded memory/handle counts
- [ ] Keep standalone global-mod coverage and authored-resource precedence tests
  so isolation does not disable valid global enhancements or campaign replacements

### 5. Release gate

- [ ] Run scoped formatting, relevant JVM/native tests, complete serial Android
  integration runs, Android builds and Windows D1/D2 builds
- [ ] Record selected source identities and sample checks, not just audible results
- [ ] Obtain a confirming phone run with the original collection still enabled

Catalog admission, scoped mounting, generated-resource ownership and cache
teardown ship together. A startup-only filter, S22-only fix, filename blacklist,
or special case for ewithin does not satisfy this plan. Optimize cache retention
only after the complete switching matrix passes

## Implementation checkpoint: package ownership catalog

- Added `MissionLaunchCatalog.kt` with owner/descriptor/game-qualified identities,
  package revisions, resource fingerprints, selected-mission resource filtering,
  case-insensitive conflict detection and rejection of ambiguous legacy names
- Added `ModManager.buildMissionLaunchCatalog` using the existing mission scanner
  and verified extraction records for enabled level packs. Standalone global mods
  are outside this mission-only catalog
- Preserve nested DXA ownership, sibling sidecars and variant-directory ownership;
  excluded variants cannot reenter the selected context as shared resources
- Added source-entry provenance for generated files, including the `descent.sng`
  alias. Extraction manifest v5 rebuilds old cached records once rather than
  guessing the origin of existing generated output
- Added importer-to-catalog tests using synthetic ewithin-style packages and the
  local `ewithin-versions.zip` and `descent_maximum_fixed.zip` distributions
- This checkpoint does not switch native mounts or fix the phone symptom yet.
  The current launcher path publication still uses the existing implementation

Next integration work: bring loose file-set content into the same ownership model,
publish descriptor-only discovery and effective-context resources, then connect
the native transition coordinator and complete the asset-reset audit. These are
required before enabling the catalog as the gameplay launch contract

### Confirmed local fixture

The real-package test imports `game_data/mission_files/ewithin-versions.zip`
through the existing variant selection. The chosen `ewithin-rebirth.zip` has:

| File | SHA-256 |
| --- | --- |
| `ewithin-rebirth.zip` | `bf8e9f58ef6993f44df5b94f38efd0235bb407485267948a4d0eaaa0a6aa8c84` |
| `ewithin.dxa` | `7602af078a17c4e419e61a857e6691b4099de622fd53643b68aaa8b7414a2ebb` |
| Its `descent2.ham` | `912e5fa5520e29ae0f33ccbbd739bc0fff1b55cf5958cb4990792d8bfcfc8752` |
| Its `descent2.s22` | `67444f8089b684c3a87f0165c9b170657f4ad96d26fed6694ede04cbd9b584bc` |

Directly inspected S22 sample 53: `snipe_1.`, 16,367 bytes, FNV-1a 64
`446c95505b42af56`, exactly matching the affected phone playback. The phone log
does not include whole-file hashes, so this matches the observed sample without
claiming a verified byte-for-byte match of the complete phone package

### Checkpoint validation

- Scoped code-quality checks passed
- Gradle `:app:testDebugUnitTest` passed with filters `MissionLaunchCatalogTest`,
  `ModManagerMissionZipTest`, and `MissionZip*`, including the local real-pack test
- Catalog tests cover enabled/disabled ownership, selected DXAs, duplicate legacy
  names, sibling resources, excluded variants, generated song provenance and
  repeated pure catalog selection. These do not constitute engine switching tests
- No native code changed in this checkpoint; native builds and emulator mission
  switching remain required for the integration checkpoint
