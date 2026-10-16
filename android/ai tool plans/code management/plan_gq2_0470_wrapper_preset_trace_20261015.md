# Chunk 0470 wrapper and preset ownership trace, 2026-10-15

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. Partial consumer checkpoint; no code changes, payload use, helper execution, new finding admission or canonical ledger mutation.

The prior turn made concrete progress tracing native extraction/publication/configuration with PASS110. Current HEAD/product scope remains unchanged. This continuation traces complete current GOG wrappers, master entry construction/arguments/deadline enforcement, run_test caller/result/cleanup boundaries and paired native preset behavior.

## Variant and deadline scope

Both wrappers default to their Windows EXE variant. They can infer Mac PKG from a supplied path or accept an explicit validated variant. They reject recognized opposite-game filenames only during inference; explicit variant/custom path identity still needs source-admission policy. Both pass the selected variant to run_test and copy LASTEXITCODE.

The master creates one entry per PowerShell wrapper and does not attach GOG InstallerVariant arguments in its entry construction. Its PowerShell launch starts with empty arguments unless an entry supplies them. Thus nominal master execution covers each Windows default, not the full four-variant matrix. Mac support in wrapper metadata is not evidence of Mac execution. Both D2 options enable audio checks; retain the earlier stale Mac-skips-audio comment as documentation-only pending full reconciliation.

Both GOG master overrides are420 seconds (captured in the preceding checkpoint). run_test receives the wrapper default300, may increase it from resolved script timing and imposes launcher minimum600. The master waits its entry deadline then kills the process tree. The420 budget includes startup, verified installer staging, child monitoring and wrapper cleanup, so it cannot cover the child's full permitted600 interval. This is source-proven composition, not an observed timeout. Reconcile existing deadline/retirement owners before assigning a new root or plan.

## Script identity and results

run_test generates a GUID per game and embeds it in both the device script filename and requested run_id. Unresolved placeholders fail before activation. Both launcher/game broadcasts and Watch-AutomationResult use this ID. Durable PASS/FAIL require exact ordinal ID equality, and logcat PASS/FAIL fallback is disabled when an expected ID is present. Preserve these repairs; source-byte binding and transactional activation remain separate from ID correlation.

Whole existing BR-0284 remains OPEN for replacement/admission lifecycle; its current definition explains why correlated PASS alone cannot prove the intended replacement body executed. No new activation-family acceptance here.

The child finally stops its power session/app through its cleanup function, but the master can kill that process before finally runs. D1 wrapper removes the payload only when not SkipPush and ignores removal errors. D2 also restarts SetupActivity and runs switch-owned, clear-audio, switch-default, delete-owned, marking nonzero broadcast results as failure while continuing subsequent commands.

Actual switch_set catches rejection by logging and returning from runIo without throwing. The shared runIo outcome behavior was captured earlier. Therefore result0 alone is not an active-set postcondition. If the owned-set switch is rejected, the wrapper can continue clear_audio_sources against the remaining active set. Full FileSetManager failure/ownership and existing GQR-0277 command-outcome reconciliation remain pending; this checkpoint does not admit a duplicate root. A failed cleanup command likewise does not stop later destructive commands.

## Preset fixture and controller route

The script writes controller-test.ngs before entering the game. Whole host Reset-GameState and launcher resetGameState remove specified configs/pilots/saves, but neither removes .ngs preset files. Paired native load_preset finds all root .ngs files, strips extensions, sorts by name, and opens selected index0. The handler maps X to Ctrl+D and Y to Ctrl+R under ANDROID; delete targets items[citem] and removes the list item only if PHYSFS_delete succeeds. Defaults rebuild the parent menu.

The assigned script asserts listbox type/title, uses Y, reopens, X/B cancels, then X/A confirms and expects zero items. It does not assert the selected preset name or pre-delete count. Thus a retained preset sorting before controller-test can become the deletion target before the later count assertion fails. This is a fixture-ownership counterexample requiring remaining mounted-search-path/whole existing-owner reconciliation before new finding admission. Do not repair this by clearing unrelated user presets. A future authorized fix should isolate the test fixture namespace and verify exact identity before destructive confirmation, preserving all unrelated presets.

ControllerInputAutomation creates Activity-dispatched gamepad key events and joystick motion events; missing pressed sends down/up, explicit pressed holds one transition, tap_axes sends neutral axes after the action. MainActivity's accepted selected boundary parses/binds and calls dispatchKeyEvent/dispatchGenericMotionEvent on UI. Native automation invokes this Activity method through JNI and fails on a nonempty returned error before advancing. The whole Activity dispatch/routing/late-UI-work and automap/Guidebot families remain pending. No hardware or runtime credit.

## Recovery and continuation

Broad locator output was clipped and gives no whole owner/source credit. Failed Windows wildcard searches give no source credit; complete paired preset functions, complete controller event builder, selected JNI/Activity/result/reset boundaries and BR-0284 were recovered independently. Only MainActivity358-407 is packaged, not its clipped later gesture/function tail. The watcher package credits ID/terminal branches, not the whole watcher.

Next trace actual paired controller/menu dispatch, automap and Guidebot producers/diagnostics, FileSetManager cleanup ownership; reconcile whole deadline, transport, command outcome, fixture and prior-plan owners. Then consolidate direct unpooled disposition and immutable publication/Drive proof. Canonical0470 remainsTODO/sourcegateonlyclosed/fourlateropen. No ledger append, new root, remediation, status, rating, inherited saving or execution evidence; broad diagnosis closure remains required.
