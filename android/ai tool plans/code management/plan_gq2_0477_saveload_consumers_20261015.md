# Chunk 0477 save-load and resume consumers, 2026-10-15

Partial actual-consumer trace; source gate closed/four later open. Whole startup_resume_shared.c and android_resume_pilot.c plus explicit selected ranges in captured package. No product/test/helper edits or execution, payload hashing or fresh runtime acceptance.

## Request dispatch and meaningful restore

game_automate6667-6742 save/load uses same queue API as UI and fails if rejected. Quick-save/load dispatch real meta actions; game menu/resume use pause queue; overlay pause is admitted only if safe. Auto-minimize and difficulty debug requests set existing request globals. Preserve distinctions: successful queue admission is not completed disk save, and immediate SET_DEBUG handler itself is not a post-delay acknowledgment.

android_input922-1001 forwards save/load to engine pause actions and debug overlay to pause coordinator. Selected shared meta_actions270-430 closes a front pause window before pending operations, rejects invalid dead/competitive-multiplayer save/load, handles single-player quick save/restore through native slot functions, and reports HUD outcomes. Autosave kind selects exit/minimize slot, clears request, saves unless dead, then AUTO_EXIT requests quit even if save failed. Difficulty uses difficulty_change_to with demo recording policy. Distinct actual completion predicates in fixtures must remain.

Pause coordinator190-304 uses bounded32 request queue and session/owner/revision admission. Dispatch runs only without graphics/background/co-op/operation blockers and with actual game/user-pause front. It acknowledges dispatch before nested menus or restoring control flow that may longjmp. Queue result APPLIED is dispatch acknowledgment, not proof of successful restore. Request sessions reject stale prior-game work.

Successful paired state_restore_all_sub wrappers notify restore ownership/visibility, retain failed-world-change menu request and call android_pause_new_session only on success. Both game close paths clear Game_wind, reset session and defer restore_game_menus to EVENT_WINDOW_CLOSED after the old window is unlinked; the handmade comments explain double-deactivation pause leak on startup resume. Paired inferno outer loops invoke android_pause_outer_tick after synchronous stacks returned/unwound; only there is orphan legacy depth cleared and dispatching repaired. Preserve these narrow inherited hooks and shared coordinator.

Pause reason bit2 is MENU; background visibility and a retained native menu are separate. Simulation/input getters account for game mode and actual front window/modal/background/operation reasons. Publication clears ship Controls when input first blocks, then resets time and flushes inputs when allowed again. This supports retaining the SDK's foreground-but-menu-paused assertions and save/load fixture's repeated restore plus held-fire energy decrease; screen/game-window presence alone is weaker.

## Native save paths and missing pilot

Selected state_android_shared1180-1270 saves through a temporary path, validates save, publishes slot, then writes last-save-set tracking and cleans temp on failure. Native slot bounds, multiplayer/co-op policy and checkpoint draining precede restore through state_restore_all_path. Private publication/validation internals were located, not read in this package; preserve existing transactional implementation without claiming full durability audit from the callers.

Native enum/header owns save kinds, slots and descriptions: quick3, abort6, progress7, exit8, minimize9; kinds and descriptions differ from slot numbers. No Kotlin duplication of binary format is proposed.

Whole startup_resume validates command path, prepares pilot through native save-header parser, logs restoring path before calling state_restore_all_path, logs actual result and flushes inputs only on successful restore. Thus script's console substring proves an attempted path; its additional live level assertion is needed. It is not a full save-state equality oracle.

Whole android_resume_pilot reads callsign through state_get_save_file_callsign. Missing/unselectable pilot uses save callsign rather than blocking resume. Existing selectable pilot is loaded through native playsave and failure is explicit. Unreadable/sentinel save callsign may fall back to an already valid current pilot; no arbitrary malformed probe or new format reader. Autosave repair chooses real current/last/enumerated selectable/default player callsign as needed.

clearPilotFilesForAutomation only walks D1/D2 engine directories for .plr/.plx, counts successful deletes and leaves save files. It does not assert every delete succeeded or that all pilots are absent. The fixture's eventual resume after requesting clear is meaningful but not independent proof the pilot was actually missing. Reconcile actual deletion/outcome with existing command-failure owners before rating or duplicating a finding.

## Launcher boundary

Selected SetupActivity474-526 captures launch owner, awaits launch-file preparation, rejects stale owner/preflight failure and invalid resume path, constructs engine launch intent with save/callsign plus automation run/step identity. Remaining intent-start/handoff and save-candidate producer not reviewed here.

Selected SetupResumePanel resolves candidate relative/absolute game-root path and candidate/path-derived callsign; no native file-layout inference. Candidate ready/UI area filters base-game required data, game-running/hash state and resume metadata. This is launcher visibility/admission, not proof save payload is complete/current. Keep native save/header/probe authority and source identity.

Continue SDK lifecycle checkpoint/park and host total budget, ISO import typed outcome/ownership, objective readiness/adoption/secret labels and profile preview admission. Reconcile actual stored save/thumbnail producer and launcher result boundary only as needed. No speculative new root/status/rating, no runtime/original saving. Console encoding failure and clipped combined read were recovered explicitly with UTF8/bounded reads before body credit.
