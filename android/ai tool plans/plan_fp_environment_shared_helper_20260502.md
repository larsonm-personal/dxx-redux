# FP environment helper extraction plan

## Goal
Extract replay and startup floating point environment setup from d1 and d2 into a shared helper under android shared code, document MXCSR defaults, and mirror replay-step calls between d1 and d2

## Steps
- [x] Add shared helper source/header in android app shared cpp path
- [x] Move startup FP setup in d1 inferno.c and d2 inferno.c to shared helper
- [x] Move replay FP restore setup in d2 game.c to shared helper and mirror call in d1 game.c
- [x] Update d1/d2 main CMake to compile/link shared helper on host and android builds
- [x] Build d1 and d2 host targets and run determinism matrix smoke check
- [x] Mark completed steps in this plan
