# D1-in-D2 and input-demo centralization cleanup

Goal: move D1-in-D2 compatibility predicates and input-demo diagnostic helper logic out of hot gameplay files where that can be done without hiding important engine behavior or making upstream-style diffs harder to review.

Plan:

1. [completed] Map the current D1-in-D2 branches and input-demo probes that are embedded in D2 gameplay files.
2. [completed] Add a D1-in-D2 gameplay semantics helper file for reusable predicates and small policy decisions.
3. [completed] Move one-off input-demo probe helper blocks from gameplay files into input-demo-specific files when the call site can remain simple.
4. [completed] Rebuild D2 and rerun the passing Level 5 D1-in-D2 replay to prove the cleanup preserved behavior.
5. [completed] Update this plan with what was centralized and what intentionally stayed inline.

Notes:

- Keep D1 save translation code in the existing D1 save translation files.
- Avoid turning every `d1_in_d2_use_d1_gameplay()` branch into an opaque wrapper. Centralize only repeated or clearly named semantic decisions.
- Preserve the current green Level 5 D1 replay result under D1-in-D2.
- Added `d2/main/d1_in_d2_semantics.c` and `d2/main/d1_in_d2_semantics.h` for small named policy decisions:
  D2 resource-drop suppression, D2 badass robot explosions, lower-AI visibility turn eligibility, D1 random-turn eligibility, and the D2 nearby-fire shortcut.
- Added `d2/main/d1_in_d2_input_demo.c` and `d2/main/d1_in_d2_input_demo.h` for the D1-in-D2 input-demo `robots_killed` result convention.
- Moved fireball-specific input-demo debris and secondary-explosion probe formatting from `d2/main/fireball.c` into `d2/main/input_demo_hooks.c`, leaving the fireball call sites as logging calls.
- Left high-context AI flow inline where wrapping would hide the relationship to the original D1/D2 code. The helper layer is intentionally a small naming surface, not a replacement AI abstraction.
- Verification:
  `.\run-windows-build.ps1 -Target d2`
- Replay verification:
  `.\android\tests\run_input_demo_replay.ps1 -DemoPath C:\local\dxx-redux\android\regression_demos\d1_descent_level5_20260616_202713.dximdemo -Game d2 -D1InD2 -Mode accelerated -Runner fast -StateLogPath temp\d1_level5_202713_d1_in_d2_centralization_state.jsonl -RngLogPath temp\d1_level5_202713_d1_in_d2_centralization_rngtrace.jsonl -ReplayDebugLog -KeepSandbox -TimeoutSeconds 300`
- Scoped quality pass:
  `.\android\run-code-quality.ps1 -Fix -Paths @("android\ai tool plans\input demo, replay, determinism\plan_d1_in_d2_input_demo_centralization_20260617.md","d2\main\CMakeLists.txt","d2\main\d1_in_d2_semantics.c","d2\main\d1_in_d2_semantics.h","d2\main\d1_in_d2_input_demo.c","d2\main\d1_in_d2_input_demo.h","d2\main\input_demo_hooks.c","d2\main\input_demo_hooks.h","d2\main\fireball.c","d2\main\ai.c","d2\main\ai2.c")`

Physics probe extraction follow-up:

1. [completed] Move the D1 object-13 physics fate diagnostic out of `d1/main/physics.c`.
2. [completed] Move analogous bulky D2 physics probe formatting out of `d2/main/physics.c` where the call sites can stay simple.
3. [completed] Build D1 and D2, then rerun the green D1-in-D2 Level 5 replay.

Physics probe extraction notes:

- Moved `input_demo_log_d1_object13_physics_fate()` from `d1/main/physics.c` to `d1/main/input_demo_hooks.c`.
- Moved D1's player/robot physics hit-object contact formatter from `d1/main/physics.c` to `d1/main/input_demo_hooks.c`.
- Moved D2's replay physics FVI fate formatter from `d2/main/physics.c` to `d2/main/input_demo_hooks.c`.
- Moved D2's player/robot physics hit-object contact formatter from `d2/main/physics.c` to `d2/main/input_demo_hooks.c`.
- Left D2's `input_demo_log_physics_fate()` and player drag/motion-detail probe helpers in `d2/main/physics.c` for now because they share a local motion-window state machine. Those are the next reasonable extraction target if more line-count cleanup is wanted.
- Verification:
  `.\run-windows-build.ps1 -Target d1`
  `.\run-windows-build.ps1 -Target d2`
  `.\android\tests\run_input_demo_replay.ps1 -DemoPath C:\local\dxx-redux\android\regression_demos\d1_descent_level5_20260616_202713.dximdemo -Game d2 -D1InD2 -Mode accelerated -Runner fast -StateLogPath temp\d1_level5_202713_d1_in_d2_physics_probe_extract_state.jsonl -RngLogPath temp\d1_level5_202713_d1_in_d2_physics_probe_extract_rngtrace.jsonl -ReplayDebugLog -KeepSandbox -TimeoutSeconds 300`
