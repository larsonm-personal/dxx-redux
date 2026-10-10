# GQR-0267: bind Mac SAF fixture URI authority to selected package, 2026-10-10

Diagnosis and future implementation only. Proposed GQF-0281 P2/high, test-gap/package-identity, score45MEDIUM23/0/2/10/10. Expected inherited reduction zero.

## Concrete problem and boundary

Debug manifest authority is ${applicationId}.saf-test and provider is exportedfalse. test_helpers accepts DXX_TEST_PACKAGE; Mac fixture stages CUE/BIN through selectedPACKAGE run-as and sends package-scoped import_picked_uris broadcast. Invoke-SafMode instead constructs both content URIs with literal com.dxxredux.app.saf-test. Under nondefault selected debug package, those URIs name a different provider or no provider. Launcher queries supplied URI without remapping; display-name query catches failure and returnsnull, so correct selected files do not reach the CUE/BIN dialog. Source-supported mismatch, no device reproduction. Defaultpackage mode remains supported; manifest placeholder must be retained for co-installed variants.

- [ ] Derive test provider authority and both CUE/BIN URIs from the same verified selectedPACKAGE used for staging, launcher and cleanup
- [ ] Preserve seekable/pipe paths, pinned source hashes and actual staging/7-file extraction assertions
- [ ] Add focused authority/package routing controls through actual fixture path, including default/nondefault and co-installed-default sentinels
- [ ] Verify selected manifest provider identity before extraction result acceptance; fail clearly if matching provider is missing

Keep change in authored test runner/common target mapping if already appropriate; do not edit production import routing to rewrite arbitrary provider URIs, export debugprovider, or restore fixedauthority in manifest. A tiny URI construction change is preferred to a new general URI abstraction. Coordinate artifact build/install identity with BR0544: direct Mac runner assembles generic debug and discards install response, so -SkipBuild and a verified matching nondefaultAPK need explicit acceptance. That existing root is not duplicated here. Keep BR0543 ownedtarget/set cleanup and BR0658 complete deadline separate. Preserve GQF0220/GQR0205 pipe endpoint/error owner and GQF0153/GQR0140 complete output-byte oracle.

## Required acceptance evidence

Default and nondefault matching debug packages, both seekable andpipe modes, correct URI authority and exact source files reach real production importer. Missing provider fails with responsible target; co-installed defaultprovider cannot be consulted when another package selected. Wrong/stale default fixture files act as negativecontrol. No accidental data mutation in unrelatedpackage. Required current-source install/payload evidence and exact output corpus belong existingartifact/oracle owners; don't call presence-only oracle fullbyte proof. No runtime/test/build/device work authorized by this plan in diagnosis tranche.
