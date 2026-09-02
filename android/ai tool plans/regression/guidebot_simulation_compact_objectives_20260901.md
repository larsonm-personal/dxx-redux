# GuideBot simulation compact objectives

## Goal

Revise the paired simulation regression contract so it stores only the
simulation RNG stream and represents completed objectives as compact named
objects with integer completion seconds.

## Phases

- [x] Define generation 4 output and short objective-name normalization
- [x] Update the reducer, validator, headed comparison, and browser display
- [x] Update schema and integration tests for the new contract
- [x] Regenerate all paired mission simulation files
- [x] Audit all files and run scoped quality and regression tests

Final audit: 83 generation-4 files, 114 mission records, 985 level
records, and 2,508 completed objectives. No checked-in simulation file
contains an effects RNG field, `objective_seconds`, a fractional objective
second, or an empty objective name. Fresh repeat-2 headless output matches the
checked-in Counterstrike level 1 record, and headed/headless objective-name
parity passes.

## Contract

Each completed objective is stored as `{ "n": "blue key", "s": 32 }`.
Trigger labels are shortened to `switch N` and `fly-through N`; other names
use concise lowercase engine labels. RNG boundaries contain only `state` and
`calls` for the simulation stream. Objective seconds use nearest-integer
rounding.
