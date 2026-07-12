# Android fatal bridge extraction plan

## Goal

Centralize duplicated fatal-file writing and Assert/Int3 breadcrumb macros,
while resolving the D1/D2 fatal-exit drift in favor of the robust logged
finish-plus-`_exit` behavior.

## Validation

- Keep desktop error behavior unchanged.
- Build desktop and Android targets.
- Verify a deliberate debug fatal writes `crash_error_<pid>.txt`, logs the
  fatal message, requests Activity finish, and cannot return.
