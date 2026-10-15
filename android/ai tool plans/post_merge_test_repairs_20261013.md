# Post-merge test repairs

Baseline: merged commit 89921f3676e9da831cfaac53481f6669b3995801

- Update the automatic-level cheat test to inspect ordered v5 commands and active D1/D2 grant profiles, preserving exact repetition and inventory/scoring assertions
- Limit imported-D1 weapon exercises to weapons granted by that profile while keeping native D1/D2 coverage unchanged
- Replace the obsolete inline emulator fallback assertion with a PowerShell AST check of the shared target-selection call; preserve behavioral precedence and inheritance tests
- Preflight Linux descendant-supervision requirements before launching work; retain fail-closed exit125 behavior and full descendant cleanup
- Keep these test/harness repairs separate from publishing the merged gameplay stack and from fresh corpus qualification

## Target-selection verification

- Regression tooling contract tests: 9/9 pass
- Android target-selection behavior test: pass, including child inheritance and physical-target recovery using its mock harness
