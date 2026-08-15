# D1 Robot Source Names Plan

## Objective

Add the indexed internal robot names documented in D1 `ai.c` to every D1 robot-name JSON
object as `source_code_name`, without changing the user-maintained display names.

## Work

- [x] Confirm the D1 comment table's index range and exact spellings
- [x] Search D2 sources for a corresponding indexed robot-name table
- [x] Add `source_code_name` to every D1 JSON object, using an empty value outside the table
- [x] Add focused catalog parsing coverage for the new field
- [x] Run JSON, formatting, and Android unit-test validation

## D2 Finding

No equivalent indexed internal-name table exists in the checked-in D2 sources. D2 retains
the table-builder `Robot_names` symbol, but retail HAM loading does not populate it.
