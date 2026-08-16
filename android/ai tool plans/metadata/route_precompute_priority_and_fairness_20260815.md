# Route precompute priority and fairness

Date: 2026-08-15
Status: completed

## Requirements

- Keep screen-off analysis disabled
- Use 20 percent CPU duty while the launcher is visible
- During gameplay, use 10 percent for the next level and 2 percent for later levels
- Do no additional mission work after every level in the current mission is complete
- Prevent partial jobs from monopolizing the queue
- Make metadata-viewer focus change actual work priority
- Persist useful lifecycle, retry, and heartbeat events

## Plan

- [x] Trace launcher and in-game request scheduling and define context-specific CPU policy
- [x] Implement partial-job fairness and actual viewer-driven reprioritization
- [x] Add persistent lifecycle, retry, and heartbeat diagnostics
- [x] Add focused unit and integration regression coverage
- [x] Run scoped quality checks, Android tests/build, native builds, and device verification

## Result

- Launcher-visible precompute uses 20 percent CPU duty
- In-game current and next-level work uses 10 percent CPU duty
- In-game later-level work uses 2 percent CPU duty
- In-game mission work exits and records 0 percent after the mission queue is drained
- Screen-off and background launcher lifecycle still stop analysis
- Partial launcher jobs yield to unattempted work before retrying
- Opening the metadata viewer pauses launcher fill so its active analysis owns the worker; closing it resumes fill
- Persistent logs now include state transitions, deferred retries, long-job heartbeats, in-game duty, and idle duty

## Verification

- Scoped code quality passed
- Focused scheduling, ordering, and monitor unit tests passed
- Android debug APK and all native ABIs built successfully
- On-device launcher workers reported cpu_duty_percent=20 and stopped on launcher pause
- In-game route priority regression passed 26 of 26 steps
- On-device in-game log reported next-level duty at 10 percent and later-level duty at 2 percent
