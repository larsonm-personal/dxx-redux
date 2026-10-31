# XL evening continuation

Authorized through October 30, 2026, 22:00 Pacific (October 31 05:00 UTC).
Reserve 21:30 onward for final checks, handoff and emulator restoration.

## Current checkpoint

- Branch: `xl_more_86_particle_rendering`. The user requested fresh `cmake`
  ancestry because PR #237 merged. Fetched cmake tip is
  `0e4587e7dedbf0f21955c6fda351c923ff5e23c5`. Commit this tranche alone, then
  rebase it onto that tip and publish with base `cmake`.
- PR #237 is merged. Its original 14 CI checks passed, timing export skipped;
  new branch CI must run for this chunk.
- Preserve unrelated manifest, Shearbase/W5AM JSON/music and import-plan files.
- No subagents or autonomous navigation. Android scenes use fixed face poses.
- Current chunk: normal strict-fire admission after completing the requirement
  audit. Diagnostic fire bypass removed; the normal source-owned loader uses
  the same validator. Other particle kinds, moving/controlled/multiplayer
  effects, triangular emission faces and unsupported layouts remain rejected.
- Metadata remains partial and does not certify mission playability. Original
  Pumo hub and campaign remain unsupported. Native/Android normal-admission validation passes.

## Completed evidence to reuse

Detailed source and implementation notes: `android/XL_compat/evidence/particle-effects.md`.
Unless stated otherwise receipts are under `temp/xl-evening-20261030`.

| Requirement | Earned evidence |
| --- | --- |
| Startup image fallback | Existing native owner, isolated copied retail data; distinct startup origin, mission precedence, malformed/missing resources, foreign shadows, retained pixels |
| Strict translation | 125 effect profiles, 3360 truncations, source payload retention and normalized native identity; original Pumo mvlev1 contains 12 initially disabled fire owners |
| Simulation and geometry | Reuse prior pinned source emitter, collision/world, lifetime, pause, effects RNG and trigger evidence |
| Resource and ordinary saves | Original Pumo Android owner: original atlas pixels, trigger activation, disk save/load, context loss, stock return and resumed emission |
| Soft rendering | Pinned source shader, 86 source/portable/backend comparisons, 24 hard-depth distinguishing cases, eight depth/MSAA formats; actual depth-copy capability probe |
| Source buffering | 263 profiles, 156088 inputs against extracted source; six GPU scenes distinguish old per-quad behavior; 25001-quad bounded submission control |
| Original scenes | Fixed poses 409/2 and 410/4 show actual RGB contributions; latest full diagnostic lifecycle 78/78 in 229.370 seconds, protected hashes match |
| Automatic secret saves | Derived Neptune geometry with point/face fire, two real entries/returns, default and saved activation, transient reset and retained atlas. Native passes; Android 30/30 in 67.100 seconds, hashes match |
| Classic isolation | Existing D1/D2 GLES array owners pass after shared strided client-array overread fix; stock-return coverage retained |
| Catalog/quality | Scoped quality and both catalogs pass; 427 top-level and 490 support scripts |

Secret transition diagnostics temporarily suppress synchronous presentation
inside the real enter/return calls, then test headed GPU output and emission.
This is persistence evidence, not an earned campaign secret exit. Test durations
are not frame-rate benchmarks. No original retail or authored image bytes edited.
Pinned XL source: `2576a98b5de310e3ac7c9777df2677d1627b8bae`.
The authored-fire source creates audio only for bubbles, not fire.

## Active validation

Normal-admission translation controls pass. Scoped quality passes; fire-only
preview API and diagnostic translation entry point have been removed.
Native build and owner pass, including normal missing-atlas rejection/recovery.
Full inventory contracts pass. Android build and installation pass. Original
Pumo passes 78/78 in 249.514 seconds via normal admission. Derived SecretFixture
passes 30/30 in 55.777 seconds. Both protected hashes match. All build/test
handles are terminal. Reuse GPU/source receipts
because this chunk changes admission and diagnostic routing, not rendering math.

## Emulator restoration obligation

Temporary host GPU was necessary because SwiftShader silently returns zero from
its depth blit. Capability checks reject it rather than silently render wrongly.
Restore the original AVD backend before the deadline:
`DxxOvernight_1 -port 5554 -no-snapshot-load -no-snapshot-save -gpu swiftshader_indirect -crash-report-mode disabled -no-window`.
Exact original command: `temp/xl-evening-20261030/emulator-original.txt`.
Last known host launcher PID 55924, child 37112; revalidate before action.
Stop with adb emu kill, verify termination, then start original args Hidden.
Do not restart on observation timeout. Check live processes 30-60 seconds apart.

## Finish policy

Keep one bounded investigation active. Select JSON fields instead of dumping
state; filter long log lines before searching. Reuse completed validation.
The user now authorizes committing/pushing completed 1-2 hour chunks on stacked
branches to start CI. Publish branch 86 from current cmake as explicitly requested; use a stacked
successor for the next chunk. No merge requested. At the deadline,
record exact remaining gaps, terminal handles, emulator backend and protected-file
hash results. The original handoff is preserved; use this current checkpoint.
