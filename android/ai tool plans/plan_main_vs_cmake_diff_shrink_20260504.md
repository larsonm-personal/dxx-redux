# main vs cmake diff shrink plan -- 2026-05-04

Status: active

Goal:
- shrink the `d1/` and `d2/` branch diff for `main..cmake` by moving Android-only helper bodies out of legacy game files and into the existing Android/shared or per-game hook surfaces

Baseline survey:
- `git merge-base main cmake` is `fb555eec75e1ed12c8348805ab335afb4c721b06`, matching local `main`
- `origin/HEAD` points to `main`, so `main` is the correct parent baseline for this pass
- `git diff --stat main..cmake -- d1 d2` shows the remaining largest legacy churn clusters in `arch/ogl/ogl.c`, `newdemo.c`, `net_udp.c`, `game.c`, `multi.c`, `state.c`, and the still-active input-demo instrumentation files
- the completed low-risk cleanup tranche moved the remaining collision probe and event helper bodies out of the legacy `d1/main/collide.c` and `d2/main/collide.c` files
- the nearest remaining shrink targets are now the larger `main..cmake` hotspots outside the extracted collision and escort helper surfaces, such as `game.c`, `state.c`, `newdemo.c`, `net_udp.c`, `multi.c`, and `arch/ogl/ogl.c`

Plan:
- [x] survey `main..cmake` branch topology and d1/d2 diff hotspots
- [x] choose the next low-risk extractable cleanup slice from the branch diff
- [x] move the remaining player-bump helper bodies out of `d1/main/collide.c` and `d2/main/collide.c`
- [x] remove stale D2-only collision debug leftovers that the extraction makes redundant
- [x] move the cross-file collision logging hook declarations into `input_demo_hooks.h` and remove the redundant local `extern` blocks
- [x] move the D2 AI and robot-fire hook declarations into `input_demo_hooks.h` and remove the redundant `ai.c` and `ai2.c` local `extern` blocks
- [x] move the remaining low-risk D2 single-function hook declarations such as `gauges.c` score logging into `input_demo_hooks.h`
- [x] move the D2 object lifecycle and robot-visual probe declarations into `input_demo_hooks.h` and remove the redundant `object.c` local `extern` block
- [x] finish the D2 path-trace header cleanup by removing the final `aipath.c` local `extern` without exposing `point_seg` from the shared header
- [x] move the D2 laser input-demo helper block into `input_demo_hooks.c` and remove the remaining static `input_demo_*` helper bodies from `laser.c`
- [x] move the remaining collision-state helper bodies out of `d1/main/collide.c` and `d2/main/collide.c`
- [x] move the remaining low-risk collision probe and frame-event helper bodies out of `d1/main/collide.c` and `d2/main/collide.c`
- [x] move the D2 escort input-demo helper block into `input_demo_hooks.c` and remove the remaining static `input_demo_*` helper bodies from `escort.c`
- [x] validate with D1/D2 host build, focused replay smoke, and Android arm64 native build
- [x] update the long-running shrink and input-demo extraction plans with the completed tranche and any remaining branch-diff candidates
- [x] refresh the current `upstream/main` baseline after the hook-file extraction wave so the next diff-reduction pass is driven by current hotspots instead of the older April OGL-heavy picture
- [x] re-survey the remaining D1/D2 diff with emphasis on deduplication opportunities, helper-sink overgrowth, and legacy-file cleanup lanes
- [ ] choose the next dedicated shrink tranche from the ranked survey lanes below

Completed tranche:
- moved `input_demo_log_player_bump_probe(...)` into `d1/main/input_demo_hooks.c` and `d2/main/input_demo_hooks.c`
- removed the now-redundant local player-bump helper bodies from both legacy `collide.c` files and dropped the stale D2-only bump gate
- moved the cross-file collision logging declarations into `d1/main/input_demo_hooks.h` and `d2/main/input_demo_hooks.h`, keeping the public hook surface out of the legacy `collide.c` files
- moved the D2 AI awareness and robot-fire hook declarations into `d2/main/input_demo_hooks.h`, removing the local declaration blocks from `ai.c` and `ai2.c`
- moved the D2 score logging declaration into `d2/main/input_demo_hooks.h`, removing the one-off local declaration from `gauges.c`
- moved the D2 object lifecycle and robot-visual probe declarations into `d2/main/input_demo_hooks.h`, removing the local declaration block from `object.c`
- moved the D2 path-trace declarations into `d2/main/input_demo_hooks.h` and changed `input_demo_log_path_points(...)` to use a read-only opaque buffer so `aipath.c` no longer needs a local declaration
- moved the D2 laser input-demo helper block into `d2/main/input_demo_hooks.c`, leaving `d2/main/laser.c` with direct call sites but no remaining static `input_demo_*` helper bodies
- moved the remaining collision-state helper bodies into `d1/main/input_demo_hooks.c` and `d2/main/input_demo_hooks.c`, leaving no local `input_demo_trace_collision_*` or `input_demo_replay_collision_probe_active()` helpers in the legacy `collide.c` files
- moved the remaining low-risk D1 collision probe helpers and D2 collision frame-event recorder helpers into the per-game `input_demo_hooks.c` files, leaving no local `input_demo_log_player_robot_contact_probe(...)`, `input_demo_log_weapon_robot_accept_seq(...)`, `input_demo_record_*_event(...)`, `input_demo_record_frame_event_json(...)`, or `input_demo_replay_powerup_probe_active()` bodies in the legacy `collide.c` files
- moved the D2 escort probe helpers and their snapshot state into `d2/main/input_demo_hooks.c`, leaving no local static `input_demo_*` helper bodies in `d2/main/escort.c`
- verified there are no remaining local `extern input_demo_*` declarations in `d1/main/*.c` or `d2/main/*.c`
- validated with `run-windows-build.ps1 -Target both`, the focused D2 headless replay smoke, and Android arm64 Gradle tasks `:app:buildCMakeDebug[arm64-v8a]` and `:app:buildCMakeDebug[arm64-v8a]-2`

## Refreshed baseline (2026-05-18)

This plan started from the `main..cmake` branch-topology question, but the
stable day-to-day shrink metric is now `android/diff_vs_upstream.ps1` against
`upstream/main`, matching the longer-running `d1d2_diff_shrink_study.md` and
`d1d2_shrink_phase3_execution_plan.md` docs.

Current `upstream/main` baseline from `android/diff_vs_upstream.ps1 -Top 40`:

```
d1/d2 diff vs upstream/main
	files:   290
	d1 files: 135
	d2 files: 155
	+added:  39579
	-removed: 2149
```

Current top churn clusters:

```
Added Removed Total Path
----- ------- ----- ----
 6081       0  6081 d2/main/input_demo_hooks.c
 1754      51  1805 d2/arch/ogl/ogl.c
 1657      83  1740 d2/main/state.c
 1619      52  1671 d1/arch/ogl/ogl.c
 1317     141  1458 d1/main/state.c
 1309      24  1333 d2/main/newdemo.c
 1009       0  1009 d1/main/input_demo_hooks.c
	798     204  1002 d2/main/net_udp.c
	929       0   929 d2/main/coop_save.c
	898       0   898 d2/main/dxa_metadata_patch.cpp
	706     176   882 d1/main/net_udp.c
	872       0   872 d1/main/coop_save.c
	765      16   781 d2/main/physics.c
	634      81   715 d2/main/newmenu.c
	613      74   687 d1/main/newmenu.c
```

Most important interpretation change versus the older shrink studies:

- OGL is no longer the only dominant story. Earlier extraction tranches worked,
	but the saved lines pooled into new per-game helper sinks, especially
	`input_demo_hooks.c` and `input_demo_start.c`.
- That is a real win for upstreamability of the original 1990s files, but it
	means the next survey must distinguish between good churn in sink files and
	bad churn still stranded in legacy files.
- The next phase should therefore be a mixed strategy: continue shrinking the
	legacy files, but also start deduplicating the sink files themselves where the
	D1/D2 copies are now near-identical.

## Survey reading of the current diff

The current D1/D2 shrink picture splits into four different kinds of churn:

1. **Successful body extractions that now need second-stage deduplication**
	 - `d1/main/input_demo_hooks.c`, `d2/main/input_demo_hooks.c`
	 - `d1/main/input_demo_start.c`, `d2/main/input_demo_start.c`
	 - These files are doing the right job structurally: they keep helper bodies
		 out of upstream-like legacy files. The next question is whether the new D1
		 and D2 sink files are themselves duplicating too much shared Android/demo
		 logic.

2. **Legacy upstream files that still carry Android-specific bodies**
	 - `state.c`, `newdemo.c`, `net_udp.c`, `newmenu.c`, `playsave.c`,
		 `multi.c`, `gamecntl.c`, `kconfig.c`
	 - These are still the highest-value shrink targets because every line left in
		 them directly increases future merge cost.

3. **Large but already-mostly-drained platform files**
	 - `d1/arch/ogl/ogl.c`, `d2/arch/ogl/ogl.c`, plus `gr.c`
	 - These still matter, but they are no longer the obvious first stop. The
		 remaining wins here are narrower helper trims, not another broad campaign.

4. **Files with high churn that are not really D1/D2 dedup targets**
	 - `d2/main/dxa_metadata_patch.cpp`
	 - D2-only AI/physics/object probe files such as `physics.c`, `ai.c`,
		 `laser.c`, `collide.c`, `object.c`, `fireball.c`, `ai2.c`, `escort.c`
	 - These may still deserve cleanup, but forcing cross-game sharing is the
		 wrong goal. The right goal there is smaller D2-local helper surfaces, not
		 artificial D1/D2 unification.

## Ranked survey lanes for diff reduction and deduplication

The ranges below are tranche-size estimates, not commitments. They are meant to
rank opportunities, not promise exact line counts in advance.

| Lane | Current hotspots | What the current diff is telling us | Best reduction move | Rough upside | Risk |
|---|---|---|---|---|---|
| A. Input-demo sink dedup | `d1/d2 main/input_demo_hooks.c`, `d1/d2 main/input_demo_start.c`, plus `newdemo.c` and `state.c` callers | helper extraction succeeded, but the sink files now duplicate shared hashing, diag capture, replay-start, and recorder/path logic | move truly shared demo helpers into `android/app/src/main/cpp/shared/input_demo/` or small per-game dedicated files; keep only thin wrappers in the sink files | very high | medium |
| B. Persistence / save / restore | `d1/d2 main/state.c`, `d1/d2 main/coop_save.c`, `d1/d2 main/playsave.c`, parts of `multi.c` | Android save metadata, rewind, coop save, and launcher bridge logic are still spread across legacy files | centralize bridge bodies in shared save/coop files and leave only game-owned serialization boundaries local | high | medium |
| C. Network / coop join flow | `d1/d2 main/net_udp.c`, `d1/d2 main/multi.c` | duplicated Android networking helpers remain in upstream-like files even after earlier shared extractions | continue the shared net helper path and keep local wrappers only for side effects, HUD, and game-specific types | high | medium |
| D. Menu / control / touch UI | `d1/d2 main/newmenu.c`, `gamecntl.c`, `kconfig.c`, smaller `controls.c` / `titles.c` tails | many Android touch, keyboard, and controller affordances are still implemented twice in legacy menu files | move Android-only UI helper bodies to shared UI helpers; keep the original menu control flow in place | medium-high | low-medium |
| E. OGL leftovers | `d1/d2 arch/ogl/ogl.c`, `gr.c` | still large, but much of the easy shared extraction work already landed | only pursue isolated helper moves with clean boundaries; do not reopen broad OGL restructuring | medium | medium |
| F. D2-only cleanup, not dedup | `d2/main/dxa_metadata_patch.cpp`, D2-only input-demo probe files | some big files are high churn but not cross-game opportunities | keep these as separate cleanup tranches when needed; do not distort the D1/D2 dedup queue around them | situational | low |

## Lane A -- Input-demo sink files are now the main dedup frontier

This is the single biggest change from the earlier studies.

`d1/main/input_demo_hooks.c` and `d2/main/input_demo_hooks.c` are no longer
just convenient sinks for one-off helper moves. They are now large enough that
the duplicated sections inside them have become a top-level D1/D2 shrink target
in their own right.

Fresh local read highlights:

- the tops of both files are visibly near-identical, including
	`input_demo_state_trace_hash_update`,
	`input_demo_state_trace_hash_i64`,
	`input_demo_capture_runtime_state_diag`,
	`input_demo_capture_player_weapon_diag`, and
	`input_demo_state_trace_hash_object`
- the same pattern continues into object-state and runtime-state capture,
	meaning the files contain a large shared diagnostic core before the truly
	game-specific D2-only AI, escort, and probe families begin
- `d1/main/input_demo_start.c` and `d2/main/input_demo_start.c` also show a
	strong overlap: command-line argument parsing, metadata validation,
	replay-player-config application, and skip-level-intro state are the same or
	differ only in narrow per-game details

What to do next here:

- split the sink-file contents into three classes instead of treating each file
	as one monolith:
	- **shared pure helpers**: hashing, diag accumulation, replay metadata checks,
		small path/file helpers
	- **shared with adapters**: replay start / result / recorder helpers that need
		callbacks or game-owned accessors
	- **leave per-game local**: D2 AI/object/escort/physics probes and any code
		that directly traverses game-specific structs or control flow
- prefer moving pure helpers into `android/app/src/main/cpp/shared/input_demo/`
	because these are Android-branch-only systems already
- if a helper is shared only between D1 and D2 but would be awkward in
	`android/`, a second acceptable shape is dedicated D1/D2 files such as
	`input_demo_state_diag.c` or `input_demo_recorder_paths.c` under each game,
	so the original sink files still shrink
- for branch-only sink files such as `input_demo_hooks.c`, raw file-size shrink
	is secondary to single-source dedup; shared helper bodies are still worth
	doing even if the sink files remain substantial

Best first sub-tranches inside Lane A:

1. shared state-trace hashing and runtime/object/player-weapon diag helpers
2. shared replay-start command-line parsing and metadata validation
3. shared quick-record and sidecar path/file helpers now stranded in
	 `newdemo.c`
4. only after that, a re-survey of the heavier replay/result printers inside the
	 sink files

Progress in this tranche (2026-05-18):

- completed a first pass of sub-tranche 1 by centralizing the shared
	state-trace hash, runtime diag, player-weapon diag, object hash, and
	object-state scan helper bodies in
	`android/app/src/main/cpp/shared/input_demo_hooks_shared.h`
- kept the D1 and D2 robot-awake rule as a tiny local macro adapter in each
	hook file, which preserves the per-game AI detail split while removing the
	duplicated traversal body
- completed an initial pass of sub-tranche 2 by centralizing the shared
	`input_demo_start.c` command-line helpers, metadata validation, replay-player
	config application, skip-intro state, and replay load plus expected-game
	check in `android/app/src/main/cpp/shared/input_demo_start_shared.h`
- kept the remaining D1 and D2 differences local through tiny adapters for the
	primary-order copy size and D2's optional headlight default restore, while the
	heavier per-game new-level and checkpoint start flow stays in each file
- extended that same shared `input_demo_start.c` helper include to centralize
	replay command-line option parsing, common actual-result plus rng-trace setup,
	loaded-replay preflight, and the checkpoint temp-file write plus restore path
- kept the remaining split local through tiny adapters for D1 mission-name
	normalization, D2 replay-label enablement, and the differing
	`state_restore_all_sub` call signatures

What not to do here:

- do not immediately move D2 AI/object/escort probe logic into Android shared
	files just because it sits next to generic hash helpers today
- do not collapse all D1 and D2 hook logic into one giant shared file; that
	would reduce file count but increase type coupling and future risk

## Lane B -- Persistence, checkpoint, and launcher bridge code remains too spread out

The refreshed baseline says `state.c` is now a larger problem than `newdemo.c`
in both games. A quick reread of `d2/main/state.c` confirms why: the file still
mixes core save/restore flow with Android-only includes and helper surfaces for
resume, rewind, save metadata, coop save, indicator lines, and logging.

This cluster is still one of the best remaining shrink opportunities because it
is both large and still living in legacy upstream-shaped files.

Current file pattern:

- `d1/d2 main/state.c`: Android save/rewind/resume glue layered onto save and
	restore flow
- `d1/d2 main/coop_save.c`: branch-added duplicate files that should not stay in
	`d1/` and `d2/` long term
- `d1/d2 main/playsave.c`: launcher bridge and config/pilot helpers that must
	stay source-of-truth but do not need to live inline in the legacy files
- `d1/d2 main/multi.c`: coop inventory or host-migration state flows that are
	coupled to the same persistence story

Recommended survey conclusion:

- treat save/restore and coop persistence as one lane, not separate tiny
	tranches
- keep the real file-format and game-struct source-of-truth in the D1/D2 files
	where project rules require it, but move Android-owned orchestration,
	metadata, memory-buffer helpers, and launcher bridge bodies into shared save
	and coop files
- use `state.c` as the call-site boundary, not as the implementation home for
	Android rewind or metadata machinery

Good next tranche candidates inside Lane B:

1. move Android rewind/memory-buffer helpers and save-metadata plumbing out of
	 `state.c`
2. finish relocating `coop_save.{c,h}` out of `d1/` and `d2/`
3. isolate `playsave.c` launcher bridge bodies into a dedicated shared bridge
	 file while leaving the authoritative serialization logic local

## Lane C -- Networking and coop join flow still offer high-value D1/D2 shrink

`net_udp.c` remains in the top 12 files for both games, and `multi.c` still
shows substantial churn. This lane is not new, but the refreshed baseline says
it is still worth doing before another OGL pass.

Why this lane remains attractive:

- the earlier `cleanup_net_udp_extract.md` work already proved the shared-helper
	pattern here
- the remaining churn is still concentrated in Android or host-migration style
	helpers rather than deep, inseparable game-loop code
- both games still carry similar logic for reconnect, rebind, address refresh,
	rejoin, and coop state handoff

Recommended scope for the next networking survey tranche:

- continue extracting helper bodies, not policy
- keep game-specific side effects such as HUD updates, score/state mutation, and
	packet-dispatch control flow in the local files
- move reusable address-selection, reconnect-reset, join-slot selection,
	rebind, and identity/callsign dedupe helpers to shared code

This lane is still one of the highest payoffs for actual upstream merge cost,
because every saved line comes out of old large engine files that upstream also
touches.

## Lane D -- Menu, control, and touch UI work is now a better target than another random D2-only probe cleanup

`newmenu.c` is still large in both games, and `gamecntl.c` / `kconfig.c` are
still non-trivial. This is exactly the sort of work where D1 and D2 are close
enough that a shared helper surface should pay off quickly.

Current reading:

- `newmenu.c` still carries Android touch, drag-scroll, keyboard affordance,
	and controller/TV-oriented adjustments in both games
- `gamecntl.c`, `kconfig.c`, and smaller control files still carry Android-only
	menu or binding helpers that are more about UI plumbing than core engine
	policy
- unlike the input-demo probe families, these helpers are usually small,
	clearly Android-owned, and structurally similar across D1 and D2

Recommended sub-tranches:

1. shared `newmenu` keyboard/touch helpers
2. shared drag-scroll and release-toggle helpers
3. shared menu/control binding helpers used from `gamecntl.c` and `kconfig.c`

This lane is attractive because it removes Android code from legacy gameplay UI
files without needing deep game-struct abstractions.

## Lane E -- OGL still matters, but it is no longer the default answer

`d1/d2 arch/ogl/ogl.c` are still large enough to stay in the queue, but the
current refreshed baseline says they should not automatically outrank the newer
input-demo and persistence work.

The correct survey conclusion now is:

- continue taking isolated OGL helper wins when they are obvious
- keep using the existing shared runtime-state and helper pattern
- do not reopen broad OGL refactors just because the total counts are still
	high; many of the easy or medium-difficulty wins have already landed

Good OGL work from here forward should look like a phase-32/33 style helper
trim, not a new giant campaign.

## Lane F -- Explicit non-goals for the dedup queue

The refreshed baseline also makes it easier to avoid false positives.

These are not good front-line D1/D2 dedup targets right now:

- `d2/main/dxa_metadata_patch.cpp`
	- large D2-only file, but not a D1/D2 dedup problem
- D2-only probe-heavy files such as `physics.c`, `ai.c`, `laser.c`, `object.c`,
	`fireball.c`, `ai2.c`, and `escort.c`
	- these may still deserve cleanup or further extraction into D2 helper files,
		but forcing a D1 mirror or Android-shared abstraction too early would add
		complexity without reducing the real branch-maintenance cost

The survey rule here is simple: if the churn is D2-only and heavily tied to
game-specific AI or probe logic, treat it as a separate D2 cleanup lane, not as
part of the cross-game deduplication queue.

## Recommended next order from this survey

1. **Lane A first**: second-stage dedup of `input_demo_hooks.c` and
	 `input_demo_start.c`, starting with pure shared helpers
2. **Lane B second**: state/coop/playsave persistence cluster, because it is
	 large and still stranded in legacy files
3. **Lane D third**: `newmenu`/control helper extraction, because it is likely
	 lower-risk than another networking tranche and gives direct D1/D2 diff
	 reduction in old files
4. **Lane C fourth**: continue `net_udp`/`multi` shared extraction using the
	 already-proven shared-helper pattern
5. **Lane E last among the main lanes**: only isolated OGL helper trims, not a
	 broad re-open of the earlier campaign

If the next tranche needs the best ratio of line savings to conceptual risk,
Lane A.1 plus Lane B.1 is the strongest combined target: dedup the new sink-file
core first, then pull the Android save/orchestration helpers out of `state.c`.