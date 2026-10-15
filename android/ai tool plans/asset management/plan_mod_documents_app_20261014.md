# Mod document browser implementation

Completed 2026-10-14

## Implementation

- Shared PackageDocuments discovery, ordering, bounded text and DOCX reading for mission archives and DXA mods
- Markdown and extensionless README files now appear alongside TXT, DOCX, PDF, RTF and DOC files
- Installed mod/level metadata offers View readme for one document and View docs for multiple documents
- Shared document selection retains the original paths and identifies README.dxx-redux.md as Redux adaptation notes
- Inline viewers return to the still-composed selection dialog; external viewers use the existing FileProvider grants
- Recommended rows offer View docs independently of download/installed state and always show published documentation
- RecommendedModDocuments derives <download-url>.readme.zip, downloads on demand off the UI thread, validates before replacing the cache, and re-fetches each new docs flow
- Validated saved copies remain usable after network or bundle errors, with a visible warning and Retry
- Cache identity includes the full canonical URL; bundle validation bounds download/expansion/count and rejects unsafe/duplicate paths, non-documents and damaged entries
- Existing mission extraction ownership remains unchanged; installed documents work offline

## Verification

- Scoped code quality checks passed for all changed Kotlin and PowerShell sources
- 88 JVM tests passed: PackageDocumentsTest, MissionZipTest, MissionDescriptorEncodingTest, ModManagerMissionZipTest and FileProviderGrantStoreTest
- assembleDebug and assembleDebugAndroidTest passed; both game libraries built for all configured ABIs
- test_package_documents.ps1 -Serial emulator-5554 -Published passed
- Emulator integration covered metadata with zero/one/multiple docs, original and Redux text, Back/Close navigation, original RTF bytes and read-granted external handoff, recommended docs before/after install, failure/retry and saved-copy notices
- The Android downloader fetched and validated all nine actual recommended companions from the public GitHub release, including every inline document
- Both automation catalog validators passed; test_package_documents is registered as an instrumentation owner

## Reproduction and artifacts

- Run android/tests/test_package_documents.ps1 for deterministic UI integration; add -Published to also verify the live release
- Build/tests: android/temp/mod-documents-final-build.log
- UI integration: android/temp/mod-documents-integration.log
- Device evidence including all nine published inventories: android/temp/package_documents_device.log
- Formatting: android/temp/mod-documents-format-final.log and mod-documents-mission-test-format.log
- Debug APK: android/app/build/outputs/apk/debug/app-debug.apk

The Gradle verification used -Pkotlin.internal.collectFUSMetrics=false for this invocation to avoid a local Kotlin build-statistics scan delay. No build configuration was changed for that workaround. The full native rebuild reported existing warnings in unchanged engine sources.
