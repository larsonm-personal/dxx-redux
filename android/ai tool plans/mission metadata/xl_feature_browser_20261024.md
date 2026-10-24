# Granular XL feature browser

Extend the existing native compatibility report and Android detail dialog, based on descriptor head 20109c141. Published d2xxl_compat remains 92aca5067. No engine admission or simulation changes.

Native inventory remains the sole source of counts. Show wall teleports, boosts, cameras, master triggers, water/lava/blocked/no-damage segments, colored walls, opacity records and colored vertices. Counts describe serialized records, not executable route edges. Object-attached action types are not counted as wall triggers.

Red means unsupported, orange experimental/partial, green supported, gray unknown or absent with distinct text. Experimental water and lighting pilots do not qualify arbitrary levels. Unknown versions, failed inspections, archives and descriptors must retain unknown counts. Successful inspection alone never admits gameplay. Existing broad resource and gameplay rows remain visible.

Validate synthetic native records, malformed/unsupported formats, original Observatory inventory, legacy JSON presentation and missing metadata. Keep inspection on the existing background metadata path; never execute levels during rendering. Format scoped changes and run native contracts and available Android checks. Publish on a separate draft child PR and monitor CI.

## Validation and limits

- Native contracts: 3/3 pass, including original Observatory package hash verification and UI counts: 26 no-damage segments, 0 wall teleports, 41 opacity records
- SDK-free metadata JVM suite: 24/24 pass, compiling the production report and label model; legacy reports keep absent counts unknown
- Scoped C++/Kotlin/Gradle/Markdown formatting passes
- Only level/game pairs 15/33 and 17/36 are inspected. Newer formats retain unknown counts. No broader parser or gameplay support is introduced
- No local emulator rendering qualification: `/dev/kvm` is absent. Compose presentation is source-reviewed; Android compilation remains a CI check
- A missing JNI library or malformed JSON still produces no compatibility report, never zero counts or a green claim. Empty report rows explicitly say inventory unavailable
- No mission binaries are committed; original resource provenance remains in the existing Observatory fixture and expansion plan

## Review corrections and actual UI qualification

Positive object-trigger declarations are not inspected record counts: the current reader does not decode their payload. The counted row stays unknown; the broader row explicitly labels declarations unverified. A malformed fixture declaring one object trigger with no payload must never display an inspected count. All four action counts explicitly say wall, since object actions are not decoded.

A fixed `xl-browser` manual campaign reuses the existing Android instrumentation and emulator workflow. Its synthetic report checks the actual production dialog's text and red/orange/green label pixels in light/dark themes. Only two bounded synthetic PNG files are retained. No mission files or engine launch are needed. This campaign does not publish nightly progress or change existing LAN selection.

Local automation catalog validation passes. The master catalog test encountered the container's unavailable `/proc/self/task/<pid>/children` process-supervisor interface; this is a host limitation, not a test pass. PowerShell lint package download returned HTTP403, so local PowerShell lint remains unavailable and must be checked in CI. Other scoped formatters pass. Full sweep-policy test results and actual render results are recorded with the final PR head.
