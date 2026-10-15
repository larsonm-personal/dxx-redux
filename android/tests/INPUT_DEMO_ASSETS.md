# Recorded input-demo asset verification

New input recordings carry `asset_fingerprints` in their header. The native
recorder binds each HOG/PIG/HAM role to bytes actually opened by the engine,
including the selected D2 HAM/PIG fallback and a mounted custom mission HOG.
It rechecks those sources at recording activation. Missing, changed, loose, or
otherwise unresolved sources produce an explicit `unknown` status and reason.
No script derives recording fingerprints from an edition label, mission name,
recording filename, or the files installed later.

The header object has `status`, `mode`, and `files`; `unknown` also has `error`.
Each file has `role`, diagnostic basename `name`, byte `size`, and lowercase
`sha256`. `native_d1` needs D1 HOG/PIG; `native_d2` needs D2 HOG/PIG/HAM.
`d1_in_d2` needs D1 HOG/PIG and all three D2 roles when the registered companion
assets were actually loaded. `mission_hog` is an optional additional dependency.
A basename is a lookup hint, not content identity.

An archive-contained PIG/HAM also records `container`, naming its existing
`d1_hog`, `d2_hog`, or `mission_hog` entry. The verifier checks that entire HOG's
identity first, then hashes the selected member through its bounded DHF directory
(up to 65,536 entries and 128 MiB per member). It respects mission-HOG precedence
over loose data and rejects wrong loose files shadowing appended base HOGs.
No files are extracted. Untracked archive owners, unsupported additional mission
HAMs, or an asset change across recording levels make the recording explicitly
unknown rather than silently omitting a dependency. Recording continues, and
unknown recordings remain excluded from verified shareware selection.

## Before playback

The shared implementation is `input_demo_assets.py`. The PowerShell adapter in
`../helpers/input_demo_assets.ps1` invokes it; Python callers use the same module.
It checks the recording's exact metadata, engine direction, and available bytes
before the host executable starts. Missing files, ambiguous case-insensitive
names, mismatches, and preferred retail files shadowing recorded demo files fail.
The native engine independently compares the actual selected/loaded assets before
replay input, covering mount precedence and level-specific selections.

A native D1 recording can intentionally run through D2 with `-D1InD2`: its
recorded D1 dependencies must still match. Any extra target-engine D2 dependencies
are not retroactively attributed to the D1 recording. Native engine reporting
identifies those additional dependencies. A D2 recording is not translated to D1.

Missing or explicitly unknown fingerprints are **unverified**. Playback requires
`-AllowUnverifiedAssets` (Python CLI: `--allow-unverified-assets`) for a deliberate
historical investigation. The warning remains visible. This switch does not
bypass malformed metadata, incomplete `complete` role sets, or a known byte
mismatch. It does not change, upgrade, or backfill a recording.

## Run every verified shareware recording

```powershell
pwsh -NoProfile -File android/helpers/setup_shareware_test_data.ps1 -Game both -Download
pwsh -NoProfile -File android/tests/run_input_demo_regressions.ps1 -SharewareOnly -RunMode headless
pwsh -NoProfile -File android/tests/run_input_demo_regressions.ps1 -SharewareOnly -RunMode graphics
```

`-SharewareOnly` searches the regression and test-fixture roots by default. Use
`-DemoRoot` to choose a different root, `-Game d1`/`d2` for a recorded-engine
subset, or `-RecordedGame d1 -D1InD2` for explicit imported-D1 playback. `-ListOnly`
prints the selected files without running an engine. Empty selection fails,
including in CI; it can never be reported as a passing replay run. This selection
always requires strict unchanged expected-result comparison and clean native exit.

Recognition requires a complete recorded base-asset set with matching sizes and
SHA-256 values from `fixtures/demo_import_oracles.json`, cross-checked with the
source archive identity in `game_data/test_data_manifest.json`. Names do not
establish an edition. Every recorded D2 companion asset must also match the
pinned public release. Custom mission HOGs can use shareware base assets; their
own recorded hash is still required and checked at playback. Unknown, missing,
malformed, incomplete, or nonpublic base hashes are excluded. The historical
opt-in cannot be combined with verified-shareware selection.

`engine-smoke.yml` runs this nonempty selection independently for both engines.
The normal runtime smoke also replays the recorded public flight fixtures.
Fresh captures must include complete matching asset metadata; the capture tool
refuses to pack a new candidate without it. Historical fixture headers are never
edited to claim that their original sources were verified.

## Execution-path audit

- Windows/Linux graphical, no-present, and console replay use
  `run_input_demo_replay.ps1`, including the interactive picker
- Headless/batch/regression wrappers, runtime smoke, automatic Android-recording
  exports, determinism matrices, D1 parity, compact public positive/negative
  runners, Windows window-progress, and trace-equivalence probes invoke that
  common host gateway
- `check_ordered_input_runtime.py` additionally gates its one direct native replay
  used to test an unwritable result destination
- Android store playback in `generate_store_assets.py` checks bytes from the
  launcher's active file set through the same verifier before launching the game;
  subordinate still/featured/boss replay helpers use this method too
- Frozen D1 parity harnesses include the verifier and its PowerShell adapter
- Offline trace comparisons, corpus parsing, and import validation do not launch
  input playback, so they do not claim runtime asset compatibility
- Classic `.dem` playback, including the Android FOV round trip, has a different
  format without this header. It explicitly reports this verification unavailable
  and is not counted as verified input-demo coverage

The scope is the recorded HOG/PIG/HAM dependencies. This is not a signature over
all loose files, POG/HXM replacements, audio, renderer settings, or arbitrary mods,
and asset compatibility does not establish full simulation parity.

## Checks

```sh
python3 android/tests/test_input_demo_assets.py
```

The asset-free tests exercise native D1/D2/imported D1, wrong-source hashes,
missing/unknown and malformed metadata, diagnostic renames, retail shadowing,
custom missions, incomplete companion sets, source-manifest disagreement,
nonempty selection, falsely labeled filenames, and Android active-set bytes. The
PowerShell test wrapper is registered in the normal automation catalog and tooling
smoke suite.
