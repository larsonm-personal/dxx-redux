# Plutonian Shores unreachable metadata research

## Goal
- Determine why Plutonian Shores levels 1 and 5 are reported as `target unreachable or blocked by unsupported door`.

## Steps
- [x] Locate the generated metadata and scanner path that produces the warning.
- [ ] Inspect the affected level entries and compare with nearby successful levels.
- [ ] Trace the travel estimator behavior closely enough to identify whether this is bad level data, unsupported scanner logic, or a scanner bug.
- [ ] Summarize findings and recommend whether a code fix, metadata refresh, or follow-up instrumentation is needed.

