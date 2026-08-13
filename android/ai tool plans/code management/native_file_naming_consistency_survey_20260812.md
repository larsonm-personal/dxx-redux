# Native file naming consistency survey - 2026-08-12

## Result

The branch-added native inventory contains 442 `.c`, `.cpp`, and `.h` files
against merge base `fb555eec75e1ed12c8348805ab335afb4c721b06`: 253 sources and
189 headers. One source/interface mismatch was found and corrected:
`rbaudio_android.h` is now `rbaudio_bin.h`, matching `rbaudio_bin.c`. Its guard,
both inherited include sites, focused contract test, and current campaign records
use the new name.

One additional guard-only mismatch was found and corrected:
`net_udp_android_autonet_shared.h` now uses
`DXX_ANDROID_SHARED_NET_UDP_ANDROID_AUTONET_SHARED_H`, including every component
of the filename inside the project-qualified guard.

The corrective declaration-level pass found four more 1:1 implementation names
that the initial filename-only survey had incorrectly excused:

- `jni_music_control.c` is now `android_music_control.c`, matching its public
  game-thread interface
- `jni_udp_reconnect.c` is now `net_udp_reconnect_jni.c`, matching its complete
  four-function interface
- `secret_area_game_adapter.c` is now `secretarea.c`, matching the paired D1/D2
  interfaces that own 30 of its 32 public definitions
- `pngfile_stb.c` is now `pngfile.c`, matching the complete paired D1/D2 image
  interface while the source comment still records the stb backend

All source registrations, tests, live queue paths, and maintained comments were
updated. Historical ledgers and completed design plans retain old paths as
historical evidence.

## Rules used

- Use one stem for a source and the unique interface that it implements
- Do not force one stem on entry points, JNI bridges, command orchestrators, test
  programs, backend implementations of inherited APIs, or aggregate stubs
- Keep header-only policy, type, limit, compatibility, and inline-math owners
  named for the contract they own
- Keep explicit ABI suffixes such as `_c` when they distinguish a C facade from
  its C++ owner
- Derive guards from the complete header filename unless compatibility requires
  the identity of a replaced upstream header

These rules reject both the old `rbaudio_android.h` name, which was the unique
interface implemented by `rbaudio_bin.c`, and cosmetic renames that would erase a
useful ownership boundary.

## Mechanical coverage

After applying the rename:

- 140 of 253 sources have at least one exact-stem branch-added header
- 81 unmatched sources are explicitly named native tests or test programs
- 32 unmatched production sources were reviewed semantically below; `pngfile.c`
  matches inherited D1/D2 headers and therefore remains mechanically unmatched
  only within the branch-added-header subset
- 48 headers have no exact-stem branch-added source and were reviewed as
  header-only, compatibility, stub, inherited-interface, or split-interface
  owners
- Every branch-added header was checked for an `#ifndef` guard containing its
  normalized complete filename stem

The only remaining mechanical guard exception is
`android/app/src/main/cpp/SDL_config_android.h`, which deliberately uses SDL's
`_SDL_config_h` guard because it is the selected replacement for SDL's canonical
configuration header.

## Declaration-level ownership results

The corrective pass extracted public function definitions from every unmatched
branch-added production source and mapped them to declarations across all
repository headers, including inherited D1/D2 headers. The four renames above are
the cases where one differently named source owns the associated interface.

The strongest retained cross-stem results are not 1:1 relationships:

- `android_input.c` has 89 public or JNI definitions; only ten belong to
  `android_screen_advance.h` and two belong to `android_lifecycle_actions.h`
- `software_renderer_debug.c` deliberately satisfies three separate debug
  interfaces: seven merged-wall functions, four texture-debug functions, and one
  overlay request function
- `difficulty_runtime_shared.c` contributes six declarations to the much larger
  inherited `game.h`/`game.c` owner; `effect_runtime_shared.c` similarly
  contributes three declarations to `effects.h` alongside inherited `effects.c`
- `multi_save_transfer.c` contributes 16 transfer declarations to the much
  larger inherited `multi.h`/`multi.c` owner
- `digi_tsf_music.c` has 27 public definitions, only seven of which belong to
  `digi_mixer_music.h`
- `android_surface.c` already matches the main `android_surface.h` interface;
  `android_surface_lifecycle.h` is a narrow two-function secondary view
- `route_planner.cpp` and `route_snapshot.cpp` already match their main C++
  headers; their `_c.h` files are explicit secondary C ABI facades

The remaining unmatched production sources either expose only `main`/`wmain` or
JNI entry points, instantiate header-only dependencies, or publish a backend
object without a public function interface.

## Unmatched production source decisions

### Boundary and orchestration owners - retain

These 22 files are entry points, JNI bridges, high-level commands, or platform
adapters. They do not have a unique same-stem interface header:

- `android_autoselect.cpp`, `android_gamepad_config.cpp`, `android_input.c`, and
  `android_pilot_prefs.cpp`
- `extract_cd.c`, `extract_gog.c`, `fingerprint_audio.c`, `fingerprint_cd.c`, and
  `fingerprint_match.c`
- `jni_disc_import.c`, `jni_gog_import.c`, `jni_cd_preview.c`,
  `jni_fingerprint.c`, `jni_level_metadata.cpp`, `jni_main.c`,
  `jni_midi_preview.c`, `jni_resume_save.cpp`, and `jni_saf.c`
- `d2_headless_runtime.c`, `headless_metadata_dump_main.cpp`,
  `input_demo_headless_main.cpp`, and `native-lib.cpp`

### Backend and compatibility implementations - retain

These seven files implement an inherited API, a selected backend, or a
single-translation-unit dependency. Their specialized source names distinguish
the implementation from the interface they satisfy:

- `difficulty_runtime_shared.c`, `effect_runtime_shared.c`,
  `digi_tsf_music.c`, and `messagebox.c`
- `physfs_archiver_saf.c`, `tsf_impl.c`, and `etc2tool.cpp`

### Aggregate implementations - retain

- `multi_save_transfer.c` owns the complete transfer protocol;
  `multi_save_transfer_policy.h` is only its small pure policy component
- `software_renderer_debug.c` is the non-GLES aggregate implementation of
  `android_texture_debug.h`, `debug_tex_overlay.h`, and `merged_wall_debug.h`

## Header-only and split-interface decisions

The 48 unmatched headers fall into deliberate categories:

- Test/dependency stubs and selected configuration:
  `test_android_stubs/android/log.h`, `test_hmp_stubs/hmp.h`,
  `test_hmp_stubs/u_mem.h`, `chromaprint_android/config.h`, and
  `SDL_config_android.h`
- Header-only limits, policy, math, layout, types, or compatibility:
  `extract_limits.h`, `android_audio_format.h`, `android_dxxerror.h`,
  `android_lifecycle_actions.h`, `android_menu_reorder.h`,
  `android_music_control.h`, `android_render_resolution.h`,
  `android_screen_advance.h`, `android_visual_policy.h`,
  `classic_demo_wall_validation.h`, `coop_indicator_lines_math.h`,
  `coop_player_session.h`, `coop_restore_remap.h`, `debug_log_categories.h`,
  `deterministic_math.h`, `fingerprint_duration.h`, `font_control_shared.h`,
  `gles3_shim_array_sources.h`, `hog_midi_catalog.h`, `homing_compat.h`,
  `hud_layout_shared.h`, `input_demo_limits.h`,
  `merged_wall_geometry_hit.h`, `multi_save_transfer_policy.h`,
  `music_decode_limits.h`, `rewind_file_compat.h`, `rgba8888.h`, and
  `etc2_limits.h`
- Narrow interfaces implemented inside a broader owner:
  `android_surface_lifecycle.h`, `bounded_music_read.h`, `debug_tex_overlay.h`,
  `net_udp_reconnect_jni.h`, and `rewind_file.h`
- Explicit C facades over C++ owners: `route_planner_c.h` and
  `route_snapshot_c.h`
- Per-game interfaces or policies implemented in existing or shared game
  translation units: both `input_demo_control_info.h` and `secretarea.h` files,
  plus `replay_debug_overlay.h`, `escort_exit_policy.h`,
  `input_demo_energy_trace.h`, and `thief_network_policy.h`

## Redbook correction

- Interface: `android/app/src/main/cpp/shared/rbaudio_bin.h`
- Implementation: `android/app/src/main/cpp/shared/rbaudio_bin.c`
- Guard: `RBAUDIO_BIN_H`
- Inherited consumers: `d1/include/rbaudio.h` and `d2/include/rbaudio.h`
- Focused contract: `android/tests/test_rbaudio_bin_header_contracts.py`

The contract explicitly rejects the old path, requires the new guard and include
name, checks the single declaration owner, and compiles the exact C and C++
signatures through both game headers.

## Validation

- `python -m unittest android.tests.test_rbaudio_bin_header_contracts`: three
  tests passed
- `run-windows-build.ps1 -Target both`: D1, D2, and their maintained headless
  targets compiled and linked
- `:app:externalNativeBuildDebug` with JDK 21: arm64-v8a, armeabi-v7a, and x86_64
  compiled and linked
- `python -m unittest` for the naming, music lifecycle, control-center,
  replacement-texture, and Redbook contracts: fourteen tests passed
- Scoped code quality accepted the renamed sources, CMake owner, Kotlin comments,
  Python contracts, and maintained records
- Final old-name, include-guard, build-reference, ASCII/BOM, and diff whitespace
  audits passed

## Repeating this audit

For future branch-added native files, first compare complete stems mechanically,
then examine only unmatched paths. A mismatch is actionable when one header is
the unique declaration owner of one source's exported interface. Record an
exception only after identifying its concrete ownership category above. This
keeps the audit fast without turning descriptive backend, adapter, policy, or
entry-point names into artificial pairs.
