## In-game Load Save Preview Highlight

Goal: restore in-game load/save screenshot previews so the highlighted save slot draws its thumbnail, including d-pad navigation and Android long-press cursor movement that only changes highlight.

- [x] Trace save preview draw and selection state in D1 and D2
- [x] Trace Android menu highlight input path for long-press/cursor movement
- [x] Apply the smallest shared-style fix in both game trees
- [x] Run focused validation or the nearest available build/test check

Validation:
- Scoped `android\run-code-quality.ps1 -Fix -Paths .\d1\main\state.c` passed; `d1/d2` are excluded from clang-format by repo policy
- Scoped `android\run-code-quality.ps1 -Fix -Paths .\d2\main\state.c` passed; `d1/d2` are excluded from clang-format by repo policy
- `run-windows-build.ps1 -Target both` passed for D1 and D2
- `ctest.exe --test-dir .\buildd1 --output-on-failure` and `ctest.exe --test-dir .\buildd2 --output-on-failure` found no tests
- Full unscoped code-quality check was not completed because it timed out and reported unrelated Kotlin lint issues in the pre-existing dirty worktree
