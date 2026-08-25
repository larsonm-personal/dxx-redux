# Release assertion game log

## Goal

Record failed non-debug Android assertions in the launcher-exportable Game Logs category, then continue execution.

## Constraints

- Debug and internal native builds must retain breadcrumb creation followed by the normal assertion stop.
- Non-debug builds must clearly state that the failed assertion is being logged and execution is continuing.
- Use the existing Game Logs category controlled from the Advanced tab.
- Keep all changes in Android-specific integration code or narrow release-compile guards.

## Plan

- [x] Route failed non-debug assertions to `DLOG_GAME` and document continue behavior.
- [x] Audit release compilation for assertions that reference debug-only locals.
- [x] Build Android debug and release variants.
- [x] Run scoped code quality and diff validation.

## Validation

- Android release assembled successfully for all configured ABIs.
- Android debug assembled successfully for all configured ABIs.
- The sole release compile conflict was D2's debug-only `pm` declaration; the declaration is now available to the assertion in both configurations.
- Scoped code quality and `git diff --check` passed.
