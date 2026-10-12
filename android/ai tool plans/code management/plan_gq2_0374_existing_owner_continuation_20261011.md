# Chunk0374 existing-owner implementation continuation

Diagnosis-only future implementation plan. Existing canonical owners/statuses remain authoritative; no new finding/remediation ID and no product/test/helper changes or runtime acceptance.

- [x] Reconcile source-set/native/generated-source consumers and prior owners
- [ ] Implement separately after diagnosis tranche, preserving concurrent work
- [ ] Exercise actual maintained entry points and acceptance before closing owners

## Variant identity and unused native-version generation: BR0556

Move BuildInfo out of ignored shared main sources into Gradle-owned variant outputs with canonical validated Git/version/build identity; remove external writer/skip handshake with current AAB/GitHub owners. Direct and scripted clean/incremental/concurrent two-commit artifacts must agree across embedded Kotlin/native identity, manifest, logs/UI/lobby and immutable artifact reports. Existing Debug distribution fixture uses skipBuildInfo and does not inspect this identity; it cannot close the owner.

Tracked-source census finds only five references to the old generated native version header: output path/task, macro definition, task dependency and secretarea.c include. DXX_ANDROID_VERSION_CODE has no tracked consumer. The generated include paths exist in both native targets, but do not make unused macro meaningful. As part of canonical native identity reconciliation, remove unused Gradle126-148 block (23 lines), dependency239 (1), paired CMake generated include lines (2) and secretarea.c include44 (1), or connect the actual canonical native identity where required. Keep existing meaningful native input-demo metadata and both engine targets; do not silently remove the native artifact identity acceptance requirement. Up to27 authored lines are removable if the exact final consumer census remains unchanged, with no1996-original saving. Validate actual native targets after removal and inspect embedded identity separately. No new generic metadata framework or duplicate finding solely for this obsolete producer.

## SoundFont incremental identity/publication: BR0557 and BR0577

Preserve current URL/digest task inputs157-158, which repair the historical omitted-input witness. Remaining Gradle acquisition writes the source asset directly and deletes old bytes before download/verification. Coordinate dependency acquisition policy so one reviewed version/URL/digest selects bytes; stage verified replacements before publication, preserve prior usable bytes on failure and avoid conflicting shared writers. BR0577 updater's generation-specific version/URL/digest admission remains its own owner; current RolandSC55 pin differs from historical TimGM description, so revalidate the actual updater before implementing its old witness.

Actual acceptance must cover unchanged second-build up-to-date, URL/digest changes, source tampering/missing output, offline failure, partial download and concurrency and inspect packaged asset digest. Fixtures for root AcoustID configuration are separate and do not exercise this SoundFont task. Source declarations alone close neither owner.

## Exact tool selection and required signing: BR0558 and BR0559

Remove CMake+ policy relaxation only with actual pinned selection/caller validation. CurrentAGP9.4.1 differs from historical9.2.1 implementation evidence; inspect actual maintained resolver and report selected executable/version before claiming the current exact/newer/older host matrix. Preserve exact3.31.6 central authority and Windows/Linux/macOS behavior.

Direct non-CI builds now require a keystore properties file; CI intentionally uses separately signed direct sources. Preserve those repaired boundaries. Default Play Release/Internal still conditionally select upload signing; require complete validated signing configuration/certificate for documented signed modes before compilation/publication, with explicit unsigned/test mode separation. Actual direct Gradle/AAB wrapper/build-only/upload controls cover missing/partial/unreadable/wrong password/alias/certificate and correct configuration, then inspect retained artifact signature. The four-distribution fixture exercises Debug only and cannot prove these release/internal modes.

## Production direct authentication and release network policy: BR0090 and BR0343

Keep direct-install stubs and conditional Play dependencies to avoid packaging unused Play runtime SDKs. Direct auth reports unconfigured and current caller emits dev token; persistent keypair/proof-of-work production fallback remains BR0090. Implement its existing client/server protocol identity and actual production-configured end-to-end acceptance, rather than importing Play SDKs into direct APKs or claiming the stub is sufficient production auth.

Common manifest selects main network_security_config with development cleartext exceptions, also copied into generated direct manifests. Keep debug developer networking explicitly scoped while release/internal/direct production modes reject cleartext before credential acquisition; preserve ordinary trustedwss and source-set coexistence. Existing BR0343 merged-resource/runtime acceptance remains distinct from BR0090 authentication. Current APK distribution fixture does not inspect network policy or provider metadata; add actual merged manifest/resource/package checks where those changes are implemented.

## Retained source boundaries

Main manifest remains authority; direct generation removes Play APP_ID/provider and uses resource-bound legacyvmSafeMode. Retain separate package IDs/labels/SDK configuration, diagnostic suffix, conditional source/dependency sets, NIO desugaring and shared mission metadata module. Both native engines link launcher-loadable dxx_graphics_safety; the explicit target is not redundant merely because engine linking also pulls it. No broad flavor migration or schema/native format relocation.

Future distribution acceptance includes actual merged Play/direct/legacy/CI manifest/resources, supported API23/24 and current devices, native library/all-ABI packaging, actual desugared dependency calls and managed artifact identity/signature. Source fixture checks for dex markers are narrow packaging evidence, not all API compatibility or concurrency proof. Current mixed Gradle newline bytes remain untouched; no broad formatting for diff cleanup.
