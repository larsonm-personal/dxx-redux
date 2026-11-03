# XL overnight execution plan: November 2-3, 2026

Deadline: **November 3, 2026, 10:00 AM America/Los_Angeles (PST, 18:00 UTC)**.
Stop new implementation at **09:40 PST**. This is a plan for a separately started
goal, not authorization to start it in the handoff-writing session.
Designed for a single Sol medium session: small changes, explicit evidence,
bounded investigations, and a durable checkpoint after each task.

Start with `temp/xl-overnight-handoff-20261102/final_handoff.md`. It supersedes
November 1's readiness, branch and phone-status claims. Follow AGENTS.md and
.github/copilot-instructions.md. Earlier plans are evidence, not a mandate to
repeat completed work. Do not read enormous historical ledgers end to end.

## Outcome and non-negotiable constraints

Produce a normal tap-install APK that loads user-imported original Pumo resources,
supports a bounded ordinary hub-to-PMC playtest, and preserves non-XL loading,
stock saves and existing user data. Deliver improvements promptly; full campaign
completion is not a prerequisite. Do not claim completion if the first transition
still fails. Keep working on the ranked queue until cutoff.

- No proprietary assets in APKs or published bundles. No DXA builder/distributor.
  Users import their own XL 7z or raw supported files through normal launcher UI.
  General Import Files must recognize addon-only distributions too.
- Pumo supplies its own HAM/VHAM/models. Its manual requires D2 v1.2 and XL,
  not Vertigo. Do not use Vertigo as a substitute for missing Pumo definitions.
- Never modify original mission bytes, weaken dependency guards, remove hazards,
  grant inventory, force exit dispatch, or bless an invalid save to get a pass.
- Preserve loading progress, import progress, briefing sound/page fixes, readable
  errors, metadata-browser fixes, D1/D2 defaults, and save isolation.
- User phone permission is read-only logs/resources/config. Do not install, stop,
  launch, clear or stage on it. Use an owned disposable emulator/profile for tests.
  Identify every device/process afresh; serial numbers are not ownership proof.
- Commit/push/open PRs only in larsonm-personal/dxx-redux. Never merge or push
  upstream. Use rebase, not merge commits, to update the stack. Set GH_REPO to
  the fork for gh stack; gh's repository default points upstream.
- Device tests are serial. Preserve backups and compare hashes after restoration.
  Do not run generic clear/reset helpers against user data.
- No subagents unless the user separately authorizes them. Do not change models
  or start another timed goal automatically.

## Working loop: use this for every task

1. Write a concrete question and expected observable behavior in progress notes.
2. Inspect the current implementation and an existing owner/test. For unfamiliar
   XL semantics, inspect the pinned reference source before changing behavior.
3. Reproduce once with exact build/assets and retain logs. Classify failures as
   engine, importer/staging, harness, planner unsupported, timeout or infrastructure.
4. Make one coherent fix. Keep the non-XL branch of behavior unchanged unless a
   shared defect is proven. Do not refactor unrelated code while debugging.
5. Run the narrow meaningful regression and corresponding stock/isolation control.
   A parser test does not prove rendering; a controlled event does not prove play.
6. Format once with a scoped mixed-language invocation. Wait for it to terminate.
   Run applicable catalogs when adding automation. Check git diff --check.
7. Commit before building a candidate; inspect HEAD after commit because the local
   post-commit hook amends dates and can change the SHA printed initially.
8. Record command, exit code, build SHA, asset hashes, result, evidence paths and
   next action. Keep an explicit tested/untested table, not a general "works" claim.

If a focused investigation has no new evidence after 45 minutes, preserve it,
state the missing observation, and choose an independent task below. Never hide
the blocker. Do not repeatedly run the same failing scenario without a change or
new diagnostic. Poll the same process handle after observation timeouts; do not
restart a live build/test. Do not use idle CI waiting to fill the overnight run.

## Time allocation and task queue

Durations are budgets from actual start, not promises. Shorten later work to keep
09:40 preservation time. If all initial gates pass early, continue down the queue.

### A. Refresh and isolate (15-25 minutes)

1. Read the handoff and stack-heads.json. Inspect status, stashes, local/fork heads
   and PR states. The stack may have merged while idle. If required dependencies
   are all in cmake, fetch and branch there; otherwise branch from the current
   stack tip and target that parent. Never reset or drop preserved user work.
2. No need to monitor the existing stack's CI: the user explicitly said to leave
   it running. Query PR/merge state as needed to select the base. Record new CI
   links on pushes; investigate failures encountered, but do useful local work
   instead of polling. Do not wait past cutoff for CI.
3. Confirm free disk space before large output. The host had only about4GiB free.
   Follow artifact retention; preserve published bundles. Remove only owned,
   reproducible scratch artifacts after verifying backups/evidence are retained.
4. Start an owned emulator if needed. The prior AVD is DxxPhoneFog20261102 under
   temp/xl-phone-fog-20261102/avds; inspect current identity, ports and processes.
   Do not assume it remains running or that emulator-5556 is available.

Exit: known branch/base, owned test resource, protected profile backup, no unknown
live test touching the device, and enough disk for the selected work.

### B. Close the actual Android firing/transition failure (60-120 minutes)

Current first question: does the GLES texture-state fix let normal firing and the
hub-to-PMC transition complete with original Pumo's own HAM/VHAM/ASE/OOF?

1. Retained pre-rebase candidate8652425/version29420 contains the fix but has not
   been installed/tested. Its six native IDs and APK hash are verified. It is
   useful for a quick causal check, but the final delivered candidate must be
   rebuilt from a committed current stack head after the rebase.
2. Use the exact protected `test_xl_pog_runtime.ps1 -OriginalHub -MissionFlow
   -PmcPlaytest -HubActivation` invocation in the handoff. Supply full original
   PumoMods and owned camera HOG; omit VertigoHam. Confirm staging has not fallen
   back to the old reduced runtime bundle. Retain logcat and automation receipts.
3. Prior failure: first firing step produced GL_INVALID_ENUM from
   glIsEnabled(GL_TEXTURE_2D), then model114 reported "RGBA POG color upload
   failed". The fix uses Android's tracked texture state and OGL_ENABLE/DISABLE.
   Do not suppress glGetError or remove upload validation if a new failure occurs.
4. Require ordinary laser activation, physical crossing to level11, correct
   original resource identities, hub save/restore and repeat activation. The
   runner uses a fixed firing pose; report that, not full human traversal.
5. If failure: capture the first engine error, model ID, source path/skin, GL
   state and last automation step. Reproduce on the same owned emulator before
   changing loaders. Follow consumers in runtime/oof_renderer.cpp,
   runtime/resource_validation.cpp, runtime/oof_resource.*, particle_renderer.cpp,
   d2/main/bm.c, mission.c and polyobj.c. Restore state even after failure.

Exit: exact-APK passing receipt and restored hashes, or a precise retained blocker
with a tested fix in progress. Do not label mere hub entry as transition success.

### C. Stock isolation and real lifecycle (60-90 minutes)

Once B passes, use the protected POG runner's separate SaveIsolation mode and
existing native mod-data-test controls. Read parameter exclusions before running.
Check Pumo -> Counterstrike -> Pumo, stock New Game, stock save/restore, a Pumo save
after a model-bearing level, cold process restart and context recreation. D1
smoke is required if shared loading/rendering code changes; shared native build
success alone does not prove D1 startup. Preserve all pilots/settings before and
after tests. Do not transplant an old save fixture without validating its source.

Model data must be cleared on mission change and restored for the saved target
level. Save identity must reject changed owned skins/definitions before replacing
the live world. Reuse the existing malformed-source and repeated stock-roundtrip
tests; do not repeat all14 levels unless the changed loader invalidates that scope.

Exit: recorded stock and XL lifecycle matrix on the exact candidate; no leaked
custom tables, models, source hashes or saves into ordinary missions.

### D. UI-only import and prompt delivery (45-75 minutes)

Use the normal launcher UI on an owned profile: import the user's local XL 7z,
then original Pumo, enable required resources, launch. No command-line installer
options, hidden adb resource staging or custom DXA may be required of the user.
The protected engine fixture proves engine behavior, not this UI import path.

Validate addon-only detection in General Import Files, the grouped D2 tracker,
the dedicated addon import action, raw supported-file import, useful progress
during the roughly20-second scan/extraction, cancellation/failure cleanup and
successful import confirmation. Test relevant ResourceModStore JVM and existing
instrumentation owners. Do not extract the whole1.5GB archive without need.
Inspect catalog first and extract only supported resources with existing generic
archive machinery. Do not silently import unrelated copyrighted campaigns.

Check briefing page-boundary sound stops, error wrapping/touch dismissal, and that
the existing show-progress-bars option covers native XL bars and defaults on in
both default/original presets. Preserve these existing changes; only fix a proven
regression. Use actual UI labels from current strings, not guessed instructions.

When B-D give a useful verified slice, commit and build a universal APK, verify
all six embedded IDs and package/version, test that exact APK, hash it, and retain
a new README/manifest. Include only APK and documentation, no imported assets.
Install by tapping APK; import through UI. Existing saves/settings must survive.
Clearly say Samsung remains untested on this candidate until user feedback.

### E. Ordinary progression, controls and persistence (2-3 hours)

Choose one concrete unverified authored interaction at a time, beginning with the
first reachable PMC1 door/switch/objective after the newly working transition.
Read PUMO_PLAYTEST.md's matrix and existing PmcOpeningGates/PmcInnerGates/
PmcSwitchPlaytest/Pmc2Selectors/Pmc3Blue/Pmc3Gold owners to avoid duplicate work.
The older controlled-exit passes are historical and are not earned-progression
proof for the full newly loaded custom model/weapon set.

For each selected interaction record original level/file, segment/side or object,
required inventory, normal activation, expected wall/trigger/object effect,
observed effect and save/reload persistence. Fixed positioning is allowed; keys,
ammo, damage, doors and exit prerequisites must be earned normally. Include
ordinary fire/contact on a custom robot where relevant, not only geometry loads.

Rank fixes: crash/data loss -> progression -> controls/combat -> save/load ->
performance preventing play -> visuals conveying information/hazards. Do not
spend this tranche on decorative lighting, new shaders or optional powerup art.
PMC3 guided missile evidence is partial: read evidence/pmc3-guided-damage.md and
the wall-contact follow-up before trying again. Do not confuse guided-missile
control with the automated GuideBot route pilot below.

Exit per task: normal activation plus persistence evidence, or a precise blocker
and next test. Publish coherent roughly one-hour child PRs rather than one huge
unreviewable overnight diff. Never relax guards just to expose another level.

### F. Rendered GuideBot follow-up (maximum1-2 hours initially)

The pilot already exists; do not repeat its feasibility work blindly. Read
android/XL_compat/evidence/rendered-guidebot.md. Historical hub->PMC route reached
segment77 in548 physics frames but correctly ended unsupported because the XL
exit did not accept a companion actor. That is not evidence to change actor gates.

Audit refreshed full-original staging and XL trigger/teleport/multiple-exit
semantics. Reuse regenerate_all_guidebot_simulations.ps1 in Desktop or Headed
mode. For the explicit hub exit use Desktop -ExitTrigger3 (current helper permits
explicit exits only in Desktop, without WriteRegression). The authored start is
segment115; exit trigger3/wall10/segment76/side2 targets level11. Inspect helper
parameters and the retained invocation in the evidence directory before running.

Use a disposable profile and full current PumoMods, not the historical runtime
tar with substituted Vertigo. Generic Headed mode clears mods/saves. Protect or
isolate before invoking it. Keep renderer, exact executable/assets, objective
trace, final position/segment, wall/trigger state, terminal status and screenshot.

The view follows a physics-driven companion; the player is sandboxed and some
objectives are assisted. Label confirmed/partial/timeout/planner unsupported/
harness failure/gameplay defect separately. Pair important results with ordinary
player probes. Never grant a key, force an exit or change mission content to make
the pilot green. If the actor limitation remains, retain it and move to one simple
original route the planner can represent; do not rewrite navigation overnight.

### G. Independent useful work if a primary task is blocked (remaining time)

Pick the first item with a concrete question and executable validation:

1. Measure full-model load phases and peak memory on the owned emulator; distinguish
   archive copying, parsing, texture decode, GPU upload and duplicate save hashing.
   Optimize only measured redundancy while retaining full source identity and
   responsive progress. Do not claim emulator timing is Samsung timing.
2. Audit custom-model gunpoints and collision on a reachable ordinary robot/weapon.
   Existing parser/desktop checks do not prove runtime contact or damage. Reuse
   source markers and native transforms; retain native POF where available.
3. Extend strict malformed/missing/foreign-owner load rollback coverage for one
   uncovered transition; verify live stock/native bytes are unchanged after failure.
4. Add one qualified original XL mission/level to the bounded navigation smoke
   only if actually rendered/loaded and verified for the intended scope. Read
   evidence/navigation-smoke.md; its seeded segment20 checks are assisted and
   are neither campaign completion nor Pumo coverage. Do not broaden allowlists
   merely because the parser accepts the mission.

## Stop and handoff at09:40

Finish or safely bound active local tests; verify restoration before stopping owned
resources. Preserve incomplete changes with exact status and next command. Do not
start a build that cannot finish by10:00. Do not extend the cutoff for CI.

Write a fresh self-contained handoff and copyable prompt. Include what changed
and WHY; APK/build IDs/hashes/UI install and launch instructions; tested/untested
boundaries; exact stack/PR targets/heads; validation and pending CI links; preserved
work/resource cleanup; ranked next tasks with reasons and reproduction commands.
The next prompt must reference that new handoff, carry these constraints and this
same repeating deliverable requirement, and use the next user-supplied absolute
cutoff/time zone. If none is supplied, leave an explicit placeholder. Do not start
the next goal automatically. Completion means verified requested behavior, not
elapsed time, a green parser, or an exhausted test queue.
