# Channel-preserving WAV foundation

Named XL audio needs authored channel data. The existing classic replacement
decoder intentionally averages stereo into a mono Redux sample. A new
`decode_wave_channels` consumer preserves mono or stereo PCM in authored channel
order. Both consumers use one bounded RIFF reader; the existing mono API,
quantization and sample-rate conversion remain unchanged.

The common validation still requires little-endian PCM RIFF/WAVE, 8- or 16-bit
samples, one or two channels, consistent alignment/rate and complete frames.
Duplicate chunks, malformed lengths, unsupported codecs, missing padding and
oversized input reject before publishing samples. Input remains bounded to
16 MiB and decoded frames to 8 Mi frames. The channel-preserving result has at
most two signed 16-bit samples per frame, bounded to 32 MiB.

## Independent checks and original bytes

The required `xl_wave_channels_reference` test compares the native output with
Python's independent WAV reader. Its 28 generated cases cover mono/stereo,
8/16-bit PCM and seven rates from 8 to 192 kHz. Every channel sample, frame count
and format field must match. Existing malformed controls exercise both consumers;
the original Dragon Pit mono golden samples still match.

The unchanged Pumo HOG is pinned to
6e3e723e549897188a8fdc19db8b04630de9d428d2b31f8e3f48af108915b043.
All 25 embedded WAVs are 44.1 kHz stereo 16-bit PCM. The independent original
comparison checks all 6005775 frames, 12011550 channel samples and 24023100
decoded bytes on Windows and Linux. It requires exactly 25 files and checks the
HOG hash before and after inspection. Native HOG indexing stages the unchanged
WAV members for the independent reader; temporary original files are removed.

The normalized per-file reports are identical across both platforms, with
SHA-256 ca5383499027c842c5461e67fe2c7d324b22317c979094263ab5bf68875c34bd.
The report remains in local/private artifacts. Repeating the comparison preserves
its bytes and modification time when unchanged. No original audio is published.

Run the public comparison with CTest or directly:

```text
python android/XL_compat/tests/wave_channels_reference.py --native <xl_wave_test> --work <scratch>
```

For an unchanged original HOG, also provide `--hog`, `--sha256` and
`--expected-waves`. The native helper handles the archive format; the Python
comparison does not duplicate HOG parsing.

Windows passes the two WAV contracts in 0.76 seconds; Linux passes those plus
the original mono golden control in 0.81 seconds. The new independent public
test takes about 0.7 seconds on either host. All nineteen CI owner tests pass.
The normal Android x86_64 build succeeds; APK SHA-256 is
092bc7d605c660c752dfa06a45e74ba2f8d4755c706d71e4c63779227c054b52.
This increment adds the independent test to the required 39/40 public and
95-case private selections. The preceding complete 94-case runtime result is
recorded separately in [level-messages.md](level-messages.md); it is not relabeled
as a new full-suite run.

## Playback remains unfinished

This is a decoding prerequisite. Action 30, ambient sound objects and original
Pumo admission remain gated. The source audit identifies 60 authored sound
actions, including looping and repeated playback. The pinned SDL mixer path
preserves stereo; its Duration() adds half a second before fixed-point conversion,
and time zero maps to infinite looping. These details, actual mixed output,
voice ownership, resource precedence, pause/stop, disk/rewind and Redux recovery
belong to the complete named-playback increment. Passing sample comparisons
does not claim audible playback or mission progression.
