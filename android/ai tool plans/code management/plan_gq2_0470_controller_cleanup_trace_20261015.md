# Chunk 0470 controller and cleanup ownership trace, 2026-10-15

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. Prior turn made progress with wrapper/preset PASS127. Current partial consumer checkpoint captures actual controller dispatch, selected paired automap hooks, Guidebot input/diagnostics, FileSetManager failures, whole BR-0287/0543 and three whole existing plans. No product/test edit or execution, payload, new finding admission or canonical publication.

## Controller input and observable state

MainActivity dispatchKeyEvent retains graphics/IME/suppressed-key ownership and releases tracked controller keys before later routing. onKeyDown sends controller handling ahead of child/menu fallback and native keyboard handling. dispatchControllerKey captures an input destination for the press lifetime. Visible overlay children take navigation; native menus reserve A/B/D-pad and X/Y/L1/R1 ahead of gameplay/meta bindings. Gameplay otherwise uses meta bindings or mixer/native joystick delivery. This preserves held A semantics and release pairing rather than replacing native menu handling with taps.

onGenericMotionEvent handles graphics/IME first and axis meta bindings, then switches raw sticks/hats into ControllerMenuAxes when an overlay/native menu is front. It clears controller mixer sources on entry, sends D-pad transitions, and resets menu axes on return to gameplay. Gameplay normalizes axes and updates the mixer. ControllerMenuAxes picks a nonneutral hat or stronger stick component and sends transitions only when direction changes. Native controller-menu detection explicitly excludes automap_handler, preserving flight axes while automap is front.

JNI nativeJoystickAxes validates matching array dimensions/count, converts values and publishes a batch mailbox; this selected body does not establish all numeric/source validation or the mailbox drain contract. nativeJoystickButton tracks button state then queues SDL events; SDL_PushEvent return is ignored here. Whole queue/held-state/JNI lifetime/finite-number ownership remains for existing-owner reconciliation, not new acceptance.

Paired automap_process_input copies the automap control state through kconfig_read_controls, stores it back and clears gameplay Controls. Paired freeflight rotation updates the live view matrix from pitch/heading/bank and normalizes it. Selected introspection producers read that matrix and freeflight setting. The script's zero-to-nonzero forward-X assertion therefore observes actual automap orientation. It does not prove full movement precision, hardware axes, frame timing or every automap path. Only selected get_view_info producer bounds are captured, not their entire geometry loops.

Guidebot native joystick handler supports D-pad up/down, A activation and B/Y close under Android; the menu handler routes it, flushes inputs on activation, and distinguishes multiplayer idle passthrough. Introspection recognizes the actual callback and reads selected_item through escort_menu_get_selection. guidebot.released is Buddy_allowed_to_talk. The test's release_guidebot_cage helper validates game/companion/segment, applies blastable-wall damage around the buddy and can fall back across the level; a later wait for released remains meaningful beyond helper success. This is controlled debug setup, not a claim that natural cage release or combat was tested.

## File-set cleanup counterexample and owner

FileSetManager.setActive loads the known-set registry and returns normally if a non-default name is absent. SetupActivity's already captured rejection branch likewise returns from runIo without failure. The D2 wrapper's result0 test therefore cannot establish owned-set activation before clear_audio_sources. A missing owned set with another active set is an ordinary source-level counterexample; no device experiment was run.

deleteSet forbids default, updates registry/active choice, saves config, then deletes the tree; failed deletion throws after the registry update. clearSet ignores recursive-delete/mkdir booleans before logging completion. These are actual outcomes to retain in storage/command/retirement reconciliation. No full file-set storage transaction or permission lifetime acceptance from these selected methods.

Whole GQR-0277 plan preserves exception-to-failure and GOG negative-count checks while requiring actual completed outcomes rather than normal-return success. The rejected switch is an additional instance of that result boundary. Its repair must fail or report the rejected operation, and cleanup must require the verified owned set before clearing audio; it must not clear whatever set happened to remain active. Whole GQF/GQR canonical rows and other material owner plans remain pending before final disposition; no status change.

## Deadline and original plans

Whole BR-0287 is specifically an immutable per-step deadline finding. It does not, by itself, establish coverage of the GOG master420/child600 budget composition. Whole BR-0543 owns runner resource lifetime, including the explicit aggregate-kills-child while Android executor survives counterexample. Preserve current child's finally improvement without claiming the forced-termination acceptance is complete. Deadline ownership must be decided separately at synthesis rather than expanding step-deadline claims silently.

Whole September25 gamepad plan documents shared Android routing, narrow guarded native hooks, paired desktop preservation and prior184/196-step integration passes. The May22 plan documents Guidebot controller menu/toggle and multiplayer goals. Those historical test claims and commands are not fresh execution in this diagnosis tranche. Preserve handmade comments and engine format/control ownership; any eventual fixture/budget/setup-result changes belong to Android test/JVM code rather than extra 1996-original edits.

## Read recovery and remaining work

Combined output clipped D1 freeflight translation middle, ControllerMenuAxes beginning, BR0543tail and the GQR0277 plan middle. Each required accepted part was recovered explicitly. Full D1 automap_apply_input is not credited, only the complete selected freeflight block; D2 equivalent and input functions are explicit. Other clipped locator or unrelated source tails earn no whole-family credit.

Next whole material transport/source/result/freshness/JNI/controller fixture and quality/prior-plan reconciliation, then direct unpooled synthesis, immutable final report/snapshot and canonical publication with terminal mounted Drive verification. Keep source gate closed and later gates open until their actual requirements pass. Canonical0470TODO, no ledger append, new root/remediation/rating/status, inherited saving or runtime evidence. Broad queue/preflights/sweeps/investigations/supplements and closure remain required.
