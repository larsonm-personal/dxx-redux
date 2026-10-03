# One Android release with two APKs

1. Build and verify current and legacy APKs sequentially before publishing one android-vVERSION release
2. Check matching source commit, versionCode and signing certificate; support UploadOnly with both saved builds
3. Label the legacy download only-for-android-6.0 and explain that it is only for devices unable to use the recommended APK
4. Keep both APK hashes and SDK information in one generated notes block, preserving custom notes
5. Verify uploaded asset digests before retiring the separate legacy release/tag
6. Extend release orchestration tests, run PowerShell 5.1/7 and scoped quality checks
7. Combine the existing verified 1.0.0 APKs without rebuilding and inspect the published release

## Validation

- All 77 release integration scenarios passed on PowerShell 5.1 and 7
- Scoped formatting/lint and git diff --check passed
- Real UploadOnly verified both signed 1.0.0 APKs and published their matching hashes in one release
- Downloaded both assets and matched their local hashes; checked the rendered notes and Latest status
- The separate android-legacy-v1.0.0 release and tag were retired after upload verification
