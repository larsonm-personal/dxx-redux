# Nightly Android tooling lane

## Scope
Expose full Android SDK/JDK/NDK tooling to the reviewed nightly sweep controller, using existing Gradle tasks and named instrumentation owner scripts rather than duplicating assertions. Keep normal universal builds unchanged and include metadata-core in the full JVM owner.

## Implementation
- Reusable workflow consumes an exact commit SHA and a same-run plan artifact
- Trusted controller/policy and target source use separate checkouts
- Existing pinned bootstrap scripts provision missing tools; the hosted SDK and licenses are reused
- JVM lane runs without an emulator; Android lanes require accessible KVM and prepare one or two dedicated headless AVDs
- CI-only ciTestAbi=x86_64 limits native compile work; absent property retains all three ABIs, invalid values fail
- Debug and instrumentation APKs are built before emulator boot and installed with existing APK helpers; the master runner reuses up-to-date tasks and owns named test assertions
- Android single-emulator setup with selected public scenarios reuses the pinned public demo extraction bootstrap, including installer/file hashes, bounded extraction and retail-shadow rejection
- Existing public wrapper covers 29 audited scenario owners expanded into 55 owner/game selections; nested cases/skips are retained in owner logs and are not counted as independent leaf coverage
- Only controller-overlay and default package-documents owners bypass retail provisioning, via an exact-name/type/arguments allowlist with regression assertions in the existing catalog test
- Always-on collection and artifact upload retain durable test evidence; setup outcomes never imply test pass
- Setup/build overhead is separate from the selected approximate 45-minute test budget; job timeout is 90 minutes

## Validation and limits
- actionlint 1.7.12 passes for the reusable workflow
- All 12 embedded Bash blocks pass bash -n
- Executed the exact extracted Groovy ABI selection code: default keeps three ABIs; x86_64 selects one; empty, arm64-v8a and mixed values fail
- Full JVM owner source selects app, metadata-core and proxy tasks; nightly enables the existing historical proxy comparison with full target history
- No PowerShell execution, SDK download, APK build or emulator runtime attempted locally because of the existing execution prohibition and storage constraints
- Runtime and test count claims require hosted execution receipts; media eligibility and per-commit coverage state belong to the sweep policy/controller
- The repository default branch is main; scheduled workflow activation requires reviewed placement there, separate from this implementation PR

## Source evidence
- `helpers/run_shareware_tests.ps1` discovers `_shareware` scenarios and dispatches the existing automation or integration owner with `-DataEdition shareware`; it verifies both game fixtures without implicit downloads
- `tests/test_shareware_automation_profiles.ps1` audits all 29 public profiles and retained assertions; `_info.games` expands these to 55 fixed selections (abort-to-menu, autoselect-crash and resolution are D2-only)
- `helpers/setup_shareware_test_data.ps1` and `helpers/shareware_test_data.ps1` retain pinned hashes and forbidden retail-shadow checks
- `ControllerOverlayChecks.kt` and `MusicVolumeTouchChecks.kt` use packaged UI, synthetic input and recording callbacks; `PackageDocumentChecks.kt` creates temporary document fixtures when Published is absent
- `helpers/nightly_public_fixture_policy.ps1` rejects every explicit argument, variant, dual-emulator owner or unknown owner; ordinary/full suite provisioning is preserved
