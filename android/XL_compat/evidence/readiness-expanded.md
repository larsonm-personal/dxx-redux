# Expanded original-content readiness

The expanded survey is a separate comparison series. It does not replace the
original eight-case catalog, reporter or status. Its 64 unique entries contain
all 20 levels from Boiling Point, Dinter, Sphere and Warp Shock; one representative
from every other uploaded archive; 12 additional secret entries; and 13 explicit
resource/structural nominations. The original eight remain first and receive a
separate subtotal. Repeated content in different original packages is retained
as distinct package-context coverage.

`android/tests/xl_readiness_expanded_cohort.json` records exact original IDs,
selection reasons, seed and the unchanged inventory digest. The producer rejects
cohort changes, unknown IDs and duplicates before execution. Nominations come
from descriptor/structural evidence, never successful engine outcomes. Changes
require review and a new comparison series.

Each case uses the existing ordinary native load proof: requested mission,
level and file, 120 unassisted frames at 60 Hz, seed 1, no death or level transition.
No optional assisted route is run in this survey. The original eight-case job
continues to execute its existing auxiliary routes independently.

The default probe budget is 360 seconds. The CI job has a ten-minute wall limit;
it reduces that probe budget by elapsed checkout/staging/build time, reserving
20 seconds for publication. No new case starts with less than 45 seconds remaining.
A parent deadline stops its entire worker process tree and leaves the interrupted
and unstarted cases untested. It never converts budget cancellation into an
engine timeout or drops cases from the denominator. Staging/engine diagnostics
and native proof tails are retained locally, with temporary extracted copies
removed between cases. Original input archives and runtime assets are untouched.

Complete measurement is separate from compatibility. Load failures, simulation
failures, unsupported formats, crashes and engine watchdog timeouts remain
unsuccessful cases. Infrastructure errors and untested entries make the percentage
unavailable. A valid complete measurement can finish successfully while the
separate `D2X readiness (expanded 64 load/tick)` status remains unsuccessful.
There is no expected-failure waiver. A 64-case pass would still not establish
rendering, earned progression, multiplayer correctness or campaign completion.

## Local execution

Use the ordinary developer-OFF `dxx-redux-d2-headless-route` target and the same
authentic resource profile documented in readiness.md. With pinned private
archives and an already verified combined D1/D2/Vertigo/XL runtime directory:

```powershell
python android/tests/xl_readiness_expanded.py run `
  --engine PATH_TO_ORDINARY_ENGINE `
  --assets PATH_TO_PINNED_PRIVATE_ASSETS `
  --data PATH_TO_VERIFIED_RUNTIME `
  --work temp/expanded-readiness-UNIQUE `
  --target-sha EXACT_SOURCE_SHA --budget-seconds 360
```

The `prepare` command accepts `--assets`, `--data` (separate d1/d2 retail
directories), `--resources` (the pinned private resource checkout) and `--work`
to stage all required original archives plus the verified combined runtime.
The `report` command validates a receipt against the frozen cohort. Only the
trusted default-branch controller may use `--publish-status`; it revalidates
owned-PR provenance before posting. The expanded jobs become active only after
their workflow and controller code reach cmake through normal review. Public PR
CI runs the integration controls before that deployment. No private credentials
are given to public PR jobs, and no candidate-controlled workflow is substituted
for the trusted controller.

The ten-minute duration is an execution budget, not a promise that all 64 cases
will fit on every runner. Record actual elapsed time and incomplete outcomes.

## First local measurement (2026-11-16)

The ordinary developer-OFF executable built from cmake
`392b22c261f255c199d334e718fbf3154889e99c` completed all 64 entries in
277.719 seconds of the 360-second probe budget: **50 passed, 11 load failures,
3 apparent simulation failures**. The original eight subtotal remains **8/8**.
This is 78.1% of the fixed survey, not an estimate of all XL compatibility.

Load failures: Boiling Point levels 1-5; Sphere levels 3-5; Warp Shock levels
1, 3 and 4. The three apparent simulation failures (Saturn levels 1 and 4, and TT level 1)
were a classifier bug: their valid native proofs preserve uppercase mission
filenames. Mission lookup is case insensitive (`d2/main/mission.c`), so the
classifier now compares mission names case insensitively, just as it already
does for level filenames. Wrong names and missing names still fail.
The eleven actual load failures remain selected and unsuccessful. No compatibility fixes are
included in this harness change.

Private diagnostics and the full binary, runner and input hash receipt are at
`temp/xl-readiness-expanded-20261116/local-64-01/qualification.json`.
The executable SHA-256 is
`aa9d3a917dcff9c5f48b22f6abe565f85bb479947fe1f8de047ada2cf2779218`;
the result SHA-256 is
`b7d37bdb22376a94896066ef665db3b6e01ad20d38ff7999027e567bf593db57`.
The runner was an uncommitted working tree for this first run. Subsequent changes reject failed CI measurement jobs before status publication
and correct the mission-name comparison described above. The initial receipt
is preserved unchanged; a fresh run validates the corrected classifier. Hosted duration remains unmeasured
until the trusted controller change is deployed.

## Corrected local measurement

The fresh run completed **53/64 (82.8%)** in **284.016 seconds (4m44s)**,
with the same eleven load failures, no simulation failures and **8/8** for the
original subtotal. All 64 cases ran; the 360-second budget was not exhausted.
The valid reporter accepted the receipt. Inputs were rehashed and unchanged.
Receipt: `temp/xl-readiness-expanded-20261116/local-64-02/qualification.json`.
Result SHA-256: `05820e1302047770f4986add60033955fd4db2784c284dc581b7e896156c7a28`.

This demonstrates that 64 entries fit locally within the probe allowance.
Hosted checkout/build overhead and runner performance still need measurement.
