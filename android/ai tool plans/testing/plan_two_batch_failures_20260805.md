# Two batch failure investigation

## Goal

Determine the causes of the two failures in the latest attached batch, account
for predecessor state, and apply narrow durable fixes or document the correct
disposition.

## Plan

- [x] Create the investigation plan
- [x] Read the batch report and identify both failures and predecessors
- [x] Inspect detailed logs and relevant recent code changes
- [x] Reproduce each failure with relevant ordering state
- [x] Implement narrow fixes where supported by evidence
- [x] Validate affected tests outside isolation
- [x] Run scoped quality checks and record the result

## Result

The requested routing pair was `test_mission_route_corpus` and
`test_secret_area_baseline`. Both failures were deterministic stale-baseline
failures caused by the key-preferred routing change and subsequent mission
metadata regeneration.

Review found 32 changed mission routes out of 1561. Every changed route kept
status `ok`, and changes were limited to route steps, route-derived distance
and time, and route-derived notes. The base-game secret-area fixture changed
only six D2 route records. Secret counts, entrances, segment membership, and
all other scanner output remained identical.

The existing update tools regenerated both reviewed fixtures. Both original
tests then passed, and scoped quality checks passed. These host tests derive
their results from checked-in files and overwrite their output, so emulator or
predecessor state does not apply.

The attached report also contains two failures outside this requested pair:
`test_server_integration` hit the known guessed-port bind race, and
`test_engine_prefs_unified` left a two-byte log with no diagnostic payload.
Neither was changed in this tranche.
