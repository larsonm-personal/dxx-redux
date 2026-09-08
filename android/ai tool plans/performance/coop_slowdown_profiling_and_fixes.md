# Co-op slowdown profiling and fixes

Created September 7, 2026

## Scope and current status

Planning only. This background task creates this document and performs read-only
source inspection. Implementation, builds, tests, device connections, and emulator
work are deferred to a later implementation task

The objective is to restore smooth 25 FPS gameplay in the measured busy views and
reduce co-op save hitches while preserving gameplay, save correctness, previews,
and rendering correctness

- [x] Review the existing captures and inspect candidate instrumentation sites
- [x] Specify profiling, prioritized fixes, and later validation
- [ ] Implement profiling and the offline summary improvements
- [ ] Collect an instrumented baseline on the affected physical device
- [ ] Implement and measure the save optimization
- [ ] Implement the renderer fix selected by the new evidence
- [ ] Complete matched performance and functional validation

## Evidence to preserve

The September 6 host logs in `C:/Users/first last/Downloads` contain eight complete
automatic captures, with 463 windows covering 11,424 frames. All use a 25 FPS cap
and VSync off, giving a 40 ms frame budget

| Evidence | Source |
| --- | --- |
| Castaway L5 segment 118: 90.991 ms rendering, 0.863 ms simulation; gameplay window reaches 15.834 FPS | `debuglog_20260906_142906.txt`, frame 36214, line 29230 |
| Castaway L5 segment 23: 77.309 ms rendering, 0.738 ms simulation | `debuglog_20260906_215106.txt`, frame 3008, line 3188 |
| Castaway L5 segment 715: 144.747 ms rendering, 2.620 ms simulation, zero live projectiles | `debuglog_20260906_224248.txt`, frame 27642, line 20124 |
| Latest L5 reactor approach: 23 of 58 windows average over 40 ms non-wait work; nine windows below 20 FPS; minimum 14.739 FPS | Same log, capture 2, 23:01:51-23:02:51 |
| 73.395 ms simulation plus 54.117 ms rendering during an autosave | Same log, frame 28060, line 20681; save events at lines 20669-20670 |
| 14 of 15 selected simulation spikes above 30 ms overlap co-op autosaves | All three September 6 host logs |
| Castaway L6 segment 386: 66.946 ms rendering, 1.462 ms simulation | Latest log, frame 31762, line 23971 |

The previous texture binding cache fix, commit `43bfd55c`, is present in both
September 6 builds. The 144.747 ms frame records 269 binds and 686 reuses. Do not
assume the older disabled-cache regression still explains these captures

The detailed investigation is in `temp/coop_slowdown_review.md`; the evidence
above is retained here so this plan does not depend on keeping a scratch report

## 1. Extend the existing recorder with bounded, attributable timings

Keep the existing automatic recorder and asynchronous log transport. Add shared
Android profiling code with small Android-only hooks in both games. Do not create
a separate profiler or increase logging on every ordinary gameplay frame

Primary files:

- `android/app/src/main/cpp/shared/android_profile.c` and `.h`
- `android/app/src/main/cpp/shared/android_slowdown_detector.c` and `.h`
- `android/app/src/main/cpp/shared/ogl_gpu_timer_android.c` and `.h`
- `d1/main/game.c`, `d2/main/game.c`
- `d1/main/gamerend.c`, `d2/main/gamerend.c`
- `d1/main/render.c`, `d2/main/render.c`
- `d1/arch/ogl/ogl.c`, `d2/arch/ogl/ogl.c`

### Renderer attribution

| Area | Proposed timing and counters | Decision it enables |
| --- | --- | --- |
| Visibility | Time `build_segment_list`; count visited/rendered segments and portal tests | Distinguish traversal or visibility work from drawing |
| Object lists | Time `build_object_lists` and sorting; count list entries and sorts | Identify list-building or sorting costs |
| Walls | Aggregate wall/segment drawing time and face counts by render pass | Identify expensive geometry or wall submission |
| Objects | Aggregate object drawing time and counts by pass; retain sampled slowest object identities and type/model totals | Separate player ships, weapons, robots, reactors, and repeated views |
| Extra views | Time cockpit subviews, missile/rear views, HUD, and overlays around `game_render_frame` | Expose costs outside the main mine view |
| GL operations | Time texture uploads and existing GPU-query collection; count state changes and submissions | Distinguish driver waits, uploads, and submission overhead |

Use explicit render contexts for the main view, auxiliary views, embedded save
thumbnail, and launcher thumbnail. Count render invocations as well as objects so
an autosave's extra views cannot masquerade as increased visible scene density

Define nesting before adding timers: parent elapsed time is inclusive; disjoint
child phase totals and the residual explain that parent. Do not add nested object,
wall, thumbnail, or GL times together as if they were independent frame costs

Coarse subphase totals should cover every frame during an active capture. Retain
the existing sparse schedule for per-object and per-draw clocks. Include a detail
validity mask and measured-frame counts: unsampled values are unavailable, not zero

Keep per-window sums, maxima, and a bounded histogram for non-wait work and frame
cadence. Report approximate percentile bounds from the histogram. Retain subphase
details for the same selected worst frame IDs, rather than unrelated nearby frames

### Save attribution

Add a compact save-operation record with session, operation ID, frame ID, monotonic
start/end, game, level, host/client role, reason, slot, result, and byte counts

Measure these separately:

1. Embedded thumbnail rendering, resolve/readback, and pixel conversion
2. Launcher thumbnail rendering, resolve/readback, and pixel conversion
3. Engine-state serialization and actual output writes
4. Co-op metadata and launcher metadata creation
5. Staged-save validation, publication, history update, and peer notification

Use shared orchestration hooks in `state_android_shared.c` and
`shared/coop/coop_save.c`, plus thumbnail hooks in both `d1/main/state.c` and
`d2/main/state.c`. Bracket the existing write paths; do not instrument each scalar
field write or change the save format to obtain timings

Keep save totals identifiable as children of simulation or other owning phases.
Distinguish automatic co-op saves, manual saves, rewind snapshots, and restore
work explicitly. The single-player periodic autosave helper already excludes
multiplayer, so do not assume it is duplicating the co-op timer

### Timing provenance and capture limits

- Carry a session/reset generation, capture ID, and frame ID through detailed
  records. Record operation timestamps directly rather than inferring them from
  the asynchronous log-write time
- Associate GPU queries with their originating frame and render context; report
  availability, age, disjoint status, and any forced result-wait duration. Label
  swap with the flip/frame it belongs to
- Add coarse thread-CPU timing during diagnostic sampling if supported. Compare
  it with elapsed time to distinguish CPU work from blocking/descheduling; report
  unsupported measurements as unavailable. Do not add `glFinish` or synchronous
  result waits just to improve attribution
- Preserve the 768-entry coarse history and the existing detector limit of
  128 KiB. Store new capture-only totals and a fixed number of detail records
  outside each historical frame rather than enlarging all 768 entries. Set and
  check a small explicit budget for that added storage, initially at most 32 KiB
- Preserve five seconds of coarse history, the 60-second capture duration, the
  256 KiB capture log cap, and the severe-stall cooldown escape. Pre-trigger
  subphase detail that was not sampled must remain explicitly unavailable
- Bound nested timer/context storage, emitted detail, and texture/save events.
  Overflow or dropped records must be counted. Reserve space for capture-end and
  overflow summaries so a full log remains interpretable
- Proposed overhead targets to verify later: under 50 microseconds of additional
  work per armed frame and under 0.25 ms per frame for capture-mode coarse timing.
  Measure sampled object-detail overhead separately on the affected hardware

## 2. Make exported captures directly useful

Extend `android/helpers/summarize-profiling-log.ps1`, which currently only recognizes
`prof_v=1`, to parse the historical automatic records and the new schema. Version
the new records explicitly. Historical log parsing is analysis support; no runtime
save/config compatibility layer or migration is needed

The summary should:

- Group by session, capture, game, mission, and level, accounting for capture ID
  resets and overlapping exports
- Report capture completeness, missing windows, dropped lines, sample coverage,
  frame cap, build, ABI, and available graphics settings
- Compute FPS and average costs from window totals. Never use the three selected
  worst frames as a representative population for averages or percentiles
- Show costly renderer phases, save operations, relevant object/view identities,
  GPU sample age, and the original source line for each finding
- Recognize explicit restore, level-load, menu/automap, and lifecycle boundaries
  as separate elapsed-time intervals. Retain their durations and flag unexplained
  gaps; a large gap alone must not be discarded as an intentional pause
- Emit stable text and normalized, pretty-printed JSON suitable for comparisons

Add compact synthetic fixtures for mixed versions, delayed batches, missing detail,
session resets, incomplete captures, and overlapping exports. Re-run the summary
on the three original host logs to verify the established eight-capture baseline

## 3. First fix candidate: share one thumbnail capture per save

Read-only inspection found two separate thumbnail render/readback paths in both
games. D2's embedded thumbnail runs at `state.c:2375-2385`; its successful path
then calls `state_android_cache_launcher_thumbnail`, which renders and reads back
again at `state.c:1357-1372`. D1 has the corresponding duplication

After recording the instrumented baseline, prioritize replacing those two captures
with one correctly sized source image per save and deriving the two required
preview sizes from it. Keep the existing save header layout, palette/RGB conversion,
image orientation, and launcher preview contract. Check aspect ratio and framing
before selecting the common source dimensions

Keep live OpenGL calls and snapshot creation on the owning game thread. Restore
the framebuffer, canvas, viewport, render context, and texture state after the
capture. A failure must not reuse an image from another mission, save, or session

Measure the result before expanding scope. If saving is still costly, choose the
next step from the measured breakdown:

| Remaining cost | Candidate follow-up |
| --- | --- |
| Thumbnail rendering dominates | Reuse a suitable normal scene render from the same snapshot generation, after proving view/state association and preview correctness |
| Readback dominates | Evaluate a bounded asynchronous readback pipeline supported by the actual GLES targets; bind images to a specific save generation and retain a working fallback |
| Serialization or duplicate save requests dominate | Remove measured redundant work and reuse immutable serialized data only for the same snapshot; preserve engine ownership of format knowledge |
| File writing/validation dominates | Evaluate bounded background publication from an immutable engine-produced snapshot; preserve staged validation, atomic replacement, result reporting, and peer ordering |

Do not make blank previews or a longer autosave interval the default fix. Preserve
save cadence and synchronized slot/game-ID behavior. Any deferred work must retain
the last valid save and handle failure, supersession, restore, and exit safely

## 4. Select renderer fixes from measured subphases

Implement one measured change at a time and compare the same view before and
after. Candidate branches are deliberately conditional:

| Profile result | Focused fix direction | Correctness constraint |
| --- | --- | --- |
| Segment traversal/list building dominates | Remove redundant work within a view or frame; use bounded scratch storage and measured algorithm improvements | Preserve portal visibility, ordering, and changed-wall behavior |
| Object sorting/transforms dominate | Avoid repeated preparation for identical inputs and improve the measured sorting path | Keep view-dependent transforms, transparency ordering, and simulation untouched |
| GL state/submission dominates | Eliminate redundant state changes or batch compatible submissions | Preserve the multi-unit texture cache, merged-wall state, masks, and draw order |
| GPU query collection blocks | Consume only available query results and skip new measurements when the bounded queue is full | Mark skipped/stale measurements; never stall gameplay to maintain profiler coverage |
| Texture uploads dominate | Prewarm only identified assets during existing load/paging work or correct measured cache churn | Keep memory bounded and texture invalidation correct |
| HUD/auxiliary views dominate | Avoid repeated preparation and drawing only where inputs and required output are unchanged | Preserve rear/missile views, overlays, and cockpit behavior |
| GPU time is actually dominant once aligned | Optimize the measured pass and unnecessary overdraw | Preserve the selected graphics quality and all visible gameplay content |

If coarse timings still leave most cost inside an undivided driver or renderer
interval, collect a short platform CPU trace in the later foreground diagnostic
session before choosing a fix. Do not reduce robot/projectile update rates or
visibility limits to mask the problem

Network processing and GuideBot planning are lower-priority branches. Instrument
them further only if the new capture identifies an unexplained cost there; the
current sustained slowdown evidence primarily points elsewhere

## 5. Later validation and completion criteria

Nothing in this section runs during the planning task

Host checks after implementation:

- Extend `test_android_slowdown_detector.c` for memory limits, detail/frame
  association, reset/overflow behavior, and preserving severe-stall capture
- Exercise profiler nesting and save attribution with meaningful synthetic timing
  cases, including an autosave inside simulation and multiple render contexts
- Run the updated log-summary fixtures and original-log baseline
- Run relevant renderer, save metadata, save format, staged publication, and rewind
  tests. Extend a high-level co-op save/restore test for thumbnail changes, including
  host/client save identity and restored state after a failed write
- Run one scoped code-quality invocation over the implemented changes, then the
  relevant D1/D2 Windows CMake builds and tests and Android builds for every
  configured ABI. Verify non-Android builds retain their existing behavior

Physical-device performance work, scheduled as a separate foreground task:

| Scenario | What to hold fixed or vary |
| --- | --- |
| Castaway L5 segments 118 and 23 | Same save, camera orientation, active fight, graphics settings, and co-op roles |
| Castaway L5 segments 715-785 | Same reactor approach, including a low-projectile view |
| Castaway L6 segment 386 and later boss area | Verify both the previously captured view and the boss interval that lacked detailed capture |
| D2 L24 final boss | Recheck Earthshaker-heavy play with the already repaired texture cache |
| D1 representative busy view and co-op save | Verify shared instrumentation and thumbnail behavior in both games |

Record device/build, resolution, graphics options, cap/VSync, thermal state, asset
warmup, save identity, camera, role, and peer build. Obtain host and client captures
where possible. Existing logs do not encode enough state to promise an exact replay
of every old frame; save reproducible new scenarios through automation/introspection

Compare multiple matched runs with profiling disabled, armed, and capturing; then
compare baseline and each fix under the same instrumentation. Use both steady
views and natural gameplay, with saves enabled. Functional automation can later use
an emulator, but physical-device measurements decide the performance outcome

Report window-average work, cadence, percentile bounds, over-40/100/250 ms counts,
save cost by phase, GPU provenance, and sustained below-cap intervals. Do not claim
an improvement from comparing unmatched missions or from sampling only worst frames

Completion requires correctly attributed captures within the memory/log budgets,
valid previews and restores on both games, and a repeatable improvement in the
measured bottlenecks without visual or gameplay regressions. The performance goal
is sustained 25 FPS in the fixed busy views and materially smaller autosave spikes;
report remaining over-budget work explicitly if that goal is not yet reached

## Suggested implementation sequence

1. Shared timing schema, save/render hooks, and offline summarizer, with host checks
2. Physical-device instrumented baseline and overhead measurement
3. One thumbnail capture per save, then matched save and gameplay measurements
4. The renderer optimization selected by the new timings, then matched measurements
5. Any justified second save/render change, followed by full scoped validation

Keep each implementation step separate enough to measure and review. Inspect the
working tree before starting: concurrent routing, metadata, and gameplay changes
were already present while this plan was written
