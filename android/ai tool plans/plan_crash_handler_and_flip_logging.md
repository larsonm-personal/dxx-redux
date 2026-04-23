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
