# d1/d2 upstream-diff cleanup study

Status: study only. No source changes made by this document.

Goals:
- Shrink `d1/` and `d2/` diff vs `upstream/main` to ease future upstream merges.
- Move Android-only code into `android/app/src/main/cpp/shared/` (same
  pattern already used by `game_introspect.cpp`, `merged_wall_debug.c`,
  `console_ringbuf.cpp`, etc.).
- Finish the `metl154` and `door45` cleanup tranches that were scoped but
  never fully executed.
- Prefer a single shared copy across D1 + D2. Per-game copies under
  `android/app/src/main/cpp/shared/{d1,d2}/` are acceptable when types
  diverge.

A helper script is now at `android/diff_vs_upstream.ps1` -- it writes a
sorted numstat and a summary to `temp/d1d2_diff_*.txt`. Run it before and
after each tranche to track progress.

Current totals vs `upstream/main`:
- 199 files changed (93 in d1/, 106 in d2/)
- +21018 / -791

---

## 1. What actually happened with the metl154 / door45 plans

Tranches 1 and 2 of `metl154_postfix_cleanup_and_debug_harness.md` landed
(rename to `merged_wall_*`, delete hardcoded focus/cover-skip/tracked-side
lists). Tranche 3 (Part B: extraction to `android/app/src/main/cpp/shared/`)
is largely unfinished.

Evidence in the source today:
- 347 lines matching `metl154|door45|METL154|DOOR45` still live in `d2/`,
  348 in `d1/`. They are concentrated in three files per game:
  - `arch/ogl/ogl.c` (the large majority)
  - `main/render.c`
  - `main/texmerge.c`
- `merged_wall_debug.{c,h}` already exists under
  `android/app/src/main/cpp/shared/` and already hosts the generic
  draw-face-context, track-face, cover-log, and snapshot entry points.
  The d1/d2 copies are effectively shims that call those helpers, but
  keep a lot of internal machinery locally. The extraction target is
  real and wired, so the move is mostly "copy static helpers + rename
  + delete the d1/d2 originals".
- The naming in ogl.c still mixes `metl154_*` (internal) with
  `merged_wall_*` (public). Tranche 1 added `#define` bridges so the
  legacy names kept compiling; tranche 3 is supposed to finish the
  rename and delete the bridges, but did not.

`door45` is already almost gone from d1/d2 -- zero matches in the current
tree. The door45 investigation left behind the `plan_door45_*.md` files
and repo-memory notes (`door45-*.md`), but no named symbols remain in
`d1/` or `d2/`. Nothing to do there besides close out the plan files.

---

## 2. Categories of the d1/d2 diff

| Category | Rough line share | Disposition |
|---|---|---|
| A. metl154 / merged-wall OGL diagnostic code | ~2500 per game in `arch/ogl/ogl.c` | move most to shared, delete dead |
| B. ETC2 + shader shim + MSAA + GPU timer + aniso/texfilt runtime controls | ~800-1000 per game in `arch/ogl/ogl.c`, `arch/ogl/gr.c`, `arch/ogl/oglprog.c`, `include/ogl_init.h` | keep in d1/d2, but minimize: move helpers to shared, keep only the `#ifdef ANDROID` dispatch lines |
| C. Cross-platform new features dropped into d1/d2 (coop saves, coop warp, auto-net, songs rewrite, playsave additions, tranches of multi.c/state.c) | ~6000 lines, spread over dozens of files | some genuinely cross-platform -- let stay; Android-only variants -- extract |
| D. Android introspection / automation / log hooks inside engine code | small-to-medium -- dozens of `#ifdef INTROSPECT_ON` / `#ifdef ANDROID` stubs scattered across `newmenu.c`, `kconfig.c`, `gauges.c`, `automap.c`, `titles.c`, `inferno.c`, `multi.c`, ... | keep call sites, move handler bodies to shared |
| E. Touch / onscreen UI hooks (keyboard affordance, "OK" buttons, drag-scroll, touch region publishing) | ~300 lines across `newmenu.c`, `menu.c`, `kconfig.c`, `titles.c`, `gamecntl.c` | keep hooks, extract helpers |
| F. CMake glue (lots of 2/2, 4/4, 3/3 changes) | ~100 lines, many files | required; not a target for shrinkage |
| G. Headers exposing new shared state to d1/d2 (`ogl_init.h`, `console.h`, `dxxerror.h`, `rbaudio.h`, `hmp.h`, etc.) | ~100 lines | keep; these are the API surface |

Totals roughly add up to the observed 21018 insertions.

Focus for shrinkage: A, D, E first (clear wins). B is a slower extraction
because ogl.c is load-bearing. C is a policy call per feature -- some of
those were meant to be upstreamable, not Android-only.

---

## 3. Per-file targets, ordered by impact

Line counts are `added / removed / total` from `git diff --numstat
upstream/main`. "After" is a target estimate, assuming the plan below
lands. Nothing here is a commitment; they are order-of-magnitude goals.

### Tier 1 -- biggest, clearest wins

| File | Now (+/-/total) | Target after | Rationale |
|---|---|---|---|
| `d1/arch/ogl/ogl.c` | 3318 / 47 / 3365 | ~600-800 | see section 4; move all metl154/merged-wall helpers to shared, move cache + MSAA helpers to shared, keep only call sites and genuine GL path changes |
| `d2/arch/ogl/ogl.c` | 3352 / 47 / 3399 | ~600-800 | same as d1 |
| `d1/main/net_udp.c` | 944 / 58 / 1002 | ~300-400 | see section 5; extract host-migration, PDATA, and QoL helpers to shared; keep `#ifdef __ANDROID__` dispatch |
| `d2/main/net_udp.c` | 1045 / 84 / 1129 | ~300-400 | same |
| `d1/main/render.c` | 415 / 15 / 430 | ~50-80 | move merged-wall tracked-list + summary logger + android_draw_face_context setter to shared |
| `d2/main/render.c` | 416 / 13 / 429 | ~50-80 | same |
| `d1/main/texmerge.c` | 120 / 0 / 120 | ~20 | the `texmerge_metl154_overlay` + `[metl154texmerge]` logger are pure diagnostic, move to shared |
| `d2/main/texmerge.c` | 120 / 0 / 120 | ~20 | same |

Per game: roughly 4000 lines of d1/d2 diff reducible to roughly 1000.
Total saving across both games: order of 6000 lines.

### Tier 2 -- new cross-platform files that should move or split

These are files upstream does not have. Each is either "android only" or
"android-only wrapper around generic logic". They should move under
`android/app/src/main/cpp/shared/`, preserving a single copy where
possible and renaming in the android tree.

| File | Now / total | Plan |
|---|---|---|
| `d1/main/coop_save.c` | 872 | d2 is 929 lines; the two are near-duplicates. Move one copy to `android/app/src/main/cpp/shared/coop_save.c`. Use forward-decl shims where D1/D2 types differ (segment layouts, player struct field differences). If diverged too far, put two copies under `shared/d1/` and `shared/d2/`. |
| `d2/main/coop_save.c` | 929 | same |
| `d1/main/coop_save.h` | 161 | move to `shared/` |
| `d2/main/coop_save.h` | 163 | move to `shared/` |
| `d1/main/coop_warp.c` | 371 | move to shared; seg/obj types differ so may need the two-copy split |
| `d2/main/coop_warp.c` | 371 | same |
| `d1/main/coop_warp.h` | 70 | shared |
| `d2/main/coop_warp.h` | 70 | shared |
| `d1/main/auto_net.c` | 97 | shared; the entry point stays in `CMakeLists.txt` |
| `d2/main/auto_net.c` | 96 | same |
| `d1/main/auto_net.h` | 62 | shared |
| `d2/main/auto_net.h` | 65 | shared |

These 3400-ish lines of d1/d2 can move to shared with essentially no
upstream diff remaining.

Open question for each: if the coop-save / coop-warp / auto-net features
were ever intended to upstream (to benefit the desktop builds), then
`shared/` is wrong and they should stay. The plan files
`coop-autosave-lobby.md`, `coop-multi-slot-autosave.md`,
`plan_coop_saves_callsign_guidebot.md` read as Android-scoped. Confirm
with the user before moving.

### Tier 3 -- heavy edits in existing upstream files

| File | Now (+/-/total) | Target | Approach |
|---|---|---|---|
| `d1/main/multi.c` / `d2/main/multi.c` | 422 / 15 / 437 and 455 / 15 / 470 | ~120-180 each | most additions are android-port coop QoL and host-migration helpers; move helper bodies to shared, keep call sites |
| `d1/main/playsave.c` / `d2/main/playsave.c` | 430 / 0 and 279 / 0 | ~60 each | large blocks are android launcher bridge helpers (native-pilot-prefs-bridge memory confirms it); move to `android/app/src/main/cpp/shared/playsave_bridge.c`, keep only the extern declarations |
| `d1/main/songs.c` / `d2/main/songs.c` | 253 / 0 and 276 / 0 | ~40 each | music system rewrite was largely meant to be cross-platform. Re-check against the `music-system-architecture.md` memory; what is truly android-only (jukebox.c changes, SAF paths, fingerprint hooks) should move to shared |
| `d2/main/newmenu.c` | 532 / 4 / 536 | ~120 | touch helpers, drag-scroll, keyboard affordance, "OK" button, introspection hooks. The handler bodies can live in `android/shared/newmenu_android.cpp`, keeping only `#ifdef ANDROID` call sites in d1/d2 |
| `d1/main/newmenu.c` | 287 / 1 / 288 | ~80 | same, smaller tranche |
| `d2/main/state.c` / `d1/main/state.c` | 216 / 14 and 180 / 6 | ~50 each | coop save/restore glue + tracked-face snapshot restore. Most of this should move next to `coop_save` in shared |
| `d2/main/titles.c` | 159 / 30 / 189 | ~40 | intro-movie skip, resume-on-input, font resolution. Movie-skip belongs in shared |
| `d1/main/titles.c` | 76 / 0 / 76 | ~20 | same |
| `d2/main/kconfig.c` / `d1/main/kconfig.c` | 167 / 5 and 100 / 5 | ~30 each | touch binding + gamepad default; move to shared |
| `d2/main/gamecntl.c` / `d1/main/gamecntl.c` | 93 / 2 and 74 / 0 | ~30 each | same as kconfig |
| `d1/misc/hmp.c` / `d2/misc/hmp.c` | 126 / 0 and 125 / 0 | ~20 each | TSF MIDI is 100% android, route through `digi_tsf_music.c` in shared |
| `d1/misc/physfsx.c` / `d2/misc/physfsx.c` | 105 / 0 and 114 / 1 | ~30 each | SAF archiver wiring; header-only glue into the d1/d2 includes |
| `d1/arch/ogl/oglprog.c` / `d2/arch/ogl/oglprog.c` | 132 / 21 (each) | ~40 each | gles3 shim selection + shader source switch. Helpers can live in `gles3_shim.c`; the `#ifdef ANDROID` dispatch stays |
| `d1/arch/ogl/gr.c` / `d2/arch/ogl/gr.c` | 227 / 1 and 228 / 1 | ~60 each | MSAA bind/unbind, texfilt live apply, menu viewport offset. Move bodies to shared |
| `d1/main/automap.c` / `d2/main/automap.c` | 33 / 5 and 130 / 7 | ~20 each | touch helpers; shared |
| `d1/main/escort.c` hook | n/a | n/a | d2 only; `d2/main/escort.c 129/5/134` |
| Many other files <= 50 lines each | collectively ~400 lines | should survive at their current size; evaluate case by case |

### Tier 4 -- cannot or should not shrink

| File / group | Why keep as-is |
|---|---|
| Every `CMakeLists.txt` under d1/ and d2/ (about 15 files, 2-30 lines each) | required wiring for target_sources, defines, includes. These are the canonical place to add shared includes. Shrinkage here would mean hiding real build info |
| `include/ogl_init.h`, `include/internal.h`, `include/hmp.h`, `include/rbaudio.h`, `include/dxxerror.h`, `include/pngfile.h`, `include/console.h`, `include/pstypes.h`, `main/config.h`, `main/kconfig.h`, `main/playsave.h`, `main/newmenu.h`, `main/multi.h`, `main/net_udp.h`, `main/weapon.h`, `main/text.h`, etc. | these are the shared-symbol declarations that `android/shared/` needs to reference. Keep them |
| `d1/CMakePresets.json` / `d2/CMakePresets.json` (1/1) | infra |
| `arch/sdl/joy.c`, `arch/sdl/timer.c`, `arch/sdl/window.c`, `arch/sdl/mouse.c`, `arch/sdl/event.c` | platform glue that has to live next to the SDL adapters. Small; leave alone |
| `d2/include/android_surface.h` | 23 lines, belongs where it is |

---

## 4. Detailed breakdown: `arch/ogl/ogl.c` (the big one)

`d2/arch/ogl/ogl.c` has 3352 inserted lines. Categorizing by function:

### 4.a metl154 / merged-wall diagnostic helpers (static, Android-only)
Roughly 2000 lines. Every one of these is a candidate to move to
`android/app/src/main/cpp/shared/merged_wall_debug.c` (or a companion
file):

- `ogl_get_metl154_source_filter_sample`
- `ogl_clip_and_draw_metl154_single`
- `ogl_is_metl154_bitmap` -- delete, not move (per tranche 1 plan)
- `ogl_metl154_experiment_name`
- `ogl_reset_metl154_tmap2_submit_context`
- `ogl_set_metl154_tmap2_submit_context`
- `ogl_get_metl154_input_codes`
- `ogl_get_metl154_point_code_summary`  <-- the one the user called out
- `ogl_log_metl154_tmap2_route`
- `ogl_log_metl154_upload`
- `ogl_get_metl154_source_bitmap`
- `ogl_get_metl154_palette_counts`
- `ogl_get_metl154_alpha_class`
- `ogl_get_metl154_alpha_value`
- `ogl_get_metl154_source_sample`
- `ogl_get_metl154_source_vslice`
- `ogl_get_metl154_filter_state`
- `ogl_get_metl154_draw_state`
- `ogl_should_clear_metl154_secondary_units_for_single`
- `ogl_clear_metl154_secondary_units_for_single`
- `ogl_get_metl154_gl_state`
- `ogl_get_metl154_screen_area`
- `ogl_get_metl154_triangle_area`
- `ogl_log_metl154_submit`
- `ogl_log_metl154_split`
- `ogl_get_metl154_uv_points`
- `ogl_mark_metl154_source_log`
- `ogl_log_metl154_palette_source`
- `ogl_log_metl154_alpha_source`
- `ogl_log_metl154_diag`
- `ogl_log_metl154_state`
- `ogl_merged_wall_single_clip_matches_wid`
- `ogl_merged_wall_single_clear_matches_face_context`
- `ogl_merged_wall_single_draw_needs_state_reset`
- `ogl_merged_wall_next_draw_order`
- `ogl_merged_wall_track_face`
- `ogl_log_merged_wall_cover`
- `ogl_log_merged_wall_snapshot_if_pending`

Rename in one pass (`metl154` -> `merged_wall`), delete the `#define`
compatibility bridges that tranche 1 left in, move the bodies to shared,
leave only the call sites behind an `#ifdef ANDROID`.

Estimate after: 150-300 lines of inline hooks remain in d1/d2 ogl.c for
merged-wall rendering (the actual GL state changes at the draw site that
have to live on the hot path). Everything else moves. Saving per game:
~1800 lines.

### 4.b plain-transparent cached-premerge cache (helper block)
About 400 lines:
- `ogl_android_texmerge_cache_entry` struct
- `ogl_android_texmerge_cache[32]` static table
- `ogl_android_texmerge_cache_clear`, `ogl_android_texmerge_reset_entry`
- `ogl_android_texmerge_visible_dim`
- `ogl_android_texmerge_init_bitmap`
- `ogl_android_texmerge_log`
- `ogl_android_texmerge_build_uvs`
- `ogl_android_get_cached_plain_texmerge_bitmap`

These have no metl154 mentions anymore but are 100% Android-only. Move
to `android/app/src/main/cpp/shared/merged_wall_cache.c` (or fold into
`merged_wall_debug.c`). Saving per game: ~400 lines.

### 4.c `ogl_draw_tmap_2` rewrite
The `g3_draw_tmap_2` / `ogl_draw_tmap_2_internal` /
`ogl_clip_and_draw_tmap2_merge` decision is a legitimate change to the
draw path that has to stay in d1/d2 ogl.c. Target size: the outer
`g3_draw_tmap_2` plus a thin Android branch (~80-120 lines). Everything
inside those branches becomes calls into `merged_wall_draw.c` in shared.

### 4.d MSAA FBO + GPU timer + aniso/texfilt runtime controls
About 400 lines including struct state, FBO create/destroy, query
tripling, aniso helpers, and texfilt live-apply. These are pure Android
extensions to the renderer.

Options:
1. Keep as-is in d1/d2 ogl.c behind `#ifdef ANDROID`. This is OK -- it is
   already neatly localized.
2. Move the heavier functions (`ogl_msaa_create_fbo`, `ogl_msaa_destroy_fbo`,
   `ogl_apply_anisotropy_all`, `ogl_apply_nomip_filter`, the GPU timer
   helpers) to `android/app/src/main/cpp/shared/ogl_android_ext.c`.
3. Per `copilot-instructions.md`: "it's ok to have shared constants".
   Leave the `volatile int g_*_pending_apply` declarations and the
   `#define GL_TIME_ELAPSED_EXT` style constants in d1/d2, move bodies.

Recommended: option 2. Saving per game: ~300 lines.

### 4.e `ogl_loadtexture` + KTX2 / ETC2 / DXA mask paths
About 400 lines that interleave Android texture upload paths with the
existing `ogl_loadtexture`. This is the hardest to extract because the
function already exists in upstream and the Android additions branch on
file format and call into `etc2_decode.c`. Leave inline, but:
- extract `ogl_apply_nomip_filter` (standalone helper, ~30 lines)
- extract `ogl_load_dxa_mask` (~80 lines)
- keep `ogl_loadtexture` body in d1/d2 but shrink each Android branch to
  a single call into a shared helper

Target remaining: ~150 lines of inline dispatch.

### 4.f Include block and forward decls
The top-of-file `#include` additions and one-liner globals are fine to
keep; ~50 lines total.

---

## 5. Detailed breakdown: `main/net_udp.c`

`d2/main/net_udp.c`: 1045 inserted, 84 removed.

Broad shape:
- ~300 lines of android-specific helpers: `mpdiag_pkt_dump`,
  `sockaddr_equal` / `sockaddr_ip_equal`, `find_player_by_identity`,
  `net_udp_rebind_for_hosting`, host-migration PDATA restore,
  inventory cache, callsign dedupe, COOP QoL flag plumbing.
- ~200 lines of `[ANDROID]` log comments inside `net_log_comment()`
  calls sprinkled through `sync_poll`, `send_sync`, `resend_*`, etc.
- ~200 lines of behavior tweaks (host-migration master-slot fix,
  auto-start-when-full, LAN debug, etc.) behind `#ifdef __ANDROID__`.
- ~200 lines of structural additions: COOP QoL option on the netgame
  menu, new packet types, new tracker fields.

Extraction plan:
- Move the helpers listed in the first bullet to
  `android/app/src/main/cpp/shared/net_udp_android.c` with a narrow
  public header. These are self-contained and callable from the
  `#ifdef __ANDROID__` call sites that remain in net_udp.c.
- Route the `[ANDROID]` log strings through a small wrapper
  (`coop_log_comment("sync_poll: ...")`) defined in the shared header;
  the wrapper just calls `net_log_comment` with the prefix applied. That
  cuts every `snprintf(logbuf, ..., "[ANDROID] ...")` to a one-liner.
  Saves ~100 lines.
- The COOP QoL netgame-menu additions depend on touching `newmenu_item`
  arrays inside existing functions. Leave these inline; they are small
  and changing their location would be invasive.

Target after: 250-350 lines of diff per game. Saving: ~700 lines total
across both games.

---

## 6. Proposed work order (plan file names)

Each tranche lands under `android/ai tool plans/` with the same
structure as the `metl154_tranche*` files. Stop after each and confirm a
smoke test passes.

1. **cleanup_metl154_rename_finalize.md** -- delete the `#define`
   bridges in `ogl.c`, rename all remaining `metl154_*` identifiers to
   `merged_wall_*` in d1/d2. No code moves yet. Close out tranche 1
   leftovers.
2. **cleanup_metl154_extract_ogl_helpers.md** -- move the ~2000 lines of
   static diagnostic helpers from `arch/ogl/ogl.c` (both games) to
   `android/app/src/main/cpp/shared/merged_wall_debug.c` and/or a new
   `merged_wall_draw.c`. This is Part B of the original plan, long
   overdue.
3. **cleanup_metl154_extract_render_and_texmerge.md** -- move
   `render_metl154_*` and `texmerge_metl154_*` diagnostic helpers out of
   `main/render.c` and `main/texmerge.c`. Rename in the process.
4. **cleanup_ogl_ext_extract.md** -- extract MSAA, GPU timer,
   anisotropy, and DXA mask helpers from `ogl.c`.
5. **cleanup_net_udp_extract.md** -- extract the self-contained
   `sockaddr_*` / `find_player_by_identity` / `net_udp_rebind_for_hosting`
   / inventory-cache helpers. Introduce `coop_log_comment` and reroute
   the `[ANDROID]` log strings.
6. **cleanup_coop_files_move.md** -- move `coop_save.{c,h}`,
   `coop_warp.{c,h}`, `auto_net.{c,h}` to
   `android/app/src/main/cpp/shared/` (confirm with the user first,
   since these might be intended to upstream).
7. **cleanup_newmenu_touch_extract.md** -- extract touch / keyboard /
   drag-scroll helpers from `newmenu.c`, `menu.c`, `titles.c`.
8. **cleanup_playsave_bridge_extract.md** -- finish the launcher bridge
   extraction started in the `native-pilot-prefs-bridge` memory; move
   the 430 lines of launcher-callable helpers to shared.
9. **cleanup_songs_hmp_jukebox_extract.md** -- move Android-specific
   music hooks from `songs.c`, `hmp.c`, `jukebox.c` to shared (they
   mostly already call into shared, but the glue sits in d1/d2).
10. **cleanup_diff_shrink_survey_round2.md** -- rerun
    `android/diff_vs_upstream.ps1` and re-audit anything still over
    ~100 lines. Repeat case-by-case.

Before each tranche:
- run `.\android\diff_vs_upstream.ps1` and record the before number
- run `.\android\run-code-quality.ps1 -Fix`
- run at least one emulator smoke (`test_launch_to_automap.json5`)

---

## 7. Sharing rules between D1 and D2

Preference order, following `copilot-instructions.md`:

1. **Single file in `android/app/src/main/cpp/shared/`** that is
   `#included` from both D1 and D2 `CMakeLists.txt` via `target_sources`.
   Use forward-declared structs + accessor functions so the shared
   file does not need D1-only or D2-only headers.
2. **Per-game folders**: `shared/d1/foo.c` and `shared/d2/foo.c` only
   when the bodies genuinely diverge (e.g. D1 lacks `segment2_*`, D2 has
   extra player flags). Keep the two copies side by side so the diff
   between them is visible; prefer to slowly converge them.
3. **Inline `#ifdef ANDROID` block in d1/d2** only when the block is
   under ~5 lines. Annotate with `// android port: ...`.

Existing working examples:
- `game_introspect.cpp` -- shared, single copy, called from both D1 and D2.
- `merged_wall_debug.c` -- shared, single copy, partially populated.
- `console_ringbuf.cpp` -- shared, accessed from engine via C API.

When moving a helper, prefer route 1. If a function signature references
`segment *` or `object *`, use `struct segment *` in the shared header
and let each game's translation unit see its own concrete definition. If
the function reads through fields that differ between games, that is the
signal to split.

---

## 8. Validation checklist (per tranche)

Follows the established pattern:

1. `.\android\diff_vs_upstream.ps1` -- record before
2. code changes
3. `.\android\run-code-quality.ps1 -Fix`
4. `.\android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
5. `adb logcat -c; .\android\run_test.ps1 -ScriptName test_launch_to_automap.json5 -Game d2`
6. For ogl.c tranches, also run a multiplayer smoke (two-emulator) and
   a merged-wall visual check (the crosshair-capture from Part D of the
   original plan if it has landed by then; otherwise by-hand on the
   emulator).
7. `.\run-windows-build.ps1` -- confirm no d1/d2 change breaks MSVC
8. `.\android\diff_vs_upstream.ps1` -- record after; include in the
   tranche plan file's "results" section.

---

## 9. Open questions to resolve before starting

1. Are `coop_save`, `coop_warp`, and `auto_net` intended to upstream?
   If yes, they must NOT move to `android/shared/`. If no, they should.
2. Is the `songs.c` rewrite considered upstreamable? Memory
   `music-system-architecture.md` implies it is general-purpose.
3. Is the `native-pilot-prefs-bridge` considered load-bearing enough
   that its `extern` surface in `playsave.c` must stay in d1/d2, or is
   an `android_playsave_bridge.c` in shared acceptable with a single
   forward-declared API in `playsave.h`?
4. For `newmenu.c` extraction: the touch-related code mutates
   `newmenu *` state. Confirm the struct layout is identical between
   D1 and D2 so the helper can be single-copy. If not, per-game copies.
5. For `render.c`: the `android_is_metl154_target` bool (user-called-out
   name) should be renamed to `android_is_merged_wall_logging_target`
   or similar, matching the generic naming already used by
   `android_merged_wall_is_logging_target_tmap2`. Confirm naming.

---

## 10. Quick answers to the specific items in the request

- **Is the metl154 cleanup complete?** No. Tranches 1-2 landed; tranche 3
  (the extraction to shared) is the bulk of the remaining work. Most
  `metl154_*` symbol names in d1/d2 today exist only because tranche 1
  kept them as `#define` aliases.
- **`render_metl154_seg_pos_or_minus1()` and neighbors**: they are in
  `main/render.c` lines ~1906-2330, they are generic portal/segment
  tracking helpers for the merged-wall render-list logger. Move them to
  shared as `merged_wall_track_render_seg()` etc., under
  `merged_wall_debug.c` or a new `merged_wall_render_log.c`.
- **`ogl_get_metl154_point_code_summary()` and neighbors**: covered in
  section 4.a. They move to shared as part of the tranche-3 extraction.
- **Diff workflow**: `android/diff_vs_upstream.ps1` produces three
  files under `temp/`:
  - `d1d2_diff_numstat.txt` -- raw `git diff --numstat` output
  - `d1d2_diff_sorted.txt` -- same, sorted by total churn descending
  - `d1d2_diff_summary.txt` -- header plus top-N table, human readable
  Run before + after any tranche. Optional `-ShowContent` also dumps the
  full patch for the top-N files.
