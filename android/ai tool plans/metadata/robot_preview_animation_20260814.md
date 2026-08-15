# Robot Preview Animation Plan

## Objective

Animate robot previews using the same joint-state data and motion style as the original D1/D2
briefing robot previews, while retaining mission HAM/HXM robot and joint replacements.

## Work

- [x] Trace D1/D2 briefing robot rendering, animation-state selection, and text sources
- [x] Identify which robot motion data lives in HAM/HXM and how custom data is applied
- [x] Add shared preview animation state with a minimal D1/D2 model-picture rendering hook
- [x] Extend preview introspection to expose animation state and joint activity
- [x] Add or extend automated coverage for stock and mod robot animation
- [x] Run scoped quality checks, Android builds/tests, emulator checks, and native host verification

## Questions to Resolve

- Briefing files contain authored mission prose, not a structured robot-name table
- D1 robot directives carry robot numbers, while D2 directives carry robot-movie letters
- HXM/HX1 replacements can replace joint lists and animation-state tables
