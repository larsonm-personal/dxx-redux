# FP Startup Hardening 2026-04-29

- [x] Add startup floating point mode setup and asserts in d1/main/inferno.c and d2/main/inferno.c
- [x] Remove the replay-only D2 floating point setup call and move it to general program start
- [x] Add deterministic floating point compiler options in d1/CMakeLists.txt and d2/CMakeLists.txt
- [x] Disable Android game-target IPO when deterministic floating point hardening is enabled
- [x] Validate with host builds for D1 and D2

Validation notes:
- `./run-windows-build.ps1 -Target d1` succeeded after replacing four legacy `fl2f(...)` static initializers with integer fixed-point expressions
- `./run-windows-build.ps1 -Target d2` succeeded after the same initializer cleanup in D2
- `./android/run-code-quality.ps1 --fix` completed with all checks passing
- `ctest --output-on-failure` in `buildd1` and `buildd2` reported that no tests are currently registered in those build trees