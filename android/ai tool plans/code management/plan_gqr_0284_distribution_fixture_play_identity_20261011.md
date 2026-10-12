# GQR-0284: accept maintained Play identity in distribution validation

Diagnosis-only plan for GQ2-CHUNK-0369. Admitted GQF-0298/GQR-0284 NEW, P3/high36 (12/0/4/10/10). Canonical admission audited 2026-10-11; no implementation or runtime reproduction.

## Source evidence and ownership

Combined builder supplies playApplicationId=com.dxxrevival.app with github/legacy false. Gradle derives applicationId from that property; AppBuildDetails labels it Play Store. Registered test_distribution_build_info.ps1 inspects APK badging but package switch20-26 accepts only old Play/default diagnostic and GitHub/legacy IDs. Explicit -Apk pointing at the maintained Play Debug artifact fails Unexpected application ID before installation or About/log assertions. This omission also occurs at frozen boundary. Current fixture delta adds install -t, unrelated to package classification.

Combined package identity fixture already recognizes new Play AAB and separate GitHub APK. test_android_distributions builds default-property old Play controls; these do not validate this producer's overridden Play identity. Mechanical canonical definition census found no matching admitted distribution/About fixture root; final admission remains pending.

Keep BR0556 generated BuildInfo/immutable stamp acceptance separate: early package rejection precedes metadata assertions. Preserve BR0548 historical exact upload protection and GQR0282 installer reset ownership. No product application ID change is proposed.

## Implementation and future acceptance

- Recognize maintained com.dxxrevival.app in test_distribution_build_info.ps1 while preserving default Play, diagnostic and GitHub/legacy controls
- Keep strict unknown-package rejection. Use an explicit supported profile if broader configured identities are needed; do not guess distribution from arbitrary suffixes
- Extend existing distribution matrix or focused host driver to invoke the actual fixture entry with controlled aapt2/ADB dependencies and a command ledger. New Play must pass the switch and reach the selected package's validation; source-string presence is insufficient
- Preserve package/version, minimum/target SDK, legacy safeguard, About text and log-header assertions. Keep all device operations bound to inspected APK package and selected serial
- Cover maintained Play, old default Play, supported diagnostic, GitHub/legacy and unknown/malformed identities. Later selected-device acceptance should inspect About/log metadata for real built Play and paired direct APK artifacts
- Reuse maintained runners; register any new top-level fixture and run relevant scoped quality/catalog checks after implementation. No build/test/device execution in this tranche

## Gates

- [x] Frozen/current witness and producer/label/registration context prepared
- [x] Canonical admission and final chunk publication
- [ ] Implement supported package validation
- [ ] Run actual-entry and relevant distribution acceptance

Narrow authored-fixture correction, inherited saving0. Avoid a generic policy framework for one missing supported identity.


## Admission checkpoint, 2026-10-11

GQR0284 remains TODO. Chunk0369 static publication audit PASS5155 SHAaf3309f9cc9092434887734f220ab124ff802037156801ab76074f75f81e0f22. Snapshot binds earlier proposal bytes before admission/gate edits. No runtime acceptance or product implementation.
