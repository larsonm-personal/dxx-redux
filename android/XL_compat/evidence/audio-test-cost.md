# PCM oracle preparation cost

The shared native/Android PCM oracle previously prepared all four generated WAVs
and all 183 classic samples for every capture. Most captures contain only a few
distinct samples. It now prepares fresh expected PCM only for those samples,
deduplicating repeated references within that capture. It reads the classic bank
only when a captured voice uses it. There is no cross-capture cache.

The sample generator, classic source conversion, resampler, gain/pan calculation,
voice iteration, clipping and per-sample comparison are unchanged. Invalid
sample indices still fail checked lookup. Capture duration, output assertions,
test membership, source oracles and all timeouts are unchanged. This only changes
the diagnostic helper in `runtime/shared_audio_pcm_test.h`.

## Measured verification

Before: `xl_more_68_external_audio` at
`70938a98ba081d778499bcb35243d750d8b006ec`.
After: the one-file oracle change on `xl_more_69_audio_test_cost`.

- Windows: both 23/40 and 27/40 runtime fixtures pass, including 1,440 actual
  source creation comparisons per format, scopes, saved playback and exact
  Redux isolation. Complete runner time is 47.207 seconds after the change.
- Linux: the unchanged `xl_named_audio_runtime` passes in 59.74 seconds, versus
  65.46 seconds in the preceding run. The preceding run also scheduled the
  effect gate concurrently, so these are local observations, not an isolated
  estimate of the optimization's speedup.
- Android: the same 101 steps and 119 output cases pass in 318.795 seconds,
  versus 444.955 seconds before. The linked-audio phase drops from 74.151 to
  34.976 seconds. These are single local runs, not a promised CI duration.

Android checks 1,556,480 samples at 48 kHz, including all eight external, ten
route and thirteen surface cases. Callback-sized capture tails vary between
runs; requested frame counts and every output assertion remain unchanged.
All 26 protected files are restored with matching hashes. Run ID:
`1a102aff75be415795e1f3372ffbad16`. The 480-second timeout is unchanged.

Artifacts are in `temp/xl-ambient-audio-20261030` in the active worktree. Baseline
receipts use `android-*` and `linux-audio.log`; the optimized run uses
`oracle-android-*`, `oracle-linux-runtime.log`, `oracle-windows-runtime-detail.log`
and `oracle-format.log`.

Qualified binary SHA-256 values:

- Windows: `0454ee74233f8b3fa599c549ab9a3c36ed739ef01ff5a37a13b857eba06c0aa6`
- Linux: `d3a12c78026ee3208f8a03ed8004be20bca560b9359b2e3c5acc6dd7e68c685f`
- Android APK: `45226135382d8c38a3375447ba9a2850de4fef56410715e4e4a147116105f8c0`
