# GitHub APK releases

## Build and publication recovery follow-up

1. Explain the final source check and exempt only the release script, which is not an APK input
2. Report dirty paths and distinguish a missing remote commit from authentication/network errors before building
3. Add explicit UploadOnly recovery using saved build metadata and fresh APK/hash/signature checks, without changing normal fresh rebuilds
4. Mark whether source validation completed so an interrupted or changed-source build cannot bypass checks on retry
5. Cover helper edits, actual source/HEAD changes, missing commits and upload retries on PowerShell 5.1 and 7
6. Run scoped formatting/lint and orchestration tests without publishing a real release

Validation:

- All 60 release integration scenarios passed on Windows PowerShell 5.1.26100.9444 and PowerShell 7.6.6
- Release-helper edits before/during builds are allowed; app edits, staged source changes and HEAD changes during builds remain blocked with useful diagnostics
- UploadOnly retains the saved APK's commit/versionCode, skips Gradle and rejects mismatched APK hashes, certificates, checksums, missing files and incomplete source validation
- Tested recovery of older saved output after helper-only changes, including sourceClean=false metadata from the reported failure
- The actual saved 1.0.0 APK passed Android signing/package/ABI/checksum verification and the complete UploadOnly flow on PowerShell 5.1 with every GitHub call mocked
- Scoped formatting/lint and git diff --check passed; the saved APK was unchanged and no real tag or release was modified

## Plan

1. Add a local PowerShell release command accepting a version, with build-only and draft modes
2. Build one signed universal release APK with explicit version metadata and a separate GitHub application ID
3. Disable Play sign-in and Play update prompts for this distribution; preserve existing Play builds
4. Verify signing, package metadata and ABIs before uploading APK, SHA-256 checksums and build metadata
5. Require clean, pushed source for publishing and bind the release tag to that exact commit
6. Document signing continuity, game-data setup, installation and failure recovery
7. Run scoped formatting, release orchestration tests and a real Android release build

## Decisions

- Use the existing local Android toolchain and ignored keystore.properties rather than copying signing secrets into GitHub Actions
- Use android-vVERSION tags to distinguish Android releases from desktop releases
- Use com.dxxredux.app.github so a Play app signed by Google's app-signing key can coexist with locally signed APKs
- Keep the existing commit-count versionCode convention with an explicit override for rebuilds
- Stage uploads as a draft; publish only after successful asset upload

## PowerShell 5.1 follow-up

- Lower the minimum shell version and retain PowerShell 7 support
- Check native-command errors, JSON responses and UTF-8 release notes under Windows PowerShell 5.1
- Run release integration tests on both shells and a real build-only invocation on 5.1

Validation:

- All 17 release integration scenarios passed on Windows PowerShell 5.1.26100.9444 and PowerShell 7.6.6
- Covered native stderr warnings and UTF-8 notes without a BOM, including non-ASCII text
- Rechecked 5.1 with the CI shell's native-exit-code handling and added the 5.1 CI step
- Scoped formatting and lint passed
- A real Windows PowerShell 5.1 build-only run for 1.2.0-ps51.1 completed, including Gradle release lint, APK signing verification, version/ABI checks and release-file generation

## Initial validation

- Implemented the release command, distribution flag, provider gating and operator guide
- Passed all 16 isolated release orchestration scenarios, including both Android SDK signing-output formats
- Added the release scenarios to the cross-platform tooling smoke runner
- Scoped PowerShell/Kotlin formatting and lint passed; git diff --check passed
- Built both native engines for all three ABIs through assembleRelease and passed Android release lint
- Completed release-github.ps1 -Version 1.2.0-rc.2 -BuildOnly; verified the 55,972,686-byte APK signature, application ID, version, non-debuggable status and ABIs
- Verified the GitHub manifest disables PlayGamesInitProvider; the default distribution's diagnostic debug manifest keeps it enabled
- Installed the signed universal APK on emulator-5556 and launched SetupActivity successfully, with Play Games initialization skipped and no AndroidRuntime crash
- Pure AOSP/no-Google-services gameplay and real GitHub publication were not tested; installed emulators have Google services and the host does not have gh installed
- Initial packaging was rerun after adding the provider manifest placeholder; real SDK verification exposed a changed certificate-output label, which was fixed and covered in the integration tests

## Same-version rebuild follow-up

1. Clean the app and assemble in separate sequential Gradle invocations without build-cache reuse
2. Reuse the local version output directory after APK verification
3. Replace the three generated assets on existing mutable releases, preserving notes and publication status
4. Retain source/tag checks and recheck remote release state immediately before upload
5. Cover repeated builds, existing drafts, immutable releases and failure recovery on PowerShell 5.1 and 7
6. Update operator documentation and run scoped formatting/lint

The real build exposed a Gradle scheduling conflict when clean and assemble shared a task graph. Separate invocations keep cleanup ahead of generated assets and avoid that conflict

Validation:

- All 30 integration scenarios passed on Windows PowerShell 5.1.26100.9444 and PowerShell 7.6.6
- Covered repeated builds, published/draft replacement, recovery after upload failure, immutable releases, source mismatches and clean/build/signature failures
- Scoped formatting and lint passed after separating the Gradle invocations; git diff --check passed
- A real Windows PowerShell 5.1 BuildOnly rerun for existing version 1.2.0-ps51.1 completed the clean and assemble steps, release lint, signing checks and version/ABI checks
- Verified the existing local output files were replaced with a new APK hash and build timestamp, matching checksums and the same signing certificate
- GitHub requests were mocked in the integration tests; no remote release was modified

## Moving the release tag on same-version updates

1. Permit existing tags and draft targets to differ from the current source commit
2. After a successful build and verification, create or force-update only the requested remote Android tag to the built commit
3. Verify the remote tag resolves to that commit before uploading, and refresh an existing release's target commit while preserving its notes and publication status
4. Keep clean/pushed-source and immutable-release checks, including a recheck immediately before mutation
5. Test real commit changes, annotated and missing tags, tag-update failures and upload retries on PowerShell 5.1 and 7
6. Document moving tags, local tag behavior and recovery from partial remote updates

Validation:

- All 39 integration scenarios passed on Windows PowerShell 5.1.26100.9444 and PowerShell 7.6.6
- Fake GitHub API operations used real bare Git repositories to verify tag movement, annotated-tag resolution, exact-name matching and a same-version rerun after committing and pushing new source
- Covered blocked tag creation/update, unsuccessful tag verification and failed release-target updates before asset upload
- Build/signature/source failures and immutable releases left remote tags untouched; draft and published status stayed intact during replacement
- Scoped formatting/lint and git diff --check passed
- Android build commands were unchanged from the prior real signed rebuild; GitHub API requests were mocked and no actual GitHub tag or release was modified
