# GQR-0282: bind installer reset to artifact package and device

Diagnosis-only plan for GQ2-CHUNK-0368. Admitted GQF-0296 / GQR-0282, REGROWTH linked to archived BR-0550; canonical admission audited 2026-10-11. P1/high, score53 (32/0/4/10/7). No implementation or runtime reproduction.

## Source evidence and ownership

The selected automatic build invokes 1_build_aab_apk.ps1 -BuildType1. The builder passes playApplicationId=com.dxxrevival.app with ciApk/legacyRelease/githubRelease false for its AAB; app/build.gradle derives actual applicationId from that property. install-aab.ps1 still uninstalls literal com.dxxredux.app after an explicitly authorized INSTALL_FAILED_UPDATE_INCOMPATIBLE failure. These identities also occur at the frozen review boundary.

Ordinary source-derived witness: the selected com.dxxrevival.app APK conflicts with an installed signing key, while a coinstalled com.dxxredux.app holds sentinel private data. ResetAppData deletes the unrelated old package, and retrying the selected APK leaves its actual signature conflict unresolved. No device was contacted and no data-loss experiment was performed.

BR-0550 remains historically FIXED: default install failure preserves data, reset requires explicit ResetAppData plus the signature-mismatch result, and uninstall/retry failures are checked. Preserve all those protections. Track the artifact-target mismatch separately as regrowth, without reopening or deleting archived history. BR-0539 remains the related device-selection owner; BR-0409 Java Properties parsing and BR-0549 password command-line exposure remain separate OPEN roots. Existing instrumentation/provider package findings concern their own consumers, not this installer reset.

## Implementation scope

- Change install-aab.ps1, with a small artifact/device inspection helper only if it reduces duplication, and a focused maintained installer fixture
- Inspect the exact universal APK using a maintained Android manifest tool; compare with selected AAB manifest and reject missing, invalid or conflicting package identities before device mutation
- Give conversion/staging an invocation-owned generation. Retain package identity and artifact hash through install/reset/retry; refuse changed artifacts instead of inspecting one APK and installing a replacement at the shared output path
- Resolve one declared, validated device serial before install. Carry the same serial through every install, diagnostic, uninstall and retry command; reject ambiguity and unavailable targets before mutation
- Keep default failure nondestructive. Only explicit ResetAppData plus proven signature mismatch permits uninstall, naming the artifact package and selected serial in the warning
- Uninstall only the inspected artifact package on that serial. Check uninstall result before retry and report preservation/reset/retry outcome precisely
- Preserve existing distribution application IDs. Replacing the old literal with a new literal is insufficient for GitHub, CI, legacy and caller-selected artifacts
- Keep signing-password and keystore parser repairs under their existing owners; do not combine unrelated tooling refactors into this fix

## Future acceptance

Exercise the actual installer CLI with controlled Java/bundletool/manifest-tool/ADB substitutes, an invocation command ledger, and package-specific sentinel data. The fixture must invoke production selection and failure branches, rather than reproduce the policy in a separate function.

Cover default, Play, GitHub, CI and custom manifest packages; the ordinary two-package mismatch witness; successful replacement; every default failure; unknown/non-signature failures even with ResetAppData; explicit signature reset; absent/invalid/conflicting manifests; changed artifact generation; zero/one/two/offline/unauthorized devices; conflicting ambient serial; uninstall failure and retry failure. Require unselected package/device sentinel bytes to remain unchanged and exact selected-package uninstall only after authorized signature mismatch. No arbitrary device enumeration may turn ambiguity into permission.

Run relevant scoped quality and maintained fixture checks after implementation. Device parity may be added later with explicit selected test targets; this diagnosis tranche does not run builds/tests/devices or destructive probes.

## Gates

- [x] Source witness and archived-owner distinction prepared
- [x] Canonical finding/remediation admission and final chunk publication
- [ ] Implement artifact and serial binding
- [ ] Run actual-entry acceptance and scoped quality

No inherited source changes or 1996-original saving are proposed.


## Admission checkpoint, 2026-10-11

Canonical GQR-0282 remains TODO; diagnosis publication does not implement the remediation. Chunk0368 publication audit PASS3022 SHA277779920f8e5a948be53876b7568b274a5fa920c402eee9831f1d45f1054b9c. Snapshot binds the earlier proposal bytes before this admission/gate update. No runtime acceptance.
