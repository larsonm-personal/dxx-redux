# Owned XL flicker timing and persistence

## Behavior and source evidence

XL uses a separate owned variable-light table. Private runtime translation
removes the stock shadow; standalone classic probes still reject more than 100
rows. Ordinary frame processing rotates fixed-width 32-bit patterns right,
consumes elapsed intervals but advances the pattern only once per frame, and
disables invisible, non-emitting or zero-delay entries. Authored off/on actions
select the first matching row and preserve its earned mask. Owned LGT entries,
including zeros, take precedence over default XL brightness. Redux retains its
left rotation and stock storage.

The pinned reference is 2576a98b5de310e3ac7c9777df2677d1627b8bae.
`tests/reference_flicker.cmake` extracts the original FlickerLights,
FindVariableLight, EnableVariableLight and DisableVariableLight functions.
The inherited source verification pins lighting/light.cpp to
ca95b9183729c7f3665ef0706206afce093b53b0d97aa6ba11379dd5fdf5f36f.
MSVC and strict GCC with UBSan match 5432 production phase/action samples and
first-duplicate controls. A localized GCC diagnostic exemption applies only to
the original source's indentation, not to production code.

The port deliberately replaces source signed-overflow arithmetic and potentially
unbounded catch-up loops with 64-bit bounded arithmetic. Another 168 controls
exercise extreme clocks and elapsed intervals. Negative authored delays reject
with a specific admission error because the reference can hang; zero delays
retain its disable behavior. These controls do not claim parity with undefined
overflow or optional epilepsy-friendly/alternate-renderer settings.

## Runtime and lifecycle acceptance

Public layout controls cover versions 15, 17 and 23 with 0, 1, 100, 101 and 257
entries, exact masks/clocks and private-header rebasing. The production fixture
retains Neptune geometry and resources and supplies 130 rows, including duplicate
sources, an independent high-index source and a zero-delay entry. Native tests
exercise ordinary frame hooks, actual light-consumer changes, authored off/on,
visibility loss, restart, zero-valued owned LGT, negative-delay rejection and
recovery. The stock control checks its original rotation direction.

Development schema 14 replaces 13 and saves every source identity, delay, timer
and unsigned mask. The source hash includes all authored flicker rows. Restore
validates counts, types, widths, identities, reachable mask rotations and timer
bounds before applying any state. Twelve corrupt payloads reject without changing
live phase or brightness; a subsequent valid payload recovers. Disk restoration
through stock preserves exact phase and earned UVL/ambient values, and subsequent
controlled frames reproduce the uninterrupted result.

All 74 configured XL tests pass in 290.82 seconds. Stock D2 passes in 42.32
seconds and stock D1 in 1.82 seconds. All twelve private-suite policy tests pass;
the required core suite now includes this integration. Both automation catalogs
pass with 416 top-level entries, 472 support scripts, 99 standalone JSON tests
and 303 standalone PowerShell tests. Scoped quality and pinned PowerShell
analysis pass.

The registered `test_xl_flicker_runtime.ps1` Android runner passes 50/50 steps in
67933 ms, run 823fb60a483d4d518b53e7202db6afba. It checks actual frame dispatch,
authored controls, deliberate phase disturbance, three memory restore cycles,
disk restoration through Counterstrike and repeated restoration. Protected
player/configuration/mission file hashes match after cleanup.

The first Android attempt passed the immediate checkpoint comparison but failed
a later comparison after an ordinary frame advanced the restored clocks. The
automation-only `roundtrip_paused` option now pauses at the restore boundary.
Exact comparisons remain in place before simulation; production timing was not
changed to accommodate the test. Native continuation controls verify progression
after restore. This is controlled behavior evidence, not autonomous navigation
or a campaign playthrough.

- Original Neptune HOG: 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303
- Derived flicker.hog: a33c847e99e9ee4a511b8f272ccdd84f568311de1c8b813dd6019e4114b2c8ac
- Derived flicker.mn2: dbf359ac93ecaeef6234f242b9fb9829c078b6035929f377d87091fd1b16d4b3
- Android APK: bd9f552fe07dbd3320ff775eb9483aa256710215035b00c99dcc99017c82a3dc

## Original missions and remaining work

[flicker-originals.json](flicker-originals.json) records fresh production reader
and admission inspection of the unchanged private survey. All 131 member hashes
match; 103 decode and 23 contain flickers. The maximum is 74 in wfactory.rl2.
No decoded flicker-bearing member has a zero or negative delay. The other 28
members retain explicit container or reader exclusions; their flicker behavior
is not inferred. Per-level independent runtime blockers remain recorded, and
none receives original-mission gameplay qualification from this audit.

The ordinary profile has epilepsy-friendly mode off. Other settings, full
renderer/light-manager behavior, complete changing-source visual comparisons,
remaining resource dependencies and original campaign acceptance remain work
toward the full-fidelity goal. The existing 8 MiB save trailer bound remains.
