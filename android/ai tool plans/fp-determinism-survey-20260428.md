# Floating point determinism survey plan

## Goal
Harden floating point behavior for input based deterministic demo replay across Windows, macOS, x86, and ARM

## Steps
- [x] Review existing build flags for floating point behavior
- [x] Survey simulation adjacent floating point operations in d1 and d2
- [x] Identify use of libm and transcendental functions in gameplay paths
- [x] Identify compiler and runtime controls that could reduce cross platform drift
- [x] Produce a prioritized hardening list with file locations and rationale
- [x] Mark completed survey work in this plan

## Output
- android/ai tool plans/fp-determinism-survey-20260428-report.md
