# Vertigo content and co-op distribution

- [x] Remove Vertigo from the base D2 inventory and remove the earlier catalog exception
- [x] Keep managed Vertigo discoverable in co-op and label it as an official expansion
- [x] Derive download eligibility independently from content placement, and enforce it at host startup
- [x] Cover proprietary wrappers, renamed HOGs, mixed packs, managed loose missions, and ordinary downloadable missions
- [x] Run scoped quality checks and affected test suites

Implementation: removed d2x.hog from D2_FILES, so normal catalog ownership keeps the pair together without a filename exception. Managed enabled descriptors now participate in the co-op picker. A separate download policy inspects all wrapper constituents and HOG/DXA entry names, rejects Vertigo and unverified nested archives, and is checked independently at host startup and before serving each authorized transfer. The native metadata mount fix remains necessary and unchanged.

Validation: 61 tests passed across the targeted content, co-op, and game-file suites; scoped quality checks passed. No APK deployment or on-device validation in this change.
