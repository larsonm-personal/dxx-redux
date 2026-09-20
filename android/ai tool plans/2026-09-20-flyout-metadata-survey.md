# Per-level fly-out timing survey

Status: Complete; full regeneration and coverage audit passed

## Scope

Add a per-level `flyout` record to native mission metadata and the checked-in
regression projection. Report uncapped geometry estimates or measured MVE
duration, with explicit absence/invalid/unavailable reasons. Do not change
the multiplayer deadline policy during this survey

## Work

1. Trace engine movie selection, END/TXB fallback, and exit tunnel traversal
2. Add shared native metadata extraction for Android and Windows workers
3. Verify built-in movie timing, rendered exits, missing content, invalid routes,
   and projection preservation through integration tests
4. Run full regeneration across all configured archive/CD sources and both
   built-in campaigns, then audit coverage and summarize timing outliers

Geometry timings use the exit trigger crossing as the reproducible starting
point, side-center distances at the engine's 50 units/second, and the compiled
short sequence's five seconds outside. They are estimates, not simulations of
camera offsets and player-dependent crossing positions. Preserve malformed
route evidence rather than replacing it with a policy timeout

## Implementation and validation

- `flyout_metadata.hpp` supplies both Android JNI and Windows metadata writers
- D2 playback and metadata share the engine's movie filename selection
- END/TXB lookup includes the level-one fallback and referenced asset presence
  checks. Presence does not prove that every bitmap/model decodes successfully
- Rendered `seconds` retains a geometry estimate even when presentation assets
  are missing. `status` and `presentation` make that distinction explicit
- MVE timing sums display-frame intervals from the selected actual asset;
  neither the 20-second floor nor the 60-second cap is applied
- The Windows runner mounts the owned OTHER-H.MVL for built-in movie analysis
  and logs its SHA256. The mount is released between requests
- Windows D1/D2 and Android x86_64 native builds passed
- Seven native integration cases passed: D1 rendered, invalid END, missing level;
  D2 stock video, missing video after unmount, 90-second synthetic video, and
  malformed video. Kotlin projection tests passed, including uncapped timing
- Full run: `regenerate_all_mission_metadata.ps1 -IncludeBuiltInCounterstrike
  -IncludeBuiltInFirstStrike -NoBuild -MaxParallel 8`, using freshly built native
  workers and Kotlin CLI. Output: `android/temp/mission_zip_host_metadata/20260919_231629`

## Survey results

The full run completed in 410.7 seconds: all 133 archives and five configured
CD collections passed, with no skipped or failed source jobs. Both built-in
campaigns also passed. The final audit covered 140 regression files, 696 mission
entries and 2,639 level entries, with no missing fly-out records. Counts retain
duplicate missions/versions present in different archives; they are not counts
of unique physical levels. Route-simulation companion files belong to their
separate simulation runner and are not mission-metadata inputs to this survey

| Timing group | Entries | Minimum | Maximum | Over 20 s | Over 60 s |
| --- | ---: | ---: | ---: | ---: | ---: |
| Rendered, presentation assets present | 577 | 8.000 s | 46.618 s | 43 | 0 |
| All calculable rendered geometry, including missing presentations | 1,788 | 5.100 s | 46.618 s | 50 | 0 |
| Actual stock D2 movie timings | 23 | 16.933 s | 17.349 s | 0 | 0 |

Notable rendered estimates with presentation assets:

- Lavafalls Extraction Center, `lfexlvl.rdl`: **46.618 s**
- Descent Vignettes, `level50.rdl`: **29.656 s**
- Diamond Gnomes, `homecave.rdl`: **29.151 s**
- Revenge O' Dravis, `drav24.rdl`: **28.952 s**
- Anachronistic, `appease.rdl`: **28.563 s** (multiple exit routes retained)

Of the remaining top-level statuses, 1,845 entries lack END/TXB presentation
data, 14 have malformed presentation data, 22 have no valid calculated tunnel,
67 lack a normal exit trigger, and one substitutes the campaign ending.
Another 90 level entries could not be loaded by the current metadata engine;
their times are explicitly null, not classified as missing fly-out content

The nested route records include nine cyclic route candidates, including
`drav17.rdl`, `drav21.rdl`, and `dravs2.rdl` in Revenge O' Dravis and `level20.rdl`
in Vignettes. These counts include repeated copies across archives

The rendered figures are reproducible conservative geometry estimates. Camera
offsets, its temporary speed boost, entering the final segment before its far
side, and the exact player crossing point can shorten actual playback. These
are whole-presentation estimates, not remaining time when the last teammate
escapes. Movie values are measured from frame/timer opcodes, without decoding
and displaying the movie

The multiplayer timeout calculation is unchanged. In particular, this survey
does not apply its 20-second minimum or 60-second cap

## Reproducing the audit

`python android/helpers/summarize_flyout_metadata.py --output temp/flyout-timing-survey.json`

This validates every mission-metadata level, checks that top-level rendered
times equal the maximum valid exit route, and writes both a summary JSON and
`temp/flyout-timing-survey.csv` containing every level. The final audit passed;
the existing metadata normalization test and scoped formatting also passed
