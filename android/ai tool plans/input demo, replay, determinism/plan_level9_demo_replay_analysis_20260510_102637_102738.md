# Level 9 replay analysis 2026-05-10 102637 102738

## Goal
- determine whether the two new D2 level 9 save-checkpoint failures share the same upstream control-flow split
- use the bundled rng traces, recorded frame diagnostics, and replay owner logs before adding more instrumentation
- decide whether the new v26 checkpoint recordings validate the recent object-signature restore fix or expose a different engine nondeterminism

## Steps
- [completed] rerun `102637` with state trace, rng trace, and durable replay probes enabled
- [completed] compare `102637` recorded vs replay traces far enough to replace the later `sig=21240` symptom with the first player-owned slot mismatch at `sig=21231`
- [completed] inspect the owning weapon-vs-robot collision path and add durable probes for accept/skip plus the unconditional badass-explosion path
- [completed] rebuild and rerun `102637` after each probe hop to validate the new owner logs
- [completed] trace the remaining transient fireball `sig=21230` upstream to debris `66 / sig=21225` and prove it is a replay-only debris `wall_hit`, not accepted weapon-hit handling, delayed explosion sequencing, small-fireball death-roll, or debris old-age expiry
- [completed] add per-frame object-state hashes to the recorded/replay state trace and compare path so hidden object-motion divergence is caught before it surfaces as later player-weapon slot churn
- [completed] repeat the same first-mismatch analysis for `102738` and decide whether it shares this root cause class
- [in_progress] summarize whether the two failures collapse to one root cause and whether another instrumentation hop or engine fix is warranted

## Notes
- target demos:
  - `android/regression_demos/d2_descent2_level9_20260510_102637.dximdemo`
  - `android/regression_demos/d2_descent2_level9_20260510_102738.dximdemo`
- both demos are fresh `save_checkpoint` recordings created after the versioned signature-seed restore fix, so unlike older DGSS v25 demos they can validate that runtime-state change end to end
- `102637` no longer points at allocator-state restore as the remaining bug:
  - `checkpoint_object_links restore=ok`
  - allocator runtime state was already being serialized/restored
  - the first player-owned slot mismatch is `sig=21231` (`recorded obj=256`, `replay obj=251`), not the later `sig=21240`
- replay-side owner probes for `102637` now show:
  - `sig=21212` accepts against robot `73 / sig=20611`, and recorded logs already showed that hit driving the robot to `dead=true` at `gt=786432`
  - later shots `sig=21216`, `21218`, `21226`, `21228`, and `21231` all hit the already exploding robot and log `step=skip_robot_exploding`
  - the earlier accepted-collision hypothesis for `sig=21228` is falsified
- replay-side owner probes also show a separate, unconditional path before that skip gate:
  - `sig=21228` logs `step=badass_explosion ... expl_obj=206 expl_sig=21229 expl_id=95`
  - `sig=21231` logs the same pattern with its own vclip-95 explosion object
  - this means the extra replay allocator churn starts from the weapon's badass splash path, not from accepted robot-hit handling
- the remaining unexplained transient is still `sig=21230 type=1 id=2` at `gt=825753`
  - allocator move-context and new debris probes now show `ctx_obj=66 ctx_sig=21225 ctx_type=8 ctx_control=12`
  - replay creates `21230` at `frame=164` from `debris_collision ... step=wall_hit debris=66/21225 ... life=127761 hitseg=342 hitwall=0`
  - the earlier debris old-age hypothesis is falsified because there is no matching `debris_probe step=age_expire` for `21225`
  - recorded vs replay `rng` metadata still match through `frame=164`, so the remaining root-cause search is now in hidden object motion/state, not RNG branch selection
- new shared tooling landed in this tranche:
  - per-frame `diag.live_object_*`, `robot_*`, `weapon_*`, `fireball_*`, and `debris_*` hashes/counts in the state trace
  - `compare_input_demo_state_trace.ps1` now compares `diag` metadata and normalizes inactive AI-probe sentinels so old traces still compare cleanly
- `102738` does not share the `102637` debris-owner pattern
  - first shared mismatch is much earlier: `frame=241` player velocity, then `frame=242` player position/orientation
  - replay owner logs around `frame=240` show player weapon `21628` hitting robot `46`, creating badass explosion `21629`, and immediately logging `player_blast_damage`
  - the nearest controlling path for `102738` is now the player blast-force / wall-resolution path, not debris lifecycle or later allocator churn