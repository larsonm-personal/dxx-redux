# Chunk0432 controller ownership acceptance, 2026-10-14

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. No implementation, build, test, device, queue-pressure probe, staging or commit. Current source inspection is not runtime acceptance.

## Current repairs to preserve

ControllerKeyDispatch captures each press destination and sends release to that same destination. MainActivity dispatchKeyEvent handles tracked ACTION_UP before its keyboard-active guard. onStop and controller-device refresh call controllerKeys.releaseAll and controllerAxisMetaKeys.releaseAll; current direct and mixer-mapped button closures therefore receive release requests on those paths. Do not repeat the historical BR-0202/BR-0492 assertion that every physical release is always suppressed or that suspension only clears a button edge tracker.

The mixer synchronizes source updates, ORs buttons, adds and clamps axes, batches changed vectors, and can clear a source prefix without discarding unrelated touch or gyro contributions. These helper guarantees do not establish native acceptance or delivery. Current MainActivity uses nativeJoystickAxes for vectors; this checkpoint does not freshly inspect the native mailbox or SDL event implementation.

## Remaining existing-owner acceptance

BR-0492 retains ownership of controller-to-IME neutralization. showKeyboard resets local hat fields and sets keyboardActive without clearing ctrl mixer contributions or releasing controller owners. While active, generic motion processes axis meta-button edges, then updates only local stick/hat direction and returns before the ordinary controller axis vector and axis-button mixer updates. A previously published nonzero ctrl vector can therefore remain retained until another path clears or updates it. Synthetic HAT directions use dispatchDpad and remain in controllerKeys, whereas motion neutralization in the IME path does not dispatch the corresponding HAT release. The early tracked physical ACTION_UP repair does not itself generate an absent synthetic HAT release.

Before IME takeover, use one idempotent controller-neutralization operation to release captured key/meta owners, reset menu-repeat ownership with actual releases, neutralize ctrl mixer contributions, and clear hat bookkeeping in that order. Retain touch/gyro ownership and captured destinations. Keep late physical releases and fresh presses well defined; do not send a release to a newly exposed menu. Coordinate native delivery with the separate BR-0245 owner; no native delivery claim is admitted here.

BR-0202 keeps its paired native focus/hold acceptance open. Current onWindowFocusChanged only restores system bars on focus gain. This checkpoint does not reread paired menu/listbox hold implementations and therefore neither closes nor reconfirms their complete historical native defect. Current onStop releases captured button owners, but resets controllerMenuAxes with an empty callback; determine menu-repeat release ownership at final input synthesis rather than assuming every producer is neutralized.

## Future verification, not executed

For both games, hold stick, trigger, virtual axis, axis-mapped action, HAT, physical face/D-pad key and meta action while entering and dismissing a native text field. Check controller mixer state and actual engine state before another motion event can repair them. Verify balanced releases, no retained movement/action or menu repeat, and acceptance of the first fresh press. Repeat on stop/resume, focus change and device removal with simultaneous touch/gyro input, preserving those independent sources where appropriate. Queue rejection remains separate native-delivery acceptance.

Do not change archived finding statuses or create a duplicate root for these current qualifications. Graphics staged-generation acknowledgement remains an unresolved separate candidate requiring explicit semantic ownership reconciliation before chunk0432 publication. Demo installation, metadata progress, Guidebot/gyro removals, native input consumer synthesis and final chunk disposition remain pending.
