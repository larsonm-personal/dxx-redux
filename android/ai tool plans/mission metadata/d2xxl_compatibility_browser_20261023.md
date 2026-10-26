# D2X-XL compatibility catalog and browser

## Scope

Add an initial maintained extension catalog and a metadata-browser compatibility
button/table. Preserve the existing first-mission work and its unresolved gates.
All XL parsing, classification, catalog and UI implementation stays under
`android/XL_compat/`; app build and browser files receive integration calls only.
No changes to `d1/` or `d2/`

## Status contract

- Green: all required features have verified production support
- Orange: load and gameplay are verified, with known presentation differences
- Red: a detected requirement prevents production loading or gameplay
- Gray: evidence is incomplete; never silently promote unknowns to green/orange
- Developer translation, parsing and assisted route tests do not grant support
- Mission results aggregate all inspected levels; partial scans remain unknown
- Detection uses native format evidence, not mission titles or download origin

## Initial categories

Container/descriptor dialect, level/game serialization, geometry/capacity limits,
segment functions/ownership, walls, geometry triggers, object triggers, objects/AI,
custom gameplay assets, baked colored lighting, dynamic lighting/render modes,
textures/models, mission flow and runtime verification. These are starting
categories, not a claim to enumerate every historical XL extension

## Work

- [x] Native catalog and evidence report with rejection/unknown tests
- [x] Isolated JNI bridge and Kotlin presentation models
- [x] Browser button and per-source extension table
- [ ] Mission and individual-level evidence coverage
- [ ] Host, JVM and Android build checks; rendered UI verification
- [ ] Update catalog documentation and first-pilot status

## Initial implementation checkpoint

The 14-category catalog is in `android/XL_compat/EXTENSIONS.md` and the executable
evidence catalog in `compatibility.cpp`. Native detection covers extended HOG,
extended level headers and `d2x-name`; detailed evidence uses the existing 15/33
reader. It never grants production playability

File details now display an isolated Compose compatibility button/table. A second
integration covers the main Level metadata dialog, inspecting its explicit source
files and archive entries off the UI thread. Emulator introspection confirmed the
pilot's red cannot-load button. Opening the dialog also exposed the existing stock
analysis worker crashing on this unsupported source. A preflight guard now skips
engine/movie analysis for known blockers. The guarded APK build and all ten JVM
metadata tests passed; that APK is installed. Final table interaction and rendered
verification are pending because emulator setup introspection is responding slowly

The follow-up identified an Android system ANR overlay, rather than a completed
table interaction. Emulator-5554 was rebooted and remains offline; emulator-5556
accepted the updated APK but its system launcher also reported an ANR and app
startup is still pending. Do not count either as successful rendered verification

Status colors now adapt to the Material surface: lighter red/orange/green on dark
surfaces, darker equivalents on light surfaces, and the theme's secondary text
color for unknown/not-required. Explicit status text is retained. Scoped formatting
and the x86_64 APK build passed for this change

Follow-up logs on emulator-5556 showed background `levelmeta_d2` workers attempting
the imported pilot's unsupported levels sequentially. This does not establish the
cause of the system ANRs, but the dialog-only guard did not protect that path.
`LevelMetadataAnalyzer.analyze` now calls an isolated XL preflight before cache or
worker work. Known blockers return a failed result with per-source diagnostics;
they are never admitted to the stock worker. Formatting, all 70 selected metadata
JVM tests and the x86_64 APK build pass. These JVM tests cover existing metadata
behavior; they do not exercise the Android JNI preflight. Device verification of
the shared guard and the rendered compatibility table remains required

Device follow-up: emulator-5556 logs at 16:47-16:48 show the imported pilot's
background jobs for levels 9-18 returning `unsupported_format` with the explicit
XL compatibility message. This exercises the Android JNI preflight in the actual
background path. It does not verify every level or the dialog interaction. A
system ANR overlay still blocks the latter; the renderer table remains unverified

Host synthetic tests pass. The JNI library builds with the Android NDK, and native
tests pass on emulator-5554. Kotlin compilation and an x86_64 APK build passed for
the first file-detail integration. The APK is installed and the private pilot ZIP
was imported as `xl-browser-pilot.zip` on that emulator. No base engine source was
changed. All ten GameFileMetadata JVM regression tests passed, and the combined
test/APK task completed successfully with Gradle's problems-report generation
disabled after a report-file collision. Scoped formatting and the native inventory
contract test also pass. The table visual check remains open; the first pilot's
gameplay/presentation gates also remain open

## Survey-driven refinement (2026-10-23)

The catalog now has 19 rows, retaining the original 14 IDs and adding `navigation`,
`generators`, `wall_materials`, `effect_objects` and `resource_scope`. Existing rows
now distinguish stateful events, segment physics and actual capacity/reference
limits. These are separate requirements, not newly implemented engine support

The source of this refinement and its qualifications are preserved in
[the evidence audit](../../XL_compat/evidence/README.md). Never use the corpus's
mission names or global frequency counts as per-source browser evidence. Native
15/33 inspection remains the detailed coverage boundary. Nonzero wall cloak
values are now reported as a specific material investigation; status stays unknown
until visibility/collision/activation semantics are verified

Keep loading, gameplay and fidelity separate in the evidence model. Any future
qualified result needs source hash, engine build, runtime profile, tested modes,
known losses and unresolved requirements. Gray unknown cannot collapse into
not-required. Orange cannot hide navigation, targeting, cue, generator or event
loss behind a 'presentation' label. Save/load, replay, network consistency and
mission-transition resource cleanup apply to every relevant row

Follow-up UI gate: verify all 19 rows, readable long support text, scrolling,
per-source selection and unknown versus absent distinction on-device. This update
does not resolve the previously pending rendered table check or certify a new APK
