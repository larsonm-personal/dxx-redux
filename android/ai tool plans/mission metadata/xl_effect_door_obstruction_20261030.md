# XL effect objects and door obstruction

The final chunk audit found that the XL door owner still delegates obstruction
to Redux's actor filter. Supported XL static explosions (19) and sound effects
(20) can therefore obstruct doors, although the pinned source explicitly skips
both types. This is a correction to supported gameplay, not wider wall-flag
admission.

1. Extend existing real-geometry actor controls to reproduce the failure on
   both sides of a door before correcting production behavior.
2. Keep obstruction filtering in the XL door owner, retain the documented
   two-sided geometry correction, and leave Redux behavior unchanged.
3. Verify authored close, automatic waiting and closing/reopening behavior with
   positive ordinary-actor controls. Exercise Android through an existing owner.
4. Run pinned source comparisons and native acceptance, document the reproduced
   defect and tested scope, then publish the isolated correction and inspect CI.

Ignore-marker wall flags and other unsupported object types remain separate
work. Do not hide those requirements or infer original campaign completion.

Implementation and local verification are complete on
`xl_more_77_effect_door_obstruction`, in
`C:/local/dxx-redux/temp/xl-batch2-20261027`, stacked on
`xl_more_76_native_pause_checks`. The unchanged production implementation failed
the new actor control before correction. Windows/Linux controls, the extracted
source filter and all 51 Android door-lifecycle steps now pass. The full native
selection has 105 passes plus an audio-scope timeout; that unchanged audio owner
passes in a fresh isolated rerun. Publication and CI remain the final gate.
See [maintained evidence](../../XL_compat/evidence/effect-door-obstruction.md),
including the stale older Android runner, corrected test-weapon construction,
paired-capture audio setup and retained failed runs.
