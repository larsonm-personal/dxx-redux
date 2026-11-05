# XL delta-row validity

- Preserve raw rows, ordering, indices, ranges and identity; retain structural bounds
- Match pinned per-row validity and guard runtime/getter consumers
- Qualify interleaved rows, overlaps, flicker, source identity and saved light state
- Reproduce unchanged Dinter and report actual load/120-tick outcome or next blocker
- Validate native/reference/classic regressions before a separate draft targeting #376
- Leave existing PR heads and stack untouched; no merges
