# Private input demo headless checks

`private_input_demos.py` owns the two unchanged Counterstrike recordings added in
private asset commit `d7999de42cf5d94cdbfd18f5eddbfc6301c1eafd`. Only their identities
are public. The owner verifies exact bytes before parsing or launching anything.

Each original runs twice through the existing native D2 headless replay CLI,
with embedded expected-result comparison, verified loaded retail fingerprints,
a bounded timeout, and required natural process exit. The owner independently
checks the full embedded terminal oracle, including the exact frame count, and
compares complete terminal results between repeats. A zero exit without a
complete matching result fails. Both demos are attempted even if one fails.

These are ARM64 recordings (level 7: source `a6e2a506`, 2,161 frames; level 9:
source `a63050a59`, 7,372 frames). Linux replay is an intentional cross-platform
check. The final oracle is a recording endpoint, not a claim that the reactor or
level exit was completed. There is no rerecording or expected-result override.
Repeated terminal agreement establishes endpoint consistency, not per-frame
equivalence. Full state traces remain available through the existing native
`-inputdemo-state-log` CLI for private diagnosis; this lightweight owner does not
generate them.

## Local invocation

Use a D2 headless executable built from the source being tested and the verified
retail D2 data directory (including water.pig and fire.pig). With a private assets
checkout and a fresh output directory:

```sh
python3 android/tests/private_input_demos.py \
  --demos /private/dxx-redux-test-assets/demos \
  --data /private/verified-retail/d2 \
  --exe buildd2/main/dxx-redux-d2-headless \
  --output /private/results/input-demos
```

Diagnostics remain under the private output directory and the per-repeat
HOME/XDG sandboxes. Do not upload these or copy
recordings into public source, logs or artifacts. A failure summary identifies
the case/repeat; its local native log contains the original oracle comparison.

Each repeat uses a fresh working directory and HOME/XDG folders. No PowerShell
process, display, network fetch, expectation override or rerecording is needed.
This owner does not qualify the separate interactive PowerShell replay wrapper.

## Trusted nightly admission

The `host-profile:retail-input-demos` owner uses the existing trusted retail
boundary: only the repository's scheduled/manual `cmake` workflow receives the
private checkout token. It additionally requires `RETAIL_ASSETS_COMMIT` to equal
the reviewed commit above; other commits leave this owner pending. Configure
that existing environment variable separately when deploying this change.

The host workflow stages only the pinned pair when this owner is selected, then
removes the checkout. Public PR CI runs synthetic controls only. Retail runs
upload only the existing filtered owner receipts and retail verification file,
never owner logs, recordings, results or traces. The owner does not update any
repository/environment configuration or access permissions.
