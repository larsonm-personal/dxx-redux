# True-color POG runtime and continuing XL support

Continue the authorized thirteen-hour run from merged cmake
1a3d9f92814ff003fd62557c97dba4a0006f58ef through
2026-10-29 18:38:02 UTC. Reuse the isolated build worktree on
xl_more_49_truecolor_pog; leave the main checkout and private originals unchanged.
Publish validated increments and inspect their CI; do not merge.

## First implementation

1. Recheck the recorded true-color defect against the merged runtime and pinned
   XL source. Derive dimensions, channel ordering, alpha/super-transparency,
   frame selection and replacement precedence from actual consumers.
2. Prepare bounded, owned RGBA POG resources transactionally. Preserve indexed
   replacements and Redux behavior. Resource inspection must validate the
   encoding actually installed; unsupported or malformed inputs need useful
   errors before live state changes.
3. Connect owned frames to existing rendering, animation and transparency
   consumers. Verify actual uploaded/output pixels and gameplay-facing coverage,
   not just a successful decoder or an installed buffer hash.
4. Exercise original Robotech Arena replacements and all original Pumo POG
   resources, with independent source/byte comparisons. Check mixed indexed/RGBA,
   malformed bounds, frame strips, replacement precedence, saves/rewind,
   graphics-context recovery and stock transitions. Do not qualify Pumo while
   its independent requirements remain unfinished.
5. Complete native, public and normal-Android acceptance, scoped formatting and
   relevant catalogs. Record exact evidence and remaining scope, then publish
   the reviewable increment. Continue named audio and dependent effect work in
   dependency order within the remaining run.

## CI side goal

Start the newly merged trusted xl-fast lane to establish actual hosted behavior
and timing. Inspect workflow/job ownership and measured slow tests before
changing selection. Prefer removing duplicate builds/parsing/setup or shortening
unnecessary waits while preserving assertions, independent source comparisons,
original-byte validation, Redux controls and failure reporting. Keep safety and
private-asset trust boundaries intact. Compare old/new results and timings for
each optimization; missing coverage must not become a successful skip.

Use controlled events and poses, not autonomous campaign traversal. The runtime,
resource fidelity and human campaign acceptance remain separate evidence claims.

## Progress

Single-frame RGBA application, production desktop rendering and gameplay pixel
consumers are implemented. All original Pumo/Arena resources have source and
native controls; normal Android passes 58 GPU/lifecycle steps. Runtime animation
and cockpit replacements retain explicit admission failures. The first complete
Linux selection passes all 100 required tests. Additional Windows native checks
found and reproduced a packed-C/C++ resource ABI mismatch and stale offsets on
palette-bank changes; both are corrected and the full Windows original controls
pass. The final Linux run passes all 100 required tests in 336.01 seconds
(339.368 seconds including original inspection); final normal Android passes all
58 steps in 85.624 seconds after those fixes. Both automation catalogs and the
nineteen private-owner unit tests pass, and scoped formatting is complete.

The trusted hosted baseline is green. A local catalog validation took 197 seconds
and repeatedly scans identical dependency text. Cache that dependency graph in a
separate CI increment and keep all assertions. Comparing all 86 owners already
proves exact closure-text equality; dependency traversal falls from 186.982 to
7.768 seconds across 567 files. Validate the complete catalog after applying it.
Named-audio runtime remains the next gameplay dependency after this increment.

## Catalog optimization

The separate catalog change caches each file's direct dependencies for the
duration of validation. It preserves the existing breadth-first traversal,
candidate order, case-insensitive literal-name matching and per-owner visited
set. Exact old/new output matches for all 86 owners across 567 files; the cache
contains 263 visited files. Traversal takes 186.982 seconds before and 7.768
seconds after. The complete updated Windows catalog passes in 24.125 seconds,
versus 197 seconds measured before this change. These are local measurements,
not promised hosted CI timings.

All 99 standalone JSON tests, 479 support scripts and 309 standalone PowerShell
tests remain represented. The master catalog also passes with the same 422
top-level entries and 479 support scripts. No test selection, assertion,
timeout, failure policy or private-asset boundary is changed.
