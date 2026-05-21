# Crash Handler And Flip Logging

## Goal
- Make launcher-native crashes produce exportable crash files reliably
- Add enough render-path breadcrumbs to narrow the SIGBUS near gr_flip

## Plan
- [x] SetupActivity: load the launcher JNI library before installNativeHandler
- [x] CrashLog: persist native-handler install failures into crashlogs/
- [x] Native crash handler: install a real sigaltstack instead of only SA_ONSTACK
- [x] d1/d2 gr_flip: add throttled stage breadcrumbs around palfx, gpu timer, MSAA resolve, framebuffer sample, swap, clear
- [x] d1/d2 ogl_swap_buffers_internal: add throttled breadcrumbs for paused return, EGL recreate, and eglSwapBuffers
- [x] Focused Android compile
- [x] Android code-quality pass

## Notes
- SetupActivity already installs the Java handler, but nativeInstallCrashHandler can fail there because no JNI library is guaranteed to be loaded yet
- MusicPicker and other launcher preview/import bridges use dxx-redux-d2 in the launcher process, so loading that once in SetupActivity is the minimal fix
- The native handler already requested SA_ONSTACK, but no alternate stack was registered yet, so stack-exhausted faults still had a weak chance to lose the report

## Follow-up After Log Review
- [x] CrashLog: leave install() Java-only and keep native handler setup in the explicit post-load installNativeHandler() call sites
- [x] Native crash handler: dump PC/LR/SP for ARM crashes so Shield reports can be symbolized without logcat tombstones
- [x] Crash breadcrumb ring: make index updates multi-writer-safe before adding audio-thread breadcrumbs
- [x] Android TSF music backend: add throttled breadcrumbs for song start, render-thread entry, callback activity, and MIDI render stages
- [x] Focused Android compile

## Cleanup Recovery
- [x] Restore SetupActivity launcher JNI load and native crash-handler arm after the file returned to HEAD
- [x] Restore d1/d2 OGL breadcrumb instrumentation in gr_flip and ogl_swap_buffers_internal after the files returned to HEAD
- [x] Investigate the cleanup scripts and confirm they do not contain git reset or checkout logic
- [x] Harden run-code-quality.ps1 with scoped -Paths support, stage-aware lock metadata, and a dirty-file summary report
- [x] Harden the helper scripts so scoped runs reach clang-format, ktlint, PSScriptAnalyzer, shellcheck, and shfmt correctly
- [x] Final focused Android compile after the scoped cleanup pass

## MIDI Crash Follow-up
- [x] Correlate launcher preview SIGSEGV and D1 level-start SIGBUS to the shared HMP-to-MIDI allocator path
- [x] Fix Android MIDI preview and in-game TSF playback to free HMP-derived MIDI buffers with d_free instead of plain free
- [x] Use d_malloc for Android raw .mid copies so all Android MIDI buffers stay on the same allocator family
- [x] Run scoped Android code-quality on the touched native files
- [x] Re-run focused Android native build after formatting

## D1 Briefing Crash Follow-up
- [x] Review the new crash report and verify the mem.c canary-failure signature is gone in the latest build
- [x] Symbolize the new ARMv7 PC/LR pair against the current D1 Android library and confirm the result is ambiguous against the runtime breadcrumbs
- [x] Add targeted breadcrumbs in the Android TSF stop/free path and meta-action SDL key injection path to disambiguate the next repro
- [x] Run scoped Android code-quality on the touched native files
- [x] Re-run focused Android native build after formatting

## Native Crash Header Parity
- [x] Mirror Kotlin crash-header app/build/device metadata into native crash reports
- [x] Add native crash timestamp and architecture fields
- [x] Keep native install-time metadata precomputed so the signal handler only writes cached strings
- [x] Run focused Android compile and scoped code-quality on the touched files

## Shield Pilot-Hold libglcore Crash
- [x] Review the Shield tombstone and correlate the libglcore SIGSEGV with the Android `gr_flip` breadcrumb path instead of the pilot-hold input helper
- [x] Verify that Android `gr_flip()` still performed unconditional framebuffer sampling (`glReadPixels`) every frame even when no debug snapshot or introspection dump was pending
- [x] Gate framebuffer sampling in D1/D2 Android OGL so it only runs when a merged-wall snapshot is pending or an on-demand introspection dump was explicitly requested
- [x] Add a small `game_introspect_dump_requested()` accessor so render code can see pending dumps without reaching into file-dump internals
- [x] Rebuild Android native code and package a fresh debug APK
- [x] Re-run the focused D1/D2 pilot long-hold threshold regression to confirm the pilot menu behavior still works after the render-path change

## Shield Pilot-Hold libglcore Notes
- The Shield crash dump showed the fault in `libglcore.so` with the app sitting in menu rendering, not inside the hold helper or input routing path
- D1/D2 `gr_flip()` sampled the framebuffer every Android frame for debug introspection, but `android_merged_wall_finish_snapshot()` already ignored those samples unless a snapshot was pending
- On drivers like Shield's, repeated menu-frame `glReadPixels` is a stronger crash candidate than the long-hold logic itself, so the fix narrowed the debug readback to real demand instead of changing menu behavior

## Shield Pilot-Hold libglcore Crash Follow-up
- [x] Review the new Shield tombstone and note that the last throttled breadcrumbs no longer showed `fb_sample`, only swap/clear around menu presentation
- [x] Verify that the crash still happened after the readback gate, so the first hypothesis did not fully explain the driver fault
- [x] Probe the next closest Android-only render path by moving the menu viewport reset ahead of `eglSwapBuffers` and skipping the inherited post-swap `glClear(GL_COLOR_BUFFER_BIT)` on Android
- [x] Rebuild Android native code, package a fresh debug APK, and rerun the focused D1/D2 pilot long-hold threshold regressions

## Shield Pilot-Hold Follow-up Notes
- The new dump still faulted in `libglcore.so`, but the last breadcrumbs had already advanced past swap and no longer included framebuffer sampling
- `event_process()` is not spinning at a runaway frame rate here; the crash is better explained by fragile Android post-swap default-framebuffer work than by a hot redraw loop
- The current probe removes Android GL calls after `eglSwapBuffers` in `gr_flip()` by preparing the next menu viewport before swap and leaving the old desktop-style post-swap clear disabled on Android

## Shield Pilot-Hold Menu Scale Follow-up
- [x] Trace the Android scaled menu path and confirm the pilot-select menus route through `android_menu_scale_blit_bitmap()` under OGL
- [x] Verify that the OGL blit helper used there, `ogl_ubitblt_i()`, creates and frees a fresh GL texture every call instead of reusing a persistent upload
- [x] Treat the transient per-frame texture churn as the next closest Android-only crash candidate, matching both the laggy menu report and the later `libglcore.so` swap-time fault
- [x] Disable Android menu scaling when the active render target is an OGL screen bitmap so D1/D2 fall back to the existing unscaled menu draw path
- [x] Rebuild Android native code, package a fresh debug APK, and rerun the focused D1/D2 pilot long-hold threshold regressions

## Shield Pilot-Hold Menu Scale Notes
- The latest Shield repro was visibly laggy before the crash, and the flip breadcrumbs slowed down sharply after the pilot menu opened, which points back toward menu-render cost instead of another swap-boundary cleanup issue
- `android_menu_scale_blit_bitmap()` allocated temporary scaled bitmaps every draw and, on OGL, fed them through `ogl_ubitblt_i()`, whose own comment already notes that it creates a new texture each call
- The current probe keeps the Android pilot-delete behavior intact while removing that transient texture path from menu rendering by falling back to the normal unscaled menu draw path on Android OGL until a persistent scaled-menu upload path exists

## Pilot-Hold Messagebox Reentry Follow-up
- [x] Review the new phone tombstone and note that the deepest fault moved out of GL and into `PHYSFSX_openReadBuffered()` while repeatedly re-entering `nm_messagebox()` from `android_pilot_listbox_hold_poll()`
- [x] Trace the long-hold delete logic and confirm that `android_pilot_listbox_trigger_delete()` invoked the listbox callback before clearing the active hold state or arming release suppression
- [x] Treat the repeated `listbox_handler -> android_pilot_listbox_hold_poll -> player_menu_keycommand -> nm_messagebox` stack pattern as a reentry loop, where the parent pilot listbox could be polled again while the delete confirmation menu was already running
- [x] Fix the shared hold helper so it clears the active hold and arms suppression before invoking the delete callback, and add targeted `pilot_hold:` crash breadcrumbs around that callback handoff
- [x] Rebuild Android native code, package a fresh debug APK, and rerun the focused D1/D2 pilot long-hold threshold regressions

## Pilot-Hold Messagebox Reentry Notes
- The long-press delete path is synchronous: the hold poll synthesizes `Ctrl+D`, `player_menu_keycommand()` immediately calls `nm_messagebox()`, and that nested menu enters its own `newmenu_do()` loop before the hold helper previously returned
- Because the helper used to clear `g_android_pilot_listbox_hold` only after the callback returned, the parent pilot listbox could still satisfy `android_pilot_listbox_hold_poll()` during the nested menu loop and recursively reopen the confirmation dialog
- The current breadcrumbs, `pilot_hold: trigger delete ...` and `pilot_hold: delete return ...`, should make any future reentry or unexpected return path visible in the xCrash breadcrumb ring without needing logcat timing guesses

## Pilot-Hold Cleanup
- [x] Remove the temporary `pilot_hold:` crash breadcrumbs now that the phone repro is fixed and understood
- [x] Restore Android OGL menu scaling so the enlarged pilot menus are active again
- [x] Rebuild Android native code, package a fresh debug APK, and rerun the focused D1/D2 pilot long-hold threshold regressions
