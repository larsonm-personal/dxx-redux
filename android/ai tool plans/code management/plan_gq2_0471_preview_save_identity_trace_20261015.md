# Chunk 0471 preview controls and save identity trace, 2026-10-15

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. This extends the accepted music/save consumer checkpoint. Source inspection only; no product or test changes, execution, payload reads, fresh runtime acceptance or canonical publication.

## Preview request and observed state

Whole MidiPreviewBridge.kt was read. reserveStart increments requestedGeneration before the setup worker begins. startReserved checks that generation under lifecycleLock, then takes nativeDataLock before nativeStart. Stop and profile changes invalidate pending starts under lifecycleLock. These are material current repairs: do not repeat BR-0016's historical assertion that no generation or serialization exists. Active-set capture occurs inside the IO worker; complete file-set/source lifetime remains pending.

SetupActivity.runIo holds a BroadcastReceiver PendingResult until its coroutine finishes and reports ordinary exceptions as failure for ordered broadcasts. Renderer, EQ and soundfont selection can throw through this boundary. Missing preview source, track or HOG entry instead logs and returns normally; failed start logs, and rejected seek logs accepted=false. Do not equate ordered completion with successful playback. Full sender/result/timeout/cancellation acceptance remains pending under existing command-outcome owners.

SoundfontStore.select, selectRenderer and selectEq activate before persisting. They check activation, catch persistence failure and attempt restoration. saveSelection checks SharedPreferences.commit, attempts to restore prior in-memory preferences, then throws. Thus these selected functions do not simply ignore persistence failure. Restoration activation and second commit results are not checked here; complete failure/rollback ownership remains to reconcile before disposition.

Native seek clamps the fraction, calculates the target and immediately stores position_snapshot before publishing seek_target. The render thread exchanges that target while holding playback_mutex, rebuilds logical channel/controller/note state through approximate_seek, resets the timeline and ring, and publishes state afterward. Ring reset takes ring_reset_mutex. Preserve render-owned application and do not reopen fixed BR-0028 merely because its historical resolution described an earlier implementation with direct locked seeking.

JNI state, MidiPreviewBridge.getState and SetupAutomationApi export the same native position snapshot. The test's 250ms delay plus position>130000 assertion cannot distinguish the optimistic requested position from applied seek. Normal progression before seek remains meaningful; pause/resume flags are actual native atomics. Approximate seek deliberately restarts envelopes and avoids rendering intervening PCM; do not demand exact historical waveform continuity. A future applied-seek oracle should correlate a request with render-thread completion and resumed render/consumption evidence. This is an oracle limitation pending full existing-owner reconciliation, not a newly allocated finding.

BR-0276 remains OPEN for producer versus consumed clock and EOF drain; this partial render/control read does not establish the whole callback/EOF path. BR-0016 remains OPEN for its full MIDI/CD/dialog matrix despite current generation repairs. BR-0593 remains OPEN for staging identity; its old statement that run_test is checked was qualified in chunk0470 and is not reaccepted here. BR-0028 remains FIXED; historical tests are not fresh runtime evidence.

## Save slot and last-save-set identity

Whole D2 state_get_savegame_filename plus save/restore wrappers were read. Successful menu selection updates both state_default_item and state_quick_item to the selected zero-based slot. Save and restore menus initialize from that same default; the fixture does not merely choose an unrelated index after saving.

On Android, save paths use current pilot/mission, while ordinary non-coop restore paths first use the stored last-save-set pilot/mission. A failed or absent last-set read falls back to current identity. The selected last-set writer checks length, exact write, flush, close and publication; its publish helper was not accepted as whole transaction proof. Path construction clears the output if construction/recovery fails. Actual save-set path encoding, last-set admission and complete recovery/publication owners remain pending.

D2 state_restore_all drains checkpoints, checks demo/multiplayer guards, selects the restore file, presents Restore Game confirmation, and invokes state_restore_all_sub on Android. Only lines2805-2895 are accepted here, not the remaining function or full restore implementation.

Launcher reset recursively attempts deletion of sg/mg slots and player/config files and clears preferences, but ignores deletion and commit results. The fixture requests save, waits for a menu/screen transition, changes music source, then opens restore and confirms with Enter. The earlier checkpoint established save wrapper result propagation; the script itself does not acknowledge a specific completed save generation. Therefore ordinary slot/default identity is supported, while a failed reset or failed overwrite leaving a stale save with the expected preferences still requires explicit fixture-identity/completion acceptance. Do not claim all saves survive reset; the code does attempt their deletion. No new save root is allocated until existing owners are reconciled.

## Evidence and remaining work

Exact current source slices, four complete existing BR definitions, parent hash, canonical identities, HEAD and product-diff identity are captured in gq2-preview-save-identity-progress-0471-20261015.json. Independent replay must compare embedded bytes with actual physical source slices and whole-heading boundaries before this checkpoint is accepted.

No assigned-source coverage is added beyond the already accepted661 lines. Consumer gate stays open. Next: MSAA scene/menu probes, native render/error/reset/composition and menu interactions; lower playback and full harness edition/params/staging/freshness/deadline/cleanup; whole material quality owners and prior plans, followed by unpooled disposition and immutable publication with mounted Drive verification. Broad remaining queue and closure requirements remain active.
