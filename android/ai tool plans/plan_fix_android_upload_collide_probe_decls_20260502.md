# Plan: Fix Android Upload Build collide.c Probe Declarations (2026-05-02)

## Goal
- Restore Android debug native build by fixing undeclared probe function calls in d2/main/collide.c

## Steps
- [x] Identify where the probe functions are declared/defined and why collide.c cannot see them
- [x] Apply minimal declaration/include fix without changing runtime behavior
- [x] Re-run the Android build path used by upload script and verify collide.c compiles
- [x] Update this plan with outcomes and any follow-up notes

## Outcome
- Added missing include of input_demo_debug_logging.h in d2/main/collide.c and d2/main/object.c
- Android native ninja build now compiles and links dxx-redux-d2 and full native target set
- Gradle task invocation is still locally blocked in this shell by Java runtime selection (JVM 8), but native C/C++ compile blockers from this report are resolved
