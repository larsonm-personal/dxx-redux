# Guided missile save ownership

- Reproduce released live missiles being reacquired by legacy reconstruction
- Append explicit validated per-player active slot/signature pairs to the runtime tail
- Use native/Android D2 version45 and D1-in-D2 version46; Android retains its current-only policy, native D2 older saves retain legacy reconstruction, native D1-in-D2 version42 remains accepted with its existing asset identity prefix
- Capture saved parent identity before coop player replacement; apply the existing authoritative player remap, validate the missile against its original parent, then reparent to the matched restored ship
- Do not change missile camera ownership or the intentional May12 fallback behavior
- Exercise active/released/multiple/stale/no-guided states with real firing and ordinary save readers/writers, malformed/truncated tail rejection, and old native save support

## Validation limits

Targeted native builds reuse unchanged older engine objects with recorded commands and provenance, rebuilding current state.c and fixture. This is not a clean current-tree build or an Android runtime test. Inventory and identity edge cases are synthetic checkpoint setup on public D2 data. Separate-process active/released restores are also tested. Coop identity mapping application is tested with extracted production helpers; full cooperative session/save integration is not tested. PowerShell quality/catalog commands are not run per task constraint. No pushes or merges.

## Native fixture

Run test_upstream_compat --guided-save-runtime DATA_DIRECTORY CASE in an isolated writable directory, with public D2 demo assets in DATA_DIRECTORY. Cases: active, released, multiple, stale, parent, none, format. Run load-active or load-released in the corresponding prior case directory using a second process to test persisted ownership without an existing in-memory pointer. The existing upstream test harness supplies graphics/resource initialization. The fixture is included from its existing native test executable, not a new Android automation entry point.

All six identity cases pass with the fix. The current-source baseline fails released/multiple/stale/parent and passes active/none. Format coverage rejects out-of-range slots, truncated tails and malformed absent identities, accepts an ordinary native41 prefix, and recovers with a valid new save. Native and Android header format tests pass. An extracted-production-helper C test validates old-parent to new-player-slot remapping and stale/missing/wrong-player/dead rejection; this does not exercise the full coop transport.

## Integration

This commit is an unpublished intermediate. The pending wiggle-phase preservation change will stack on it and append its phase word in the same new format45/46, avoiding two public save-format bumps. Update the guided format fixture's trailing64-byte assumptions when adding that word. The separate missile-camera ownership change touches no production file edited here; no intentional wake fallback behavior is altered.
