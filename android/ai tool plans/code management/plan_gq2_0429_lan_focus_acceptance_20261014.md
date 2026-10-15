# Chunk0429 LAN invitation, service lease and focus acceptance, 2026-10-14

Conditional implementation and validation plan only. No product edits or runtime acceptance in this tranche.

Preserve current invitation parsing, address checks, explicit QR reveal and conceal-on-detach, request IDs and consumed-intent removal. Preserve launcher conflict confirmation, pending request across saved state, resumed/permission/discovery gates and coroutine ownership checks. These selected routes establish source intent; they do not prove Android activity recreation, permission or socket behavior.

When implementation is authorized, exercise external ACTION_VIEW and scanner results through actual launcher entry, with invalid addresses, repeated IDs, new IDs, rotation, activity recreation, permission denial/grant, pause/resume and an existing game/lobby. Require no join before the guards, one join after admission and no replay of consumed requests. Check cancellation during the IO join and replacement by a newer request. Treat invitations as a discovery address, not host authentication or content authorization.

Preserve separate game/LAN service leases, latest-start stopSelfResult guard, UID admission, runtime registration gate, deadline tokens, alarm and handler cancellation, wake-lock release and game-only timeout retirement. Validate game-start/LAN-start/stop permutations, queued newer start, bind/unbind, background timeout, service destruction and notification transitions on Android. Boolean helper tests alone cannot establish process survival or scheduling guarantees.

BR-0473 owns the existing unconditional delayed focus retry. Current LAN/browser pages explicitly skip RequestControllerInitialFocus and use page focus plus first-D-pad navigation. Preserve that behavior. Default reveal callers still use two requests around 300 ms; do not claim the page repair removes the dialog occurrence. Repair only under the existing owner with a real Compose clock, covering failed initial attachment, navigation before/at/after retry, touch/accessibility input-mode change, dismissal and recreation. Inspect all independent launcher retry owners before declaring whole-owner acceptance. Do not add a compatibility layer or generic focus abstraction without a demonstrated need.

No new finding, remediation, owner status/rating, runtime credit or inherited-file saving is admitted. UDP JNI/protocol, EQ/native audio, mouse callers and final publication remain pending.
