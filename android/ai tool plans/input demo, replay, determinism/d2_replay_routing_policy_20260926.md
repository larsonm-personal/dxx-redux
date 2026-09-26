# D2 replay and Guide-Bot routing investigation

1. Compare every retained D2 recording against current Enhanced and Original routing using temporary diagnostic copies, preserving the committed evidence
2. Check current-engine repeatability and inspect the earliest meaningful divergences against recording-time behavior
3. Fix genuine engine defects if found; assess whether legitimate routing improvements justify fresh recordings
4. Document a proposed recording policy that isolates general engine regressions from Enhanced routing work without overriding a recording's declared mode

## Findings

All 11 retained D2 recordings are May 2026 arm64 captures and lack an explicit
Guide-Bot routing mode. Nine identify revision b5222e6; the remaining two identify
4d16d8e7 and 815634fa. Their recorded behavior is not current Original Redux
routing: b5222e6 already used integer path smoothing and Android companion
velocity averaging, whereas current Original deliberately restores Redux's
floating-point smoothing and does not apply that averaging

Changing a temporary copy's header to Original while retaining every input,
checkpoint and expected result produces 10 failures out of 11. The unmodified
corpus produces 9 failures out of 11 under current Enhanced routing. In
particular, level9_20260511_192804 passes Enhanced and fails Original. Changing
the missing-mode default or overriding playback to Original would not repair
this corpus

The prior frame/RNG investigation of level9_20260511_215654 found changed
Guide-Bot object 166/id 33 path-generation draws at frame 51, with robot-state
divergence reported at frame 52. This agrees with substantial intentional
changes in return paths, prerequisite selection, endpoint behavior and recovery
since the recordings. It does not prove that every difference in all nine
recordings is desirable

## Useful behavior verified on the current engine

- Original comparison: levels 1 and 11 each passed twice against the frozen
  Redux reference, including save/replay mode restoration with opposing
  launcher defaults (3,695 and 7,415 checks per run)
- Enhanced live navigation: Counterstrike 11 routes around a grate and follows
  a moved player; Counterstrike 1 retains an unfinished reactor objective while
  patrolling; Maximum 16 preserves the Hostages command through switch/key
  prerequisites and resumes hostage guidance. All three cases pass twice with
  identical result JSON
- The previous fix also passed all 24 seed-268 physical route cases and the
  complete route-regeneration audit

These are behavioral improvements with physical/live coverage, not reasons to
restore the old path decisions merely to satisfy historical terminal values

## Recording recommendation

Fresh recordings are appropriate for the nine failing historical scenarios, with
an immediate capture-versus-playback qualification before retiring the old
evidence. Use Original for new general gameplay/engine regression fixtures;
retain short, explicit Enhanced fixtures and the existing routing integration
tests for Enhanced behavior. Playback must always honor the recorded mode

Set Original before starting a new game, or use an Original-mode checkpoint;
changing the launcher default does not convert an existing save. Verify header
player_cfg.guidebot_routing_mode is 0. Do not patch old headers, replay old input
streams and adopt their outputs as expectations, or globally force Original
during playback

The corpus README now describes this policy and replacement acceptance checks.
No recordings, expectations, runtime defaults or mode-restoration behavior were
changed by this investigation

## Reproduction evidence

| Comparison | Exact matches |
| --- | --- |
| Current Enhanced versus historical recorded terminal state | 2/11 |
| Diagnostic Original versus historical recorded terminal state | 1/11 |
| Current Enhanced versus previous current-engine run | 11/11 |

The complete Enhanced repeat batch exits 0, and an independent parsed-JSON
comparison confirms equality of every terminal field for all 11 results. This
supports repeatability on the tested Windows host, not a claim of complete
frame-by-frame fidelity or Android/graphics parity. The normal historical
corpus still has nine failures. No engine defect requiring another code change
was established by this investigation

Artifacts are under temp/d2_replay_policy: manifest.json preserves source hashes
and recording versions; original/ contains diagnostic copies only;
original_results/ and comparison.json capture the mode comparison. Prior
Enhanced terminal results were recovered from the complete
temp/suite_fix_d2_replays.log into prior_enhanced/ before rerunning. The current
build is based on revision 54404ddc with documentation-only working changes
summary.json also records the tested executable hash; all 11 original recording
hashes were rechecked unchanged after the experiment

Logs: original.log, original_navigation.log, enhanced_navigation.log and
enhanced_repeat.log. Repeat comparisons use ReferenceResultRoot only as an
explicit experiment; the normal suite's historical expectations remain intact
