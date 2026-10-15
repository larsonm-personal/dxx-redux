# Chunk0431 level-preview early-close disposition, 2026-10-14

Diagnosis and conditional implementation plan only. No product changes or device/runtime reproduction.

Disposition: EXTENDS existing GQF-0183 / GQR-0170 preview session and terminal ownership. No new finding or remediation ID. Historical GQ2-0354 explicitly assigned synchronized user-close, surface-loss, startup-failure and native-completion ownership to this continuation. That historical robot-preview analysis preserved its SDL_QUIT/wake path. Fresh level-preview evidence supplies a narrower lost-command schedule without that alternate path.

Source-supported schedule

1. LevelPreviewActivity.surfaceCreated sets nativeStarted=true before its raw thread calls JNI.
2. Back invokes closePreview. With nativeStarted true it only invokes nativeRequestClose and does not finish.
3. LevelPreviewActivity JNI close only stores the shared atomic close flag. It does not queue SDL_QUIT or wake the event owner. This differs from RobotPreviewActivity.nativeCloseRobotPreview.
4. The worker later enters android_start_preview, then native main, then the paired guarded inferno preview dispatch.
5. android_level_preview_run unconditionally stores zero before parsing the request. This later store can overwrite the earlier close publication. Subsequent cancellation checks and loop exchange see no retained close.
6. Whole Activity supplies no retained cancellation generation or replay after native readiness. The schedule establishes lost request from source; it does not establish measured duration, every initialization failure outcome, a permanent hang or a device reproduction.

Semantic owner reconciliation

- GQR0170 already owns synchronized preview startup/close/terminal lifetime and exported JNI/native disposition. Extend that existing continuation rather than invent a new root from the atomic flag reset.
- BR0016 concerns MIDI/CD start-stop ordering and global audio resources, not this level-preview lifecycle. Its generic title is not a matching implementation.
- BR0078 concerns unique native engine admission and Activity/process retirement. Current atomic admission and TERMINATING repairs remain; they do not retain an early close request.
- BR0244 concerns admitted UI/game command handoffs and their counting/coalescing rules. The selected flag is already atomic; this failure is initialization erasing a lifecycle request, already assigned by0354 to0170.
- BR0484 concerns the separate isolated metadata-service request and observer. Preserve strict-generation/deadline/result acceptance there; do not claim this level Activity uses that service protocol.
- BR0029 owns acknowledged mutable engine snapshots and menu-transform coherence. Whole owner was reconciled, including current queue/snapshot repairs; the early-close flag needs no new engine-world snapshot owner.
- BR0460 physical transport and BR0654 probe readiness remain separate. No canonical status changes.

Conditional implementation

Establish the preview session before admitting asynchronous startup, with one explicit close/terminal contract shared across Activity/JNI/native boundaries. A close accepted while startup is pending must either prevent startup or remain latched for that same session until consumed. Reset only while establishing a new owner before close can be admitted; do not blindly remove reset if that could carry cancellation into another owner. Keep dedicated-process single-engine admission and existing startup/error cleanup.

Use the existing preview ownership machinery. Do not introduce a broad common Activity hierarchy or alter legacy engine formats. If an event/wake fallback is chosen, prove its admission before SDL initialization and its target lifetime rather than assuming robot behavior applies. Preserve first pending Java exception, exact request/runtime-write lifetime, and exactly-once terminal result and bounded retirement from the0170 continuation.

Later actual barriers must stop the worker before JNI, after engine admission, before native preview initialization, immediately around the reset and before first frame. Issue ordinary Back and surface-loss close at each point in both games, then release the worker. Require accepted close never to disappear, no later preview reopening, one terminal result, exact owned request cleanup after readers retire, and no close affecting another admitted session. Retain ordinary post-start close and startup-failure controls. These are future tests; no build/device/fault/race execution occurred here.

The prior SOURCE-CANDIDATE-OWNERSHIP-PENDING decision is superseded by this explicit existing-owner extension. Include this disposition and exact source/historical bindings in final0431 publication. Whole earlier test-scope reconciliation, final report/publication/replay/Drive and broader diagnosis remain open.
