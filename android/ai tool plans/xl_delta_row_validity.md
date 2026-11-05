# XL delta-row validity

- Preserve raw rows, ordering, indices, ranges and identity; retain structural bounds
- Match pinned per-row validity and guard runtime/getter consumers
- Qualify interleaved rows, overlaps, flicker, source identity and saved light state
- Reproduce unchanged Dinter and report actual load/120-tick outcome or next blocker
- Validate native/reference/classic regressions before a separate draft targeting #376
- Leave existing PR heads and stack untouched; no merges

## Rebase onto merged fixes

- Back up published head 4c0df50bc before replay onto current cmake
- Preserve raw delta rows and all runtime guards; the v21 witness correction is already upstream
- Keep candidate FAST_TESTS mandatory at 113 while freezing distinct historical 111 and reviewed 113 controller cohorts
- Revalidate native, original Dinter, classic and controller/publication tests before an exact-lease push
- Do not merge or alter native stack 371
