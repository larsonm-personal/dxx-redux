# Overnight recommended-mod fuzz testing

Status: design only, requested 2026-10-11; no overnight job has been started

## Objective and entry point

Build a reproducible, bounded stateful test campaign that finds native crashes, failed valid restores, hangs and incorrect mod activation while starting D1/D2 levels with real recommended packs. Proposed runner: `android/tests/test_mod_fuzz.ps1 -Serial ... -Seed ... -Hours 8 -Profile overnight`, with `-ReplayCase case.json` and a short `-Profile smoke`

Use the existing launcher automation, `run_test.ps1`, native fixtures and introspection. Avoid random screen coordinates. Run serially per device because launcher, logs and save state are shared. Default to a disposable emulator/test package and dedicated file set; reuse `Assert-IsolatedPhysicalTestApp` before any physical-device run

## Inputs and deterministic cases

Read the checked-in recommended-pack catalog and resolve exact local/downloaded assets before the clock starts. Record hashes, sizes, game applicability, archive order, APK/build ID, data hashes, device/driver and seed. Missing proprietary data or packs are explicit coverage gaps, never silent passes. Keep network downloads and provisioning outside the timed campaign

The current nine recommendations include D1 menus/fonts, Mac missing sounds, UUD1/UUD2 texture repairs, UUD2SP sounds, D1/D2 high-resolution sounds and D1/D2 256px textures. Also test available 128/512px texture variants as mutually exclusive alternatives, with precedence cases for intentionally overlapping packs

Materialize every case before execution: case ID, PRNG/version and seed, effective ordered mods, game/content mode, mission/level, configuration, transition sequence, action seed and timeouts. A replay must not depend on random choices or leftover device state from earlier cases. For stateful failures retain the full prefix and the intermediate saves

## Coverage schedule

1. Mandatory baseline and single-pack starts in native D1, native D2 and D1-in-D2. Include PC demo/retail distinctions and a separate Mac D1 fixture for the Mac-only recommendation; do not count an inapplicable pack as exercised
2. All supported pairs, then all-on and targeted order reversals: UUD textures versus high-resolution textures; UUD2SP versus high-resolution sounds; UUD2SP plus UUD2TP composed HAM patches. Verify effective owners through diagnostic introspection so a passing case cannot simply have failed to activate its packs
3. Level coverage across First Strike, Counterstrike, Vertigo (especially level 2), all D2 palettes and available secret levels. Derive valid level IDs from engine/mission metadata, not guessed numeric ranges. Include UUD demonstration and extra-asset fixtures because base levels cannot exercise unused added textures/effects
4. Seeded weighted subsets and permutations for the remaining budget. Favor combinations/levels and transitions not covered yet. Use some repeats with identical cases to separate deterministic crashes from timing failures
5. Stateful sequences: installed-disabled -> enabled -> launch; enabled -> disabled while retaining installation -> relaunch; precedence reversal; level A -> B -> A; game/mission switch; abort to menu -> new game; save -> return to launcher -> toggle -> cold resume; pause/resume and rapid but valid menu navigation

Do not use `delete_mod` to represent disabling. Existing `test_uud_mod_combinations.ps1` uses deletion for its off phase and omits UUD2SP; the new campaign must use the launcher's enabled toggle and assert that archive files and manifest rows remain installed

## What each case does

- Apply the manifest through supported launcher commands and verify readiness, effective enabled flags and order
- Launch through the launcher, including normal projection/composition work. Fresh-process cases prevent cached state from hiding problems; explicit in-process re-entry cases probe stale references
- Wait for a real playable level and expected mission, level, player object and live frame advancement; assert no restore/menu fallback masquerades as a successful start
- Run 10-30 seconds of bounded actions: turn/move, fire at authored safe targets, open a door, exercise effect animation and a monitor, cycle the automap/settings, pause/resume, then exit through a named path. Use authored poses and goals for effects requiring specific geometry
- Sample diagnostics briefly to establish actual loaded/played mod owners. Normal soak gameplay has Game Logs off so sound-report hashing does not dominate the workload; include a smaller logging-on/off transition set
- For save-toggle cases, compare semantic player/world state as defined by the save-compatibility design. Until that feature is implemented, label the known effects mismatch as an expected limitation with an explicit issue ID, but still fail any native crash, lost save or broken recovery menu
- Exercise both touch and keyboard/controller Quit after failed restores. Require clean expected exit, no remaining game PID, and a responsive launcher

## Bounds and outcomes

Suggested initial budget: 30 minutes mandatory baseline, 90 minutes systematic pair/level coverage, 5 hours weighted transitions and 1 hour repro/minimization. These are scheduling targets, not coverage promises; report actual throughput and allow unused time to flow to later phases

Give launches/restores a 45-second normal deadline, a whole case 120 seconds, and each adb operation its own timeout. Slow first-load cases can have a declared larger budget. Enforce a monotonic overall deadline and leave time to flush evidence. Catch process death, ANR, black/stuck state and timeouts independently; a loading timeout is not automatically a crash

Persist planned/started/completed JSONL records after every case and a stable aggregate JSON summary. Outcomes: passed, crash, hang, assertion failure, unsupported combination, missing prerequisite, infrastructure failure, expected limitation. Infrastructure recovery must preserve the original failure and must not convert it into a pass

## Failure artifacts and shrinking

Before restarting or clearing logs, preserve tombstones/build IDs, crash diagnostics, logcat/debug logs, automation result/log, last introspection, effective launcher paths/manifest, test configuration, relevant saves and exact case sequence. Keep sensitive/proprietary game data local; reference hashes in shareable reports

Reproduce in a fresh process. Minimize within a bounded share of the overnight budget by removing packs, shortening the action sequence, removing transition steps and simplifying precedence while keeping the failure signature. A successful reduction must fail again; preserve the unreduced case when a failure is intermittent. Deduplicate by symbolicated stack/signature plus restore stage, retaining occurrence counts and distinct configurations

Use `retain-recent-artifacts.ps1` before creating a campaign directory; keep three prior generations and retain current failure evidence until the run finishes. On interruption, resume from the durable case journal without repeating passed cases unless requested

## Implementation and acceptance

- Reuse `test_uud_mod_combinations`, `test_uud2sp`, `test_dxa_asset_usage`, `test_hires_pack_release` and native `mod_sounds`/`mod_renderer` fixtures as building blocks, not parallel independent device controllers
- Add owned support JSONC scripts with `_standalone: false` and `_owner: test_mod_fuzz`; register the runner in `test_suite_coverage.ps1`. Normal master-suite execution uses only bounded smoke coverage, with a corresponding timeout in `run_all_tests.ps1`; overnight mode remains an explicit invocation
- Test campaign control with simulated crash/hang/adb failures, durable restart, deterministic enumeration, deadline enforcement and shrinking. Then run the real smoke suite on both games, inject one controlled failure to verify artifact capture, and finish one real eight-hour campaign
- Deliver coverage counts by pack pair, order, content mode, palette, level and transition, plus exact replay commands and minimized failures. Zero crashes in an incomplete campaign is not a claim that every combination was exercised
