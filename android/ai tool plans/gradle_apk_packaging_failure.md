# Gradle APK packaging failure

- Locate the reported failure in local build logs and reproduce the Internal APK build
- Validate the full Play AAB followed by direct-install APK workflow
- Preserve full Gradle output and stack traces in the packaging script so an intermittent failure includes useful diagnostics
- Run scoped formatting and validate the packaging script and produced artifacts

Evidence: the 2026-10-05 16:11 failure was in packageInternal / IncrementalSplitterRunnable, not compilation or the Gradle deprecation summary

Validation complete

- Standalone assembleInternal passed with --warning-mode all and --stacktrace
- Full 1_build_aab_apk.ps1 -BuildType 3 passed: Play AAB followed by signed direct-install APK, including signature, package/version, and both engine libraries in all three ABIs
- The original incremental packager exception did not recur; its underlying cause remains undetermined because the original invocation omitted the stack trace
- Packaging now saves separate bundle/APK logs alongside its outputs, enables stack traces, and reports the relevant log path on failure
- Scoped formatting/lint and git diff whitespace checks passed; a failing native-command probe confirmed both output streams and the nonzero exit code survive the logging pipeline
- Gradle deprecation warnings remain separate follow-up work; they were also present in the successful builds
