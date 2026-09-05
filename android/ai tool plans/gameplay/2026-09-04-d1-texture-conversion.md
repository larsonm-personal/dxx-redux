# D1-in-D2 texture conversion warnings

- [x] Trace unknown texture 1056 to level data and converter behavior
- [x] Repair conversion or scope unavoidable warning suppression to simulation
- [x] Build and run focused regression coverage

## Findings

- The D2 converter has no mapping for D1 texture 1056; native D1 defines an 800-entry texture table
- Read the original compiled version-1 level data independently: Alexander contains four raw 1056 references (segments 8, 9, 18, 27), Abyss contains two (342, 343); no other out-of-mapping-range texture IDs were found in these two levels
- These values are authored/nonstandard input references, not introduced by D1-in-D2 conversion; no replacement texture can be established from the conversion table
- Headless entry points installed msgbox_warning/msgbox_error, which resolve to blocking Windows MessageBox calls; repeated warnings stalled unattended workers and opened desktop dialogs

## Repair and verification

- Shared headless-only diagnostic handlers send warnings/errors to stderr, flushed immediately, never to OS dialogs
- Repeated identical unknown-D1-texture warnings are emitted once per process; distinct texture warnings and all unrelated warning/error messages remain logged
- Applied to GuideBot, metadata, and replay headless entry points; normal interactive engine diagnostics and texture mapping are unchanged
- Alexander headless run emitted exactly one 1056 warning, collected the blue key, and ended at the deliberately short 10-second simulation budget without waiting for any UI
- First Strike D1-in-D2 level 1 still confirms successfully through the normal regression runner
- New test_headless_diagnostics ran as written and checks deduplication, distinct texture IDs, repeated unrelated warnings, and errors
- Windows D1 and D2 builds passed; all 46 D2 CTest tests and scoped quality passed; build warnings were existing weapon.c return-path warnings
- No checked-in mission/simulation data was regenerated
