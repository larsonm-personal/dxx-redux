# Windows host build repair and emulator verification

## Goal

- repair the intended Windows desktop build path for this repo
- validate the repaired path with a real host build command
- finish with an emulator verification run so the metl154 fix can be visually checked

## Plan

- [x] confirm the intended Windows build entrypoint and local toolchain assumptions
- [x] diagnose why the current local host build directories fail
- [x] repair the Windows host build path with the smallest repo or environment-facing change needed
- [x] run a successful Windows host build command and record the exact working invocation
- [x] end with an emulator verification run for the metl154 wall
