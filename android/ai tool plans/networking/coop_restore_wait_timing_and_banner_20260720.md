# Coop restore wait timing and banner

## Goal

Make server-started coop save restoration occur at a predictable synchronization
point, show a fixed waiting banner on every peer until restoration completes, and
show a timed error banner if restoration fails.

## Plan

- [ ] Trace server launch options, native lobby readiness, restore scheduling, completion, and failure paths in D1 and D2
- [ ] Identify the source of variable restore delay and define a bounded synchronization rule
- [ ] Implement shared wait/error status state and fixed top-of-screen rendering in D1 and D2
- [ ] Add or extend high-level regression coverage for waiting, success, timeout, and failure transitions
- [ ] Run scoped formatting/lint, focused tests, D1/D2 or Android builds, and relevant integration checks
- [ ] Record findings and verification here

## Findings

Pending investigation.

## Verification

Pending implementation.
