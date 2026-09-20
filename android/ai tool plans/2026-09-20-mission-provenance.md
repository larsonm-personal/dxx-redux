# Mission provenance

- Inspect representative archive metadata, author declarations, and date distributions
- Add compact native provenance with attributed credits and conservative vintage estimates
- Preserve original archive date evidence in Android and host metadata requests
- Pass provenance through regression projection and expose a launcher details dialog
- Verify native D1/D2 builds, relevant Kotlin tests, and archive-to-report integration
- Regenerate representative regression metadata and document limitations

Existing flyout work and modified regression files predate this task and must be preserved

## Outcome

- Surveyed 106 ZIPs and added attributed author/contact/editor/version/date declarations
- Shared native HOG catalog filtering reads provenance independently of mount precedence
- ZIP/7z and native RAR adapters capture original dates; unavailable evidence stays unknown
- Added the provenance projection and scrollable/selectable launcher details dialog
- Preserved dates through archive staging, persistent extraction, and individual-file targets
- Refreshed Rogue, Chasm, Chronolos, Vignettes, Castaway Redux, and KCXF2RMv11 reports
- D1/D2 Windows builds, Android APK builds, targeted JVM tests, and HOG catalog CTest passed
- Native integration passed six cases, including conflicting/invalid/absent dates and a shadowed descriptor
- Android import/metadata tests passed on emulator-5556 for Rogue and Chronolos; complete provenance matches host output

JVM fixtures are generated in test scratch directories; native archive integration uses local mission ZIPs
