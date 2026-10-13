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
