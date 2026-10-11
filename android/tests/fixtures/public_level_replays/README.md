# Compact public level-start recordings

Six fresh production-recorder captures use pinned Descent 1 v1.4 shareware and
Descent 2 v1.0 demo data. They start from ordinary level-one spawns. No player
pose, inventory, object or geometry is staged; no save/checkpoint data is stored
in the replay or its companions. Existing retail recordings remain unchanged.

## Size and evidence contract

Each replay is plain uncompressed UTF-8 JSONL. Admission rejects any replay and
oracle pair totaling more than **100000 bytes**, a stricter limit than 100 KB per
replay alone. There is no compression, base64, split recording or hidden save.

The engine first records complete live frame state/diagnostics and actual RNG
events into temporary capture output. The packer retains ordinary controls,
per-frame SIM RNG state and the exact terminal result, replacing bulky state
observations with a SHA-256 oracle for every complete frame. A separate digest
covers every normalized SIM RNG event, including values, order and actor context.
Expected hashes are never generated from replay output.

Only explicitly reviewed diagnostic event kinds are removed. The engine executes
only `direct_command` events; these recordings forbid those commands entirely.
Removed telemetry has a provenance hash/count and is explicitly not claimed as
replay-compared. Unknown event kinds fail admission rather than being ignored.

Each oracle pins the engine source revision, capture executable, capture fixture
source and all extracted public asset hashes. `demo_import_oracles.json` and
`game_data/test_data_manifest.json` identify the source distributions. The source
revision denotes the engine base; `capture_source_sha256` identifies the exact
new fixture header shipped in this segment. The protocol restores SIM RNG per
frame; it does not seed the separate FX stream. Capture leaves ordinary FX
initialization untouched, including the level-entry starfield draws.

## Real actions

- `flight`: retreat from the normal spawn while firing into the level. Solid
  geometry stops continued backward thrust; the runner requires at least ten
  live-player contact frames, movement, actual projectiles and energy expenditure
- `pickup`: a deterministic controller follows connected public segments to a
  real existing pickup, using only ordinary thrust. D1 collects energy; D2 collects
  shields. The runner requires the gain at the exact recorded `pickup_gain_frame`
  boundary, separately from the subsequent object-slot retirement `action_frame`
- `door`: ordinary thrust follows a reachable path to an existing unlocked door,
  with firing. The selected door must start closed and actually enter opening or
  waiting state at the recorded boundary

The route controller is used only to create input recordings. Replays consume the
recorded controls without navigation assistance. Captures end shortly after their
selected action succeeds. Fixed timesteps are 1/60 second for `flight` and 1/30
second for route scenes, rounded down to engine fixed-point units.

## Reproduce

Prepare public data with `android/helpers/setup_shareware_test_data.ps1 -Game both
-Download`. Build Debug `test_upstream_compat` and `dxx-redux-d1` / `dxx-redux-d2`
targets. Run under a software-rendered X11 display:

```powershell
./android/tests/test_compact_public_replays.ps1 -D1Exe ./buildd1-replay/main/d1x-redux -D2Exe ./buildd2-replay/main/d2x-redux
```

To create candidates in a new temporary directory, use
`capture_compact_public_replays.py --repo . --output temp/new-capture
--capture-d1 buildd1-replay/main/test_upstream_compat
--capture-d2 buildd2-replay/main/test_upstream_compat
--data-d1 game_data/shareware/d1 --data-d2 game_data/shareware/d2
--engine-revision <full-engine-commit>` under the same display. Capture does not
update this corpus. Independently repeat capture and verify clean native playback
before admitting any new expectation.

Native playback must match every live frame hash, terminal field and SIM event
stream twice, plus the action-specific assertions. The existing paired D1 runner
retains its full detailed report and explicit full-simulation INCOMPLETE status.
Its recording-to-native frame/RNG checks are incomplete because compact files do
not carry expanded observations; the independent immutable hash checks provide
that evidence. Native repeatability retains every existing strict check. The separate imported
D1 `flight` qualification compares exactly `f`, `ft`, gameplay `state`, SIM `rng`, the
terminal result and every actual SIM RNG event. It does not qualify diagnostics,
object state or world state: the original report retains those failures and the
full-simulation **INCOMPLETE** status. Native D1 starts the player at signature 1,
while D2 explicitly initializes its player signature to 0; derived identity
hashes differ. Initial effect-animation phase also differs and remains
unqualified. The longer door trace also has two otherwise-identical SIM events whose player
context signatures differ. Door and pickup are native-only required coverage;
their imported runs are diagnostic probes and cannot satisfy the paired gate.
Their original and scoped comparison failures remain visible. No normalization
hides these observations. No full-game or
Android/headless-console fidelity claim is made.
