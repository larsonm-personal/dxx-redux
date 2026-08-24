# GuideBot minimize upstream diff implementation

## Goal

Reduce changes to original D2 GuideBot files relative to `upstream/main` by
moving added behavior into cohesive new files under `d2/main`, without changing
runtime behavior or weakening existing route tests.

## Plan

- [done] Establish the current upstream diff and map cross-boundary
  dependencies for route runtime, commands, diagnostics, and multiplayer code.
- [done] Add a dedicated GuideBot extension header and move added public
  declarations out of original `escort.h` where call sites permit.
- [done] Extract the largest cohesive GuideBot extension implementation from
  `escort.c` into new D2 source modules with narrow integration hooks.
- [done] Update D2 CMake and standalone tests while preserving Android shared
  planner, certifier, snapshot, cache, and decision modules.
- [done] Compare the resulting upstream diff, run scoped code quality, all D2
  native tests, Android debug compilation, and D1/D2 Windows builds.

## Results

- Added `d2/main/guidebot_extensions.h` and reduced the branch change to the
  original `escort.h` from 146 added lines to one include line.
- Added `d2/main/guidebot_route.c` and
  `d2/main/guidebot_route_internal.h` for the Android route controller,
  diagnostics, event tracking, metadata orchestration, goal projection, and
  route-monitoring state that had accumulated at the top of `escort.c`.
- Reduced additions to the original `escort.c` from 4,024 lines to 2,308 lines.
  Together with `escort.h`, this removes 1,861 added lines from original files
  without changing GuideBot behavior.
- Kept D2-specific code under `d2/main` because it directly uses D2 engine
  globals and types. Portable planner, certifier, snapshot, cache, and decision
  modules remain under the Android shared source directory.
- Added the route module to the D2 CMake source list. It is compiled into the
  regular, headless, headless-metadata, and Android targets.
- Scoped code quality and `git diff --check` passed.
- The Windows D1 and D2 builds passed. The D2 native CTest suite passed all 43
  tests, including route decision, route certifier, metadata progress, and the
  three escort policy suites. The D1 build has no registered CTest tests.
- Android `assembleDebug` passed for arm64-v8a, armeabi-v7a, and x86_64.
