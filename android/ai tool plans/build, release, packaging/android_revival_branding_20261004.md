# Android DXX-Revival branding

- Change Android launcher variants, setup/About, notifications, help text and
  exported config/download names to DXX-Revival
- Keep Redux package IDs, namespaces, JNI/native targets, storage paths,
  network discovery/protocol identifiers and external download URLs
- Guard native game display names with __ANDROID__ so desktop names stay Redux
- Update future Android release/CI artifact branding while preserving internal
  saved release filenames and old public asset cleanup
- Update Android privacy/documentation and existing integration expectations
- Run scoped mixed-language quality, release integration scenarios, and build
  and inspect Android distributions with unchanged IDs/libraries and new labels
- Commit only this Android branding work; leave current published APKs intact
