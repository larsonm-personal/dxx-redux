# Expanded routing missions

Add Descent Maximum Fixed, af_d1_beta, and Mandrill to the targeted manifest and discovery tests. Reproduce one failure, diagnose it with engine evidence, implement a general fix in shared source, and validate repeated completion and the expanded corpus. Keep implementation out of headers and leave outstanding_bugs.md untouched.

## Retained changes

The manifest includes all three archives. Descent Maximum contributes 36 levels across two mission variants, AF contributes ten, and Mandrill contributes seven. The set now contains twelve mission files and 262 levels. Discovery and master-regeneration tests verify the expanded manifest and counts.

## Investigation, unresolved

Selected af_d1_beta level 6 (`alarlof.rdl`). Baseline completes the blue-key objective and times out at 6,217 frames while pursuing the gold key. The actor is in segment 435 with cursor 82 targeting an outgoing point in segment 440. The inserted approach point at index 81 overlaps walls at the actor radius of 310,325 fixed units. The raw segment center is distinct from that inserted point; do not infer that the entire segment is impassable from this overlap.

Diagnostics confirmed that the neighboring-waypoint acceptance rule can consume this point while the successor leg remains blocked. Requiring that leg to be clear prevents the skip but still stalls at the approach. Tests of earlier-point recovery, interior sampling, clearance filtering, raw-center repair, waypoint pruning, and guarded smoothing did not complete the level. All experimental engine edits and probes were reverted; no level fix is claimed.

Useful artifacts under `android/temp`: `af6_baseline`, `af6_advance_probe` (blocked successor at 435 -> 440), `af6_leg` (individual sweep checks), `af6_target` (inserted point occupancy), `af6_wall` (raw-center checks), and `af6_safe_smoothing`. Each corresponding build/run log is under `temp`.

Also reproduced Mandrill 1's post-blue-key frontier stall and Mandrill 2's post-blue-key `gold key unreachable` replan. A probe for hard-blocked keyed doors did not confirm that hypothesis. Artifacts: `android/temp/mandrill1_baseline`, `android/temp/mandrill2_key_probe`.

Final validation: scoped formatting/lint, discovery tests, and master-regeneration tests pass. The engine was rebuilt after reverting all experiments. The full expanded corpus at `android/temp/core_twelve_expanded_baseline` confirms all 262 checked-in statuses unchanged: 237 passes, 15 failures, nine timeouts, and one unsupported level. No simulation JSON was changed. The level-fix portion remains unresolved.
