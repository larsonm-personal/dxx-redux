# Required private XL checks

The fast private selection reuses the existing trusted `retail-ci` workflow,
read-only asset token and immutable checkout. Public pull-request jobs remain
asset-free. Private files, original inventories and raw engine output stay on
the private runner; published receipts retain fixed counts and outcomes.

`xl-fast` selects one Linux owner, `host-profile:retail-xl-fast`. It builds the
normal D2 metadata executable and the developer lifecycle executable, then runs
91 required CTests in independent directories, two at a time. Missing tests,
skips, disabled tests, errors and failures all fail the lane. The adapter checks
the same exact membership as the owner, replacing the stale hardcoded count of 40. The longer `xl-originals` selection retains its separate campaign controls
and explicitly reported unfinished level-two experiment.

The fast selection also checks the registered commands for 35 tests whose
source comparisons are optional in CMake. Each required reference executable
must exist, including Observatory's no-damage gates and lava's separate motion
oracle. A runtime pass with an omitted source comparison cannot satisfy this
selection.

## Coverage

| Area                   | Required evidence                                                                                                                  |
| ---------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| Format and admission   | Canonical 25/27 runtime, older admitted layouts, malformed inputs, bounded HOG streaming, unsupported requirements and recovery    |
| Events and world state | Wall/object triggers, delays, flags, teleports, speed boosts, spawn points, resources, wall/door variants and lights               |
| Actors and generators  | Robot/equipment assignment, 25/27 startup, boss ownership/groups/transport, liquids, repair and capacity controls                  |
| Persistence and Redux  | Real disk/checkpoint ownership, mission changes, stock recovery and a normal executable without developer probes                   |
| Source comparisons     | Required extracted reference readers, arithmetic, geometry, resource precedence and gameplay functions; no optional missing oracle |
| Rendering              | Production offscreen fog backend, depth formats, MSAA, GL state and inactive stock behavior; physical Android remains separate     |
| Original bytes         | Nine older witnesses plus all seven outcomes in the original modern Boiling Point HOG; three full 27/40 source-reader comparisons  |

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

The public policy, receipt and admission controls pass on Linux, including the
final seventeen private-owner tests. Both automation catalogs and scoped mixed
formatting pass. A clean final selection and native Linux filesystem timing are
pending before publication. Hosted private execution requires the trusted
merged workflow; public PR checks alone cannot establish that result.
