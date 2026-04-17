# Metl154 post-fix cleanup and graphics debug harness

## Scope

This is a planning document only. No code changes in this file. It covers
the cleanup of the metl154 investigation fallout plus the creation of a
lasting graphics debug harness that would have caught the original
regression.

Related prior planning docs (do not duplicate their content here):

- `phase5_metl154_new_angles_diagnosis.md` (active diagnosis notes, includes
  the current best-fit root cause and the implemented cached-premerge fix)
- `metl154_hires_premerge_fix.md` (the landed permanent fix plus joined
  label anchor follow-up)
- `metl154_prior_work.md` (archived investigation history)
- `overlay-rendering-four-cases.md` (bottom+overlay size-mismatch cases)
- `debug-logging-black-textures.md` (existing pattern for gated graphics
  logging, use as a style reference)

The fix that actually removed the defect is the cached-premerge path. Any
cleanup that touches rendering MUST keep that path working unchanged.
`metl154` is no longer special at the renderer level once this cleanup is
done.

## Guiding principles for this cleanup

- Zero rename or move should change runtime behavior in the default mode.
  Every tranche below needs a build + emulator smoke run before merging.
- Minimize the d1/ + d2/ diff. Move Android-only functions out to
  `android/app/src/main/cpp/shared/` files included into both builds via
  `target_sources` (same pattern as `game_introspect.cpp`).
- Keep `#ifdef __ANDROID__` inline blocks where the inline branch is short
  and where extracting it would hurt readability more than it helps the
  diff size. Document each remaining inline block with a one-line
  `// Android port: ...` comment so future upstreaming is mechanical.
- Remove hardcoded segment and side lists. The cleanup makes the fix and
  the harness fully generic. Nothing in d1/ or d2/ may mention
  `metl154`, `portal82`, `83/3/1`, or any specific seg/side/face after
  this work completes.
- Treat diagnostics as first class. Keeping the visual debug modes and
  live log streams available is a feature, not debt. What gets removed is
  only ad-hoc investigation code that has no future use.

## Terminology the rest of this plan uses

- "merged wall": a two-texture wall face. Base bitmap plus transparent
  overlay. Rendered via the merged-wall draw path.
- "cached-premerge path" (`merge_impl=gpu_cached_single`,
  `route=merge_cached`): the landed fix. A merged bitmap is produced once
  per unique combination and then drawn through the normal single-texture
  path. This replaces `metl154`-specific special cases.
- "two-pass shader path" (`merge_impl=gpu_two_pass`): the earlier Android
  default that sampled base and overlay in one shader pass. Still used for
  mask-backed super-transparent overlays in the current tranche.

---

## Part A: Generic-naming and metl154 decontamination

### A.1 Rename everything in d1/ and d2/ that says metl154 to a generic name

Every string, symbol, comment, log tag, and flag in `d1/` and `d2/` that
mentions `metl154`, `m154`, or a specific tracked seg/side must be renamed
to a neutral, generic name that describes what the code actually does.
This fix applies to every transparent-overlay wall in every level, not
only metl154 and not only `counterstrike!` level 1.

Proposed neutral naming scheme:

| Old name | New name | Rationale |
|---|---|---|
| `is_metl154_plain` | `is_plain_overlay_wall` | describes the classification |
| `ogl_is_metl154_bitmap()` | (delete) | never needed once the path is generic; callers should test texture properties, not the name |
| `METL154_EXPERIMENT_*` enum | `MERGED_WALL_EXPERIMENT_*` | experiment machinery is generic |
| `g_metl154_debug_mode` | `g_merged_wall_debug_mode` | alpha/RGB visualization mode |
| `g_metl154_experiment_mode` | `g_merged_wall_experiment_mode` | |
| `g_metl154_frame_id` / `g_metl154_draw_seq` / `g_metl154_render_pass` | `g_merged_wall_*` (same suffixes) | |
| `metl154_snapshot_*` | `merged_wall_snapshot_*` | crosshair snapshot harness |
| `[metl154clip]` / `[metl154upload]` / ... log tags | `[mwall_clip]` / `[mwall_upload]` / ... | `mwall` keeps tags short and greppable |
| `render_set_android_draw_face_context` | keep name, move to shared | the context struct is generic already |
| `METL154_TRACK_SIDE_COUNT` | (delete) | tracked sides array goes away entirely |

Log tag namespace proposal: `[gfx_*]` for gated graphics-category diagnostics
under `DLOG_GRAPHICS`, `[mwall_*]` for merged-wall specific events that
remain gated behind the graphics debug flag. Pick exactly one prefix per
system so the log stream is greppable.

Open research item A.1.R1: there are currently two independent places in
d1/d2 that decide whether this path fires: `render.c` and `ogl.c`. Audit
that the classification is done in one place only after the rename, and
that the call chain is `render_face` -> classify once -> pass a bool
down. Further research: read the final post-fix version of both files and
decide whether the bool should live on `android_draw_face_context` or be
a plain function parameter.

### A.2 Delete all hardcoded seg/side/face lists

Targets for deletion once the generic path is confirmed working:

- `metl154_cover_skip_pairs[6]` and its `cover_skip` / `cover_skip2`
  experiment modes. These were diagnostic workarounds that became moot
  once the cached-premerge path landed.
- `metl154_focus_faces[5]` and the `[metl154focus]` logger.
- `metl154_track_sides[METL154_TRACK_SIDE_COUNT]` and the tracked-side
  snapshot logger in `render.c`.
- Anything that mentions `portal82`, `portal83`, `rock8330`, `rock8331`,
  `rock2920`, `32/0/0`, `30/2/0`, `83/1/0`, `83/2/0`, `82/1/0`,
  `82/3/0`, `82/4/0`, `29/2/0`, `28/0/0`, `28/1/0`, `28/2/0`, `32/2/0`.

Keep only the general `android_draw_face_context` + the new debug-trigger
path (see Part D) which captures whatever face is under the crosshair at
the moment the user presses the debug trigger, not a predetermined list.

### A.3 Delete or downgrade dead experiment modes

The 11-mode experiment cycle grew organically during the hunt. Most modes
are no longer useful now that the fix is identified.

Recommended disposition (to be finalized in Part B after a shader audit):

| Mode | Recommend | Reason |
|---|---|---|
| DEFAULT | keep | normal path |
| OLD_MERGE | keep, rename to `FORCE_LEGACY_TEXMERGE` | useful regression control: force the cross-platform CPU texmerge path for any merged wall |
| CoverSkip / CoverSkip2 | delete | tied to hardcoded pair lists |
| ClipAll | delete | post-hoc clipping diagnostic, superseded by the real fix |
| KTX2 no-mip, decoded RGBA, decoded RGBA no-mip, stock fallback | delete | metl154-only texture reload experiments, no longer needed |
| Alpha visualization / RGB visualization | keep as "overlay alpha" and "overlay RGB" debug modes in Part C's harness |

Open research item A.3.R1: confirm by grep that no automation script or
regression test currently depends on any of the modes that we plan to
remove. If any script references them, remove those scripts or migrate
them to the new names before the cleanup tranche.

---

## Part B: Extract Android-specific OGL code out of d1/ and d2/

### B.1 Functions to move to `android/app/src/main/cpp/shared/`

The goal per `copilot-instructions.md` is to keep d1/ and d2/ diffs small.
Any function that is purely Android-only and does not need access to
D1-vs-D2 private headers should move. Candidates (current names):

- `ogl_is_metl154_bitmap()` -- delete, not move.
- Experiment name / clamp / helper functions
  (`ogl_metl154_experiment_name`, `ogl_metl154_alpha_cutoff`,
   `ogl_metl154_overlay_only`, `ogl_metl154_clip_all_tmap2`, etc.).
- Cached-premerge slot management helpers (currently in ogl.c near the
  cached-premerge path).
- Tracked-face, focus-face, and snapshot-cover-event arrays and helpers
  (deleting per A.2 but whatever remains for the new harness moves here).
- Joined-texture label accumulation and grouping helpers.
- `android_draw_face_context` struct + setters/clearers (the struct
  header is already shared; the setter/clearer implementation currently
  lives in d1/d2/main/render.c and should move).
- `render_log_metl154_*` family -- delete most per A.2, keep only the
  generic per-face log behind the graphics debug flag, move to shared.

Proposed new files under `android/app/src/main/cpp/shared/`:

- `merged_wall_draw.{h,cpp}` -- cached-premerge slot management, plain
  overlay classification (now name-agnostic: decides based on bitmap
  flags, not the overlay filename), and the generic two-pass vs
  cached-premerge route selection.
- `graphics_debug_overlay.{h,cpp}` -- per-face label accumulation,
  supertransparency highlight, alpha-visualization mode dispatch,
  joined-wall anchor grouping. Also hosts the crosshair debug-trigger
  capture described in Part D.
- `graphics_debug_log.{h,cpp}` -- small wrappers over `debug_log` that
  emit `[mwall_*]` / `[gfx_*]` tagged lines only when
  `g_graphics_debug_log_enabled` is true. One entry point per event type
  so tag spelling stays consistent.
- Keep `debug_tex_overlay.h` header-only as it already is; move any
  extern volatile int declarations onto the new headers and retire this
  file once all callers migrate.

Wiring pattern (match existing `game_introspect.cpp` pattern in
`android/app/src/main/cpp/CMakeLists.txt`):

```
target_sources(dxx-redux-d1 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/merged_wall_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/graphics_debug_overlay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/graphics_debug_log.cpp
)
target_sources(dxx-redux-d2 PRIVATE ...)  # identical list
```

Open research item B.1.R1: some of these helpers currently pull in
D1-vs-D2 types (e.g. `grs_bitmap` or `segment`). Determine for each
candidate function whether the minimum needed interface is:

- a neutral value type that we can define in the shared header, or
- a forward-declared pointer that d1 and d2 each `typedef` to their own
  struct.

If the answer is "neither", the function has to stay in d1/d2 with a
`// Android port: ...` marker. Expected result: most helpers will be
moveable; a handful tied to `grs_bitmap` and `ogl_bindbmtex` may need
thin shim wrappers.

### B.2 What stays inline in d1/ and d2/

- Call sites like `render_set_android_draw_face_context(...)` at the top
  of each face draw. These are one-liners and keeping them inline is fine.
- Any `#ifdef __ANDROID__` branch that is five lines or fewer. Annotate
  with `// Android port: ...` on the `#ifdef` line.
- The thin per-face classification call inside the OGL tmap draw path
  (calls into the shared helper and acts on the returned route).

### B.3 Extraction order and validation

Do the extraction after the rename in Part A so git history is cleaner
(rename commit, then move commit, then delete-dead-code commit).

Every extraction tranche must:

1. Build D1 and D2 Android (`android\gradlew.bat :app:assembleDebug`).
2. Run unit tests (`android\gradlew.bat :app:testDebugUnitTest`).
3. Run at least one emulator smoke script
   (`android\run_test.ps1 -ScriptName test_launch_to_automap.json5`).
4. Run `android\run-code-quality.ps1 -Fix`.

Open research item B.3.R1: desktop CMake builds are currently blocked by
local vcpkg/SDL_mixer setup in multiple prior tranches. Before the first
extraction tranche, fix desktop build setup enough to run one desktop
validation run for each of D1 and D2. If that is not possible, document
explicitly in this plan that desktop coverage is Android-only for this
work and file a follow-up repo memory note.

---

## Part C: In-engine debug options and graphics harness

### C.1 Launcher graphics page: add "Debug options" section

Add to `GraphicsSettingsPage.kt`. New section "Debug options" containing:

- `Show debug options in the in-game overlay` (bool). Enables the new
  debug controls described in C.2 inside `VideoInfoOverlay`.
- `Graphics debug logging` (bool). When on, the `DLOG_GRAPHICS` category
  passes through `debug_log` for the `[mwall_*]` / `[gfx_*]` tags. When
  off, all gated graphics diagnostic lines are dropped before formatting.
- `Force legacy CPU texmerge for all merged walls` (bool). This is the
  rename of `METL154_EXPERIMENT_OLD_MERGE`. Kept for regression control.
- `Disable cached-premerge path` (bool). Forces every merged wall back to
  the two-pass shader path. Kept for regression control.
- `Force merged-wall alpha cutoff`: cycle between engine default, 0.5,
  0.25, 0.75 (or similar). Used to investigate alpha boundary issues on
  new level data. Pre-game only (requires a texture reload).
- `Capture texture dump on debug trigger` (bool). Enables the
  Part D crosshair capture.

Open research item C.1.R1: confirm which of these options can change mid
game without reload vs. require a pre-game set. Mark each option in the
UI with a `(requires restart)` affordance where relevant. Expand this
list after walking the current experiment-mode code.

Persistence: write these through the existing `GraphicsSettingsPage`
preferences channel so they survive app restarts. Mirror the current
MSAA/AF persistence pattern.

### C.2 In-game video overlay: add a compact debug subpanel

Gated on the "Show debug options in the in-game overlay" checkbox from
C.1. When off, no UI change vs. today. When on, append below the existing
metl154 button row a new row of buttons:

- `Overlay alpha` -- cycle OFF / show overlay alpha in red / show overlay
  RGB. This is the current `metl154_mode` 0/1/2 cycle, renamed and
  applied to every merged wall rather than only metl154.
- `Highlight supertransparency` -- cycle OFF / outline / fill. When on,
  any pixel the engine treats as super-transparent gets visibly tinted.
  Designed to surface super-transparency regressions on any level, not
  only counterstrike!.
- `Label merged walls` -- cycle OFF / overlay name / both stacked. Uses
  the existing joined-texture label machinery from the recent fix but
  generalizes it to any merged wall.
- `Show draw route` -- cycle OFF / `cached|two_pass|legacy` tinted
  corner per wall, in a small color legend. Surfaces the actual route
  decision visually for future regressions.
- `Debug log: graphics` -- mirror of the launcher toggle so it can be
  flipped on the phone.

All of these are live, no reload, except where noted.

JNI wiring pattern: follow the existing `nativeSetDebugFlag(name, value)`
flow used by `metl154_mode`. One new string -> volatile int per toggle.
Clamp and log on set. All volatile ints live in
`graphics_debug_overlay.h`.

Open research item C.2.R1: double-check how the current
`VideoInfoOverlay` lays out buttons. The new row may push the overlay
past the short-axis limit on small devices. If so, gate the debug row
behind a secondary "Debug" tab button that slides the debug controls in.

### C.3 Shader and GL-state audit

The investigation added several shader variants, polygon-offset calls,
cull/depth disables, and wrap-state resetters. All of them need a pass
with fresh eyes.

Deliverables for this sub-tranche (planning only here, do in a later
tranche):

1. Walk every new shader variant in the GLES3 shim. For each variant,
   decide: keep, merge into a parent variant, or delete.
2. Walk every `glPolygonOffset`, `glDisable(GL_CULL_FACE)`,
   `glDepthMask(GL_FALSE)`, `glDepthFunc` change added during the hunt.
   Decide for each: keep (needed by cached-premerge path), gate behind
   the new overlay-alpha debug mode, or delete.
3. Remove the unconditional `glPolygonOffset(-1, -1)` call for the
   previously-named `is_metl154_plain` path unless a test case proves it
   is still needed after the cached-premerge landing.
4. Confirm that `GL_TEXTURE_WRAP_S/T` force-restore added during the
   `[metl154wrap]` tranche either stays as a permanent safety measure or
   moves into the default GL state block where it belongs.

Open research item C.3.R1: gather one capture after the rename tranche
and compare rendered output against a reference to confirm no visible
change before and after individual shader/state cleanups. Automate with
the existing framebuffer-compare test infrastructure if one exists; if
not, describe that gap in the regression-test plan (Part E).

### C.4 Cache and efficiency check

The cached-premerge path is explicitly "not a performance concern on my
phone", but check these obvious traps:

- Cache entries: confirm the cached-premerge cache evicts entries that
  are no longer in view, and that its size has an upper bound. Right now
  the cleanup should at minimum verify there is a bound; setting an
  explicit LRU policy can be a follow-up.
- Multi-draw: confirm that clip code does not cause the same merged
  polygon to be uploaded twice in one frame via two code paths. The
  earlier `[metl154upload]` instrumentation showed one route; re-run
  once after each cleanup tranche to confirm no new duplicates.
- Shader program switches: audit `gles3_shim_use_external` calls per
  frame. If the same program is re-selected multiple times per frame
  because of a stale pointer, log a warning once per frame when
  `DLOG_GRAPHICS` is on.
- Label anchor math: the joined-label anchor fix already projects from
  the original face. Confirm no per-frame allocation happens in the hot
  path. Move to a pre-sized static buffer if so.

All of the above are planning items. Each produces its own small
verification tranche in the work order below.

---

## Part D: Crosshair debug-trigger capture

Existing plan context: the user already plans to render texture labels on
merged walls. Extend that machinery so that pressing the debug trigger
(already bound to a key on desktop and a button on Android) while the
crosshair is over a wall dumps everything known about that wall to the
`DLOG_GRAPHICS` log stream.

### D.1 What to capture

For the face under the crosshair at the moment of the trigger, emit a
single multi-line `[mwall_snapshot]` event containing:

- Face identity: seg, side, face index, side type, wid flags, child
  segment.
- tmap1 and tmap2 values plus resolved bitmap names.
- Route taken this frame: `cached|two_pass|legacy`, plus the cached
  slot id and last-use frame if applicable.
- For each bitmap: source file, dimensions, `bm_flags`, whether it was
  loaded from a hires pack, whether it is super-transparent-capable.
- Shader program id and variant, texture handles bound at draw time.
- Wrap/filter state read live from GL at capture time.
- UVs per vertex, screen-space positions per vertex, signed screen-space
  triangle area, fan split decision.
- Sampled alpha at the crosshair pixel on the overlay.
- Nearest texel indices / values in base and overlay.
- Any recent per-face log tags from the current frame, batched.

The goal is that a future broken-looking wall can be diagnosed from one
log entry without adding new instrumentation.

### D.2 Mechanism

- The debug trigger already raises a volatile int via JNI. Reuse it by
  reading `g_graphics_debug_snapshot_request` on every merged-wall draw.
- On draw, if the crosshair screen coordinate is inside the projected
  polygon AND the request flag is set, capture and clear the flag.
- Falling back when not over a merged wall: also support capturing the
  current frame's visible walls closest to the crosshair, so the trigger
  always produces something useful.

### D.3 Generic, not merged-wall-only

Also capture non-merged walls via the same trigger. The user specifically
called out a garbled texture inside a closed hidden door on the same
level that is on the non-hires path. The capture for that wall should
still include: bitmap identity, source, flags, shader program, alpha
behavior, UVs, and any per-frame log tags.

Open research item D.3.R1: decide whether the crosshair capture should
include the contents of a small GL `glReadPixels` rectangle around the
crosshair (for after-the-fact visual verification). Likely yes in debug
builds only, sized 16x16, base64 or hex encoded in the log.

---

## Part E: Regression-test harness

The original defect survived because nothing automated could detect it.
The harness below is designed so a similar defect in the future fails
loudly on emulator CI.

### E.1 Test level content (user will author)

The user will create a trivial level with:

- A corridor of several short rooms connected by doors.
- At least one door that uses a transparent overlay wall (plain
  transparent, equivalent to the metl154 grate case).
- At least one door that uses a super-transparent mask (equivalent to a
  hires mask case).
- At least one see-through grate wall, not a door, with normal
  transparency.
- At least one see-through grate wall with super-transparency.
- At least one joined wall adjacent to a rock texture so the
  cached-premerge path exercises its joined-label anchor machinery.
- Repeat pairs both on the original-64x64 stock path and on a hires
  pack, so the test can run twice and cover both cases.

Open research item E.1.R1: confirm the level file format accepts a
minimum-complete level with no enemies or mandatory exit. Target a level
load time below two seconds on emulator.

### E.2 Harness components

- New test script `test_graphics_debug_harness.json5` under
  `android/game_scripts/`. Loads the new level, steps the player through
  each viewpoint, triggers the crosshair debug capture at each stop, and
  asserts on the captured log entries.
- Assertions run against the durable `automation_log.jsonl` and
  `automation_result.json` plus a new durable log file for graphics
  harness output: `files/graphics_debug.jsonl`. This file is written by
  the new `graphics_debug_log` module (Part B.1) when the graphics
  debug flag is on.
- Each viewpoint in the script names the expected route
  (`cached|two_pass|legacy`), expected bitmap identity under the
  crosshair, and expected overlay alpha sample range.
- Test runner (`run_test.ps1`) already parses result files; extend to
  also parse `graphics_debug.jsonl` and compare per-step expectations.

### E.3 What the harness would have caught

The original metl154 defect had these observable traits in a live log:

- A face with `bot=rock* ovl=metl154` on `route=gpu_two_pass` whose
  screen-space bbox overlapped a later face carrying rock content that
  the editor says should be blocked by the transparent overlay.

A harness assertion of the form "for viewpoint N, the face under the
crosshair must be on `route=merge_cached` and must not report any
`[mwall_coverbox]` overlap with a visible rock face" would have failed
on the original defect. Phrase the actual assertion in terms of route
and per-face log output, not in terms of specific seg/side numbers, so
the assertion is stable across level edits.

Additional assertions to add while here:

- No per-face log may fire more than once per frame for the same face.
- No cached-premerge slot may be created more than once per
  combo-per-session unless explicitly evicted.
- Shader program id used for a merged wall must match the cached
  program id for the current route enum.

Open research item E.3.R1: the harness needs a "live log stream"
behavior if the user wants real-time feedback while authoring the test.
Current test infra only reads final durable log files. Either:

1. Extend the Kotlin debug log export to tail and broadcast new lines
   during automation runs (larger change), or
2. Keep the current durable-file model and have the harness poll
   `files/graphics_debug.jsonl` between steps (smaller change).

Recommend option 2 for first implementation.

### E.4 Test ordering

Run the new harness after every `phase5` / cleanup tranche below, and
wire it into the standard pre-commit script so it runs alongside
`run_all_tests.ps1`.

---

## Part F: Proposed work order

Each tranche ends with build + unit tests + at least one emulator smoke
run + `run-code-quality.ps1 -Fix`. Each tranche lands before the next
starts.

1. **Rename tranche** (Part A.1, A.3). Pure renames in d1/, d2/, android/.
   No behavior change expected. Delete the dead experiment modes here.
2. **Hardcoded-list removal tranche** (Part A.2). Delete focus faces,
   cover-skip pairs, tracked-side lists, and the associated log tags.
3. **Extraction tranche** (Part B). Move Android-only code to
   `android/app/src/main/cpp/shared/` files. Minimize `#ifdef __ANDROID__`
   in d1/ and d2/.
4. **Shader and GL-state audit** (Part C.3). Remove any added shader/GL
   state change that is no longer needed after the rename and extract
   tranches.
5. **Launcher debug options** (Part C.1) and **in-game debug panel**
   (Part C.2).
6. **Crosshair debug capture** (Part D).
7. **Regression harness** (Part E). This includes authoring the new
   test script and extending `run_test.ps1` to compare
   `files/graphics_debug.jsonl` output.
8. **Cache + efficiency pass** (Part C.4). Done after everything above
   so the audit runs against clean code.

Each of 1 through 8 should be its own plan file under
`android/ai tool plans/` once implementation starts, with this file
linked.

---

## Recommendations summary

Short list for quick reference while implementing:

| Topic | Remove | Keep and generalize | Move to android/shared |
|---|---|---|---|
| metl154-named symbols | all, after rename | none | n/a |
| Hardcoded seg/side lists | all | none | n/a |
| Experiment modes | CoverSkip, CoverSkip2, ClipAll, 4 KTX2 variants, stock fallback | DEFAULT, OLD_MERGE -> rename to FORCE_LEGACY_TEXMERGE, overlay alpha, overlay RGB | yes |
| Log tags `[metl154*]` | most | keep one `[mwall_*]` per event type, behind DLOG_GRAPHICS | yes |
| Polygon offset in merged path | probably yes | re-verify with capture before final removal | n/a |
| Cull disable / depth tweaks | most | only those the cached-premerge path actually needs | audit in shared helper |
| Joined-label anchor fix | no | keep and generalize to any merged wall | yes |
| render_set_android_draw_face_context | no | keep, it is already generic | yes, move impl |
| DbgAltTexMerge Android default | no | keep, now accurate given cached-premerge path | n/a |
| Video overlay metl154 buttons | in current form | rebuild as generic "Overlay alpha" / "Overlay RGB" / "Merged wall labels" / "Show draw route" / "Highlight supertransparency" | UI stays in Kotlin |
| Launcher graphics page | no | add new "Debug options" section | UI stays in Kotlin |
| Crosshair debug capture | n/a (new) | new generic capture of everything under the crosshair | yes, fully in shared |
| Regression harness | n/a (new) | new generic harness exercising doors, grates, super-transparency, joined walls | test script in android/game_scripts |

---

## Explicit research gaps to close in the first tranches

- A.1.R1: single classification site audit.
- A.3.R1: automation script usage of experiment modes before rename.
- B.1.R1: per-function move feasibility for d1/d2 type dependencies.
- B.3.R1: desktop CMake build setup so cleanup tranches can validate
  non-Android builds.
- C.1.R1: which debug options can toggle live vs. require restart.
- C.2.R1: overlay layout on small devices with the new debug row.
- C.3.R1: before/after capture compare to verify visual neutrality of
  shader/state cleanup.
- D.3.R1: include small glReadPixels window in crosshair capture.
- E.1.R1: trivial-level minimum content requirements and load time.
- E.3.R1: live log stream vs. polled durable file approach for harness.

Each item above should be answered in the relevant tranche's own plan
file, not in this document.
