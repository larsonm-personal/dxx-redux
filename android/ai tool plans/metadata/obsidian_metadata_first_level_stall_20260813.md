# Obsidian Metadata First-Level Stall

## Reported behavior

- A live build stalls while opening Obsidian level metadata
- The viewer remains at `Overall 0/18` and `Starting first level` for at least 30 seconds

## Investigation plan

- [x] Identify the code path represented by `Starting first level` and every blocking operation before the first native progress event
- [x] Inspect retained profiling and game logs for worker request, staging, cache, and native analysis activity
- [x] Reproduce an interactive Obsidian request while Uneasy 4 occupies the reusable D2 worker
- [x] Confirm FIFO worker starvation rather than slow Obsidian staging or lost native progress
- [x] Add profiling diagnostics for submission, service queueing, worker start, and preemption
- [x] Let higher-priority work cancel and wait out only lower-priority background work before submission
- [x] Preserve retryability for preempted background work and avoid preempting foreground requests
- [x] Run focused unit tests, Android build verification, and the high-level contention regression

## Findings

- `Starting first level` is shown after service submission but before the first native checkpoint
- Each D1/D2 service processes requests through a FIFO single-thread executor
- Launcher precomputation can occupy that executor for up to ten minutes, especially on unusually large levels
- An interactive metadata request previously queued behind that work with no indication that it was waiting
- Instrumented baseline logs showed queued work starts strictly after the current request completes

## Validation

- `LevelMetadataWorkerIdentityTest` and `RouteMetadataSchedulingTest` pass
- `:app:assembleDebug` passes for all configured Android ABIs
- The emulator regression starts Uneasy 4 at `next` priority, observes an `active` Obsidian request preempt it, and starts Obsidian in a fresh worker about 2.75 seconds later
- The Obsidian request completes successfully with all 18 levels and the background Uneasy 4 request resumes afterward

## Constraints

- Do not make guidebot metadata nondeterministic
- Preserve single-flight cache publication and request cancellation
- Keep viewer and background precomputation from blocking each other indefinitely
