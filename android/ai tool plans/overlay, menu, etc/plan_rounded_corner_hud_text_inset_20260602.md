# Rounded Corner HUD Text Inset Plan

## Goal
- Add a saved graphics option, default on, to move corner HUD text horizontally inward so rounded display corners do not hide it.
- Prefer automatic rounded-corner data if Android exposes usable values, with a conservative 5% screen-width fallback.
- Keep the HUD text y positions unchanged.

## Steps
- [x] Survey existing graphics option persistence, export, and native application paths.
- [x] Survey D1 and D2 HUD gauge drawing to identify the corner text coordinates.
- [x] Add Android option plumbing and rounded-corner inset delivery to native code.
- [x] Apply horizontal-only HUD text offsets in both D1 and D2.
- [x] Add or update focused tests where practical.
- [x] Run formatting and targeted verification.

## Notes
- The existing worktree has an unrelated modified plan file. Do not touch it.
