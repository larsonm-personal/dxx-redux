# Regeneration diff review 2026-08-01

## Goal

Review the listed Chromaprint and mission metadata regeneration diffs, distinguish formatting churn from semantic changes, and identify generator or publication behavior that can alternate files between representations.

## Plan

- [x] Inventory the listed diffs and classify formatting-only versus semantic changes
- [x] Trace Chromaprint serialization, lookup, cache identity, and publication paths
- [x] Trace mission metadata normalization and per-file publication timing
- [x] Compare improved and degraded metadata examples to source logs and generator behavior
- [x] Report likely causes and prioritized fixes without modifying generated data
