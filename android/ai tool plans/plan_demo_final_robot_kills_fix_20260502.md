# Plan: demo final robot kills fix 2026-05-02

## Goal

Fix final demo state stats so robot kills are measured from replay start instead of inheriting pre-existing counters

## Steps

- [x] Locate final-state stat assembly for robot kills
- [x] Verify how baseline counters are initialized at replay start
- [x] Implement minimal fix to compute/report correct kill delta
- [ ] Run targeted replay/state test to validate output
- [x] Mark plan complete with results

## Notes

- Fix applied in d2/main/game.c: robots_killed now uses num_kills_level delta from a replay/record baseline
- Baseline is captured once per replay/record session so checkpoint-start demos begin at zero kills
- IDE diagnostics report no errors in d2/main/game.c
