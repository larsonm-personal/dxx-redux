# d1/d2 diff shrink -- phase 3 execution plan

Status: baseline established 2026-04-30. No tranches landed yet.

This is the current-state follow-on to `d1d2_diff_shrink_study.md` and
`d1d2_shrink_phase2_remaining_and_phase3_candidates.md`. Read those first
for the rationale, category taxonomy, and per-file breakdowns. This document
records the current baseline, answers the open questions, and lays out the
priority-ordered execution plan.

---

## Baseline (2026-04-30)

Run `.\android\diff_vs_upstream.ps1` before and after each tranche.

```
files:   243  (114 in d1/, 129 in d2/)
+added:  24822
-removed: 1519
```

Top files by total churn:

```
Added  Removed  Total  Path
-----  -------  -----  ----
 1627       49   1676  d2/arch/ogl/ogl.c
 1545       50   1595  d1/arch/ogl/ogl.c
 1189       21   1210  d2/main/newdemo.c
  798      204   1002  d2/main/net_udp.c
  929        0    929  d2/main/coop_save.c
  706      176    882  d1/main/net_udp.c
  872        0    872  d1/main/coop_save.c
  720      131    851  d1/main/state.c
  787       60    847  d2/main/state.c
  734       12    746  d2/main/escort.c
  632       26    658  d2/main/game.c
  598        6    604  d2/main/newmenu.c
  570        0    570  d1/main/playsave.c
  507        4    511  d1/main/newdemo.c
  460       14    474  d1/main/game.c
  455       15    470  d2/main/multi.c
  431       10    441  d2/main/inferno.c
  422       15    437  d1/main/multi.c
  419        0    419  d2/main/playsave.c
  383        6    389  d2/main/aipath.c
  372       10    382  d1/main/inferno.c
  357        4    361  d1/main/newmenu.c
  276        0    276  d2/main/songs.c
  252        4    256  d2/main/physics.c
  253        0    253  d1/main/songs.c
  243        1    244  d2/arch/ogl/gr.c
  242        1    243  d1/arch/ogl/gr.c
  229       18    247  d2/main/ai.c
  234        3    237  d2/main/laser.c
  203        5    208  d2/main/kconfig.c
  208       21    229  d2/libmve/mveplay.c
  181        7    188  d1/maths/rand.c
  179        7    186  d2/maths/rand.c
  170       30    200  d2/main/titles.c
  164       12    176  d2/arch/sdl/joy.c
  162       11    173  d1/arch/sdl/joy.c
  163        0    163  d2/main/coop_save.h
  161        0    161  d1/main/coop_save.h
  160        3    163  d2/main/gamerend.c
  156        3    159  d2/main/collide.c
  151        4    155  d2/main/controls.c
  132       21    153  d2/arch/ogl/oglprog.c
  132       21    153  d1/arch/ogl/oglprog.c
  136        5    141  d1/main/kconfig.c
  130        7    137  d2/main/automap.c
  124        3    127  d2/main/gamecntl.c
  126        0    126  d1/misc/hmp.c
  125        0    125  d2/misc/hmp.c
  116        1    117  d2/misc/physfsx.c
  114        0    114  d1/main/texmerge.c
  114        0    114  d2/main/texmerge.c
  110        1    111  d1/main/gamecntl.c
  106        0    106  d1/misc/physfsx.c
   97        0     97  d1/main/auto_net.c
   96        0     96  d2/main/auto_net.c
```

### What has already landed since the original study

- OGL shared-helper extraction phases 15-36: `d1/arch/ogl/ogl.c` went from
  3365 to 1595; `d2/arch/ogl/ogl.c` from 3399 to 1676. (~3500 lines saved)
- `coop_warp.{c,h}` moved to `android/app/src/main/cpp/shared/coop/`; d1/d2
  stubs are 2-line `#include` redirects.
- `net_udp.c` partial extraction: `sockaddr_*`, `mpdiag_pkt_dump`,
  `net_udp_rebind_for_hosting`, and `reattemptDirect` helpers moved to shared.

### Why total churn grew from +17362 to +24822

New Android-specific instrumentation landed in d1/d2 after the original study:
- Input-demo recorder/replay engine embedded in `newdemo.c`, `game.c`,
  `inferno.c`, `escort.c`, `state.c`, `aipath.c`, `ai.c`, `laser.c`, etc.
- Multi-stream RNG (`d_rng_stream`, `d_rand_annotated`) rewrite in `rand.c`
- Host-migration and guidebot-coop additions in `multi.c`, `state.c`
- Debug physics probes and collision logging in `physics.c`, `collide.c`

---

## Open questions -- now answered

**1. Are `coop_save`, `coop_warp`, `auto_net` intended to upstream?**
No, not initially. They are Android-scoped for the first merge. They may
upstream later when the desktop builds get coop save support, but that is a
future decision. Move them all to `android/app/src/main/cpp/shared/coop/`.
`coop_warp` is already there. `coop_save` and `auto_net` are next.

**2. Is the input-demo system (`newdemo.c`, `game.c`, `inferno.c`, etc.)
intended to upstream?**
Same answer: Android-scoped initially. The recorder/replay/dump code is a
major addition and needs to be proven on Android first. Move the bodies to
`android/app/src/main/cpp/shared/input_demo/` and leave only
`#ifdef __ANDROID__` call sites in d1/d2. This is Tier A in the plan below.

**3. Is the `songs.c` rewrite upstreamable?**
The core music-system rewrite (SAF paths, track list, jukebox) is
Android-scoped. Only the two overlay helpers (`songs_next_track`,
`songs_get_track_list`) are Android-only even within that rewrite. Move just
those two helpers to `shared/audio/songs_overlay.{c,h}`; the rest of songs.c
stays as-is.

**4. For `playsave.c`:** the bridge functions must remain the source of truth
per project conventions. Move the serialization bodies to
`shared/playsave_bridge.c`; keep the public API declarations in `playsave.h`.

**5. For `newmenu.c`:** the `newmenu *` struct is identical between D1 and D2;
a single-copy shared helper is valid.

---

## Categories of remaining churn

| Category | Rough share of remaining +24822 | Plan |
|---|---|---|
| Input-demo instrumentation (Tier A) | ~5000 across 8+ files | Extract to `shared/input_demo/` |
| coop_save + auto_net new files | ~2125 | Move to `shared/coop/` |
| net_udp.c remaining | ~1200 | Continue `cleanup_net_udp_extract.md` |
| state.c combined | ~1400 | Depends on coop_save + input_demo moves |
| ogl.c remaining | ~3271 | Mostly ETC2/KTX2 inline; hard to extract |
| newmenu.c | ~965 | Touch/keyboard/drag-scroll bodies to shared |
| multi.c combined | ~877 | Coop QoL helpers to shared |
| playsave.c combined | ~989 | Bridge bodies to shared |
| gr.c + oglprog.c combined | ~942 | GLES3 shim + texfilt to shared |
| songs.c combined | ~529 | Two overlay helpers |
| render.c combined | ~487 | Merged-wall tracking |
| escort.c | ~746 | Input-demo trace + guidebot-coop hooks |
| kconfig + gamecntl combined | ~449 | Touch binding helpers |
| hmp + physfsx combined | ~453 | TSF MIDI + SAF archiver glue |
| aipath + ai + laser + physics etc. | ~1200 | Input-demo probes + physics probes |
| titles + automap | ~370 | Touch/skip helpers |
| rand.c combined | ~374 | Leave -- RNG rewrite is engine-level |

---

## Execution order -- easy wins first

### Tier A: input-demo extraction (new, largest single opportunity)

The `input_demo_*` functions are uniformly prefixed, clearly Android-only
instrumentation, and live in isolated static-function blocks inside each file.
Extracting them establishes `android/app/src/main/cpp/shared/input_demo/` as
the canonical home, reducing d1/d2 to thin call-site stubs.

Target directory layout:
```
shared/input_demo/
    input_demo_bootstrap.c    -- startup: validate metadata, start replay, dump classic
    input_demo_recorder_engine.c  -- quick-record, recorder settings, classic paths
    input_demo_replay_engine.c    -- apply/finish replay frame, stop replay
    input_demo_dump.c         -- newdemo_dump_* JSON serializer
    input_demo_trace.c        -- per-file probes: escort, ai, laser, physics
    input_demo_control_trace.c -- player control trace serializers (newdemo)
```

**TA.1 -- escort.c input-demo trace block** (~280 lines in d2 only)
- Plan file: `cleanup_input_demo_escort_trace.md`
- Block: `input_demo_trace_escort_active`, `input_demo_log_escort_*`,
  `input_demo_reset_escort_state_probes`, `g_input_demo_escort_*` statics --
  all consecutive under `#ifdef __ANDROID__` after the include block
- Target: `shared/input_demo/input_demo_trace.c`
- Call site in d2/main/escort.c: a handful of `#ifdef __ANDROID__` one-liners
- d1 escort.c has no guidebot, so this is d2-only
- Expected shrink: ~280 lines (d2 only)
- Risk: low -- pure logging, no side effects on game state

**TA.2 -- inferno.c input-demo bootstrap** (~310 d2 + ~250 d1 lines)
- Plan file: `cleanup_input_demo_inferno_bootstrap.md`
- Block: `find_cmd_arg`, `maybe_validate_input_demo_metadata`,
  `input_demo_replay_hash_u8_sequence`, `input_demo_apply_replay_player_cfg`,
  `maybe_start_input_demo_replay`, `maybe_dump_classic_demo_json`,
  `configure_startup_fp_environment` -- all inside the large `#ifdef` block
  at the top of each game's inferno.c additions
- Target: `shared/input_demo/input_demo_bootstrap.c`
- d1/d2 blocks are near-identical; single shared copy with minor ifdefs where
  they diverge (d2 has `maybe_dump_classic_demo_json`, d1 does not)
- Expected shrink: ~530 lines combined
- Risk: low -- called once at startup, no hot path

**TA.3 -- newdemo.c control-trace serializers** (~70 lines each game)
- Plan file: sub-tranche of `cleanup_input_demo_newdemo_extract.md`
- Block: `nd_write_player_control_trace`, `nd_read_player_control_trace`,
  plus the `nd_player_control_trace` struct and the static
  `nd_dump_v_player_control_trace` -- isolated sections
- Target: `shared/input_demo/input_demo_control_trace.c`
- Risk: low

**TA.4 -- newdemo.c quick-record block** (~470 lines in d2, ~350 in d1)
- Plan file: `cleanup_input_demo_newdemo_extract.md`
- Block: `input_demo_android_quick_recording` statics,
  `input_demo_quick_record_mission_name`, `input_demo_clear_quick_recording`,
  `input_demo_release_recorder_settings`, `input_demo_capture_recorder_checkpoint`,
  `input_demo_is_mid_level_record_start`, `input_demo_ascii_equal_ignore_case`,
  `input_demo_fill_recorder_player_cfg`, `input_demo_strip_hog_extension`,
  `input_demo_sanitize_slug`, `input_demo_prepare_recorder_settings`,
  `input_demo_build_quick_record_name`, `input_demo_build_classic_demo_path`,
  `input_demo_delete_classic_demo_sidecar`, `input_demo_trim_new_recordings`,
  `maybe_start_input_demo_recording`, `maybe_flush_input_demo_recording`,
  `newdemo_stop_quick_recording`, `newdemo_toggle_quick_recording`
- Target: `shared/input_demo/input_demo_recorder_engine.c`
- Expected shrink: ~820 lines combined
- Risk: medium -- touches file paths and PhysFS; carefully wrap the two
  public entry points (`newdemo_stop_quick_recording`,
  `newdemo_toggle_quick_recording`) as externs declared in the existing
  `newdemo.h`

**TA.5 -- newdemo.c JSON dump block** (~449 lines in d2, ~150 in d1)
- Plan file: same `cleanup_input_demo_newdemo_extract.md` (second sub-tranche)
- Block: `newdemo_dump_set_error`, `newdemo_dump_active`,
  `newdemo_dump_reset_player_control_trace`, `newdemo_dump_reset_player_wiggle`,
  `newdemo_dump_note_player_wiggle`, `newdemo_dump_write_json_escaped`,
  `newdemo_dump_write_vector`, `newdemo_dump_write_matrix`, and all the
  remaining `newdemo_dump_*` helpers plus the `newdemo_dump_v_*` statics
- Target: `shared/input_demo/input_demo_dump.c`
- Expected shrink: ~600 lines combined
- Risk: low -- pure serialization, no game state mutation

**TA.6 -- game.c replay control block** (~480 d2 + ~430 d1 lines)
- Plan file: `cleanup_input_demo_game_extract.md`
- Block: `input_demo_player_motion_probe_active`,
  `input_demo_player_motion_frame_index`, `input_demo_log_player_motion_state`,
  `input_demo_record_game_frame`, `input_demo_update_rng_trace_context`,
  `input_demo_count_live_objects_of_type`, `input_demo_count_live_player_weapons`,
  `input_demo_capture_current_result`, `input_demo_log_result_state`,
  `input_demo_replay_fire_probe_active`, `input_demo_log_replay_fire_state`,
  `input_demo_write_replay_result`, `input_demo_finish_replay_from_level_exit`,
  `input_demo_finish_replay_from_mine_exit`, `input_demo_stop_replay`,
  `input_demo_delay_replay_frame`, `input_demo_apply_replay_frame`,
  `input_demo_finish_replay_frame`
- Target: `shared/input_demo/input_demo_replay_engine.c`
- Expected shrink: ~910 lines combined
- Risk: medium -- `input_demo_apply_replay_frame` injects player inputs; needs
  the per-game `Controls` struct. Wrap with thin per-game shims where the
  struct layouts diverge.

**TA.7 -- state.c input-demo checkpoint blocks** (~400 lines combined)
- Plan file: sub-tranche of `cleanup_state_input_demo_extract.md`
- Block: `input_demo_capture_recorder_checkpoint` calls and the
  `#ifdef __ANDROID__` blocks scattered through `state_save_all_sub` and
  `state_restore_all_sub` that save/restore input-demo state alongside the
  savegame
- Target: fold into `input_demo_recorder_engine.c` or a new
  `shared/input_demo/input_demo_state_bridge.c`
- Expected shrink: ~300 lines combined (some state.c additions are coop which
  belong to Tier B, not here; audit carefully)
- Risk: medium -- interleaved with coop save/restore; do after TA.4 and P3.1

**TA.8 -- ai/aipath/laser/physics probe blocks** (~600 lines combined)
- Plan file: `cleanup_input_demo_probe_files.md`
- Files: `d2/main/aipath.c` (+383), `d2/main/ai.c` (+229),
  `d2/main/laser.c` (+234), `d2/main/physics.c` (+252),
  `d2/main/collide.c` (+156), `d2/main/gamerend.c` (+160)
- All are uniformly `input_demo_*` probe blocks under `#ifdef __ANDROID__`
- Target: route through existing `input_demo_trace.c` or per-subsystem files
- Expected shrink: ~600+ lines (exact count needs per-file audit before starting)
- Risk: low-medium -- probes are read-only; no game state mutation

Total Tier A expected shrink: **~4500 lines** combined across d1+d2.

---

### Tier B: coop_save + auto_net relocation (P3.1 / P3.14)

- Plan file: `cleanup_coop_files_move.md` (existing; update status there)
- Files: `d1/main/coop_save.c` (872), `d2/main/coop_save.c` (929),
  `d1/main/coop_save.h` (161), `d2/main/coop_save.h` (163),
  `d1/main/auto_net.c` (97), `d2/main/auto_net.c` (96),
  `d1/main/auto_net.h` (62), `d2/main/auto_net.h` (65)
- `coop_warp` is already in `shared/coop/coop_warp.c`; this finishes the set
- Target: `android/app/src/main/cpp/shared/coop/coop_save.{c,h}` and
  `shared/coop/auto_net.{c,h}`
- The two `coop_save.c` copies differ by ~57 lines; attempt a single shared
  copy first using forward-declared struct accessors. Fall back to
  `shared/coop/d1/coop_save.c` + `shared/coop/d2/coop_save.c` only if
  the type divergence is too deep to bridge cleanly.
- d1/d2 `coop_save.c` and headers must drop to zero-line stubs or be deleted;
  engine call sites in `multi.c` and `state.c` are unchanged.
- Expected shrink combined: **~2125 lines**
- Risk: medium -- high line count but code is self-contained

---

### Tier C: net_udp.c continuation (P3.2)

- Plan file: `cleanup_net_udp_extract.md` (existing; continue from current state)
- `sockaddr_*`, `mpdiag_pkt_dump`, `net_udp_rebind_for_hosting`, and
  `reattemptDirect` helpers are already in shared. The remaining large
  candidate is the `net_udp_welcome_player` slot-selection and
  reconnect-reset cluster.
- See `cleanup_net_udp_extract.md` for the tranche history and continuation.
- Expected additional shrink: ~400-500 lines combined
- Risk: medium

---

### Tier D: state.c coop save/restore glue (P3.7 -- after P3.1)

- Plan file: `cleanup_state_coop_extract.md`
- The `state.c` diff is large partly because of coop save/restore and partly
  because of input-demo checkpoint saves. Once Tier A (TA.7) and Tier B
  (P3.1) land, this tranche extracts the remaining coop orchestration.
- Expected shrink per game: ~130 lines after the Tier A and B pieces are gone
- Risk: low after dependencies land

---

### Tier E: gr.c + oglprog.c (P3.8)

- Plan file: `cleanup_gr_oglprog_extract.md`
- `d1/arch/ogl/gr.c` (+242 -1), `d2/arch/ogl/gr.c` (+243 -1)
- `d1/arch/ogl/oglprog.c` (+132 -21), `d2/arch/ogl/oglprog.c` (+132 -21)
- Per-frame texture filter reapply loop (twin of phase 31/33 aniso helper)
  moves to `ogl_texture_android.c`; GLES3 shim selection moves to
  `gles3_shim.c` or `oglprog_android.c`; MSAA remainder folds into
  `ogl_msaa_android.c`
- Expected shrink combined: ~400 lines
- Risk: low-medium

---

### Tier F: multi.c coop QoL helpers (P3.3)

- Plan file: `cleanup_multi_coop_extract.md`
- `d1/main/multi.c` (+422 -15), `d2/main/multi.c` (+455 -15)
- Coop autosave init, guidebot owner save/restore, inventory broadcast helpers
- Expected shrink per game: ~250-300 lines
- Risk: medium (Netgame/Players state differs between games)

---

### Tier G: playsave.c bridge (P3.4)

- Plan file: `cleanup_playsave_bridge_extract.md`
- `d1/main/playsave.c` (+570), `d2/main/playsave.c` (+419)
- Move launcher-callable bridge function bodies to `shared/playsave_bridge.c`;
  keep extern prototypes in `playsave.h`
- Expected shrink per game: ~200-300 lines
- Risk: medium

---

### Tier H: newmenu.c touch/keyboard/drag-scroll (P3.5)

- Plan file: `cleanup_newmenu_touch_extract.md`
- `d1/main/newmenu.c` (+357 -4), `d2/main/newmenu.c` (+598 -6)
- Soft-keyboard show/hide, drag-scroll body, deferred check/radio toggle,
  keyboard field Y-coordinate tracking
- Target: `shared/ui/newmenu_android.{c,h}`
- Expected shrink combined: ~300-400 lines
- Risk: medium

---

### Tier I: small mop-up tranches

Each is a standalone 1-2 hour tranche; do in any order after Tiers A-H.

| Tranche | Files | Est. shrink | Plan file |
|---|---|---|---|
| songs.c overlay helpers | d1+d2 songs.c (+529) | ~200 | `cleanup_songs_overlay_extract.md` |
| render.c merged-wall tracking | d1+d2 render.c (+487) | ~300 | `cleanup_render_merged_wall_extract.md` |
| titles.c intro-movie skip | d1+d2 titles.c (+246) | ~150 | `cleanup_titles_touch_extract.md` |
| kconfig.c + gamecntl.c touch | d1+d2 (+482) | ~270 | `cleanup_kconfig_touch_extract.md` |
| automap.c touch helpers | d1+d2 (+163) | ~120 | `cleanup_automap_escort_extract.md` |
| hmp.c TSF routing | d1+d2 (+251) | ~200 | `cleanup_hmp_physfsx_extract.md` |
| physfsx.c SAF archiver | d1+d2 (+222) | ~180 | same |
| texmerge.c metl154 overlay | d1+d2 (+228) | ~200 | `cleanup_render_merged_wall_extract.md` |

---

### Do not extract (Tier J)

| File | Reason |
|---|---|
| `d1+d2 maths/rand.c` (+374 combined) | Multi-stream RNG rewrite is an engine-level change, not Android-only; leave for upstreaming as-is |
| `d2/libmve/mveplay.c` (+208 -21) | Movie decoder changes are cross-platform fixes; leave as-is |
| `d1+d2 arch/sdl/joy.c` (+326 combined) | Platform SDL glue; must stay with the SDL adapters |
| `d1+d2 arch/ogl/ogl.c` ETC2/KTX2 blocks | Too interleaved with the existing upload function to extract cleanly; flagged as skip in prior study |
| All CMakeLists.txt | Required build wiring; not a shrink target |

---

## Projected totals after all tranches

| Group | Est. shrink |
|---|---|
| Tier A: input-demo extraction | ~4500 |
| Tier B: coop_save + auto_net | ~2125 |
| Tier C: net_udp.c continuation | ~450 |
| Tier D: state.c coop glue | ~260 |
| Tier E: gr.c + oglprog.c | ~400 |
| Tier F: multi.c coop QoL | ~550 |
| Tier G: playsave.c bridge | ~500 |
| Tier H: newmenu.c touch | ~350 |
| Tier I: small mop-up | ~1620 |
| **Grand total** | **~10755** |

Bringing insertions from +24822 down to roughly **+14000**, concentrated in
the few genuinely-shared engine changes (ogl.c ETC2 paths, host-migration
dispatch in net_udp.c, RNG in rand.c, SDL joy.c platform glue).

---

## Validation checklist (each tranche)

1. `.\android\diff_vs_upstream.ps1` -- record before number
2. Code changes
3. `.\android\run-code-quality.ps1 --fix` -- wait for full exit
4. `.\android\gradlew.bat :app:assembleDebug :app:testDebugUnitTest`
5. `adb logcat -c; .\android\run_test.ps1 -ScriptName test_launch_to_automap.json5 -Game d2 2>&1 | Out-File temp\test_output.txt -Encoding utf8`
6. `.\run-windows-build.ps1` -- confirm no d1/d2 regression
7. `.\android\diff_vs_upstream.ps1` -- record after number; update this file

---

## Progress log

(Updated as tranches complete)

- [ ] TA.1 escort.c input-demo trace
- [ ] TA.2 inferno.c input-demo bootstrap
- [ ] TA.3 newdemo.c control-trace serializers
- [ ] TA.4 newdemo.c quick-record block
- [ ] TA.5 newdemo.c JSON dump block
- [ ] TA.6 game.c replay control block
- [ ] TA.7 state.c input-demo checkpoint
- [ ] TA.8 ai/aipath/laser/physics probe blocks
- [ ] TB: coop_save + auto_net relocation
- [ ] TC: net_udp.c continuation
- [ ] TD: state.c coop glue
- [ ] TE: gr.c + oglprog.c
- [ ] TF: multi.c coop QoL
- [ ] TG: playsave.c bridge
- [ ] TH: newmenu.c touch
- [ ] TI.1 songs.c overlay helpers
- [ ] TI.2 render.c merged-wall tracking
- [ ] TI.3 titles.c intro-movie skip
- [ ] TI.4 kconfig + gamecntl touch
- [ ] TI.5 automap touch helpers
- [ ] TI.6 hmp + physfsx
- [ ] TI.7 texmerge metl154 overlay
