# Obsidian saved-game metadata progress diagnosis

## Goal

Determine why a restored Obsidian game reported Guide-Bot calculation without visible progress, while the metadata browser and Advanced progress views appeared to restart at zero.

## Plan

- [x] Establish the level, save-restore sequence, metadata cache state, and scheduler requests from both logs.
- [x] Trace the runtime Guide-Bot, metadata browser, and Advanced progress displays to their backing jobs and counters.
- [x] Determine whether work restarted from level 1, duplicated across surfaces, stalled, or only displayed stale or incomplete progress.
- [x] Report the root cause, performance implications, and a scoped repair plan without changing behavior during this diagnostic pass.

## Findings

- The first save restore loaded Obsidian level 8 (`pelorus.rl2`) at 09:49:43. The game advanced to level 9 (`cerulea.rl2`) at 09:50:42. A later restore at 09:57:17 loaded the now-updated save directly into level 9.
- The in-game scheduler did not start at level 1. It selected level 8 as the most recent level, prepared level 9 and secret level -2 next, then ran an active level 9 analysis followed by level 10 and fill-priority level 11.
- The active level 9 worker reported completion in 7,019 ms at 09:50:47. The Guide-Bot nevertheless had no published route goal for the rest of that session. A request made after that completion could therefore still receive `Still calculating` only if the game failed to adopt the newly published cache or failed to converge its readiness state.
- Level 11 was not the current-level blocker. It was low-priority fill work at two percent CPU. It remained in switch firing-path visibility work for more than four minutes, emitted heartbeats, retained checkpoint work across cancellation, and completed in 4,192 ms after the next launch resumed it.
- Opening the Obsidian metadata dialog stopped the coordinator and launched a separate all-level foreground analyzer. Its Compose state is explicitly initialized to overall 0/0 and estimated level 0/1000. With the mission aggregate result cache absent after the version change, it begins the 18-level presentation at zero and normally scans the mission from its first listed normal level, although shared per-level route artifacts may reduce individual work.
- Advanced reads the coordinator's global ledger snapshot, not the live Guide-Bot counter or the metadata dialog counter. Cache generation 7 made old ledger identities nonterminal and pruned two old cache directories, so zero of 126 discovered jobs was initially a truthful but poorly explained generation rebuild. After the recent Obsidian work, the global ordering explicitly selected base Counterstrike level 1 at 09:57:36.
- The three surfaces therefore expose three scopes: current live level, foreground mission analysis, and global background corpus. They currently look like one calculation and give no generation or scope explanation.

## Recommended repair

1. Add a durable current-level publication handshake: log the worker cache filename and key, JNI generation acceptance, cache-load result, and final readiness transition. Treat worker completion without cache adoption within one poll interval as a visible adoption failure with a bounded retry, not indefinite `calculating`.
2. Add an integration fixture that starts with no generation-7 cache, restores a save on a later mission level, waits for background publication, and requires the same running game process to leave `calculating`. Do not allow a relaunch to hide a hot-adoption failure.
3. Give the automap bar an explicit scope and numeric label such as `Current level route: 63%`, plus a stalled/adoption-failed state. It must use only the current-level request generation.
4. Make the metadata browser attach to or seed from the shared per-level ledger instead of presenting an unrelated zeroed mission job. At minimum label it `Mission metadata: completed levels / 18` and credit reusable terminal per-level artifacts before launching work.
5. Label Advanced as a global cache-generation rebuild and show both global and focused-mission counts. Preserve the saved mission as the focus until its remaining levels are terminal before returning to base-game level 1 fill work.
6. Retain the existing low-CPU resumable visibility checkpoints, but expose actual completed/total checkpoint counts in heartbeats so a long phase such as Obsidian level 11 is visibly advancing.

## Implementation

- [x] Make prepared route-cache identity immutable across live progression, trigger, and progression-object state, with a cache-generation bump and convergence tests.
- [x] Add bounded same-process publication/adoption diagnostics and a terminal adoption-failure state instead of indefinite `calculating`.
- [x] Label automap readiness as current-level numeric progress and expose adoption diagnostics through introspection.
- [x] Make metadata-browser startup distinguish shared reusable mission artifacts from its independent aggregate analysis.
- [x] Keep global background ordering on the recent saved mission after its current level completes, before returning to base-game fill work.
- [x] Label Advanced as generation-wide work and show focused-mission counts.
- [x] Include checkpoint counts in long-running worker heartbeats.
- [x] Extend cold-cache same-process adoption integration coverage with numeric scope and adoption-failure assertions.
- [ ] Add a dedicated later-level saved-game fixture and complete the remaining platform validation.

## Validation

- [x] Focused Kotlin scheduler and progress-monitor unit tests pass.
- [x] Debug APK builds successfully for arm64-v8a, armeabi-v7a, and x86_64.
- [x] Cold-cache automap integration passes with same-process route adoption and zero publication-adoption failures.
- [x] Scoped code-quality checks and `git diff --check` pass.
- [ ] Run the dedicated later-level saved-game fixture once its Obsidian save artifact is added.
