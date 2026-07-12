# D1/D2 texmerge owner diagnostics extraction plan

## Goal

Remove the duplicated Android-only merged-texture owner state and event logging from the upstream-original `d1/main/texmerge.c` and `d2/main/texmerge.c` files while preserving their cache behavior and diagnostic output.

## Baseline

- `d1/main/texmerge.c`: 133 additions, 1 deletion versus `upstream/main`
- `d2/main/texmerge.c`: 133 additions, 1 deletion versus `upstream/main`
- Each file carries the same ten owner fields and roughly 80 lines of reset, update, filter, and log helpers
- The existing shared `merged_wall_debug.c/.h` module already owns the draw-face context, merged-wall frame state, bitmap naming, and texture logging dependencies

## Work

- [x] Define a compact shared texmerge-owner record in `merged_wall_debug.h`
- [x] Move owner reset, owner update, and diagnostic event logging into `merged_wall_debug.c`
- [x] Replace the ten Android-only cache fields in both game files with one shared record
- [x] Keep only small calls at cache initialization, flush, reuse, and creation sites
- [x] Preserve event severity, target filtering, field values, and the D1/D2-specific flush tag
- [x] Run scoped code quality on the shared module and both game files
- [x] Build all Android ABIs
- [x] Build Windows D1 and D2
- [x] Record the measured upstream-diff reduction and update the candidate catalog

## Risk controls

- Do not alter cache selection, bitmap creation, merge orientation, or cache timing
- Keep the owner state embedded per cache entry so cache lifetime and slot association remain unchanged
- Pass bitmap and tmap values explicitly to the shared logger rather than adding new global state
- Keep all new declarations and calls inside `#ifdef ANDROID`

## Completed result

- `d1/main/texmerge.c`: 133 additions to 38 additions versus `upstream/main`
- `d2/main/texmerge.c`: 133 additions to 38 additions versus `upstream/main`
- Exact inherited-file reduction: 190 additions
- Shared implementation growth: 90 lines across `merged_wall_debug.c/.h`
- The two cached tmap fields were removed because they were assigned only during creation and never read; event calls still pass the same tmap values directly
- Scoped code quality passed
- Android `externalNativeBuildDebug` passed for arm64-v8a, armeabi-v7a, and x86_64
- Windows D1 built and linked; Windows D2 built and linked after a stale locked generated metadata executable was renamed within `buildd2`
- `git diff --check` passed
