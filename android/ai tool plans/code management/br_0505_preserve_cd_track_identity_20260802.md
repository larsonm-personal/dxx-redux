# BR-0505 preserve CD track identity

## Plan

- [x] Read the adversarial review process and BR-0505 ledger entry
- [x] Trace unnamed CD track identity through generation and consumption
- [x] Implement the narrow identity-preservation fix
- [x] Run focused tests, build, and scoped code quality
- [x] Update the review ledger with evidence and record the result

## Result

Audio sources now persist the ordered physical CUE track numbers for audio tracks. Local, SAF, and GOG source registration populate that mapping from parsed CUE identity. The preview row builder looks up each optional name by physical track number while retaining a separate 1-based audio ordinal for native playback. Sources without an identity mapping use fallback labels without compacting sparse names.

Validation passed the focused sparse-name row mapping, audio-source persistence, and GOG registration tests; Android debug assembly for all configured ABIs; scoped Kotlin quality checks; and `git diff --check`.
