# Chunk0432 native input, demo and progress acceptance, 2026-10-14

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. No code changes, build/test/device execution, runtime probes, media or user-data reads, staging or commit. Source inspection does not establish runtime acceptance.

## Native input

Preserve the shared mailbox implementation and narrow paired event_poll hooks. Controller channel offset16 matches Kotlin ControllerInputs; normalized channels16-31 remain independent from legacy touch/gyro channels. One producer vector is clamped and published with one generation and mutex acquisition. Production updates remain available behind automation ownership, release restores the latest production vector, snapshot acknowledgement advances consumed generations only through the captured values, and discrete event flush deliberately leaves continuous state pending. The deque retains legacy-axis threshold crossings; the drain delivers crossings through the captured snapshot generation before its final continuous values. Nonzero values dispatch again on later drains. Disabled joystick handling discards threshold transitions and acknowledges the snapshot without dispatch; preserve this distinction rather than treating generation diagnostics as proof of engine delivery.

JNI axis vectors use this mailbox; they no longer rely on SDL queue admission. Direct buttons still update native tracked state and ignore SDL_PushEvent result. Automation button injection calls the handler directly, so it cannot by itself validate the production SDL transport. Existing BR-0245 retains native admission/failure acceptance. Existing GQF-0183/GQR-0170 explicitly owns sequential JNI array acquisition: the batch bridge performs all three acquisitions before checking failure. Preserve partial cleanup, stop after first failed acquisition, and retain first pending exception in a later implementation. No CheckJNI or allocation probe ran.

The transition deque has no enforced bound. Existing GQI-0005 already records this and requires actual supported-stall producer/drain observation before admitting a budget defect. Historical synthetic crossing measurements are not fresh production evidence. Do not delete short pulses or final releases to reduce storage, and do not create a duplicate finding from this source reread.

The prior controller ownership plan remains applicable: neutralization must reach the appropriate mailbox or button owner before IME takeover. Native delivery remains a separate concern from the current repaired destination-bound physical key-up routing.

## Demo installation

InputDemoManager lists primary files and associates optional RNG/classic sidecars by basename. Preserve fallback listing for unreadable headers, current duration handling including reused frame time, name sanitization and exact-copy helper repairs. Listing/header estimation is not the native format validator and must not gain engine-format authority. The source reader scans all frame lines; this review ran no recording/payload parsing or resource-pressure experiment.

Actual Advanced Settings installation captures a staged target, checks available bytes and copies on IO, then refreshes or displays failure. Automation selects the newest staged entry matching optional game, installs into the active set and advances after return. Setup introspection directly uses staged/installed listing. These callers do not add a complete artifact-set commit.

Existing BR-0222 explicitly covers sequential installation and stale optional companions. A failed later copy can leave a replaced prefix while staging is retained; successful replacement without an optional companion leaves its prior installed counterpart. Source deletion occurs after copies and deletion results are ignored. Preserve per-file exact-copy/owned-temp repairs while designing one committed recording generation, explicit replacement policy and sidecar removal within that commit. Keep native recording-format and transaction ownership in shared Android-native code with paired engine hooks, rather than rewriting engine formats in Kotlin.

Future acceptance: every staged/prior sidecar-presence combination, sanitized-name collision, late copy/close/publication failure and process interruption must yield one coherent old or new generation, preserved staging on failure and no stale sidecar pairing. Coordinate native recorder publication with the same existing owner; native recorder bodies were not freshly reread here.

## Metadata estimates and file labels

Preserve the estimator as presentation: phase bands, per-level identity reset, monotonic maximum and exponential route-task estimate. level_done and a complete route_planning task can report1000. The actual worker loop gives raw checkpoint updates to LevelMetadataProgressDeadline, then adds the estimator only to the displayed copy. Parser admits only supported stages with positive task totals and builds separate task/overall/identity fields. This checkpoint covers the complete estimator/parser, selected deadline/observer and actual UI/monitor/current-route tracker consumers, not the whole worker/native producer or cancellation lifetime.

UI displays estimated level percentages separately from overall/current task. Monitor detail/state prefers estimated progress. Current-route tracker filters level identity, clamps calculating progress to999, and changes to1000/COMPLETE only on readiness complete; next_ready remains USEFUL. Keep readiness as the authoritative completion boundary. Existing worker-generation/terminal/cache owners remain open; a helper percentage test supplies no worker completion or timeout acceptance. Future verification should follow actual checkpoints through UI and readiness on task/level transitions, checking no fabricated liveness or completion credit.

Launcher labels delegate to GameFileFormats. Storage inventory uses linked-ZIP ownership labels first, then storagePurpose. Actual generated merged audio detection checks its cue marker, including the sibling cue for BIN files, before route-cache checkpoint/cache/temp naming rules. These are presentation classifications, not authorization to delete, proof of cache provenance, or cleanup ownership. Preserve shared labels rather than duplicate naming rules.

## Remaining chunk work

Complete Guidebot/gyro migration-removal/default/native consumer review, reconcile prior scopes and owners, and explicitly decide graphics-staged-generation-before-apply-ack ownership before direct0432 disposition/publication. No new root, status change or inherited saving is admitted in this checkpoint. Broader pending queue, sweeps, investigations, final-head/worktree supplement and closure remain required.
