# Reproducing the Counterstrike sound problem

Use a build containing the `[SFX] trace_v=1` diagnostics

1. In the launcher's Advanced page, enable **Game Logs** before starting D2
2. Keep the configuration that produces the unexpected sound for the first run
3. Start Counterstrike or restore a save, trigger the sound, and note approximately
   when it occurred and whether the robot was spotting you, firing, or dying
4. Return to the launcher and use **Share** on the current debug log
5. Change one setting/package at a time, restart D2, and repeat. Keep a log from
   both a bad run and any good run; label what changed

Do not clear or reinstall game data to collect this evidence. Logs contain local
asset paths so the loaded file/package can be identified. A screen/audio recording
is optional if the exact triggering event is unclear

## What the trace records

- Bank source at opening, sample name, byte offset, read success, and loaded
  sample fingerprint. Fingerprints use FNV-1a 64-bit, not SHA-256
- D2 HAM, sound-header, extra-HAM, HXM, and HAM-patch source paths when opened
- Mission/level, ordered search paths, and robot sound mappings at the first
  traced playback in each loaded level. Each `event=logical:sample` pair maps the
  robot's sound ID to the physical sample index. The catalog's IT Droid is robot
  37; these are mapping candidates, not identification of the emitting object
- Current sample bytes versus their bank-load snapshot
- Current sample bytes versus the bytes used to create the mixer's cached sound
- Cached output bytes versus their original converted snapshot, plus sample
  rates, channel count, and SDL output format

Stable repeated plays are silent. The trace reports the first sample state and
up to eight distinct states per sample between level/load/conversion resets,
then reports suppression. Expensive playback fingerprints run only while Game
Logs is enabled. Load/conversion fingerprints are retained in memory even when
logging is disabled; sparse asset-opening lines are always logged so their
origins are available when logging is enabled later

`context` groups a level load; `bank` identifies a bank opening within that engine
process. Preserve the complete log so sample lines can be joined to their bank
source. Normal debug-log retention still applies

## Reading comparison results

| Field | Meaning |
| --- | --- |
| `bank_match=1` | Current bytes and length match the sample captured after its bank read |
| `bank_match=0` | Sample changed since that read; intentional custom/D1 compatibility replacements must be excluded before calling this corruption |
| `cache_input_match=0` | Current sample differs from the input used for the cached conversion |
| `cache_output_match=0` | The converted output bytes/length changed after conversion |
| `source_rate` differs from `cached_rate` | The cached conversion used a different source sample rate |
| Any match field is `-1` | Comparison is unknown, not a mismatch |
| `read_ok=0` on a tracked sample | The bank read was short/failed; its load fingerprint is intentionally unknown |

If all bytes/rates match, compare robot mappings and bank origins between good
and bad runs. Matching input/output data cannot exclude a later mixer/device
output issue. The log does not capture the final mixed audio sent to the speaker

### Local stock-bank reference

The inspected retail/GOG `descent2.s22` has SHA-256
`4f10632dd4efcbffe532c35b6763edd22817135442bbcc4171381706f3893728`
and 183 samples. Its sniper-related samples provide reference values for the
phone trace; use the robot mapping lines to identify which actually applies

| Sample index | Name | Bank offset | Bytes | FNV-1a 64-bit |
| --- | --- | --- | --- | --- |
| 48 | `sniper_1` | 1184987 | 25106 | `97af1ee6e82f3294` |
| 49 | `sniper_2` | 1210093 | 24498 | `69d8788c02bdd1b0` |
| 53 | `snipe_1` | 1335652 | 24098 | `9a629f9713cdff17` |
| 54 | `snipe_2` | 1359750 | 23010 | `7e08721730a93677` |

These values apply to that exact bank, not every legitimate D2 release

The stock Counterstrike emulator run mapped robot 37's see sound as `59:53`
(`snipe_1`) and attack sound as `60:54` (`snipe_2`)

## Scope and limitations

Playback instrumentation covers the SDL_mixer backend used by the Android game.
It also runs in D1, with load provenance for PIG-backed samples, including
decompressed shareware samples. D1 Mac external samples and D1-in-D2/custom
replacements do not yet have individual replacement-owner snapshots; their
bank comparison can be unknown or legitimately differ. The target reproduction
is ordinary D2 Counterstrike

No samples are altered, caches flushed, packages disabled, or audio files written
by the diagnostic. A match uses a non-cryptographic fingerprint and is evidence
for diagnosis, not a proof against every form of corruption

## Verification

The native host CTest target `test_sound_trace_fingerprint` checks byte changes,
stale conversion input versus altered conversion output, length differences,
known FNV output, and unknown/oversized buffers. It is included in the existing
`android/tests/test_native_host_unit_tests.ps1` runner for both games

`android/tests/test_sound_trace.ps1 -Install` runs a stock Counterstrike smoke
test on the repository emulator and verifies bank provenance, IT Droid mapping,
and matching load/input/output fingerprints in logcat. It resets the test
emulator's default game state; do not use this test runner on the reproduction
phone. Manual phone collection uses only the steps at the top of this document
