# Controlled public matcen production

This is native station-loop coverage using the real public D1 shareware level 3
(two matcens) and D2 demo level 2 (three matcens). Level 1 is never used as a
vacuous production/suppression fixture.

Run `test_public_matcen_stations.ps1` with `-D1Exe` and `-D2Exe` pointing to each
engine's `test_upstream_compat` binary, plus `-D1DataDir` and `-D2DataDir` pointing
to pinned public data. The runner validates every asset hash, removes display
variables, and selects an invalid video driver. The fixture asserts that video
was never initialized.

The 40 cases cover every authored station at difficulties 2 and 4:

- Default produces multiple robots in both first and second activations
- One-round preserves the complete ordinary first production sequence and rejects
  another activation. One round does not mean one robot
- Paused rejects an initial activation without consuming lives or counts
- Pausing an active center during its genuine morph delay suppresses production
- D1 still consumes a life at the highest difficulty; D2's ordinary highest
  difficulty remains unlimited, while D2 one-round still consumes its one life
- Every actual energy center gives the native refueling amount without changing
  loaded station state and positive refueling rate across every policy transition
- Rejected second activations preserve lives as well as counts

Production uses `trigger_matcen` and `fuelcen_update_all`, with loaded native
robot masks, shields and morph creation. The fixture records the actual station,
robot ID, signature, production frame, life/count transitions and energy-center
count. The engine must increment its robot total without fabricated player kills.
Two independent processes must produce byte-identical reports. Omitting the
actual native activation in each engine must fail the positive production
assertion; empty suppression alone cannot pass.

## Scope boundary

This is not an ordinary player-route replay. The fixture invokes station triggers
directly, clears any authored robot occupying the tested production segment, and
retires each generated robot to isolate station production from congestion and
combat. Those removed authored identities are retained in the report. The player
stays at the normal spawn. No trace claims a key/switch traversal, combat kill,
Android execution, or imported-D1 parity. Existing native mode, station-transition
and Android save/restore tests are retained separately.
