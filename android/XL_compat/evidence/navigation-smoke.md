# Original XL assisted navigation smoke

`android/tests/run_xl_navigation_smoke.py` loads an unchanged original HOG and
mission descriptor through `StartNewGame`, then uses the existing headless
Guide-Bot route controller to reach segment 20. It requires a successful process,
`confirmed` route, actual XL runtime admission, the exact mission/level/format,
nonempty geometry, at least three distinct visited segments and the requested
final segment. Parser admission, unsupported results, timeouts and baseline-matched
failures cannot pass. No retry or replacement sample occurs after failure.

The `retail-xl-fast` owner runs one mission-first, then level-first sample after
building its existing headless targets. Its default selection seed comes from the
target commit's first 16 hex digits; `--navigation-seed` overrides it. SHA-256
selection is stable across Python versions. The engine retains seed 1 at 60 Hz.
Simulation is limited to 30 seconds and each process to 15 wall-clock seconds.
`selection.json` records seed, mission and level beside private engine output.
Raw receipts and mission data must stay private; no new public asset checkout or
credential path is introduced. The existing immutable asset and manifest pins
in `xl_private_assets.py` remain authoritative.

## Qualified pool

| Mission     | Level | Format | Segments visited | Frames | Local wall seconds |
| ----------- | ----- | ------ | ---------------- | ------ | ------------------ |
| D2-XL       | 1     | 15/33  | 9                | 189    | 0.72               |
| D2-XL       | 3     | 15/33  | 10               | 181    | 0.92               |
| Observatory | 1     | 17/36  | 9                | 238    | 0.82               |
| Neptune     | 1     | 23/40  | 4                | 76     | 0.77               |

Measured on Linux with the actual runner, excluding compilation/staging. The
pool's mission files and manifest were also verified against assets revision
`9dad21bd65ba05665ac6fe315f37fe7461e85725`; their identities match the existing
trusted pin. No asset repository metadata changes are needed. Only the selected
HOG and descriptor are copied into the isolated run directory.

D2-XL level 2 loaded and moved through 50 segments but timed out at segment 117;
it is deliberately not eligible for this initial bounded objective. The newly
uploaded `sphere-1.6.zip` level 1 was tried directly and rejected at its bubble
effect profile before navigation. It is not silently skipped or counted as smoke
success. The remaining newly uploaded campaigns are not qualified by this test.

This is assisted navigation, not a campaign, combat, rendering or ordinary
Guide-Bot gameplay qualification. The existing controller spawns/positions a
companion at the authored player start, normalizes its radius, removes ordinary
robots, protects the player, assists projectiles and pauses reactor countdowns.
It does not grant keys or delete walls for this smoke. Reaching one destination
says nothing about unvisited branches or finishing a level.

## Reproduce

Build `dxx-redux-d2-headless-route` from `d2` with OpenGL enabled and stage the
existing verified XL inputs using the trusted retail workflow. For a standalone
run with an already built executable and a fresh private work directory:

```sh
python android/tests/run_xl_navigation_smoke.py \
  --engine BUILD/main/dxx-redux-d2-headless-route \
  --stage game_data/retail-ci/xl --data game_data/retail-ci/d2 \
  --work temp/xl-navigation-private --seed 318
```

Seeds 0, 9, 3 and 1 select the four rows above, respectively. A seeded sample is
one fast check, not exhaustive coverage of the pool. Extend the allowlist only
after running and qualifying the exact new level/objective pair.
