# Public replay performance measurements

## Reproducible scope

The public corpus retains its exact scene inventory, two independent native runs
per backend, complete wrong-oracle and bad-input sweeps, the loot no-collect
control, required imported flight comparison and all imported diagnostic probes.
Every process keeps a separate sandbox and the original natural-exit requirement.
No timeout, expectation, capture payload, retail test or sanitizer setting is
relaxed for performance.

`report.json` records each native run's monotonic wall-clock seconds for the replay
wrapper, immutable fidelity checks and semantic action checks. A failing phase
still records its elapsed time and retains its original failure. The wrapper log's
`Elapsed:` measurement isolates native execution from wrapper overhead. Compare
first-use and repeated runs on the same host; do not interpret CI runner variation
as an optimization result.

CI also uploads small `public-*-reports` artifacts before the complete evidence
artifacts. These contain JSON reports, logs and screenshots, so routine review does
not require downloading a trace archive exceeding the client's size limit. The
complete raw evidence is still retained in its existing artifact.

## Initial profile, 2026-10-13

Baseline source: `423689514af9fb0d9b34698e345a8ae829d25370`, public runtime run
[38324936045](https://github.com/larsonm-personal/dxx-redux/actions/runs/38324936045).
Its verified shared binary artifact identifies the tested merge source
`7386ff219fb1070dee5be3bbf8d97b02ac4e9058` and all four binary hashes were checked.

The build/save job completed in approximately 4 minutes 18 seconds. Packaging the
19.5 MB runtime artifact took 1.7 seconds; sharing it took 2.5 seconds. This does not
support prioritizing repeated downloads or packaging over replay execution.

A 221-frame ordinary D2 Guide-Bot replay, using that binary and unchanged pinned
public data, measured 22.37 and 22.92 seconds through the wrapper in fresh isolated
sandboxes. Native execution accounted for 21.54 and 21.76 seconds. PowerShell-only
startup measured 0.18 and 0.26 seconds. Batching PowerShell would therefore address
only a small fraction of this case's time and is not used.

Unprofiled fidelity plus action validation of the same retained trace took 15.42
and 15.57 seconds. cProfile counted 28,056,969 schema-validation calls; recursive
validation accounted for 19.69 of 31.30 profiled seconds, with JSON decoding the
next largest cost. Profiling overhead is not included in the unprofiled timings.

## Integer-leaf validation

The comparator now performs the same exact integer type check inline in dictionary
and array loops. It still visits every element on every call, including unchanged
values; there is no value cache or skipped schema validation. Diagnostic paths are
constructed on failure instead of for every successful integer leaf. Tests mutate
first, middle and last pool entries and all tuple entries after a successful check,
including booleans, nulls, floats, strings and containers, and require the original
precise rejection path.

For 100 validations of the same complete retained world boundary, process CPU time
fell from 1.00/1.05 seconds to 0.67/0.59 seconds. The full unprofiled Guide-Bot fidelity
plus action checks fell from 15.42/15.57 to 14.08/13.75 seconds. These are local
first-use/warm-repeat observations, not a forecast of full CI wall time.

## CI-only optimized Debug compilation

The public-runtime workflow explicitly chooses `-O1 -g1` within `Debug`; ordinary
user Debug defaults and all sanitizer configurations are unchanged. Assertions
remain enabled. CI checks the actual generated compile commands for every normal
engine, headless engine and native compatibility fixture: exactly `-O1`, debug
information, no `NDEBUG`, and retained `-ffp-contract=off`/`-fno-fast-math`. The
compile-command evidence is archived. No fast-math or gameplay/source change is
part of this profile.

Using the same GCC 14.2.0 host/compiler and base engine source, the 221-frame D2
Guide-Bot case measured:

- `-O0`: native 20.88/21.53 seconds; wrapper total 21.67/22.31 seconds
- `-O1`: native 3.82/3.63 seconds; wrapper total 4.78/4.51 seconds

Both optimized repetitions matched all 221 immutable frame hashes, all 1,320
simulation RNG events, the terminal hash and the independent Guide-Bot action
checks. The first measurement starts a new process and fresh sandbox; the second
repeats with warm file caches. No privileged system-cache flush is claimed. A
single-worker clean D2 headless build took 50.7 seconds at `-O0` and 77.2 seconds at
`-O1`; this adds compilation work while reducing every subsequent traced replay.

Hosted-runner variation is substantial: unchanged engine/corpus PR #26 completed
its headless positive matrix in 11.5 minutes and negatives in 24.9 minutes; PR #27
needed 21.7 and 45.6 minutes for the same gates. Both passed and produced raw
artifacts of approximately 870.7 MB. This approximately 1.85x variation is why a
single before/after CI duration is not a controlled performance benchmark, and why
small separately downloadable reports matter.

## Local qualification

Both backends passed all thirteen original scenes twice (52 native positive
replays), plus every wrong-oracle, bad-input and no-collect control (108 negative
replays). Both negative-suite reports confirmed the original corpus was unchanged.
The headed imported flight gate passed; the existing imported diagnostic probes
remain diagnostic, with full imported simulation qualification incomplete.

The optimized native save, pickup, combat, matcen, compatibility and headless-audio
suites also passed. Six visual recaptures matched the original immutable
frame/RNG/terminal/semantic oracle fields and retained 84 screenshots. All 173
Python replay tests passed. Independent review compared 7,325 baseline/new schema
outcomes across 537 populated world slots, including exact exception text.

The container's existing `/proc/self/task/<tid>/children` limitation prevented the
master-catalog supervisor and Guide-Bot fixture wrapper from completing normally.
Direct Guide-Bot fixture repetitions passed with identical reports; CI retains the
original supervised wrappers. These local binaries share the unchanged base engine
source, but embedded build revisions precede publication. Exact published-head
qualification is provided separately by hosted CI.
