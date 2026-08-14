# Metadata Viewer Reopen Mission Identity Repair

## Reported behavior

- Obsidian analysis completed normally in the launcher metadata viewer
- Closing and reopening the viewer briefly showed reset progress bars
- The background analyzer then failed with
  `Could not find required mission file <d2.mn2>`

## Findings

- [x] The viewer creates a fresh analysis request on every open; Compose does not
  retain a stale result or mission name
- [x] The D2 analysis service runs in the reusable `:levelmeta_d2` process
- [x] Runtime initialization mounts `descent2.hog` for the lifetime of that process
- [x] A base-D2 request then mounts the same physical HOG again as a request-owned
  mount, and request cleanup unmounts it
- [x] The next request reuses the initialized runtime without restoring the base HOG
- [x] D2 mission-list construction sees an unknown or missing built-in HOG and reaches
  the fatal `Could not find required mission file <d2.mn2>` fallback
- [x] Existing emulator logs contain two crashes with the reported error and show a
  successful base-D2 metadata request immediately before the later process failure
- [x] The progress UI has three possible linear bars: overall, current raw phase,
  and estimated monotonic level progress. A bar is omitted when its value is absent

## Repair plan

- [x] Make request mount ownership idempotent: if a PHYSFS source is already on the
  search path, use it without recording it as request-owned
- [x] Preserve reverse-order cleanup for paths actually added by the request
- [x] Extend the native integration regression to alternate base-D2 and mission-pack
  requests in one reusable worker runtime
- [x] Verify the second base-D2 and mission-pack requests both complete after cleanup
- [x] Run scoped formatting, JVM tests, Android build, and emulator verification

## Validation

- [x] `LevelMetadataTargetsTest` and the focused stale-extraction repair test pass
- [x] `:app:assembleDebug` passes for arm64-v8a, armeabi-v7a, and x86_64
- [x] `test_level_metadata_request_mount_scope.json5` passes all 6 steps on the emulator
- [x] Scoped code-quality checks pass

## Constraints

- Preserve built-in D1/D2 mission handling and arbitrary imported mods
- Do not clear or restart completed analysis merely because the viewer closes
- Keep worker coordination single-flight and cache publication atomic
