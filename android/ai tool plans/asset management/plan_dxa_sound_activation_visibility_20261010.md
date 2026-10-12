# DXA sound activation and usage visibility

## Objective

Make applicable sound and Mac presentation packs effective in native D1, D2, and D1-in-D2, and distinguish enabled archives from assets actually consumed

## Work

- [x] Apply named PCM replacements after base sound-bank loading, respecting archive priority, source rate, ownership, cache invalidation, and mission resets
- [x] Handle D1-in-D2 sound-bank publication and record the legacy Mac sound-pack loader
- [x] Record successfully loaded sound/presentation provenance and actual sound playback; expose evidence through introspection and a user-visible diagnostic surface
- [x] Correct misleading recommended-pack descriptions and support both existing 22 kHz and converted 11 kHz D1 archives
- [x] Add reusable enabled/disabled integration coverage for both engines, D1-in-D2, and applicable Mac assets
- [x] Run scoped formatting, relevant builds, catalog checks, and integration tests

## Initial evidence

- Native bank loaders read PIG/S11/S22 bytes without consulting named PCM replacements
- Local D1 converted archive contains 113 .r22 entries although the corrected converter emits .raw at 11025 Hz
- Mac sound archive contains 98 numbered RAW files and a sound mapping for Mac D1 data
- Mac presentation archive contains 22 PCX images and five fonts, not mine textures
- Existing sound trace already fingerprints resident and mixer bytes; existing mod test asserts only texture use
- Preserve unrelated worktree edits, including existing changes in game_introspect.cpp and SetupActivity.kt

## Verification contract

Use fresh engine launches with each pack disabled, enabled, then disabled again. Require matching archive PCM fingerprints in resident samples and actual mixer input, correct playback rate, and restoration of baseline. Check presentation provenance after successful decode and font selection. Exercise mission transitions and D1-in-D2 separately. A mounted archive alone is never proof of use

## Implementation and validation

- Shared Android loader overlays named `.raw` / `.r22` PCM after bank publication, with PhysFS archive priority and explicit sample rates
- D1-only packs are mounted under `d1-mods` in the D2 engine; D1-in-D2 sound and presentation consumers consult that namespace
- Authored D1 mission sample replacements retain precedence; their original rate is restored with their original data, and mixer caches are invalidated at replacement boundaries
- Mod Details displays archive-specific sound load/play and image/font load counts from the last run of each engine; native introspection contains filenames, source archives, rates, lengths, resident fingerprints and playback status
- Existing D1 `.r22` pack is now supported directly, so rebuilding it at lower sample rate is unnecessary. Integration also exercises the Mac pack's `.raw` format
- Mac sound pack remains specific to Mac D1 data. Mac D1 data support in the D2 engine is outside this change; PC D1 content and the D1 presentation pack are supported there
- Build and 9 focused JVM tests passed (`temp/dxa-final-build.log`)
- Both automation catalog checks passed (`temp/dxa-catalog.log`, `temp/dxa-master-catalog.log`)
- Scoped mixed-language formatting passed (`temp/dxa-final-lint.log`)
- Native D1 passed off/on/off, archive-byte and mixer-cache verification: 98 matching 22 kHz replacement samples; five replacement fonts and 640-pixel presentation images; baseline playback restored after disabling
- Native D2 passed the same verification with 183 matching 22 kHz samples and no D1 presentation leakage; both engines also passed playback verification after mission re-entry
- Follow-up native packaging passed after preserving authored mission presentation precedence (`temp/dxa-presentation-build.log`)
- D1-in-D2 passed off/on/off, with enabled/restored runs on the final rebuilt APK: 98 matching D1 samples, actual changed playback after mission re-entry, five replacement fonts and five presentation images; its persisted launcher report exactly matched the final engine snapshot (`temp/dxa-remaining-run.log`)
- Mac D1 passed off/on/off with the real Mac demo files: 98 matching 11 kHz RAW samples and three distinct effects played; disabling restores the missing-sounds/silent baseline (`temp/dxa-mac-run.log`)
- All four modes passed the final payload/mixer verifier. Evidence includes 12 scenario snapshots and logs under `temp/dxa-asset-usage`, with the combined result in `verification.txt`
- All five final persisted reports (D1-in-D2 enabled/restored and all three Mac phases) exactly matched their engine snapshots
- Validation used emulator-5586. The temporary 1280x720 display override was restored to its original 2340x1080 setting, and the test app was stopped
