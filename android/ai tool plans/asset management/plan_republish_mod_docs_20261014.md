# Republish mod documentation

2026-10-14

- Run documentation packaging fixture checks
- Rebuild and publish the existing texture-packs-v1 assets with the DXX-Revival headings and required base-file checksum lists
- Verify uploaded asset hashes and download the doc companions to confirm the published contents
- Refresh recommended mod download sizes from the new manifest

Status: completed

- Six packaging fixture tests passed
- Updated 11 adapted DXAs, their 11 companions, and three metadata/checksum indexes (25 changed assets)
- Two unchanged upstream DXAs and their companions retained byte-for-byte
- Publisher verified sizes and SHA-256 digests for all 29 GitHub assets and the release notes
- Downloaded all 13 companions and verified all 17 documents; all 11 adaptation notes match current source bytes
- Checked required/optional checksum tables against each DXA manifest and the updated DXX-Revival headings in both pack and companion
- Updated nine recommended download-size constants from mod-packs.json; scoped Kotlin formatting passed

Release: https://github.com/larsonm-personal/dxx-redux/releases/tag/texture-packs-v1
Evidence: android/temp/mod-docs-republish-20261014/{fixture-tests.log,publish.log,download-verification.json,documentation-preservation.json,format.log}

This republishes mod assets and docs; no APK release was created.
