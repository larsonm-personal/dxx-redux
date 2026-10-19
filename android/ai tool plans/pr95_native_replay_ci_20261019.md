# PR95 native replay CI prerequisites

Scope: port the prebuild and generated-file freshness fixes from PR93 without
its unrelated public LAN selection changes

- Apply the selected-game prebuild and `--game-only` support from e2c21ce57465ddbe330a64a27419263197f42799
- Apply the generated CMake freshness exclusions from 52b828f5f98500eebc7b210551faf181198eef99
- Run focused CLI, workflow-selection and freshness tests plus scoped quality checks
- Push new commits to fix_wall_thing and inspect exact-head CI to completion

Evidence: fac031aa9220f9c8a725b128d49fccce78158c18's Android sample in run
39178888702 timed out while rebuilding a missing buildd2/main/d2x-redux inside
the auto-level-cheats owner budget. Owner timeouts and test assertions remain
unchanged. Native prebuild failures remain setup failures

Local verification: 17 Linux build CLI tests, 61 sweep/planner tests, replay
freshness guard tests, automation catalog validation, Bash syntax and
`git diff --check` passed

The catalog-runner integration is blocked by this execution environment's
unavailable `/proc/self/task/<pid>/children` interface. Scoped mixed-language
quality was attempted but its pinned PSScriptAnalyzer, shellcheck, shfmt, Ruff
and Prettier tools are unavailable locally. Hosted CI must validate those checks
