# Integrate public fixtures into the existing replay picker

## Goal

Expose all 28 tracked compact demos through `run_all_tests.ps1` -> `R. Replay a demo` without changing recording or oracle bytes, retail recordings, or regression success criteria

## Work

1. Extend the existing discovery and selection path with known fixture roots, deterministic ordering, data-edition metadata and imported-D1 qualifications
2. Reuse verified shareware resolution/extraction, offer explicit public-package setup when needed, and stage the native-written comparison pilot only inside the replay sandbox
3. Preserve visible realtime playback and existing explicit display/render controls; correct compact-record timing detection for 8/16/20 FPS recordings
4. Extend asset-free picker integration coverage and include it in Windows/Linux tooling CI; exercise real Linux headed replays with existing qualified binaries and inspect screenshots
5. Obtain independent source and runner/UX review, publish a focused stacked draft, and observe exact-head CI to completion

## Constraints

- No new engine build unless evidence requires it; retain at least 4 GiB free
- Never edit source demos/oracles or rewrite native comparison expectations
- Label monitor phase, mine force, and fan RNG abort as imported faults; any failure remains a failure
- All-weapons compact recordings are native-only; imported live capture does not qualify imported compact replay
- Windows/device runtime cannot be claimed without actual evidence

## Validation

- Existing picker integration and two independent source/runner reviews completed
- Asset-free picker tests, 65 paired-evidence tests, automation catalog and direct suite discovery passed; scoped mixed-language formatting passed
- Final source exercised all 28 native fixtures in actual Linux/Xvfb visual realtime windows; all terminal comparisons passed
- Imported monitor, mine and delayed-flare control completed with scoped terminal matches; positive fan retained its exact frame-301 RNG abort, native exit 1 and absent result without timeout
- Real pause/resume and single-step held frame count 5 -> 5 -> 6, then completed; an actual data path containing spaces also passed
- Reused byte-verified D1/D2 binaries from published source `7145f682f4dd946ce0a8019d087eba6523794c16`; its engine sources equal PR34, with no engine build or source changes here
- Checked all 158 tracked corpus files against PR34: unchanged; native-produced comparison profile files remain byte-identical
- Hosted Windows/Linux tooling CI will cover the complete catalog runner; local catalog-wrapper execution encounters the pre-existing `/proc/self/task/.../children` supervisor limitation
- Windows/macOS/device runtime and native window-manager close-button behavior are not claimed; the cloud Xvfb session has no window manager
- Source is published as a separate stacked draft after PR34, with exact-head hosted CI tracked before final handoff
