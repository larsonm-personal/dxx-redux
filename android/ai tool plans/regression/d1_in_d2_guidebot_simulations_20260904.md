# D1-in-D2 GuideBot simulations

## Phase 1: Loader and corpus inventory

- [x] Trace the existing D1-in-D2 launch path and required data layout
- [x] Inventory native D1 mission metadata, archives, and level formats
- [x] Define stable simulation identity and output rules without colliding with native D2 records

## Phase 2: Engine runner support

- [x] Confirm that the existing D2 headless executable enters D1-in-D2 mode without a native change
- [x] Stage D1 base data and mission assets through the D2 loader
- [x] Translate D1 metadata levels into valid D2-engine simulation work items
- [x] Preserve deterministic routing inputs, objectives, RNG, timeouts, and incremental writes

## Phase 3: Corpus integration

- [x] Make every eligible D1 mission metadata file produce adjacent simulation JSON during a run
- [x] Include D1-in-D2 work in full and focused regression categories
- [x] Replace the routing development set's fan conversion with canonical D1 First Strike
- [x] Report unsupported missions or levels explicitly instead of silently omitting them

## Phase 4: Verification

- [x] Add discovery, staging, output-contract, and focused-run integration tests
- [x] Run representative base and custom D1 missions through the D2 headless engine
- [x] Compare repeated runs for determinism
- [x] Confirm no native engine change is required, so no Windows rebuild or D2 CTest run is needed
- [x] Run scoped code quality and review the final diff and generated-file scope

## Verification notes

- Full dry-run discovery found 777 D1 levels in 52 metadata files, all using native `.rdl` level files
- The complete canonical First Strike pass produced 20 `ok` and 10 `timeout` level results through the D2 headless route executable
- A custom training mission loaded and produced byte-identical results across two simultaneous runs
- Brat's Maze level 1 exposed a repeatable engine process crash after route startup; it is now retained as an `infrastructure_error` instead of being omitted from the corpus
