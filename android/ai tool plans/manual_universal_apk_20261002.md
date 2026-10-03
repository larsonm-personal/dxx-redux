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
