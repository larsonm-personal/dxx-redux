# PR 107 JVM failure investigation

Current head 3ae5489ee77f294ab4d8cb8a863ca872c877837f already contains
the D1 timeout dispatch fix. Its base is 2169d7162f939996aebd306c12846432924fb229.
Run 39536373034 fails in JVM job 119019864320; the other four workflows pass.
The JVM owner is master:test_gradle_unit_tests with historical proxy comparison.

1. Inspect current logs and changes since the previous green head
2. Reproduce the JVM owner locally where dependencies permit
3. Expose bounded public JVM owner logs on failure, preserving retail exclusions
4. Test the diagnostics, fast-forward the topic branch, and inspect exact-head CI

Artifact downloads return HTTP 403 despite successful metadata/log reads.
The initial job log contains only exit code 1, so no assertion is yet identified.
The diagnostics export only the fixed JVM owner log and master log, bounded to
three files of 64 KiB and 200 lines each, encoded as JSON to prevent workflow
command interpretation. Retail plans and other lanes emit no log content.

Local reproduction invoked test_gradle_unit_tests.ps1 with
ORG_GRADLE_PROJECT_compareProxyHistory=true and ciTestAbi=x86_64, matching CI.
It stopped before compilation with UnknownHostException: services.gradle.org
while fetching Gradle 9.8.0, so it provides no JVM assertion evidence.
The sweep regression suite passed 68 tests. Scoped code-quality checks passed;
unrelated formatter changes to existing Python code were removed afterward.
