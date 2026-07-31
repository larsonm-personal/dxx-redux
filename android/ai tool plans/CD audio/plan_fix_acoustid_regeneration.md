# Fix AcoustID regeneration regressions

## Goal

Make the full fingerprint regeneration pipeline complete successfully while
keeping AcoustID labels only when they agree with maintained source metadata.

## Work

- [x] Treat optional legacy mission and tracklist properties as optional under
      strict mode
- [x] Add focused mission metadata compatibility regression coverage
- [x] Replace exact title equality with conservative maintained-title matching
- [x] Cover accepted mix/level qualifiers and rejected unrelated titles
- [x] Run scoped code quality checks and regression tests
- [x] Rerun the affected regeneration stages and review the resulting diff

## Verification

- [x] Mission soundtrack regeneration completes without schema-property errors
- [x] Legitimate qualified titles retain reviewed AcoustID evidence
- [x] Unrelated labels such as `Torche - Vampyro` remain rejected
- [x] Album merge completes and produces structurally valid deterministic output

## Anniversary ISO follow-up

- [x] Add a single-data-track cue sheet for the data-only Anniversary ISO
- [x] Verify the disc fingerprint stage accepts the image
