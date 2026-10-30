# Required XL CI checks

The fast private selection reuses the existing trusted `retail-ci` workflow,
read-only asset token and immutable checkout. Public pull-request jobs remain
asset-free. Private files, original inventories and raw engine output stay on
the private runner; published receipts retain fixed counts and outcomes.

`xl-fast` selects one Linux owner, `host-profile:retail-xl-fast`. It builds the
normal D2 metadata executable and the developer lifecycle executable, then runs
105 required CTests in independent directories, two at a time. Missing tests,
skips, disabled tests, errors and failures all fail the lane. The adapter checks
the same exact membership as the owner, replacing the stale hardcoded count of 40. The longer `xl-originals` selection retains its separate campaign controls
and explicitly reported unfinished level-two experiment.

The fast selection also checks the registered commands for 36 tests whose
source comparisons are optional in CMake. Each required reference executable
must exist, including Observatory's no-damage gates and lava's separate motion
oracle. A runtime pass with an omitted source comparison cannot satisfy this
selection.

## Public source coverage

Every matching pull request builds the pinned XL source adapters on Windows and
Linux, without private files or credentials. The public owner requires 43
contracts on Windows and 46 on Linux, including the standalone XL source comparisons,
named-audio event and voice contracts, an independent WAV-channel comparison,
the POG reader/source contract, Linux's offscreen fog/POG backends and actual
SDL_mixer output comparisons.
Both platforms reject omissions, duplicates,
skips and unexpected tests before granting a pass. Optional source executables
must be present in the registered commands. Runtime comparisons that need actual
game assets remain in the private selection.

The public and private owners share the same pinned checkout, consumed-file
hashes and source parameters. Clean local public runs pass in 56.578 seconds on
MSVC and 64.013 seconds on Linux GCC, including checkout and build. Their test
phases take 17.438 and 5.988 seconds respectively. These are local measurements,
not hosted-run durations.

```text
python android/XL_compat/tests/public_contracts.py --work temp/xl-public-ci --gpu --audio
```

Omit `--gpu --audio` on Windows. The GPU cases require Linux EGL/Mesa; the audio
oracle requires SDL 1.2 and SDL_mixer development libraries. Explicit options
make missing renderer or audio contracts fail selection instead of disappearing.

## Coverage

| Area                   | Required evidence                                                                                                                                           |
| ---------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Format and admission   | Canonical 25/27 runtime, older admitted layouts, malformed inputs, bounded HOG streaming, unsupported requirements and recovery                             |
| Events and world state | Wall/object triggers, delays, flags, teleports, speed boosts, spawn points, resources, wall/door variants, lights, messages, texture actions and countdowns |
| Actors and generators  | Robot/equipment assignment, 25/27 startup, boss ownership/groups/transport, liquids, repair and capacity controls                                           |
| Persistence and Redux  | Real disk/checkpoint ownership, mission changes, stock recovery and a normal executable without developer probes                                            |
| Source comparisons     | Required extracted reference readers, arithmetic, geometry, resource precedence and gameplay functions; no optional missing oracle                          |
| Rendering              | Production offscreen fog backend, depth formats, MSAA, GL state and inactive stock behavior; physical Android remains separate                              |
| Original bytes         | Nine older witnesses plus all seven outcomes in the original modern Boiling Point HOG; three full 27/40 source-reader comparisons                           |

The nine older witnesses retain positive inspection for 15/33, 17/36 and 23/40.
The 15/34 witness `fbasin.rl2` retains its specific invalid packed-light rejection;
it is no longer rejected merely for its format. Controlled native and source
tests separately verify valid 15/34 layouts. Other witnessed pairs remain
explicit format rejections. Boiling Point has one decoded 23/40 level
and three decoded 27/40 levels. Its other three levels retain specific packed
light, movement-layout and reactor-target rejections. The original HOG is used
whole; selecting a member for source comparison does not repack or modify it.
Inspection and source parity do not grant original gameplay admission.

The native command is:

```text
python3 android/tests/run_xl_originals.py --repo . --work temp/xl-fast --fast
```

It requires the verified retail and XL stages, including `--modern` staging.
After this workflow is merged into `cmake`, select `xl-fast` in the existing
Extended test suite workflow on that branch. The same owner can be selected by
the nightly rotation. Private execution is deliberately unavailable to PR or
branch workflow contexts; the trusted controller and target must agree.

## Existing inputs

Private repository revision: 8c9988a96a7e306f8aac04c85e2610492917b7ac.
Manifest SHA-256:
cfa5a81f003958798cb37ee177f07b74c77812c4ab43d1986ccff37244347647.

The original `single_player/boilpnt.7z` is already present and has SHA-256
c16004c8e08354bd554f629a1f29f060207dbfbb5ff9786f73ab5a55ccba25d2.
Its unchanged 68710431-byte HOG has SHA-256
5cde12e3188fca12b655de3be424871f61bd464e9318b0da6bcdbc65b79551f3.
All extraction uses the existing bounded archive reader and validates the
original archive and staged HOG. The original private checkout is unchanged.

## Proposed additional assets

Add original distributions, with their ownership and provenance, rather than
checking generated test levels into the private asset repository. Generated
25/27 variants remain reproducible controls in the public test generator.

| Original           | Additional evidence                                                                                                                          | Archive SHA-256                                                  |
| ------------------ | -------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------- |
| `anthology.7z`     | A real 25/40 file and complete D2X package; the Anthology already in the old levelpack is a different 23/40 file                             | 380552e767767147dcbd52f361657acf2a08071e5894c6cd9aa511e6232e0f3f |
| `pmines_v11.7z`    | All fourteen 27/40 Pumo levels, the large HOG, and complete HXM/POG/audio/message/configuration ownership as those consumers are implemented | 82c8cc4cfc740c8169f9d21a961c9ac5d2518342b3e27ac3ac438c91ca8f2ddf |
| `levels/lor-xl.7z` | Optional smaller standalone 27/40 witness, with 4930 canonical segments and authored fog                                                     | 7c80710e8a317905dee1d027df1079b9a5d7fd63b3a0cbe99cac57a3090994c8 |

The first two add the most useful coverage beyond existing Boiling Point.
Lost Relics is useful if a smaller complete 27/40 package is preferred. None is
added or repacked by this change. Their current inspection and source evidence
is recorded in [modern-layouts.md](modern-layouts.md).

## Validation status

The fresh Linux build found a boss source-adapter error: `Center()` returned a
temporary instead of the reference used by XL. The adapter now matches the
pinned header. The first complete selection passed 90/91 tests and exposed a
stale lifecycle fixture built for classic light tables. Its explicit source-runtime
generation now matches the runtime's owned tables; the lifecycle and unchanged
classic-loader control both pass. All sixteen original outcomes and three full
modern source comparisons also pass.

A subsequent clean ext4 run exposed three case-dependent rejection fixtures:
an uppercase loose-file override did not shadow the lowercase original on
Linux. Those controls now use the requested spelling and assert the resolved
file's owner before testing rejection. The unchanged original had been selected;
the loader had not accepted the malformed bytes. The clean final run verifies
all three corrections and passes every required check.

A fresh Linux checkout of 13c42fefc5a43ae5d76caa0b11065172c5477f08 passes all
91 CTests, sixteen original-file outcomes and three modern source comparisons.
The complete build and owner take 478.196 seconds on native ext4 storage with
two build/test workers; CTest takes 190.43 seconds. Input staging and checkout
precede that measurement. The receipt has no experimental exception and retains
`level_two_completion_qualified: false`.

That run identified 66.93 seconds spent in the original pilot archive control,
mostly reparsing a large JSON report in CMake. Its final Python owner parses once
and passes the same complete original control in 1.89 seconds, including two
actual inspections, all thirty level statuses, eleven exact pilot counts, input
hashes and unchanged report content/timestamp. The timestamp check now retains
nanosecond precision. This focused measurement follows the clean full run; a
new full-run duration is not inferred by subtracting the difference.

The combined Linux policy, adapter and trust-boundary suite passes all 190
then-current tests. The updated original-inventory controls add one test;
the final nineteen XL owner tests pass separately. Both automation catalogs
and scoped mixed formatting pass. Hosted private execution requires the trusted
merged workflow; public PR checks alone cannot establish that result.

The subsequent owned-message increment adds its decoder, source oracle and
native lifecycle test. Its complete local owner passes all 94 required tests,
sixteen original-format outcomes and three modern source comparisons in 317.377
seconds for the test/inspection phase on a Windows-mounted filesystem. Windows
passes all 38 public cases in 19.89 seconds. The earlier clean native-ext4 timings
above remain distinct measurements. Message runtime and original Pumo evidence
are recorded in [level-messages.md](level-messages.md).

The [stereo PCM prerequisite](stereo-pcm.md) adds a roughly 0.7-second independent
WAV test to both required selections. Its focused decoder, original mono and
complete Pumo sample comparisons pass. This does not relabel the preceding
94-case full run as a 95-case result or qualify named playback.

The subsequent [named-audio source contract](named-audio-source.md) brings the
complete local Linux selection to 96 passing cases, sixteen original-format
outcomes and three modern source comparisons. CTest takes 267.35 seconds and
the test/inspection phase takes 270.05 seconds on the same Windows-mounted
workspace. The incremental Windows public owner passes all 40 cases in
16.531 seconds, including 9.33 seconds of CTest execution. These are actual new
full-selection results; neither the source audio adapter nor the separate
passing Android classic-audio control qualifies named playback.

After the stack merged, the trusted hosted `xl-fast` run
[40824895601](https://github.com/larsonm-personal/dxx-redux/actions/runs/40824895601)
passed on cmake 1a3d9f92814ff003fd62557c97dba4a0006f58ef. The owner step ran
for 385.5 seconds (05:44:18 through 05:50:43 UTC, October 29), including its
builds and original inspections. The private asset checkout is now pinned to
8c9988a96a7e306f8aac04c85e2610492917b7ac, which includes the manifest's required
modern originals. This is the measured hosted baseline before the POG addition
and further CI trimming, not a timing inferred from local results.

The owned named-audio increment passes all 103 required local private tests,
sixteen original-format outcomes and three modern source comparisons. CTest
takes 320.16 seconds; the test/inspection phase takes 323.479 seconds. The new
required controls cover pure voices, actual SDL output and native lifecycle.
Public Windows passes 43 cases in 12.203 seconds of tests; public Linux passes
46 in 6.242 seconds, including its GPU and audio backends. See
[named-audio evidence](named-audio-runtime.md) for normal Android output,
classic-audio checks and the remaining qualification boundaries. These local
durations do not replace the hosted baseline above.
