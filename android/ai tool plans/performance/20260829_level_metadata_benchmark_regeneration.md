# Level metadata benchmark regeneration

## Goal

Regenerate `android/benchmarks/level_metadata_analysis_history.json`, expose the
focused workflow in the appropriate interactive menu, and determine whether the
new sample demonstrates the texture-bind cache improvement.

## Plan

- [x] Identify the benchmark generator, input workload, output schema, and
  existing menu conventions
- [x] Add a focused interactive menu entry with regression coverage
- [x] Run the benchmark regeneration and preserve normalized output
- [x] Compare the new results with the directly comparable pre-fix history
- [x] Run scoped formatting and relevant tests, then record results

## Results

- The canonical benchmark appended a snapshot at `2026-08-29T19:43:44Z`
- Aggregate CPU increased from 2.377 seconds to 2.651 seconds, an 11.5 percent
  regression, while the measured work counters remained unchanged
- This result cannot measure the texture-bind cache: the benchmark uses Windows
  headless metadata targets, while the changed cache source is compiled only by
  the Android app targets and requires the OpenGL render path
- The menu regression test, scoped code-quality pass, benchmark build and run,
  and focused guidebot CTest all passed
