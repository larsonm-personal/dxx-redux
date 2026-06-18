# D1 level 9 recreated-demo instrumentation plan

Goal: add general replay diagnostics before the next hand-recorded D1 level 9 demo, so a new recording with different object numbers still exposes the first deterministic split clearly.

Context:

- Current level 9 replay fails in plain D1 before testing d1-in-d2 is useful.
- The old recording first diverges in robot AI static/animation state around frame 468.
- AI-local hashes, player pose, weapon state, and robot object pose still match at the first static split.
- Object 175 then physically diverges a few frames later, but object 175 should be treated as an example, not as a hardcoded target.

Plan:

1. [done] Keep the existing broad state trace additions, but clean them up so they are useful for any new demo:
   - first changed robot `ai_static` slot and signature
   - current versus previous per-slot `ai_static` hash
   - changed `CURRENT_GUN`, `CURRENT_STATE`, `GOAL_STATE`, path, danger-laser, and behavior fields
   - alternate whole-robot-static hash with the first changed entry removed

2. [done] Add robot animation-pose diagnostics, because the strongest current theory is that `rtype.pobj_info.anim_angles` and AI animation state advance out of phase:
   - aggregate hash for robot `pobj_info.anim_angles`
   - first changed robot animation-pose slot/signature/model
   - first changed robot animation-pose hash and previous hash
   - sample `anim_angles`, `goal_angles`, `delta_angles`, `goal_state`, and `achieved_state` hashes for that same changed robot
   - current `CURRENT_STATE` and `GOAL_STATE` for that same robot

3. [done] Add bucket-level hashes for the robot AI categories so a new recording can be narrowed quickly without hardcoded object ids:
   - robot object-state buckets, if not already enough
   - robot AI static buckets
   - robot AI local buckets
   - robot animation-pose buckets
   These should mirror the existing object bucket style and use small fixed arrays.

4. [done] Add a compact "first mismatch neighborhood" helper to the replay runner or comparison output:
   - when state compare fails, print 3 to 5 frames around the first mismatch
   - include the new category hashes and first-changed slots
   - keep raw JSONL files as the source of truth, but make the first diagnosis visible without hand-written PowerShell extraction

5. [done] Avoid gameplay changes during this instrumentation tranche:
   - no special replay correction
   - no hardcoded level 9 object number
   - no D1 engine behavior change unless the new recording proves a specific engine nondeterminism

6. [done] Build and validate the instrumentation:
   - `.\run-windows-build.ps1 -Target d1`
   - `.\run-windows-build.ps1 -Target d2`
   - scoped `.\android\run-code-quality.ps1 -Fix`
   - replay the existing level 6 and level 15 D1 demos to confirm the trace additions do not perturb passing demos
   - replay the existing level 9 D1 demo once with `-TraceState -CompareRngTrace` to confirm the new fields populate

7. [pending] After the next hand-recorded level 9 demo is available:
   - first run it in plain D1 with `-CompareRngTrace`
   - if it fails, rerun plain D1 with `-TraceState -CompareRngTrace`
   - use the first mismatching category and bucket to decide the next engine hypothesis
   - only then test d1-in-d2, once plain D1 replay is understood or passing

Validation notes:

- `.\android\tests\test_input_demo_state_trace_compare.ps1`: pass
- `.\android\run-code-quality.ps1 -Fix` scoped to the changed C/C++ and PowerShell files: pass
- `.\run-windows-build.ps1 -Target d1`: pass
- `.\run-windows-build.ps1 -Target d2`: pass
- `.\android\tests\run_input_demo_replay.ps1 -DemoPath C:\local\dxx-redux\android\regression_demos\d1_descent_level6_20260617_153740.dximdemo -Game d1 -Runner auto -CompareRngTrace -TimeoutSeconds 600`: pass
- `.\android\tests\run_input_demo_replay.ps1 -DemoPath C:\local\dxx-redux\android\regression_demos\d1_descent_level15_20260617_154210.dximdemo -Game d1 -Runner auto -CompareRngTrace -TimeoutSeconds 700`: pass
- Existing level 9 replay with `-TraceState -CompareRngTrace`: expected failure, but the new fields populated. The first useful old-demo split remains `robot_ai_static_state_hash` at frame 468, while `robot_ai_local_state_hash`, `robot_state_hash`, and weapon state still matched at that point. The new `robot_anim_pose_state_hash` was present and did not report a changed robot at that frame in the old recording.

Expected outcome:

- If the next level 9 recording exposes the same class of failure, the trace should say whether the first split is robot animation pose, AI static flags, AI local timing, object physics, or RNG.
- If it is animation-pose related, the trace should identify the robot slot/signature and enough current/goal angle state to decide whether save translation, save restore, or frame animation stepping is the actual nondeterminism.
