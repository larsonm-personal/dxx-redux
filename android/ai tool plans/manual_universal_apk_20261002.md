# Manual universal APK

- Add a manual-only GitHub Actions build for the direct-install Android distribution
- Use a separate CI application ID and unsigned release-mode output
- Optionally sign with a stable private test keystore supplied through Actions secrets
- Install pinned build tools, verify all three ABI payloads, and upload the APK and checksum with short retention
- Validate workflow syntax, Gradle distribution behavior, and a complete hosted build
- Preserve unrelated checkout changes by testing an isolated branch

## Progress

- Existing Gradle configuration already builds ARM32, ARM64 and x86_64 payloads for both games
- Production GitHub releases require signing; CI needs an explicit separate mode
- Added a workflow_dispatch-only APK job with unsigned output by default and optional private test-key signing
- Added the CI app ID and direct-install distribution without changing production signing requirements
- Fixed SDK package discovery for the new slash-separated Android CLI package listing and extended the SDK provisioning fixture
- Scoped mixed-language formatting/lint, actionlint, ShellCheck and PowerShell parsing passed
- Local Gradle configuration succeeded in CI mode with an intentionally unusable production keystore; production GitHub mode still rejected a missing keystore
- Hosted Android tooling passed on both Windows and Ubuntu, including after integrating the current cmake branch: runs 37035774947 and 37037160155
- The first complete hosted APK build exposed an incorrect graphics-library filename in the verification step; corrected it and run 37035774982 passed, uploaded the APK, and saved dependencies
- Downloaded that artifact, verified its checksum and all ABI library payloads, and signed it with a private temporary key outside the repository; apksigner verification passed and the key was deleted
- Final current-source APK validation: run 37037829265
- Production signing secrets were not configured; the full four-distribution runner and on-device gameplay were outside this workflow test
