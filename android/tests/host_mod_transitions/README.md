# Shared PCM mod transition fixture

This bounded host fixture compiles the production `android_mod_assets.cpp` and the
engine's `ignorecase.c` against real PhysFS and SDL. The only adapters are engine
sound/argument globals, no-op Android logging/trace transport, and the simple
case-sensitive PhysFS existence wrapper. It defines `ANDROID` for diagnostic
interfaces, without pretending that the host executable is an Android engine.

It independently extracts public sound-pack bytes using Python's ZIP reader,
then exercises replacement selection, archive priority across `.raw`/`.r22`,
D1 and D2/imported-D1 mount namespaces, resampling for the legacy fixed-rate
backend, repeat apply, reset/unmount/re-enable, and A -> B -> A transitions.
Resident PCM is compared byte-for-byte with independently extracted payloads.
Loaded owner, size, rate, original pointer restoration, resident fingerprint,
and the explicitly invoked played callback are checked. Deliberately incorrect
owner and PCM expectations must fail the matching assertions. Corrupting a
resident byte must suppress callback attribution until it is restored.

## Boundaries

Stock sound slots are small synthetic PCM buffers, not retail/shareware banks.
No game-format data is authored or presented as a playable game fixture.
The played callback is invoked directly; actual mixer playback is **not** tested.
This does not test menus, Android launcher wiring, GPU textures, presentation,
UUD retail banks, real game save/reload, or cross-process engine transitions.
Failures are component-level findings until reproduced in the actual engine.

## Build and run

Use installed SDL 1.2, PhysFS 3, and nlohmann JSON headers, or pass an isolated
prefix with `CMAKE_PREFIX_PATH` and `JSON_INCLUDE_DIR`. The fixture records and
checks exact PhysFS header/runtime version agreement. For Android VFS-version
parity, build the sibling `host_texture_transitions/physfs` helper and pass the
`PHYSFS_INCLUDE_DIR` and static `PHYSFS_LIBRARY` values in its generated
`paths.txt`. That helper verifies the production dependency pin. Sanitizers are on by
default. LeakSanitizer is disabled in the runner because the executor uses ptrace;
set `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` outside that environment to
enable leak checking. Address/undefined checks remain active. Build artifacts and run evidence belong under ignored `temp/`.

```sh
cmake -S android/tests/host_mod_transitions -B temp/host-mod-build \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build temp/host-mod-build -j2
python3 android/tests/host_mod_transitions/run.py \
  --assets temp/overnight-assets --build temp/host-mod-build \
  --out temp/host-mod-seed-20261014 --seed 20261014 --steps 100
```

The assets directory must contain the already obtained public
`d1-hires-sounds.dxa` and `d2-hires-sounds.dxa`. No network access is performed by
the runner. It records archive SHA-256, independently extracted PCM SHA-256,
full transition sequences, logs, and summaries. Replay the exact scenario with:

```sh
temp/host-mod-build/host_mod_transitions_d2 \
  temp/host-mod-seed-20261014/d2-scenario.json temp/replay.json
```

A run is capped at 2,000 transitions per game with a subprocess watchdog of
`max(120, steps)` seconds. Use `--exhaustive-payloads --steps 400` to cover every
public PCM in its native/imported namespace across all backend/output-rate
settings before randomized transitions. Both reset-before-unmount and
unmount-before-reset teardown preserve the expected ownership lifecycle.
`*-coverage.json` records distinct configuration states, ordered transitions,
and sound/config selections, separately from execution counts.
Each step selects 12 bounded samples. Result artifacts should be retained or
removed by the owning campaign, rather than accumulated without a limit.

For a long same-process resource observation, set `record_usage: false` in a
materialized scenario. Full per-step output is then suppressed; the original
scenario remains replayable. Linux RSS and open descriptor counts are sampled
every 50 transitions and after teardown. Compare matching mount counts after
warm-up; ASan quarantine and allocator retention affect RSS, so growth alone
is not a leak verdict. The runner itself does not impose a resource-trend pass
threshold.

Use `--enumerate-configurations --exhaustive-payloads --steps 1500` for the
complete bounded primary domain: 256 D1 and 1,024 D2 states, combining ordered
applicable public/authored sound packs, native/imported identity, backend, four
output rates, and both teardown orders. D1 packs use `d1-mods` in D2 and root in
D1; D2 packs stay at D2 root. These are component inputs compatible with launcher
namespace rules, not a test of launcher publication or every launcher setting.
Prior arbitrary-namespace component experiments are retained separately.

Native timeouts retain logs plus `failure.json` classified as
`timeout_incomplete`; they are never counted as passes. A parent campaign may
request a between-batch stop by creating `.pause-requested` in the output
directory's parent; the runner exits 75 before starting work.
