# Plan: `.dem` -> JSON converter and desync analysis using paired demo artifacts

Date: 2026-04-29
Inputs (in `android/temp_game_logs/`, ignore `old/`):

| set | dem | dximdemo | rngtrace.jsonl | game log |
|---|---|---|---|---|
| L1 | `d2_descent2_level1_20260429_183734.dem` (177 KB) | `d2_descent2_level1_20260429_183734.dximdemo` (52 KB, 279 frames) | `d2_descent2_level1_20260429_183734.dximdemo.rngtrace.jsonl` (213 KB, 999 rand events) | `debuglog_20260429_183714.txt` (6.4 MB, both sets) |
| L2 | `d2_descent2_level2_20260429_183812.dem` (894 KB) | `d2_descent2_level2_20260429_183812.dximdemo` (113 KB) | `d2_descent2_level2_20260429_183812.dximdemo.rngtrace.jsonl` (4.5 MB, 21498 rand events) | (same shared log) |

## Status update

- Tool implementation is complete, but not as a standalone `tools/dem2json` target. The working implementation is an engine-backed host dump mode in the D2 executable:
   `buildd2/main/dxx-redux-d2.exe -hogdir <D2 data dir> -classicdemo-dump-json <input.dem> <output.jsonl>`
- Reason for the change: classic `.dem` decode depends on live level geometry and model metadata, so the standalone byte-parser plan was not robust enough for the supplied demos.
- Implemented code lives in `d2/main/newdemo.c`, `d2/main/newdemo.h`, `d2/main/inferno.c`, and `d2/main/gameseq.c`.
- Dump mode is headless and skips playback-only side effects that are not needed for JSON capture and were crashing host playback: sound, trigger handling, wall hit/toggle handling, HUD messages, and effect blowup handling.
- Final validation succeeded on both supplied demos with no trace sidecar left behind:
   - L1: `temp/l1_classic_demo_final.jsonl`, 225 lines, result `{"type":"result","frames_decoded":223,"objects_emitted":2721,"truncated":false}`
   - L2: `temp/l2_classic_demo_final.jsonl`, 661 lines, result `{"type":"result","frames_decoded":659,"objects_emitted":13473,"truncated":false}`
- Frame-detail follow-up outputs are now available for direct comparison work:
   - L1 classic dump with direct player motion fields: `temp/l1_classic_demo_direct_wiggle.jsonl`
   - L2 classic dump with direct player motion fields: `temp/l2_classic_demo_frame_detail.jsonl`
   - L1 correlated frame-detail TSV: `temp/l1_compare_frame_detail.tsv`
   - L2 correlated frame-detail TSV: `temp/l2_compare_frame_detail.tsv`
- New recordings made with the current D2 build now serialize a dedicated per-frame player control trace into the `.dem` stream. The dump exposes it as `player.control`, and the correlator now carries `classic_same_control_*` TSV columns for it.
- The classic demo format version is now `16`, while dump/playback still accepts older `15` demos. Validation re-dumped the existing L1 demo successfully after the change (`temp/l1_classic_demo_after_control_trace.jsonl`), confirming old artifacts still decode.
- Next phase is no longer tool construction. It is correlation and desync analysis using the emitted classic-demo JSONL alongside the `.dximdemo`, `.rngtrace.jsonl`, and debug log.

## Pre-work confirmation: format of existing artifacts

- `.dximdemo` is JSONL: 1 header + 1 checkpoint + N `{"type":"frame","f":N,"ft":FT,"input":{...},"rng":{"s":STATE,"c":CALL_COUNT}}` + 1 result
- `.rngtrace.jsonl` is JSONL: 1 meta + N `{"type":"rand","seq":N,"frame":F,"gt":GT,"call_count":C,"state_before":S0,"state_after":S1,"result":R,"line":L,"file":F,"func":FN}`
- `.dem` is the legacy binary classic demo, framed by `ND_EVENT_START_FRAME` opcodes; per-frame events listed in `d2/main/newdemo.c` lines 102-153 (51 opcodes). Per-frame state is captured by `ND_EVENT_VIEWER_OBJECT` (player) and one `ND_EVENT_RENDER_OBJECT` per visible object via `nd_read_object()`.
- Game log already has rich `Input demo replay player motion`, `AI state`, `AI robot`, `escort state`, `follow probe` per-frame lines; this is REPLAY-side instrumentation. We need a record-side equivalent (the `.dem`) in JSON to compare.

## Replication attempt (cheap, do first)

Before building the converter, attempt a host replay of both `.dximdemo` files against the matching `.rngtrace.jsonl` to confirm replication still desyncs. The recent FP startup hardening / `gameseq.c fl2f(.9)` change may have shifted behaviour.

Commands:
1. `run-windows-build.ps1` to refresh `buildd2_host`.
2. Use the existing input-demo replay desktop smoke harness (see `plan_input_demo_phase5_desktop_runtime_smoke.md`) pointed at each `.dximdemo` with rngtrace cross-check enabled.
3. Capture stdout/`gamelog.txt` and grep for `mismatch`, `divergence`, `bad rand`, `replay aborted`.
4. If both pass without divergence -> desync was build-dependent and is fixed; record outcome and stop. If either still desyncs, continue with converter.

## Tool plan: `dem2json`

Update: this plan was implemented through the existing D2 host executable instead of a new standalone converter. Keep the schema/alignment goals below, but use the engine-backed command above as the source of truth for current work.

### Goal
Produce one JSONL line per classic-demo frame so it can be diffed/grepped against `.dximdemo` and the replay debug log. Field names mirror the input-demo schema where possible.

### Location
- Current implementation: `d2/main/newdemo.c`, `d2/main/newdemo.h`, `d2/main/inferno.c`, `d2/main/gameseq.c`
- Invocation:
   ```powershell
   .\buildd2\main\dxx-redux-d2.exe -hogdir <D2 data dir> -classicdemo-dump-json <input.dem> <output.jsonl>
   ```
- The executable currently needs an explicit `-hogdir` when the build tree does not already have staged game data beside the binary.

### Implementation note
`newdemo.c` proved too intertwined with engine state for a reliable blind parser. The final implementation uses the existing playback path in a headless host mode so `shortpos` decoding, object reconstruction, and level/model data all come from the engine's normal runtime state.

### Per-frame JSON schema

Header line (mirrors `.dximdemo` header where applicable):
```json
{"type":"header","format":"classic_dem","game":"d2","version":<DEMO_VERSION>,"level":N,"difficulty":N,"frame_count":N,"viewer_type":N}
```

One frame line per `ND_EVENT_START_FRAME`. `objects[]` aggregates the `ND_EVENT_VIEWER_OBJECT` plus any preceding `ND_EVENT_RENDER_OBJECT` records for that frame:
```json
{"type":"frame","f":N,"ft":FRAMETIME,"gt":CUMULATIVE_GT,"objects":[
  {"role":"viewer","obj_type":N,"id":N,"flags":N,"sig":N,"seg":N,"pos":[X,Y,Z],"orient":{"f":[..],"r":[..],"u":[..]},"render_type":N},
  {"role":"render","obj_type":N,"id":N,"flags":N,"sig":N,"seg":N,"pos":[X,Y,Z],"render_type":N},
  ...
],"events":[
  {"event":"sound","sound":N},
  {"event":"trigger","seg":N,"side":N,"objnum":N},
  {"event":"player_shield","value":N},
  ...
]}
```

Field-name reuse:
- `f`, `ft`, `gt` match `.dximdemo` frame lines.
- `seg`, `pos` match the replay debug log fields.
- `obj_type`, `id`, `sig`, `flags` match the engine `object` struct names.
- Positions stay in raw fix-point ints (no float conversion), same convention as the replay log so values diff exactly.
- Orient vectors stay as raw int triples for the same reason.

Trailer line:
```json
{"type":"result","frames_decoded":N,"objects_decoded":N,"truncated":false,"trailing_event":"EOF|UNKNOWN_OPCODE|TRUNCATED"}
```

### Phases

| phase | task | status |
|---|---|---|
| 1 | Establish a working classic-demo JSON dump path | completed via `-classicdemo-dump-json` in the D2 host executable |
| 2 | Decode header and per-frame object state through the engine playback path | completed |
| 3 | Make level loading and playback headless-safe for dump mode | completed |
| 4 | Guard playback-only side effects that crash dump mode | completed |
| 5 | Validate against the supplied L1 and L2 demos | completed |
| 6 | Add automated regression coverage for the host dump command | pending |
| 7 | Add user-facing documentation for the host dump command | pending |
| 8 | Run broader formatting / code-quality cleanup if needed | pending |

### Out of scope
- No replay execution, no engine state reconstruction.
- No `.dem` writing.
- No D1 build target initially (file format is broadly the same; flag `-DDEM2JSON_D1` for later).

## Analysis plan: tracking desync with the seven files

### Data alignment
- `dximdemo.frame.f` -> classic `dem.frame.f`: both should be 0-based and contiguous.
- `dximdemo.frame.gt` is absolute `GameTime64` snapshot. Classic `dem` stores per-frame `nd_recorded_time` (delta); cumulate to get gt and cross-reference.
- `rngtrace.frame` matches `dximdemo.frame.f`.
- Game log `Input demo replay player motion: frame=N stage=... gt=...` matches `dximdemo` frame index and gt.

### Step-by-step procedure

1. **Run the implemented classic-demo JSON dump on both dems**:
   ```powershell
   .\buildd2\main\dxx-redux-d2.exe -hogdir <D2 data dir> -classicdemo-dump-json android\temp_game_logs\d2_descent2_level1_20260429_183734.dem temp\l1_classic_demo_final.jsonl
   .\buildd2\main\dxx-redux-d2.exe -hogdir <D2 data dir> -classicdemo-dump-json android\temp_game_logs\d2_descent2_level2_20260429_183812.dem temp\l2_classic_demo_final.jsonl
   ```

2. **Build correlated frame tables (L1 first; smaller)**. Helper PS script `temp/correlate.ps1`:
   - Current implementation status: completed.
   - `temp/correlate.ps1` now reads the classic JSONL dump, the `.dximdemo`, the `.rngtrace.jsonl`, and the per-run replay `gamelog.txt` from the host replay sandbox, then writes normalized TSV output.
   - The script still takes an explicit `-LogSessionIndex`, but the current frame-detail runs use one sandbox log per replay run, with session `0` selected for both L1 and L2.
   - The script reconstructs `.dximdemo` `gt` using checkpoint `start_gt` plus carried-forward `ft`, then exposes several candidate alignment columns side by side instead of assuming a single frame mapping.
   - Do not use interpolation or any other synthetic midpoint calculation between classic-demo frames. Only compare directly observed classic frames and directly observed replay states.
   - Outputs produced so far: `temp/l1_compare.tsv`, `temp/l2_compare.tsv`, `temp/l1_compare_frame_detail.tsv`, and `temp/l2_compare_frame_detail.tsv`.
   - The frame-detail TSVs now include direct classic `player.vel` / `player.orient` state, replay `log_wiggle_same_*` fields, and raw classic-vs-replay velocity delta columns.

3. **Identify first divergent frame**:
   - Current coarse finding: in both sets, the useful baseline is `classic_nearest_by_rel_gt` against same-frame replay-log `entry`, not a naive same-frame or same-index `after_move` join.
   - L1 first coarse threshold crossing (`max(|dx|,|dy|,|dz|) > 10000`, excluding frame 0): frame `17`.
   - L2 first coarse threshold crossing (`max(|dx|,|dy|,|dz|) > 10000`, excluding frame 0): frame `7`.
   - At both frames, `rng_event_count = 0`, the replay and classic data are still in the same segment, and the largest error is dominated by `y`, with `x/z` still comparatively small.
   - This points away from an immediate RNG branch divergence and toward a systematic motion/timebase mismatch that needs closer inspection before source localisation.
   - Discard the earlier interpolation experiment. Any conclusion that depended on synthesized midpoint classic positions is invalid and should not be used.

4. **Classify the divergence kind by what changed first**:
   - **Pure RNG drift**: rngtrace state lines stop matching the corresponding dxim frame `rng.s` -> RNG was consumed by a non-deterministic code path between record and replay. Use the `func`/`line` field on the first divergent rand to localise.
   - **Player physics drift**: dem viewer pos diverges before any RNG drift -> floating-point or input-application path. Cross-check `controls=(...)` from the log against `input` from the dxim frame at that exact f.
   - **AI / non-player object drift**: viewer pos matches but a render-object pos differs at frame F -> AI update is non-deterministic; locate which object id/type drifted first.
   - **Frametime drift**: `ft` differs for the same f -> timer feeding into per-frame seeds.

5. **Same procedure on L2** to confirm class. L2 is much longer and is more likely to expose a slow drift, so it is the better signal source for AI desync. L1 (279 frames) is fast to iterate on.

Current classification lean:
- This is currently leaning toward **player physics / motion-state divergence**, not immediate RNG drift.
- Evidence:
   - the direct nearest-by-`rel_gt` comparison and the replay-side probes both point away from immediate RNG drift
   - both L1 and L2 show coarse direct mismatch before any nearby RNG activity
   - the onset coincides with active player control input and rapidly increasing thrust / orientation change in the replay log
   - the classic frames near these onsets contain too few visible objects to establish an equally early non-player object mismatch from the `.dem` alone
   - replay-side `kconfig.c` float ramp math is not on the active path here: `input_demo_apply_replay_frame()` restores `Controls` directly from the `.dximdemo` state each frame
   - host replay is a good proxy for Android replay once aligned by `gt`; in the checked L1 window the motion lines matched exactly aside from the host log's frame label being one lower than the Android log
   - new D2 probes around `read_flying_controls()` and the early drag step show that velocity is already changing before `do_physics_sim()` finishes its thrust+drag integration
   - in `read_flying_controls()`, the only direct velocity mutation on this path is the `PF_WIGGLE` block (`vm_vec_scale_add2(&velocity, &orient.uvec, fixmul(swiggle, Player_ship->wiggle))`), so that is now the next narrow player-motion suspect
   - `PF_WIGGLE` is enabled on the player object by default and `Player_ship->wiggle` is loaded from ship data via `bmread.c` using `fl2f(atof(...))`, which keeps it relevant to the FP determinism survey
   - fresh frame-detail TSVs for both L1 and L2 now show the replay-side wiggle terms directly on frames `18` through `24`, without any interpolation or synthetic classic-side estimates
   - new direct replay-side wiggle probes for L1 frames `18` through `24` show per-frame wiggle deltas of roughly `(+2k..+7k, -22k..-38k, +2k..+9k)` before drag, with the `y` term alone already comparable to or larger than the early coarse classic-vs-replay mismatch scale
   - new direct replay-side wiggle probes for L2 frames `18` through `24` show the same motion path is active there too, but with opposite-sign `y` deltas after frame `19` because the ship `uvec` differs in that run
   - at L1 frame `18`, thrust is still zero and the wiggle delta is the only `read_flying_controls()` velocity mutation before drag, which confirms this path is active at the desync onset window rather than being a later secondary effect
   - the attempted classic dump hook for `read_flying_controls()` wiggle terms is still not firing during classic demo playback: `player.wiggle.valid` remains `false` in the fresh classic dumps, so direct classic-side comparison currently relies on end-of-frame `player.vel` / `player.orient` instead of a captured classic pre-drag wiggle term

6. **Bisect into source**. Once class is known:
   - **RNG**: open the file/line printed in the first divergent rand event; check whether it sits behind a sim/non-sim gate (`plan_rng_sim_vs_nonsim_split.md`).
   - **Physics**: check FP determinism survey notes (`fp-determinism-survey-20260428-report.md`, `fp-determinism-test-notes-20260429.md`) for any function in the first-diverging stack.
   - **AI**: compare AI state lines in the log between recorded build and replayed build (the log only shows replay; we will need a recording-side snapshot. The classic dem encodes object pos/orient/segment, which is exactly what `dem2json` provides.)

7. **Document**. Append the discovered class and first-diverging-frame to `/memories/repo/` as a finding so subsequent debug runs start with that hypothesis.

### Concrete questions to answer
- L1: at what frame does `dem.viewer.pos` first diverge from log `replay.player.pos[stage=after_move]`?
- L2: same.
- For the first divergent frame, does the dxim rng `(s, c)` match the rngtrace state at that frame?
- Does the divergence appear simultaneously across viewer pos and AI object pos, or is one strictly earlier?

### Phases

| phase | task | status |
|---|---|---|
| A | Run replication attempt (host replay both dxim files); record pass/fail. | deferred during tool implementation |
| B | Produce classic-demo JSONL for both supplied demos. | completed |
| C | Write `temp/correlate.ps1` joining dxim + dem + rngtrace + log on `f`. | completed |
| D | Identify first divergent frame for each set and classify (RNG / physics / AI / frametime). | in progress |
| E | Localise to source (file:line via rngtrace func, FP survey notes, or AI subsystem). | pending |
| F | Optional: add a host integration test that loads one `.dximdemo` + `.dem` pair and asserts viewer pos parity for the first K frames. | pending |

### Notes / pitfalls
- Classic `.dem` rounds positions through `nd_read_shortpos` (compressed 16-bit-relative-to-segment-center form). Expect small bounded quantisation differences vs the replay log's full-fix `pos`. Treat differences within ~1 unit as noise; only flag frames where the delta jumps suddenly.
- `ND_EVENT_RENDER_OBJECT` is only emitted for objects the recording player rendered that frame. Off-screen AI is invisible in the dem -> use the replay log's AI lines to fill that gap; do not treat absence as divergence.
- `rngtrace.jsonl` for L1 is capped at 999 events (events:999, truncated:false but at the cap of the configured limit) -- if needed for deeper L1 analysis, raise the cap before re-recording. L2 has 21k events and is fine.
- Keep all derived files under `temp/` so they do not need write approval.
