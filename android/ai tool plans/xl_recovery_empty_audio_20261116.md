# Empty authored ambient names: first recovery checkpoint

Sphere 3 and Warp Shock 1 contain stationary sound objects with an empty authored
filename stem, raw volume 10, and initial enabled values 0 and 1 respectively.
The old admission rule rejected the empty stem. The qualified XL source instead
appends `.wav` and retains an ambient object owner even when output is unavailable.

The change uses that existing unavailable-output path. It preserves the authored
name, enabled state, object signature and lifetime. It still rejects negative
volume, invalid object shapes, unsafe paths, malformed present audio and saved
voices that claim unavailable PCM. It does not remove the object or fabricate
audio. A real resource named `.wav` follows the usual resource lookup.

## Source and tests

Reference `audio/objectaudio.cpp`, `SetSoundSources`, SHA256
`e44484af297ce0c20c92feb00bfb2795316980434e3f1e1d85194c1d46b992e3` matches both
the local reference checkout and the qualified reference build input.

The existing named-audio integration owner now includes two empty-name controls:
version 23 initially disabled, and version 27 initially enabled. Each compares
three loaded owners and six wall/attached activation cases with extracted source,
then exercises output availability, activation, lifetime and save/restore.
Activation receipts must exactly match the corresponding available-output case.
Malformed `.wav` and fabricated unavailable-output voices remain negative cases.

Private evidence root: `temp/xl-recovery-20261116/`.

- `empty-audio-debug-01`: initial disabled-only controls completed successfully.
- `empty-audio-debug-02/runtime.log`: final enabled/disabled controls and all
  subsequent static and seismic controls reached their passing final markers.
  The tool session's terminal exit was not retained across context compaction;
  `empty-audio-debug-03` repeats that final script and has a durable exit-0
  receipt. These are direct Debug CMake-script runs, not a pass under the
  registered 150-second CTest timeout. The last run took approximately 154
  seconds by output timestamps; exact-candidate CI timing remains required.
- `empty-effects.log`: 150 effect profiles and 4025 truncations passed.
- `build-audio-native-03.log`, `build-audio-contracts-02.log` and
  `build-audio-ordinary.log`: incremental Debug/reference and ordinary
  developer-OFF builds completed. Existing C4805 in unchanged
  `changing_light_test_records.h` and runtime MEM shutdown warnings remain visible.
- `empty-audio-originals-01`: both authentic cases still fail loading. Sphere 3
  now reaches boss profile 2; Warp Shock 1 reaches neutral segment owner 0/group
  -1. Neither is a newly passing level. No inputs or criteria were changed.

Current working-tree binary SHA256 values:

- Debug `xl-preview-lifecycle.exe`:
  `fdb31b8d43edc3de9382d17733414ee94bf93e0f06961b5dfc4c9050e520b8b0`
- Ordinary `dxx-redux-d2-headless-route.exe`:
  `2a4e2bd61fdf819231f43261205181533a4a47ab52e4acaa38d2d7a02cfe7052`

These binaries include the working-tree patch above parent
`d983d3f0728a32b86c2aba2779817cc93ef8fe1d`; a receipt naming that parent does not
mean the binary was built from the unmodified parent. Source patch SHA256:
`01967b00ecd65e81cfa8bfb6c0dcd2bf019452eb9d81730f477f5e5d9994532d`.
Exact committed candidate CI, Linux and Android checks remain pending here.

## Budgeted measurements

The working-tree `empty-audio-64-01` measurement stopped at its unchanged budget
guard after 317.609 seconds: 50 passed, 11 load failures, three untested, original
eight passed. Exit 2 and `budget_exhausted: true` are retained. This is incomplete,
not 53/64. Native timings increased across many unrelated cases relative to the
previous local baseline; the measurement alone does not attribute that to this
patch. The three untested entries passed separately in `empty-audio-remaining-01`,
exit 0, for supplemental coverage. This does not change the budgeted receipt
or establish a completed budgeted run.

The unmodified parent has now completed hosted measurement in trusted run
43979815263, controller `eba98b69662658ee0e98cf4ddc30425b11054e42`, candidate
`d983d3f0728a32b86c2aba2779817cc93ef8fe1d` (PR 551). Expanded measurement job
133694805587 passed integrity with 53/64, eleven load failures and original 8/8.
The 64 probes took 244.969 seconds with 303 seconds remaining for probing after
setup; no budget exhaustion. Separate original-eight job 133694805489 also
completed with 8/8. These results qualify the baseline harness, not this patch.

Artifacts are retained under `ci-baseline-551`, with downloaded ZIP digests
verified against GitHub: sampled artifact 14283323375,
`5d88ba9cb1982047932f1b871bf8e31acef41ff22fa96ea51f62c6dc636fe1af`;
expanded artifact 14282454672,
`1b901bb3bce2b90a39a1c6345b630fb3cacee3b2706233ba787da882db9c772f`.
Workflow: https://github.com/larsonm-personal/dxx-redux/actions/runs/43979815263

## Updated recovery implications

The original eleven failing levels remain the target. Removing one blocker can
expose another: Sphere 3 adds positive boss profile 2 to the boss workstream,
alongside Sphere 4's signed -21. Track individual blockers and level passes
separately. The complete native inventory is retained under
`inventory-baseline-01`, including all equipment masks and trigger records.

Neutral segment ownership is the next bounded audit. Source consumers gate
ownership effects on Entropy multiplayer; qualify the actual single-player
consumers and retained identity before changing admission. Image/door resources,
equipment producers and Boiling Point 4's object decode retain their separate
workstreams and must not be reduced to blanket allowlist changes.
