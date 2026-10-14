# Public matcen production regression

## Scope and coverage boundary

Use real public D1 shareware level 3 and D2 demo level 2. Preserve existing mode,
station-transition, retail and Android save/restore tests. This fixture directly
invokes native station triggers, clears authored robots blocking the selected
production segment, and retires spawned robots to isolate the station loop. It
must not be presented as a normal player-route recording or imported parity.

## Implementation and validation

1. Require exact nonempty authored station identities and pinned public assets
2. Exercise every station at difficulties 2 and 4 in default, one-round, paused,
   and pause-during-morph cases
3. Verify multiple real attributed robots, full first-round policy equivalence,
   repeat-activation suppression, life/count rules and unaffected energy centers
4. Require byte-identical independent native reports without a display and a
   real omitted-activation negative for each engine
5. Retain strict report-schema tests, existing native tests and automation catalogs
6. Publish a focused draft after the Guide-Bot segment and monitor exact-head CI
