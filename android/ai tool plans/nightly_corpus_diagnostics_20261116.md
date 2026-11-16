# Nightly corpus diagnostics and known unavailable geometry

## Goal
Preserve safe diagnostic distinctions from nightly metadata and route owners without publishing fixture text or weakening semantic comparison

## Changes
- Retain fixed route comparison reasons through the retail sanitizer
- Emit bounded metadata mission/level indices and fixed geometry failure categories
- Decode only exact HXM normalization warning syntax into numeric before/after counts
- Classify byte-equivalent decoded metadata with exclusively known missing/unsupported geometry as BLOCKED, never PASS or full coverage
- Keep new geometry loss, unknown loader failures and any baseline drift as FAIL

## Verification
Run corpus, retail boundary and sweep tests, code quality and automation catalog checks
Private pinned mission reruns remain necessary to diagnose entropy/TT and route failures; this change does not claim to repair those engine behaviors or refresh baselines
