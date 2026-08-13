# Level-load route-cache log review

- [x] Extract the long pauses and nearby profiling/cache events from the supplied log
- [x] Trace route-analysis cache creation, reuse, and invalidation in source
- [x] Compare the observed timing with the guidebot/navigation hypothesis
- [x] Record the conclusion and any remaining diagnostic gap

## Findings

- The saved-game restore took 14,858,836 us. Level initialization accounts for
  14,850,713 us, and the `file_us` subphase accounts for 14,246,105 us, or 96.1
  percent of `LoadLevel`.
- Texture work is not the long pause in this log. Replacement-texture indexing
  took 1,441 us and texture paging took 468,619 us.
- In logged build `4c309f66`, D2 `file_us` starts before `load_level()` and ends
  after `secret_area_rescan_current_level()`. The latter synchronously builds the
  canonical end-of-level route on a persistent-cache miss.
- The persistent route-analysis cache key includes the Android version code and
  stores records under `route-cache/<versionCode>/`. Consequently, every new app
  version deliberately misses each level's older cached route once, computes it,
  and saves a version-specific record.
- The hypothesis is therefore strongly supported. More precisely, the work is
  shared canonical route/level-metadata analysis consumed by Guide-Bot and
  objective overlays, not classic Guide-Bot waypoint generation alone.
- The supplied log does not directly contain cache hit/miss/write counters, so it
  cannot prove the individual miss by itself. Those counters currently exist in
  introspection, while the phase placement, duration, cache design, and reported
  once-per-level/per-version pattern all agree.
