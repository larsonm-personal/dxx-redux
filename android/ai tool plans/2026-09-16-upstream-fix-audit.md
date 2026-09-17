# Upstream fix audit and Vertigo robot loading

Follow-ups: the deferred PNG/menu change `ceba2c7a` is implemented and validated in [the PNG/menu backport](2026-09-16-png-menu-backport.md), and the D1 monitor fix `86dcf9ca` in [the monitor backport](2026-09-16-d1-monitor-backport.md). Cumulative coverage is 22 implemented, two already covered, and one remaining deferral (Autoselect Only Once). The original batch findings below are retained as its audit record

## Scope and plan

Research the mission 2305 warning, trace the matching upstream fixes, and compare every upstream commit absent from this branch with the current implementation

- [x] Establish upstream revision and identify the mission warning and robot fixes
- [x] Trace level loading, exit model replacement, and robot verification in this branch
- [x] Review every outstanding upstream commit for applicability and existing equivalents
- [x] Record backport recommendations, dependencies, and validation needs

The initial audit was research-only. The user subsequently requested broad backports that preserve future upstream mergeability

## Implementation plan

- [x] Apply independent upstream fixes with original patch structure wherever possible
- [x] Adapt robot asset reload and property verification without repeated bookkeeping
- [x] Integrate applicable desktop/build and graphics fixes while preserving Android behavior
- [x] Add and run meaningful native integration coverage for asset reload and boundary cases
- [x] Run scoped quality checks, D1/D2 host builds, and relevant Android builds/checks
- [x] Record exact upstream coverage, adaptations, and any deferred changes

## Result

Implemented 20 of the 25 outstanding upstream commits. Two were already covered locally; three remain deferred. Both Vertigo fixes and terrain bitmap cleanup are included, with integration coverage exercising the actual asset loaders across repeated exit/HXM/V-HAM transitions

Both Windows games and Android arm64 native libraries build successfully. All 107 host tests pass (D1: 50, D2: 57), as does the real Vertigo metadata regression script. Full Strike has not been playtested through a rendered exit

## Implemented scope and merge notes

The table below records the original audit recommendations, not the final implementation status. Final coverage is:

- Implemented: `9cdd9399`, `86923251`, `fe74f729`, `ef73945b`, `79a155f5`, `62802aa6`, `c6e7b87b`, `8be8929e`, `8cb8e624`, `7a66b717`, `eeeeafdc`, `9f8d4693`, `83354a4c`, `029a67c2`, `83fb8760`, `338d7bd3`, `29b77fc6`, `6e76c5d8`, `4b2e777e`, `9fd90f03`
- Already covered, retained: `069eaff5` (unterminated text lines), `917704d3` (Windows absolute paths)
- Deferred: `ceba2c7a` (PNG/menu changes), `86dcf9ca` (D1 monitor handling), `8537b01e` (Autoselect Only Once)

Original patch structure and upstream names were retained wherever applicable, without broad refactoring. Shared integration tests live under `android/tests/`, with small CMake registrations for each game. These are working-tree backports; no upstream merge ancestry or cherry-pick commits have been recorded

A future merge is not guaranteed conflict-free. A read-only, per-file three-way text merge simulation against the common ancestor reported 29 conflict sections before these backports and 50 afterward. This counts textual sections, not independent bugs or Git's complete merge behavior. Exact imports can overlap existing Android changes, and deliberate adaptations differ from upstream. Keep the following decisions when resolving a later merge:

- Vertigo: mark assets dirty before exit loading can fail; restore mission models before the initial object read; free old extra object bitmaps before reloading HAM so their cleanup cannot shrink the newly loaded bitmap table
- Robot refresh: retain `verify_robot_object()` and the robot-only pass in shared `load_level_robots_file()`. Do not restore upstream's second full-object verification, which repeats player initialization and robot/powerup counters. Retain the later D1-emulation asset refresh
- Desktop audio: use upstream's 2048-frame buffer (1024 on macOS), while keeping Android's 256-frame buffer and native sample-rate selection
- Joystick/icon/shader fixes: retain the Android virtual joystick path, existing platform guards, and the `!OGLES` guard around the desktop GL function-pointer diagnostic
- Windows codecs: use local `dxx-redux-d1`/`dxx-redux-d2` target names, the configured vcpkg triplet, configuration-specific DLL paths, and only codecs present in the installed package. Copy and install dynamically loaded codecs
- High-resolution weapons/powerups: retain upstream mapping/API changes plus Android visual-policy gating, software-renderer guards, corrected energy/shield comparisons, and guarded 64-bit animation arithmetic. The new model feature was brought in after resolving the original audit concerns
- Lives cap: retain local safe `"%s"` message formatting alongside upstream's 255-life limit

The three deferred changes intersect substantial local functionality. PNG/menu work needs adaptation to local filtering, transparency, and texture caching; monitor handling needs validation against reconstructed D1 effect frames; the new autoselection flags need defined launcher/save/rewind/replay behavior. None is silently treated as merged

## Completed validation

- `run-windows-build.ps1 -Target both`: successful MSVC D1/D2 builds, including headless tools and integration targets
- CTest: all 50 D1 and 57 D2 tests passed
- `android/tests/test_upstream_compat.cpp`: synthetic binary fixtures exercise the actual D1 HX1 reader, model-texture seek semantics, life pickup/score/bonus boundaries, and D2 HAM/V-HAM/HXM/exit loaders. Repeated successful and failed exit reloads verify model radius, physics, bitmap counts, and preservation of player/powerup objects and bookkeeping
- `android/gradlew.bat :app:externalNativeBuildDebug -Pandroid.injected.build.abi=arm64-v8a --console=plain` with JDK 21: both Android game libraries built successfully; the final incremental build reported no compiler warnings
- `android/tests/test_vertigo_metadata.ps1`: passed descriptor-only Vertigo loading, unrelated archive isolation, missing-HAM rejection, and reused-worker cleanup
- Scoped `android/run-code-quality.ps1 -Fix -Paths ...`: passed. The script excludes upstream engine files and native test fixtures from formatting; the new C++ fixture was separately formatted with the pinned clang-format 20. `git diff --check` passed
- Confirmed six music codec DLLs and Windows PDBs in build outputs. Verified the macOS 15 runner labels against the [official runner inventory](https://github.com/actions/runner-images)

Limits: no Android device playtest, Full Strike rendered-exit playthrough, high-resolution asset visual review, clean-machine music playback, or remote CI/macOS build was performed. The host tests exercise the relevant native asset loaders, not the complete game presentation loop

## Audit boundary and sources

- Audited on 2026-09-16 against local starting HEAD `89d926499cafd3fabf02497ab26119b5c4804216`
- Fetched `upstream/main`: `9fd90f03513663ce1372c8cfa723b7a73c4219fa`, committed 2026-09-15
- Common ancestor: `fb555eec75e1ed12c8348805ab335afb4c721b06`, 2026-02-22
- Reviewed all 25 commits in `HEAD..upstream/main`, including their diffs and the affected local implementations. This covers outstanding upstream changes, not a re-audit of all commits already inherited before the common ancestor
- The initial research used `git cherry` plus forward/reverse `git apply --check` through stdin; those checks did not apply patches. Implementation and runtime validation were performed afterward as recorded above
- [Upstream history](https://github.com/dxx-redux/dxx-redux/commits/main/)
- [Mission 2305: DESCENT : FULL STRIKE](https://sectorgame.com/dxma/mission/?m=2305), version 1.0f, an 83-level D2 mission combining First Strike, Counterstrike, and Vertigo
- The mission page links [Arne's temporary build](https://pumosoftware.com/files/descent/d2x-redux-1.1-pumo2.zip). The page was retrieved directly over HTTPS after the web tool failed. The binary was not downloaded, executed, or compared, so its exact commit contents are not established

## What went wrong with the Vertigo robots

V-HAM supplies additional robot definitions and polygon models. The rendered D1-style exit sequence also needs extra polygon models, and uses the same model slots

1. `d2/main/endlevel.c:376` calls `load_exit_models()` when a rendered exit is available and no movie plays. This path is single-player in the current code
2. `d2/main/bm.c:516` starts `load_exit_models()` by freeing extra models and extra object bitmaps. Models above the base D2 count of 166 are removed; the normal and destroyed exit models then occupy the next slots
3. Local code does not mark the robot/model tables as needing restoration after this change
4. On the next level, `LoadLevel()` calls `load_level()` before `load_level_robots()`. During that initial object read, `d2/main/gamesave.c:174` calls `verify_object()` and derives each robot's model and collision radius from the currently installed model tables
5. A robot can therefore inherit an exit model's large radius. Other extra robot models have been freed. Even if an HXM or multiplayer modification already set the reload flag, the later HAM reload alone does not correct the radius previously copied into the object

This explains both symptoms in the warning: excessive collision size and incorrect extra models across level transitions. It is specifically relevant to add-on missions combining extra robots with rendered exits, rather than evidence that every ordinary Vertigo level is broken. Co-op does not take the rendered-exit path here, but changes to shared level loading must still preserve co-op state

The fixes are:

- [79a155f5 - Fix V-HAM robot properties with exit sequences](https://github.com/dxx-redux/dxx-redux/commit/79a155f5d9e9cc482ef0f2f1df4055c7d3560387), committed 2026-05-28: run a second object verification after robot replacements load
- [eeeeafdc - Fix V-HAM with exit sequences part 2](https://github.com/dxx-redux/dxx-redux/commit/eeeeafdc4222784544be4f3bc0b319429d43660f), committed 2026-07-08: set `Robot_replacements_loaded = 1` after successfully loading exit models, causing the next level to reload its HAM/V-HAM

Both were absent at the start of the audit. The existing September Vertigo HAM work concerned archive mounting, HAM lookup, and import classification; it did not repair this exit/model lifecycle

### Adaptation needed for this branch

The upstream first patch invokes `verify_object()` on every object a second time. This routine is not safe to repeat as a general refresh:

- Increments `Gamesave_num_org_robots`, which is used by robot-maker spawning limits in `d2/main/fuelcen.c`
- Assigns player IDs using `Gamesave_num_players++` and can initialize the console player object
- Increments network powerup counts and allowed counts
- Reinitializes lifetime and other object bookkeeping

The original counters are reset only before the initial file read (`gamesave.c:832`), not before the added upstream pass. These are source-level hazards; this audit does not claim to have reproduced each downstream consequence

Recommended implementation:

1. Mark extra robot assets dirty as soon as `load_exit_models()` discards them, covering failure returns as well as successful loads. Upstream marks only the success path even though failures occur after the destructive cleanup
2. Restore dirty mission HAM/V-HAM before the next level's initial object verification where feasible. This also avoids validating IDs against stale tables; a later property refresh cannot recover an ID that was already changed
3. After level-specific robot replacements are loaded, refresh robot model number, radius, and applicable physics properties without repeating player, powerup, and counting initialization
4. Preserve the D1-emulation ordering. `d1_in_d2_apply_robot_assets()` already refreshes D1 robot/player properties after installing its own assets (`d2/main/d1_in_d2.c:2214`); do not replace those assets or refresh against the wrong table generation
5. Review `load_level_robots_file()` callers in the host metadata tool, JNI metadata analysis, and level preview code. They share the loader but have different surrounding object-load ordering. Keep game-format logic in the engine

The reload flag portion is very small and applies cleanly. The property-refresh portion needs a deliberate adaptation to the current `gameseq.c`, not a wholesale cherry-pick

## Every outstanding upstream commit

"Clean" means a forward patch check passed against the working tree during this audit, not that the behavior has been tested

| Commit | Change and local finding | Recommendation |
| --- | --- | --- |
| [9cdd9399](https://github.com/dxx-redux/dxx-redux/commit/9cdd93996e37afb279f251e225d6e901abcff47e) | D2 anarchy-only mission check incorrectly rejects CTF, Hoard, team Hoard, and bounty before reaching their mode branches | Take; clean, small multiplayer menu fix |
| [86923251](https://github.com/dxx-redux/dxx-redux/commit/86923251d9cd42b9ee4adbef5cc70b537fe98221) | Joystick limit 8 to 16, clamp SDL enumeration, enlarge label buffer, remove duplicate warning. Local desktop code still iterates unclamped; Android uses a separate virtual joystick path | Adapt for D1/D2; bounds clamp matters more than increasing the limit. Preserve Android mappings and review persisted binding/index widths before increasing capacity |
| [fe74f729](https://github.com/dxx-redux/dxx-redux/commit/fe74f7298a15e8e2d3ad68bfd0e2110595f63b0a) | D2 exit terrain/satellite bitmap replacement frees only CPU data, leaving texture resources stale | Take with Vertigo work; clean, use `gr_free_bitmap_data()` |
| [ef73945b](https://github.com/dxx-redux/dxx-redux/commit/ef73945bea57137f80bec89b7d9bd326d05c80e5) | Face the exit after DELSHIFTB in both games | Take if desired; clean, low-priority convenience fix |
| [069eaff5](https://github.com/dxx-redux/dxx-redux/commit/069eaff5c97b10031aaa0b27d4626ad5f7e1daa3) | Preserve the final unterminated text line. Local D1/D2 already do this using an immediate terminated return | Already covered; keep the local implementation. Upstream's `break` exits the inner `do` loop, not the enclosing character loop |
| [79a155f5](https://github.com/dxx-redux/dxx-redux/commit/79a155f5d9e9cc482ef0f2f1df4055c7d3560387) | Refresh robot properties after HAM/HXM loading | High priority; adapt as described above |
| [62802aa6](https://github.com/dxx-redux/dxx-redux/commit/62802aa62a192bdef58bf2ec781c3213e8d1b5d8) | Explicitly copy dynamically loaded music codec DLLs into MSVC output. Local CMake targets differ and install rules list SDL_mixer but not these codecs | Adapt when validating Windows packaging; verify actual codec dependencies and clean-machine music playback. Do not copy the target names or assume all listed codecs are built |
| [c6e7b87b](https://github.com/dxx-redux/dxx-redux/commit/c6e7b87be3f3d902643c04b6e5234ef9e55e8388) | Diagnose a missing desktop `glCreateShader` entry point instead of calling it. Local shader builder lacks this guard | Take with a desktop loader guard; preserve GLES/direct-function builds and avoid always-true address warnings |
| [917704d3](https://github.com/dxx-redux/dxx-redux/commit/917704d3727404a6f8f8f33d3fa464c6d16a75fa) | Recognize Windows drive-letter paths in `PHYSFSX_getRealPath()` | Already present in D1/D2; full reverse patch check passes |
| [8be8929e](https://github.com/dxx-redux/dxx-redux/commit/8be8929e86205c4ad481fea857bbab0c24c367f3) | Restore `GAME_FONT` before observer weapon/damage summary | Take; clean, small D1/D2 rendering correction |
| [8cb8e624](https://github.com/dxx-redux/dxx-redux/commit/8cb8e624ae63bf0973ae8212212a0da0ad33ec4b) | Retain Windows PDB artifacts and identify them by commit | Take for Windows release CI; clean. Confirm the configured build actually emits PDBs before enabling error-on-missing |
| [ceba2c7a](https://github.com/dxx-redux/dxx-redux/commit/ceba2c7a712eb53102e707c316d222bacb580da8) | Custom `scores.png` plus indexed PNG palette expansion. Local texture loader has extensive filtering, transparency, and cache changes and still has the two-argument API | Separate graphics task; useful palette correctness work plus optional menu feature. Adapt to current texture handling and validate transparent/indexed images |
| [7a66b717](https://github.com/dxx-redux/dxx-redux/commit/7a66b7173b4e8ab12c5575c9177ea17ef6083bfa) | Revert 512-frame desktop mixer buffer to 2048 (1024 on macOS) because of irregular plasma sounds. Local buffers are 256 Android and 512 desktop | Investigate desktop audio; preserve Android tuning until measured. Do not apply the revert globally |
| [eeeeafdc](https://github.com/dxx-redux/dxx-redux/commit/eeeeafdc4222784544be4f3bc0b319429d43660f) | Mark exit-model replacement dirty so V-HAM reloads | High priority with 79a155f5; clean, extend to failure paths |
| [9f8d4693](https://github.com/dxx-redux/dxx-redux/commit/9f8d46932f3ed57ff10c7d0d70806ba4693dd277) | Replace packed-member pointer traversal with local fixed-point arrays in xmodel transforms | Take; clean D1/D2 portability/alignment fix |
| [83354a4c](https://github.com/dxx-redux/dxx-redux/commit/83354a4c608615b395db91c5477f90ef609b15e6) | Expand ASE loader diagnostic buffer from 40 to 64 bytes | Take; clean D1/D2 overflow fix |
| [86dcf9ca](https://github.com/dxx-redux/dxx-redux/commit/86dcf9ca67c7cabde082c158bc60fb2d4ff5bf19) | Exclude effect frames and destroyed-monitor textures from generic D1 bitmap replacement | Review against local D1 emulation; it separately restores D1 effect frames in `d1_in_d2_apply_effects()`. Upstream exclusion is absent, but local behavior is substantially different. Test intact, animated, and destroyed monitors before selecting a fix |
| [8537b01e](https://github.com/dxx-redux/dxx-redux/commit/8537b01e90e683fcde9aa34ed8b1574e8651bc2a) | New Autoselect Only Once option, absent locally. Uses one primary and one secondary pickup flag per ship, not a flag per weapon | Optional feature task. Needs launcher settings, save/rewind/replay state handling, and interaction checks with local weapon-wheel behavior |
| [029a67c2](https://github.com/dxx-redux/dxx-redux/commit/029a67c2b0b3012ecf4723790b0abab5e1c1690b) | Make xmodel `CFile::Seek()` return 0 on success and -1 on failure instead of returning the position; fixes TGA reader expectations | Take; clean D1/D2 model texture loading fix |
| [83fb8760](https://github.com/dxx-redux/dxx-redux/commit/83fb876001b007948961f28f7f1035d511499a98) | Expand high-resolution model mapping to weapons/powerups; absent locally | Defer as a feature. Both powerup implementations contain `obj->id == POW_ENERGY == obj->id == POW_SHIELD_BOOST`, which needs correction; also review renderer guards and animation divisor before porting |
| [338d7bd3](https://github.com/dxx-redux/dxx-redux/commit/338d7bd357181c0c024cbccf8572a9e440e5b883) | Guard failed BMP icon loads and free successful surfaces, fixing sdl12compat crashes | Take with existing Android guards preserved; desktop OpenGL and software paths in both games |
| [29b77fc6](https://github.com/dxx-redux/dxx-redux/commit/29b77fc6c677ca7af41d1b34e7264ddd66d63aca) | Increase nested model instance stack from 5 to 10 for EAF2 models | Take; clean, small D1/D2 add-on compatibility fix |
| [6e76c5d8](https://github.com/dxx-redux/dxx-redux/commit/6e76c5d8dafd02dc32a1c6312ce9440dc95b1aca) | Change macOS workflow runners from 14/13 to 15/15-intel | Clean infrastructure update; take if maintaining these workflows, checking runner availability at implementation time |
| [4b2e777e](https://github.com/dxx-redux/dxx-redux/commit/4b2e777e4e9f176407e0d7616043a2f06d2429d7) | Read D1 HX1 replacement polymodels with the serialized field reader instead of `sizeof(polymodel)` | High priority; clean native D1 file-format/ABI correctness fix, especially relevant to 64-bit targets |
| [9fd90f03](https://github.com/dxx-redux/dxx-redux/commit/9fd90f03513663ce1372c8cfa723b7a73c4219fa) | Cap lives at 255 on pickup and score/bonus awards; local byte-sized lives still increment unchecked | High priority; clean D1/D2 correction, useful for long campaigns. Preserve local safe message formatting while adapting |

## Suggested work batches and verification

1. Vertigo lifecycle: adapt 79a155f5 + eeeeafdc, include fe74f729. Test an actual rendered exit followed by a V-HAM level in Full Strike. Compare robot ID, model, radius, mass, and drag with a fresh direct load of the destination. A metadata-only run or level warp cannot exercise the entire exit sequence
2. For the same batch, test missing exit assets, HXM replacements across levels, stock D2 and Vertigo, D1 emulation, save/load, and co-op. Assert robot totals, player IDs, and multiplayer powerup counts are unchanged by property refresh. Check metadata and previews against live engine results
3. Small correctness batch: HX1 decoding, lives overflow, TGA seek, packed-member access, ASE buffer, instance depth, observer font, and D2 multiplayer mode gating. Test lives at 254/255 with pickup and both scoring paths, and use a real HX1/model-texture fixture for loaders
4. Desktop robustness batch: joystick bounds, icon handling, GL diagnostic, codec packaging, and symbols/runner CI. Test through the applicable Windows build; desktop-only fixes do not justify changing Android behavior
5. Separate decisions: D1 monitor behavior, PNG/menu enhancements, audio-buffer revert, new autoselection policy, and high-resolution powerups

These were the initial suggested work batches. See the completed implementation and validation sections above for what was subsequently brought in and tested
