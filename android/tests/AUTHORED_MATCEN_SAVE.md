# Fresh-process authored matcen saves

This extends the ordinary-save fixture and uses its real renderer, save writer,
and restore reader. Run with a display (Linux CI uses `xvfb-run`):

```sh
python3 android/tests/run_authored_matcen_save.py \
  --executable /absolute/path/test_upstream_compat \
  --base /absolute/path/pinned-base --extra /absolute/path/pinned-mission \
  --inputs android/tests/fixtures/authored_matcen_save/vignettes-secret.json \
  --output /absolute/path/new-output
```

The manifest pins every input file and the historically defined ordinal → station
→ segment → robot-mask binding. No asset bytes or generated native saves are
checked in. The Vignettes case uses native D1, secret −1, original trigger 1;
its reference is the defined presecurity `bc19490fb58d0db5f6ba0fd440db33b59b9861fe`
binding (ordinal 0, station 0, segment 560, mask 32768). The caller must supply
those private resources. This does not qualify missing-record/stale-slot cases.

Public CI uses `public-secret.json`: an explicitly synthetic mission descriptor
exposes the unchanged public D1 third level as secret −1. Original trigger 0,
geometry, masks and station bindings are unchanged. This tests a negative-level
save identity, not an authored public secret entrance or secret-exit lifecycle.
The base directory must contain only the two pinned HOG/PIG files; the existing
CI asset setup is copied into an isolated directory for that purpose.

Four native invocations write an ordinary save, restore it in a fresh process,
repeat that restore in another process, and run without trigger dispatch.
Checkpoint station state, exact relative timers, SIM RNG, ordered births and
terminal state must match the uninterrupted branch. The pinned bindings prevent
both branches drifting to the same incorrect mapping. Positive cases require
real births; the no-dispatch control requires none. Reports, logs, binary/input
hashes and the unchanged save hash are retained. No observation updates a golden.

This is a controlled station-production continuation. It dispatches an original
trigger directly, retires blocking/new robots without fabricating kills, and
steps `fuelcen_update_all` at 60 Hz for 20 seconds. It does not simulate combat,
claim full-world trajectory equivalence, or verify Android launcher/SAF resume,
thumbnail appearance, secret transitions, network or demo compatibility.

Source-only rejection tests: `python3 android/tests/test_authored_matcen_save.py`.
