# d1/d2 diff shrink -- phase 2 remaining + phase 3 candidates

Status: phase 2 complete through phase 36; phase 3 started with validated `coop_warp` extraction in `cleanup_coop_files_move.md`. This is the follow-on to
`d1d2_diff_shrink_study.md` and the 31 completed
`ogl_shared_helper_extraction_phase*.md` tranches.

Anchor numbers after phase 31:
- 199 files changed, `+17362 / -801`
- `d1/arch/ogl/ogl.c`: `+1672 -50 total 1722`
- `d2/arch/ogl/ogl.c`: `+1696 -49 total 1745`

Update after phase 36:
- phases 32 through 36 completed and validated
- `d1/arch/ogl/ogl.c`: `+1508 -50 total 1558`
- `d2/arch/ogl/ogl.c`: `+1590 -49 total 1639`

Purpose of this file: enumerate the concrete next-tranche candidates, ranked by
expected per-file line shrink, with enough detail to start each one without
re-surveying. Phase 2 = still inside the two ogl.c files. Phase 3 = everything
else in d1/ and d2/.

---

## Phase 2 complete -- historical candidates inside `d1/d2 arch/ogl/ogl.c`

Prior phases 15-31 mostly drained category 4.a (metl154 / merged-wall
diagnostic) and 4.b (cached-texmerge plain-transparent cache). What is still
inline in ogl.c now:

### 2.a `ogl_load_dxa_mask()` -- category 4.e
- Scope: ~45 line self-contained PNG-mask loader, fully under `#ifdef ANDROID`
- Dependencies: PNG reader, `ogl_loadtexture` -- both callable from a shared
  translation unit via existing headers
- d1/d2 near-identical; single shared copy
- Target: move body to `shared/ogl_texture_android.c`; leave a thin wrapper if a
  caller still references the original name, otherwise delete entirely
- Expected shrink per file: ~40 lines
- Risk: low. No private macros, no pointer arithmetic into ogl.c globals

### 2.b `ogl_apply_texfilt_all()` (runtime texfilt reapply loop) -- 4.d
- Scope: ~40 line loop over `ogl_texture_list` that rebinds + calls
  `glTexParameteri` when the launcher changes TexFilt live
- Lives inline in `ogl_start_frame()` today; already has an obvious twin in the
  extracted `android_ogl_apply_anisotropy_all()`
- d1/d2 identical
- Target: add `android_ogl_apply_texfilt_all(...)` next to the anisotropy
  helper in `shared/ogl_texture_android.c`, taking the same
  `android_ogl_texture_list_state` + the texfilt pending-apply flag pointer
- Expected shrink per file: ~35 lines
- Risk: very low. Pattern already validated in phase 31

### 2.c GPU timer triple-buffer query rotation -- 4.d
- Scope: ~50 lines split between `ogl_start_frame` (begin + read-back) and
  `gr_flip` (end query, rotate write pointer)
- Currently reads/writes private `ogl_gpu_queries[]`, `gpu_query_write_idx`,
  `gpu_query_count` globals
- d1/d2 identical
- Target: `shared/ogl_gpu_timer_android.{h,c}`, with a runtime-state struct
  holding pointers to the three globals, following the
  `android_ogl_texture_runtime_state` pattern from phase 31
- Expected shrink per file: ~45 lines (a bit less than raw because the struct
  construction adds ~5 lines at the two call sites)
- Risk: low. The function boundary is clean -- only integer state, no GL object
  pointers

### 2.d Framebuffer introspection grid sampler in `gr_flip` -- 4.a
- Scope: ~28 line `glReadPixels` center + 4x4 grid that already calls into
  shared `android_merged_wall_finish_snapshot`
- d1/d2 identical
- Target: add `android_merged_wall_sample_snapshot_framebuffer(...)` to
  `shared/merged_wall_debug.c` and reduce the call site in both ogl.c files to
  one line
- Expected shrink per file: ~25 lines
- Risk: very low. This block has no private ogl.c state and already depends on
  the shared header

### 2.e First-N bind-logging block + experiment-mode apply logging -- 4.a
- Scope: two small runs of logging code that wrap `android_texture_debug_*`
  calls. Each ~10-15 lines
- d1/d2 identical
- Target: fold into `android_texture_debug.c`; the inline blocks reduce to a
  single shared-helper call each
- Expected shrink per file: ~20 lines combined
- Risk: very low

### 2.f `ogl_android_texmerge_cache_clear` + cache-entry `static` table -- 4.b finisher
- Scope: the cache-entry `ogl_android_texmerge_cache_entry` struct and the
  `ogl_android_texmerge_cache[32]` table still live in both ogl.c files.
  The clear+slot helpers already delegate to shared, but the storage itself
  does not
- Moving the storage is the bigger unlock: once the table lives in
  `shared/merged_wall_debug.c`, the two shim functions (`cache_clear` and
  `get_cached_plain_texmerge_bitmap`) can drop their pass-through argument
  threading
- d1/d2 identical
- Expected shrink per file: ~60-80 lines (table + struct + wrapper tightening)
- Risk: medium. The table holds `ogl_texture *` pointers into each game's
  `ogl_texture_list`, so the shared file must treat them as opaque and not
  deref fields. Needs a careful check that every access already goes through
  existing helpers
- This is the one phase-2 tranche big enough to justify a standalone phase file

### 2.g `ogl_loadtexture` ETC2/KTX2 upload inline block -- 4.e
- Scope: the ~140 line KTX2 open/parse/upload block and ~100 line ETC2
  self-test FBO render, both inline in `ogl_loadbmtexture_f`
- Previous study already flagged this as "stays inline; too invasive to
  extract cleanly"
- Revisit only if all other phase-2 tranches are exhausted and ogl.c is still
  above target. Most likely answer: do not extract
- Expected shrink: N/A (skip for now)

### Phase 2 remaining -- summary
Combining 2.a through 2.f, realistic phase-2 shrink per file was **~220-240
lines**, bringing ogl.c to roughly `+1450 / -50`. With phase 36 validated,
phase 2 is complete and attention shifts to phase 3.

Order of execution (best first):
1. **phase 32** -- 2.a + 2.e (dxa mask + small logging blocks). Short, low risk.
   Target shrink: ~60 per file.
2. **phase 33** -- 2.b (texfilt live-apply). Exact twin of the phase 31 aniso
   helper. Target shrink: ~35 per file.
3. **phase 34** -- 2.c (GPU timer). Mid-size runtime-state extraction, same
   pattern as phase 31's bind/runtime state. Target shrink: ~45 per file.
4. **phase 35** -- 2.d (framebuffer sampler). Target shrink: ~25 per file.
5. **phase 36** -- 2.f (cached-texmerge table relocation). Largest remaining;
   do last because it is the most intrusive to the shared file. Target shrink:
   ~60-80 per file.

---

## Phase 3 -- candidates outside ogl.c

Order of appearance in this list is **largest expected shrink first**. Each
item lists one proposed phase plan file name, the target files, the expected
shrink per game, and the key concrete blocks to extract.

### P3.1 -- `coop_save.{c,h}` + `coop_warp.{c,h}` relocation
- Plan file: `cleanup_coop_files_move.md`
- Files: `d1/main/coop_save.c` (872), `d2/main/coop_save.c` (929),
  `d1/main/coop_save.h` (161), `d2/main/coop_save.h` (163),
  `d1/main/coop_warp.c` (371), `d2/main/coop_warp.c` (371),
  `d1/main/coop_warp.h` (70), `d2/main/coop_warp.h` (70)
- Dispositions:
  - The two coop_warp copies are reportedly identical -- move a single copy to
    `android/app/src/main/cpp/shared/coop/coop_warp.c` and include it from both
    games' `CMakeLists.txt`
  - The two coop_save copies differ by ~57 lines. Attempt single shared copy
    first with forward-decl shims for d1/d2 type differences. Fall back to
    `shared/coop/d1/coop_save.c` + `shared/coop/d2/coop_save.c` if compile
    divergence is too deep
- d1/d2 coop_save.c and coop_save.h must drop to zero-line stubs or be deleted
  entirely, with the engine call sites in `multi.c` and `state.c` unchanged
- Expected shrink combined (both games): ~2900 lines
- **OPEN QUESTION**: are coop_save / coop_warp / auto_net intended to upstream?
  Study doc flagged this; reading the coop-*.md plan files suggests they are
  android-scoped, but confirm before moving
- Risk: medium. Lots of raw lines, but the code is self-contained

### P3.2 -- `net_udp.c` android helper extraction
- Plan file: `cleanup_net_udp_extract.md` (new; supersedes
  `net_udp_cleanup_candidates.md` once work starts)
- Files: `d1/main/net_udp.c` (+944 -58), `d2/main/net_udp.c` (+1045 -84)
- Status on current branch: the shared `sockaddr` / identity helper extraction is validated; follow-up trims have removed the `read_sync` per-player address dump, the generic `net_udp_listen` heartbeat, and the success-path `[ANDROID]` sync or level-transition comments in `wait_for_sync`, `send_sync`, `request_poll`, and `level_sync` while keeping warning and failure paths intact; next narrow candidate is `mpdiag_pkt_dump`; see `cleanup_net_udp_extract.md` for the tranche history and validation sequence
- Concrete blocks to extract to `shared/net/net_udp_android.{h,c}`:
  - `mpdiag_pkt_dump` -- ~20 lines
  - `sockaddr_equal` and `sockaddr_ip_equal` -- ~20 lines combined
  - `find_player_by_identity` -- ~20 lines
  - `net_udp_rebind_for_hosting` -- ~25 lines
  - PDATA RX/TX diagnostic counter blocks -- deletable, ~60 lines
  - DROP status/addr/size per-frame diagnostics -- deletable, ~60 lines
  - Inventory-cache restore on rejoin (~30 lines near line 2556 in d2)
  - Callsign deduplication logic in join handler -- move body, ~50 lines
  - `[ANDROID]` log-comment prefix wrapper (`coop_log_comment`) -- collapses
    dozens of `snprintf(logbuf, ...)` calls into one-liners, ~100 lines saved
- Expected shrink per game: ~500 lines
- Risk: medium. Many blocks, but each is individually safe. Recommend 3-4
  sub-tranches (diagnostics-delete, sockaddr/helpers, host-migration, log-
  comment wrapper)

### P3.3 -- `multi.c` coop QoL + host-migration extraction
- Plan file: `cleanup_multi_coop_extract.md`
- Files: `d1/main/multi.c` (+422 -15), `d2/main/multi.c` (+455 -15)
- Concrete blocks:
  - Coop autosave init and checkpoint entry points (~50 lines) -- move to
    shared coop_save
  - Coop guidebot owner save/restore (~130 lines) -- move to shared coop_save
    (or new `shared/coop/coop_guidebot.c`)
  - Coop inventory broadcast helpers (~180 lines) -- shared
  - Callsign dedupe on join (~30 lines) -- shared or leave inline
- Expected shrink per game: ~250-300 lines
- Risk: medium. Touches mutable Netgame/Players state, which differs between
  games. Use accessor functions at the boundary

### P3.4 -- `playsave.c` native-pilot-prefs bridge isolation
- Plan file: `cleanup_playsave_bridge_extract.md`
- Files: `d1/main/playsave.c` (+430), `d2/main/playsave.c` (+279)
- Target: move all launcher-callable bridge functions to
  `shared/playsave_bridge.c`; keep only `extern` prototypes plus a small
  number of read/write helpers that require direct engine struct access in
  d1/d2 playsave.c
- Verification: the memory note `native-pilot-prefs-bridge.md` identifies the
  entry points
- Expected shrink per game: ~200-300 lines
- Risk: medium. These functions read/write `GameCfg`, `PlayerCfg`, and the
  per-pilot file format. They must remain the source of truth (per
  copilot-instructions.md), which means the shared file may need its own
  `extern` access to engine globals. Prefer moving the serialization logic;
  keep the file I/O close to existing upstream code

### P3.5 -- `newmenu.c` touch / keyboard / drag-scroll extraction
- Plan file: `cleanup_newmenu_touch_extract.md`
- Files: `d1/main/newmenu.c` (+287 -1), `d2/main/newmenu.c` (+532 -4)
- Concrete blocks:
  - Soft-keyboard show/hide on INPUT / INPUT_MENU items (~60 lines each game)
  - Touch drag-scroll body (~30 lines)
  - Deferred check/radio toggle on button-up (~40 lines)
  - Keyboard field Y-coordinate tracking (~20 lines)
- Target: `shared/ui/newmenu_android.{c,h}`; keep the call sites as one-liners
  under `#ifdef ANDROID`
- Expected shrink combined: ~300-400 lines
- Risk: medium. These mutate `newmenu *` state. If the struct diverges between
  games, fall back to per-game copies

### P3.6 -- `songs.c` overlay / picker helpers
- Plan file: `cleanup_songs_overlay_extract.md`
- Files: `d1/main/songs.c` (+253), `d2/main/songs.c` (+276)
- Concrete blocks (two isolated `#ifdef __ANDROID__` regions in each file):
  - `songs_next_track()` unified touch-overlay track control (~32 lines)
  - `songs_get_track_list()` JSON track picker builder (~60 lines)
- Target: `shared/audio/songs_overlay.{c,h}`; call sites stay
- Expected shrink combined: ~200-230 lines
- Risk: low. Both helpers call existing cross-platform music APIs only
- **OPEN QUESTION**: is the songs.c rewrite intended to upstream? If yes, only
  the two overlay helpers move; the rest of the rewrite stays

### P3.7 -- `state.c` coop save/restore glue
- Plan file: `cleanup_state_coop_extract.md`
- Files: `d1/main/state.c` (+180 -6), `d2/main/state.c` (+216 -14)
- Concrete blocks:
  - `coop_save_restore_from_metadata` orchestration (~150 lines) -- shared
  - Coop tracked-face snapshot on restore (~40 lines) -- shared
  - Autosave save-list init (~10 lines) -- keep inline
- Expected shrink per game: ~130-170 lines
- Risk: medium. Depends on coop_save being relocated first (P3.1)

### P3.8 -- `gr.c` + `oglprog.c` GLES3 + runtime-control extraction
- Plan file: `cleanup_gr_oglprog_extract.md`
- Files: `d1/arch/ogl/gr.c` (+227 -1), `d2/arch/ogl/gr.c` (+228 -1),
  `d1/arch/ogl/oglprog.c` (+132 -21), `d2/arch/ogl/oglprog.c` (+132 -21)
- Concrete blocks:
  - `gr.c`: per-frame texture filter reapply loop (~50 lines) -- move to
    `ogl_texture_android.c` (same helper as phase 33 if that lands first)
  - `gr.c`: menu-mode viewport offset (~10 lines) -- keep inline (hot path,
    already a few lines)
  - `gr.c`: MSAA bind/unbind helpers -- some already in `ogl_msaa_android.c`;
    fold remainder
  - `oglprog.c`: GLES3 shim selection and shader-source switch -- move body to
    `gles3_shim.c` or new `shared/ogl/oglprog_android.c`; keep the `#ifdef`
    dispatch
- Expected shrink combined: ~350-400 lines
- Risk: low-medium. The gr.c changes are the most likely to have subtle state
  interactions with frame flip

### P3.9 -- `render.c` merged-wall tracking finisher
- Plan file: `cleanup_render_merged_wall_extract.md`
- Files: `d1/main/render.c` (+243 -16), `d2/main/render.c` (+244 -14)
- Concrete blocks:
  - Merged-wall render state + tracking (~150 lines)
  - `render_merged_wall_cover_summary` (~50 lines)
  - `coop_indicator_lines_render()` call site (~5 lines, keep inline)
- Target: `shared/merged_wall_debug.c` (already exists)
- Expected shrink per game: ~150-180 lines
- Risk: medium. Some of the tracking state is already partly extracted; audit
  before starting to avoid re-opening already-closed phases

### P3.10 -- `titles.c` intro-movie skip + resume-on-input
- Plan file: `cleanup_titles_touch_extract.md`
- Files: `d2/main/titles.c` (+159 -30), `d1/main/titles.c` (+76 -0)
- Target: `shared/ui/titles_android.c`
- Expected shrink combined: ~150 lines
- Risk: low

### P3.11 -- `kconfig.c` + `gamecntl.c` touch binding / gamepad defaults
- Plan file: `cleanup_kconfig_touch_extract.md`
- Files: `d2/main/kconfig.c` (+167 -5), `d1/main/kconfig.c` (+100 -5),
  `d2/main/gamecntl.c` (+93 -2), `d1/main/gamecntl.c` (+74 -0)
- Target: `shared/input/kconfig_android.{c,h}`
- Expected shrink combined: ~270 lines
- Risk: low

### P3.12 -- `automap.c` + `escort.c` touch helpers
- Plan file: `cleanup_automap_escort_extract.md`
- Files: `d2/main/automap.c` (+130 -7), `d1/main/automap.c` (+33 -5),
  `d2/main/escort.c` (+129 -5)
- Target: `shared/ui/automap_touch.c` + fold escort guidebot-coop state into
  P3.3 (`shared/coop/coop_guidebot.c`)
- Expected shrink combined: ~220 lines
- Risk: low-medium

### P3.13 -- `hmp.c` + `physfsx.c` + minor misc files
- Plan file: `cleanup_hmp_physfsx_extract.md`
- Files: `d1/misc/hmp.c` (+126 -0), `d2/misc/hmp.c` (+125 -0),
  `d1/misc/physfsx.c` (+105 -0), `d2/misc/physfsx.c` (+114 -1)
- Target: TSF MIDI routing through `digi_tsf_music.c` in shared; SAF archiver
  glue into a shared `physfsx_android.c`
- Expected shrink combined: ~450 lines
- Risk: low

### P3.14 -- `auto_net.{c,h}` relocation
- Plan file: part of P3.1 `cleanup_coop_files_move.md`
- Files: `d1/main/auto_net.c` (+97), `d2/main/auto_net.c` (+96),
  `d1/main/auto_net.h` (+62), `d2/main/auto_net.h` (+65)
- Target: `shared/coop/auto_net.{c,h}` (single copy)
- Expected shrink combined: ~320 lines
- **OPEN QUESTION**: same upstreaming question as P3.1

---

## Rolling totals if phase 2 and phase 3 land fully

Current total: `+17362 / -801` across 199 files.

| Tranche group | Est. shrink |
|---|---|
| Phase 2 remaining (2.a-2.f, across two ogl.c files) | ~500 |
| P3.1 coop_save + coop_warp + auto_net | ~3200 |
| P3.2 net_udp.c | ~1000 |
| P3.3 multi.c | ~550 |
| P3.4 playsave.c | ~500 |
| P3.5 newmenu.c | ~350 |
| P3.6 songs.c | ~200 |
| P3.7 state.c | ~300 |
| P3.8 gr.c + oglprog.c | ~400 |
| P3.9 render.c | ~300 |
| P3.10 titles.c | ~150 |
| P3.11 kconfig + gamecntl | ~270 |
| P3.12 automap + escort | ~220 |
| P3.13 hmp + physfsx | ~450 |

Grand-total potential shrink: **~8400 lines** if every tranche lands cleanly
and the coop / songs / playsave upstreaming questions all come back "move to
shared". That would bring the overall diff from ~17k insertions down to
roughly 9k, with ogl.c / net_udp.c the two files still dominating.

---

## Open questions to resolve before starting phase 3

1. Are `coop_save`, `coop_warp`, and `auto_net` intended to upstream? If yes,
   P3.1 and P3.14 cannot move files to `android/shared/`
2. Is the `songs.c` music-system rewrite upstreamable? If yes, only the two
   overlay helpers move in P3.6
3. For `playsave.c`: the native-pilot-prefs bridge must remain the source of
   truth per `copilot-instructions.md`. Confirm that moving serialization
   bodies to shared -- while keeping the public API in `playsave.h` -- is
   acceptable, or whether the bridge should stay entirely in d1/d2
4. For `newmenu.c`: confirm the `newmenu *` struct layout is identical between
   D1 and D2. If not, P3.5 uses per-game copies under `shared/ui/d1/` and
   `shared/ui/d2/`
5. For `render.c`: audit what merged-wall tracking state has already been
   extracted (partial work in earlier phases) to avoid churn

---

## Suggested execution order

Short, safe tranches first to keep validation pipeline green, then the biggest
wins:

1. Phase 32 (2.a + 2.e) -- small ogl.c cleanup
2. Phase 33 (2.b) -- texfilt twin of aniso
3. **Resolve open question 1** (coop upstreaming)
4. P3.1 coop_save + coop_warp + auto_net -- biggest single win
5. Phase 34 (2.c) -- GPU timer
6. P3.2 net_udp.c -- split into 3-4 sub-tranches
7. Phase 35 (2.d) + Phase 36 (2.f) -- finish phase 2
8. **Resolve open questions 2, 3, 4**
9. P3.3 multi.c, P3.7 state.c (pair them; they share coop_save dependency)
10. P3.4 playsave.c
11. P3.5 newmenu.c
12. P3.6 songs.c
13. P3.8 gr.c + oglprog.c
14. P3.9 render.c
15. P3.10 titles.c, P3.11 kconfig, P3.12 automap/escort, P3.13 hmp/physfsx --
    small mop-up tranches at the end
