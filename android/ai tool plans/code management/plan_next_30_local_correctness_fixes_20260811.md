# Next 30 local correctness fixes

## Scope

Fix the ranked non-network correctness queue selected from the active adversarial review ledger. Preserve unrelated dirty-worktree changes and archive a finding only after its complete acceptance boundary and focused tests pass.

## Queue

- [x] BR-0438: transactional combined-configuration import
- [x] BR-0441: publish imported-root switch before retiring source
- [x] BR-0461: preserve destination-owned migration data
- [x] BR-0364: synchronize MVE startup queue trimming
- [x] BR-0078: atomic native engine admission through teardown
- [x] BR-0016: serialize launcher preview commands
- [x] BR-0029: marshal overlay state through the engine thread
- [x] BR-0044: make repeated overlay JNI callbacks safe
- [x] BR-0394: finish software-renderer build guards
- [x] BR-0493: complete launcher/native HAM patch-schema parity
- [x] BR-0235: propagate and roll back save metadata failures
- [x] BR-0236: roll back partial all-pilot preference writes
- [x] BR-0338: preserve complete mission identities in save paths
- [x] BR-0255: abort resume when pilot loading fails
- [x] BR-0264: resolve controller configuration from current user
- [x] BR-0268: surface pilot-preference conflicts before edits
- [x] BR-0472: publish launcher ownership registries atomically
- [x] BR-0418: confine provider display names to import roots
- [x] BR-0462: bound and cancel provider-tree scans
- [x] BR-0469: enforce active-mod path capacity before launch
- [x] BR-0415: prefer complete retail D2 data over demo fragments
- [x] BR-0442: canonicalize game filenames independently of locale
- [x] BR-0481: classify loose secret levels as file-set data
- [x] BR-0436: preserve radial binding types through import
- [x] BR-0417: repair last-save selection after deletion
- [x] BR-0432: reject empty music-source launches
- [x] BR-0258: retry the same autosave slot after failure
- [x] BR-0262: resolve tied pilot timestamps deterministically
- [x] BR-0280: propagate music render-thread startup failure
- [x] BR-0021: propagate extraction cancellation

## Validation

- [x] Add or extend focused tests for each completed finding
- [x] Run scoped formatting and linting
- [x] Run focused native, JVM, and script tests throughout
- [x] Run the full maintained test suite after the queue
- [x] Verify ledger uniqueness, dispositions, and `git diff --check`

## Final validation notes

- The maintained suite completed all 112 selected tests: 90 passed and 22 failed. The deterministic secret-area baseline mismatch was regenerated from rebuilt D1/D2 metadata targets and then passed. Most later device failures shared one root cause: the assembled APK omitted `androidx.startup.R$string`, so `InitializationProvider` crashed before `Application.onCreate`; retaining dependency R classes repairs startup, and the native xCrash test subsequently passes.
- Focused native D1/D2 host suites, JVM tests, all-ABI OpenGL and software builds, audio lifecycle tests, route corpus tests, test-helper process tests, and the regenerated secret-area baseline pass. The remaining axis-routing assertion is an independent pre-existing production-path timing/dispatch failure and is not hidden by weakening its test.
- Ledger reconciliation found 674 unique BR/INV headings and no duplicates. Broad findings and P1 findings lacking the process-required independent verification remain open rather than being archived as false completions.
