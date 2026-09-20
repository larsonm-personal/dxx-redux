# Fly-out availability categories

Report movie selection/availability, normal exit trigger presence, valid tunnel geometry, and exterior presentation independently. Preserve route failure details and geometry timing even when exterior data is absent. Top-level missing-data categories distinguish a present tunnel from no valid tunnel; unloaded levels remain uninspected.

Validation: build both Windows engines and Android native engines, extend native integration assertions, regenerate all mission metadata, and audit the component counts.

Status: Complete

## Schema

Every inspected level now reports independent components:

- `movie.status`: `not_applicable_d1`, `not_selected_for_level`, `campaign_ending_instead`, `measured`, `missing_movie`, or `invalid_movie`. Selected movies retain filename and measured timing in this object
- `exit_trigger`: `present` or `absent` (normal exits only, not secret exits)
- `tunnel.status`: `present`, `no_valid_route`, or `no_exit_trigger`. Its `seconds` remains a geometry estimate regardless of movie selection or missing exterior assets
- `presentation.status`: `present`, `missing_endlevel_data`, `invalid_endlevel_data`, or `missing_assets`. The last includes the missing asset list. Asset validation remains presence-only
- `routes`: individual paths retain invalid-exit, disconnected, and cyclic failure evidence

Top-level `tunnel_present_exterior_missing` distinguishes a calculable tunnel without its exterior description from `no_valid_tunnel_exterior_missing`. No normal exit trigger has its own top-level status. Components are still inspected for movie levels and campaign-ending levels, so selection no longer hides geometry or exterior evidence. Unloadable levels remain explicitly uninspected, not labeled absent

The top-level timing continues to describe the selected movie or candidate rendered sequence. Component timings are separate, and these labels do not change runtime playback or multiplayer timeout policy


## Validation and results

Both Windows engines and Android x86_64 native engines built successfully. All seven native integration cases, scoped formatting, JSON normalization, and the complete field/timing audit passed.

Full regeneration completed all 133 archives and five CD collections (138 passed, zero skipped or failed), plus both built-in campaigns. Output: `android/temp/mission_zip_host_metadata/20260920_091212`. Coverage remains 140 regression files, 696 mission entries, and 2,639 level entries, including duplicate archive copies.

The previous 1,845 ambiguous top-level entries now split into 1,206 `tunnel_present_exterior_missing` and 639 `no_valid_tunnel_exterior_missing`. Separately, 67 entries have no normal exit trigger and 90 could not be inspected because level loading failed.

Independent components now expose 1,812 valid tunnels, 670 entries with exit triggers but no valid tunnel route, and 67 without normal exit triggers. Exterior descriptions are absent in 1,936 entries, malformed in 14, and present with referenced assets in 599. These broader component counts include movie and ending levels whose geometry/exterior evidence was previously omitted. Movie status distinguishes 917 inspected D1 entries, 1,608 D2 entries with no selected movie, 23 measured movies, and one campaign ending.

Survey outputs: `temp/flyout-timing-survey.json` and `.csv`.

## Follow-up: publish only available presentation timings

User correction: omit hypothetical geometry estimates when the fly-out cannot play. Keep tunnel presence and route failure evidence, but omit tunnel/route timing and distance fields unless the selected presentation is an available rendered fly-out. Unavailable presentations now use `kind: none` and `seconds: null`. Selected movies retain their actual movie timing without a hypothetical rendered estimate. This supersedes the earlier statements about retaining component timings regardless of presentation availability.

Validation complete: scoped formatting, Windows D1/D2 and Android x86_64 builds, all seven native integration cases, full metadata regeneration, JSON normalization, and the complete audit passed. Regeneration output: `android/temp/mission_zip_host_metadata/20260920_093942` (138 source jobs passed, plus both built-in campaigns). All 2,639 level records passed the check forbidding unused route/tunnel timing fields. Timings remain for 577 rendered presentations and 23 movies.

## Naming

The fly-out kind is now `in_engine` for the real-time ship sequence and `video` for a pre-rendered movie. `none` denotes no available presentation. Updated the generator, consumers, tests, and 577 checked-in fixture records with this label-only rename; no timing recalculation is needed. Earlier references to a rendered kind in this report mean `in_engine`.
