# Render GL_GEQUAL Build Fix 2026-07-08

## Goal
- Fix the Windows D1/D2 build break where `GL_GEQUAL` is undefined in `render.c`

## Steps
- [x] Inspect the failing render code and recent graphics cleanup context
- [x] Identify the cross-platform GL constant pattern used by this codebase
- [x] Patch D1 and D2 consistently with minimal source churn
- [x] Run scoped formatting and Windows build verification

## Notes
- Full Windows builds currently fail before linking on `GL_GEQUAL` in both D1 and D2 render code
- The July 7 cleanup commit removed `#include "ogl_init.h"` from both `render.c` files while deleting Android-only graphics diagnostics
- `ogl_init.h` is still needed under `OGL` so Windows render code sees `loadgl.h` and GL constants such as `GL_GEQUAL`
- Validation passed:
  - `.\android\run-code-quality.ps1 -Fix -Paths d1\main\render.c`
  - `.\android\run-code-quality.ps1 -Fix -Paths d2\main\render.c`
  - `.\run-windows-build.ps1 -Target both`
