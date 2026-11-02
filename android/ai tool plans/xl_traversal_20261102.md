# Original Pumo traversal and progression - November 2, 2026

Cutoff: November 2, 2026 10:00 America/Los_Angeles (18:00 UTC)
Cleanup and handoff reserve begins at 09:40 (17:40 UTC)

## Baseline and preservation

- Refreshed fork cmake: 8548c6da777283d11db4b832458a70555229d82b
- PRs 278-293 are merged; PR293 exact-head workflows all passed
- New branch xl_playtest_138_guidebot_pilot targets cmake
- Preserve the existing gameplay-first plan addendum and user stash
- Published baseline remains temp/pumo-final-playtest-20261101.zip
- No user devices or processes are owned by this goal
- Receipts: temp/xl-traversal-20261102

## Work sequence and acceptance

1. Time-box initial GuideBot feasibility to two hours from 22:18 November 1
   Audit original staging, XL actor/trigger/teleport/exit semantics, then attempt
   one explicit authored route in the existing rendered runner
2. Prefer a disposable native profile if the XL rendered build supports it;
   otherwise provision an owned Android emulator and retain identity evidence
3. Record build and asset hashes, objective trace, last actor state, terminal
   classification and visual evidence. Assisted companion routing does not
   establish normal-player combat or earned campaign progression
4. Pair unsupported planner steps with normal-player activation and persistence
   probes. Select concrete original doors, switches, exits and hub returns
5. Fix evidenced gameplay defects, preserve original bytes and useful visuals,
   run relevant validation and publish coherent fork-only stacked PRs
6. Commit before replacement builds, verify embedded IDs and exact APK, preserve
   saves/settings and earlier bundles, and deliver useful verified improvements
7. Maintain tested/untested boundaries and exact-head CI while continuing work

## Repeating final deliverable

Write a fresh self-contained handoff with changes and reasons, artifact IDs,
hashes and launch steps, supported/untested boundaries, stack order/PR targets/
heads, validation/pending CI, preservation/resource cleanup, ranked next work
with reasons, and a copyable continuation prompt referencing that handoff.
Carry all objective constraints and this repeating requirement into the prompt.
Use an explicit user-filled cutoff placeholder if the next cutoff is unknown.
Do not automatically start the following goal

## K: PMC boss combat and difficulty-dependent vulnerability

Normal starter-laser probes confirm front-hit rejection, rear damage, and a boss
retiring from six required objectives to five without starting the countdown.
The successful kill probe lost the player; sustained-fire and early-retreat
attempts also lost aim under combat. Preserve those limits rather than calling
this a surviving encounter or earned progression.

A separate source audit found that pinned XL ai/aiinit.cpp::InitRobotsForLevel
sets the vulnerable-spot dot threshold to 1/4 - difficulty/8. Our D2 variable
remains zero at every difficulty. Add a native collision-decision regression
across all difficulties, demonstrate the mismatch, then initialize the XL
threshold while retaining stock behavior. Check save/load and stock isolation.
Continue the original normal-input boss probe on the exact resulting APK; no
mission edits, granted weapons/health, forced damage or controlled exit dispatch.
