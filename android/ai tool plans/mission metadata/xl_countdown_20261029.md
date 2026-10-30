# Owned XL countdowns

Implement the remaining authored countdown action through a separate XL timer,
frame and persistence path. Keep Redux's implementation as its alternate. Pumo's
nine actions all use a 99-second `time` field; their `value` is not the duration.
Other package and gameplay requirements remain independently gated.

1. Execute the pinned `objects/reactor.cpp` initializer and frame function,
   plus the trigger's scheduling exception. Do not use the stale
   `main/cntrlcen.cpp` copy. Compare default/custom durations, preserved initial
   displayed seconds, reset/retrigger, paused/dead behavior, accumulated frames,
   the 40 Hz ceiling, warning order, rotation and the four-second terminal flash.
2. Add an owned XL frame consumer and authored wall/object dispatch. Countdown
   times are seconds, not scheduler milliseconds. Preserve ignored targets and
   existing one-shot/master/autoplay semantics. Validate fixed-point bounds and
   required ordinary sound resources before admission.
3. Persist the actual total duration, displayed seconds, pending frame time and
   cadence phase alongside native timer state. Validate before publishing; test
   corrupted state and stock isolation. Use simulation time for reproducible
   pause/rewind continuation and document the clock boundary explicitly.
4. Use controlled events in both native layouts and Android to verify visible
   countdowns, actual rotation/warnings, ordinary saves, rewind, transitions and
   terminal death. No autonomous level traversal. Existing audio-resource gates
   continue to reject unapplied replacement banks; shared ambient audio remains
   a separate consumer.
5. Reuse the existing source owner, add one native integration owner if needed,
   run affected regression selections and required catalogs/formatting, refresh
   the unchanged original inventory, and publish the bounded increment with
   honest evidence. Do not qualify a campaign from a fixture pass.

## Result

The owned countdown path and action dispatch are implemented. The source owner
executes 370,998 comparisons and 180 terminal episodes; the scheduler also
executes its countdown/message/sound exceptions. Both native layouts preserve
earned special-reactor extensions and partial cadence through disk/stock
transitions, with fifteen atomic malformed-state rejections. Android passes
all 80 HUD, warning PCM, frame, rewind/disk and normal-death controls.

All 43/46 public and 105 private contracts pass. The private suite's earlier
boss timeout under concurrent builds is retained; its isolated rerun passes
without a timeout change. Final browser classifications have focused checks
on both hosts. The source tests reuse existing owners; only one native runtime
owner is added. Catalogs, ownership checks and scoped formatting pass.

Fresh inspection removes exactly nine original Pumo countdown rejections and
preserves every other inventory field and blocker. All fourteen levels remain
blocked. See [countdown evidence](../../XL_compat/evidence/countdowns.md) for
the explicit simulation-clock boundary and precise validation receipts.
