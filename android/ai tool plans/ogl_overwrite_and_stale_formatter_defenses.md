# D2 OGL Overwrite And Stale Formatter Defenses

## Goal

Confirm whether the unexpected `d2/arch/ogl/ogl.c` overwrite was useful, discard
it if it was stale, and add concrete defenses against long-running formatting
tasks that continue writing files after an AI/tool session has moved on.

## Work items

- [x] Inspect the current `d2/arch/ogl/ogl.c` diff against `HEAD`
- [x] Discard the accidental D2 overwrite and verify the content matches `HEAD`
- [x] Add a helper to list/kill stale file-mutating formatter tasks
- [x] Add a lock/guard so concurrent `run-code-quality.ps1 -Fix` runs fail fast
- [x] Document the new workflow in repo instructions

## Notes

- The `d2/arch/ogl/ogl.c` popup overwrite reintroduced stale recovery-era code
	and malformed content like a nested duplicate `ogl_loadbmtexture_f(...)`, so it
	was not a useful change
- The format scripts themselves are synchronous; the likely failure mode is a
	terminal command timing out at the tool layer while the underlying formatter
	keeps running and later writes files into a newer worktree state
- Added `android\stop-stale-formatters.ps1` to list or kill stale formatter
	process trees before another cleanup pass starts
- Added `android\temp\run-code-quality.lock.json` locking to
	`android\run-code-quality.ps1` so overlapping cleanup passes fail fast
	instead of silently racing and rewriting files later
- Validated the helper with a list-only run and validated the lock with a
	synthetic active-lock probe that correctly aborted the second
	`run-code-quality.ps1` invocation