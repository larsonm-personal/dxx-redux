# Host mission-ready terminal status

## Goal

Ensure the host lobby shows a downloaded client's final mission state as `Mission ready` instead of retaining `Finalizing mission - 100%`.

## Plan

1. [Completed] Trace client finalization status publication and host player-status updates.
2. [Completed] Correct the terminal status propagation and add focused regression coverage.
3. [Completed] Run scoped quality checks, relevant unit tests, and the Android debug build.
