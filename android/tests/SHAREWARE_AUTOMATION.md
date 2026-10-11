# Public D1/D2 automation profiles

27 existing automation scenarios (51 declared engine variants) now default to
pinned PC shareware. The source scripts retain their retail dependencies and all
behavior assertions. `run_test.ps1 -DataEdition retail` selects the original
retail mission/level/save namespace and stages the original verified dependencies.
No copyrighted retail files are supplied or downloaded by these profiles.

## Run

Prepare public fixtures once (the installer extraction is hash-verified):

```powershell
pwsh -NoProfile -File android/helpers/setup_shareware_test_data.ps1 -Game both -Download
pwsh -NoProfile -File android/helpers/run_shareware_tests.ps1 -ListTests
pwsh -NoProfile -File android/helpers/run_shareware_tests.ps1 -Game both -Serial emulator-5554
pwsh -NoProfile -File android/helpers/run_shareware_tests.ps1 -Filter '*death*' -Game d2
```

The public runner selects only explicitly audited existing scenarios and invokes
the runtime texture fixture through its existing PowerShell integration owner.
It never treats an owned fixture as a new standalone test. Tests run serially,
and nonzero/skip results fail the aggregate. A source-built debug APK and a ready
Android target are prerequisites; use the repository emulator/build helpers.
The full master suite still includes valuable retail campaigns and is not a
public-data-only suite.

Individual existing scenarios support explicit editions:

```powershell
pwsh -NoProfile -File android/helpers/run_test.ps1 test_engine_prefs_unified.jsonc -Game d1
pwsh -NoProfile -File android/helpers/run_test.ps1 test_engine_prefs_unified.jsonc -Game d1 -DataEdition retail
```

For runtime texture options, invoke its owner
`android/tests/test_ogl_runtime_texture_options_unified.ps1 -DataEdition retail`
to retain the generated capability/probe/confirmation steps in retail mode.

An unaudited script rejects `-DataEdition shareware`. Missing/corrupt public files
fail with setup guidance; tests do not silently download or fall back to retail.
Both demo asset sets are staged together because the D2 mission picker needs
more than one installed mission. Before staging either edition of an audited
scenario, the stopped test app's disposable `files/imported/sets/default` set is
cleared. Other imported sets are untouched. Staged bytes are SHA256-verified,
preventing stale retail HOG/HAM/PIG or loose levels from shadowing demo content.
Legacy retail provisioning also clears a previous demo set before checking
filenames, so it cannot accidentally retain public D1 assets beside retail D2.
`-PreserveState` is incompatible with these isolated profile runs.

## Fixture-specific changes

- Both engines select their actual public mission titles
- Death/respawn runs the same explicit damage, held-button/release gating and
  respawn assertions at demo level 1 (typed `01`); explicit retail keeps D1 level
  11 and D2 level 24
- D2 missing-pilot resume uses the engine's `d2demo` save namespace; D1 retains
  `default`. Retail D2 retains `d2`
- D1-in-D2 cockpit rendering selects the D1 demo while using the D2 engine
- Idle-screen-saver now declares its original retail prerequisites explicitly,
  rather than accidentally reusing previously staged shareware in retail mode

## Verification boundaries

`test_shareware_automation_profiles.ps1` exercises all 51 resolved variants and
checks that every action, assertion and timeout is retained, with only the above
fixture identifiers changed. It checks exact public hashes, original retail
dependencies, owned-runner routing, retail/public staging transitions, device
hash failure and bounded cleanup. This is host contract coverage, not proof that
51 scenarios executed on Android.

At this migration commit, Android execution of the newly profiled variants is
pending the emulator-support segment. All 51 device variants are unexecuted.
The existing native generated-replay and public metadata baselines from the
previous segment are retained. No native engine implementation changes are made.

The full automation catalog validator and direct master `-ListTests` discovery
passed locally. The child-process master-catalog regression is blocked in this
container by its existing Linux supervisor `/proc/self/task/.../children` access
failure; its safety guard has not been disabled.

## Deliberately retained retail coverage

- Async checkpoint secret entry/revisit: PC D2 demo has no secret levels
- Launch-to-automap paired shoot-switch route: retail D2 level-2 geometry
- Boss health bars, endgame, and content-specific wall/trigger routes
- Full music-track controls: D1 demo has no track slots 11 and 12
- Recorded replay corpora and saved worlds tied to original mission hashes
- Disc/GOG/SAF imports, mods and external mission archive fixtures

See [the full 698-file inventory](shareware-audit/inventory.tsv) and
[per-scenario semantic review](shareware-audit/selected-cases.md). The original
388 JSONC + 310 PowerShell snapshot contains 111 no-data tests, 62 public
candidates, 165 content-locked tests and 360 owned fixtures. The new profile
contract test is additional to that snapshot. Candidate classification means that migration is plausible, not that
it has been implemented or runtime-verified.
