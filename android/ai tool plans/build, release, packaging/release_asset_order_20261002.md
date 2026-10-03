# Android release asset order

Use a shared product/version prefix followed by `android-1-recommended` and
`android-2-only-for-android-6.0` so alphabetical asset lists show the recommended
APK first. Keep saved verification filenames unchanged for UploadOnly recovery.

- Update helper public filenames, generated notes and obsolete-asset cleanup
- Extend existing integration checks for ordering and old-name migration
- Run release integration coverage and scoped formatting
- Rename the published 1.0.0 assets and update their note references without
  rebuilding or replacing APK bytes; verify public order and unchanged digests
- Commit and push only the release helper, test, documentation and this plan

Completed: scoped quality checks passed and all 77 existing release scenarios
passed on PowerShell 5.1 and 7. GitHub's rendered asset list shows recommended
first and Android 6.0 second. Renaming preserved both asset IDs, sizes and
SHA-256 digests, and the release remains Latest with updated note references.
