# XL stack cleanup: completion report

The redundant per-switch Pumo test chain is removed and the repaired stack has completed its required local builds and gameplay checks. The reviewed candidate is ready for a local integration review. No required local validation remains blocked; remote CI has not been updated or rerun by this work.

## Reduction and retained behavior

Deleted 77 Tulev1/guided-input files containing 34,020 lines. Five shared scenario/data files replace them with 1,064 lines. The complete cleanup against the frozen reviewed tip changes 110 files: 2,705 insertions and 34,902 deletions, a net reduction of 32,197 lines. This includes bounded build fixes and adoption of the current cmake readiness baseline.

| Removed machinery | Retained evidence |
| --- | --- |
| Supplies, switch6, master5 and chained progression fixtures | Compact resource/key acquisition, actual switch operation and dependent door state |
| Guard269, quad_guard255, blast136 and courtyard variants | Actual production laser damage/kill, retained independent blast controls |
| Switch12/13/14 guided chains and guided-input wrappers | Actual guided projectile control/contact, release and persistence |
| Per-switch provenance/forgery validators and repeated world snapshots | Shared fail-closed receipt boundary, fresh identities, executable/input/save hashes and representative negative cases |
| Repeated combat selector dispatch and redundant force samples | One linked suite entry point and selected meaningful boundaries; independent arithmetic/source controls remain |

Gauss integration cases fell from 240 to 24 selected boundaries. Every case starts with valid live victim/mode state and cleans newly allocated effects, including reused low object slots. Pure torque checks isolate splash; actual blast integration remains separately exercised. The CPU consumer's selector duplication and repeated samples were reduced, although formatting expanded that file from 5,531 to 5,997 lines. The net deletion above does not depend on minification or moved fixtures.

The shared native runner now performs ordinary movement/firing without positioning assists, live combat, key/resource progression, guided contact and cold restore. Combat uses one local positioning assist; progression uses seven. Guided control adds none but inherits an assisted checkpoint. These are focused behavior checks, not connected travel or a campaign completion claim. Fresh-process restores compare player state/position and relevant XL switch, door and remaining-objective state.

## Repairs

- Corrected countdown post-zero display and multiplayer projectile-history expectations to match production semantics
- Gave synthetic multiplayer fixtures a scoped developer-only recording destination, retaining production packet validation and normal transport behavior
- Repaired Gauss fixture lifetime/state contamination without changing production torque arithmetic
- Fixed Windows developer compile blockers, x86 reference-adapter indexing, the host SDL include and audio index narrowing
- Corrected an older music fixture's false source-ownership assumption after SDL_mixer stop callbacks
- Added a real Windows developer compile job behind the existing stack CI gate
- Kept explicit required test selections and added bounded allowlisted native error codes at the existing private-report boundary

## Completed validation

Artifact paths below are relative to `temp/xl-stack-review-20261109/` unless specified otherwise.

| Check | Result | Evidence |
| --- | --- | --- |
| Windows D1 build and full host suite | 89 pass, one existing skip | `trim-d1-build.log`, `trim-d1-tests.log` |
| Windows D2 build and full host suite | 100 pass, one existing skip | `trim-d2-build.log`, `trim-d2-tests.log` |
| Windows developer lifecycle and original five failures | All five pass; final Gauss cleanup rebuilt/retested successfully | `native-trim-final/results.json`, `native-cleanup-final/results.json`, `trim-final-cleanup-build.log` |
| Windows public XL | 56/56, no skips | `trim-final-public-windows.log` |
| Linux public XL, GPU and audio | 61/61 | `trim-public-linux.log`; final private selection plus `trim-extra-public-linux.log` cover final code |
| Linux linked combat, full suite and Gauss selector | Pass | `trim-final-combat-linux.log` |
| Full Linux private fast gate | Exact 113 required cases pass, no skips; original timeouts | `native-fs-results/original-ctest.xml`, `original-status.json`, `native-ctest.log` |
| Source/format/navigation checks | 16 format witnesses, three source levels, four navigation anchors, navigation smoke in 3.873s | Same private status receipt |
| Pumo ordinary input, combat, progression, guided and cold restore | All phases pass with actual assets | `pumo-trim-persistence-final.log`, `pumo-trim-persistence-final/` |
| Android x86_64 candidate APK | Build successful | `../trim-android-build.log` |
| Android Neptune runtime | 65/65; rendering, save/restore, stock transition, negative inputs; device restoration hashes match | `trim-android-runtime.log`, `source/temp/xl-neptune-android/result.json`, `render.png` |
| Combined private/readiness controllers | 45/45 | `trim-final-controller.log` |
| Artifact boundary tests | 6/6 | `trim-final-artifact.log` |
| Other affected host/retail/developer controllers | Pass; existing platform skips retained | `trim-retail-controller.log`, `trim-host-xl-controller.log`, `trim-developer-ci.log` |
| Scoped mixed-language formatting/lint | Pass, including final persistence and fixture edits | `trim-quality-final.log`, `trim-persistence-quality.log`, `trim-final-fixture-quality.log` |
| Final diff, source/patch match and real indexes | Pass | Candidate manifest and final audit |

The private gate initially exceeded native test and aggregate timeouts while its high-volume outputs were on WSL's Windows-mounted filesystem. A controlled identical close-door case completed in 16.95s on Linux storage within its original 75s limit. The complete canonical gate then passed with verified assets and outputs on native Linux temporary storage: 254.709 aggregate test seconds. The scratch launcher changes physical paths only; required names, pass criteria and timeouts are unchanged. This is an execution-environment finding, not a source timeout relaxation. A directly comparable pre-cleanup runtime for the entire scenario chain was not measured.

An existing C4805 warning remains in unchanged `runtime/changing_light_test_records.h`. Optional camera-addon subchecks report `not_run_missing_addon`; all 113 required cases ran. The four unsupported campaign readiness failures remain explicit backlog. Slow-motion pickup activation and the Omega replay render-clock timeline remain separate feature work, with the existing replay rejection preserved.

## Reviewable integration artifacts

Validated source: `temp/xl-stack-review-20261109/source`, detached at frozen PR #461 tip `bf388fc1064ea3ce70bfe88daceff3348d359608`. Candidate tree: `d01c46fbe590d56cfb69ba63a1bbf7432e91a74f`.

- `candidate-cleanup.patch`: apply to that frozen tip; SHA256 `ec73cd380ad25c2d038a377f38465c3333b67ab16f562fb3c6fc681102afbf32`
- `candidate-integration.patch`: aggregate final result against cmake `49e92bbcdf92b31f77c705f65587d3dfe81f299c`; SHA256 `3c8b69161bc74a59aa479df01c4adfafc6dd99eca8cba3b5e94393cc7400ac86`
- `candidate-manifest.json`: immutable bases/tree/digests and application-check outcome

Both patches passed forward and reverse application checks using temporary indexes. The working source matches the exported candidate. Actual indexes remain unstaged. The aggregate result against current cmake is 193 files, 21,698 insertions and 206 deletions. The original stack comparison was 254 files and 54,223 insertions against its earlier common ancestor, so those two totals have slightly different baselines.

Prefer one aggregate integration PR with the simplified final tree over requiring reviewers to merge 58 historical steps. Keep the separate tooling PR outside this gameplay integration. After an authorized publish, run remote tip CI on the published SHA and review the aggregate patch. No commits, pushes, merges, rebases, PR changes or external comments were made here; the root branch and all three existing stashes are intact.

Run commands for the retained public suite and asset-backed scenarios are documented in `source/android/XL_compat/README.md`, under "Focused Pumo gameplay scenarios". The original execution plan's phases and acceptance requirements are complete locally.
