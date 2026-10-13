# Public secondary-fire capture and playback

1. Minimize the two-tap mismatch at normal public level starts in both engines
2. Capture through ordinary keyboard/idle control dispatch with production frame ordering; inspect missile cooldown and event-boundary behavior before changing engine code
3. Preserve immutable existing oracles and keep any new replay/oracle pair under 100000 bytes
4. Add focused semantic and negative checks to the existing replay runner
5. Validate independent live captures, headed and true headless replay, natural exit, and scoped formatting/catalog checks
6. Publish exact source provenance and a focused stacked draft, then monitor exact-head CI

No save/checkpoint payload, warp, injected world state or merge is authorized

## Diagnosis

Both native D1 shareware and D2 demo level-one starts reproduce the same information
loss through actual SDL_PushEvent, event_poll, game-window input dispatch and
ReadControls. At a fixed 3276-tick frame, secondary presses at frames 2 and 12,
with releases at frames 3 and 13, place the second press eight ticks before the
loaded 32768-tick concussion cooldown. Swap an ordinary zero-motion mouse event
before or after the frame-13 release. Both recordings have exactly the same old
held-state/pulse input stream, yet mouse-first spends two missiles and release-first
spends only one. Normal gameplay is event-order dependent at this boundary.

Unchanged native headless playback chooses two shots in both engines, rejecting
the release-first terminal ammo (expected 4, actual 3). D1 mouse-first also matches
every complete live frame state, diagnostic and SIM RNG snapshot. The first
release-first state divergence is boundary 13. This is a production recording
information gap, not evidence for making the capture fixture imitate replay.

The existing held-secondary post-frame pass was deliberately selected for an
opposite historical schedule. Removing it or switching its condition would merely
choose another schedule and risk old recordings. Existing immutable expectations
must remain unchanged.

## Proposed correction, pending approval

A new format version should preserve a bounded, ordered control-dispatch timeline
while legacy versions retain their existing playback path. The new path must not
change ordinary gameplay timing. Capture actual input-dispatch FrameTime, preserve
command/sample ordering and marker resets, and distinguish final residual controls
from already-consumed event pulses. Include initial pre-input RNG context where
needed; never regenerate expected simulation state from replay output.

Admission and parser tests must reject malformed, oversized, unsupported or
mis-versioned samples. Acceptance covers both actual SDL event orders in both
engines, hold/release/tap behavior, repeated independent captures, headed and true
headless replay, natural exit, removed/reordered input negatives, corrupted oracle
negatives, and every existing immutable public expectation.

### Required dispatch boundaries

- Record semantic input operations rather than raw platform-specific SDL events
- Record ReadControls entry resets even when a key handler returns before the
  control sample; a sample at only do_weapon_n_item_stuff is insufficient
- Preserve direct commands in the same ordered operation stream
- Keep the actual input-time FrameTime separate from the later simulation step
- Verify replay-computed residual controls and post-input SIM state/count rather
  than overwriting a divergence with the recorded values
- Establish initial pre-input SIM state/count and trace context before the first
  executable operation, including recording frame zero
- Define pending operations at stop/truncate boundaries explicitly; do not report
  a successful recording whose terminal state includes unrecorded input effects
- Encode empty operation lists explicitly and fail closed on count/byte overflow,
  unknown executable operations, invalid values, wrong versions or unsupported
  command phases; no silent truncation or fallback to the legacy heuristic
- Compact packing must retain every executable operation in order and hash it;
  only already-reviewed observational telemetry may be stripped

Additional acceptance should exercise variable input/simulation timesteps,
reset-only handled events, held/release/press ordering, repeated input events,
queued missiles, flares/bombs, cycle/select ordering, primary-pulse nonduplication,
first-input SIM draws, direct-command interleavings and stop/truncate behavior.
Rear-view wall-clock timing and modal automap need honest scope boundaries or
explicit unsupported-capture rejection; these missile scenes do not qualify all
input determinism. Preserve the existing FX qualification boundary.

## Local verification checkpoint

All eight native combinations completed 25 frames and exited naturally through
the existing strict replay wrapper: D1 and D2, release-first and mouse-first,
headed under Xvfb and true headless with no display and an invalid SDL video
driver. Release-first fails in each backend at complete frame boundary 13 with
expected concussion ammo 4 versus actual 3. Mouse-first passes every full live
frame gameplay state, diagnostic and SIM RNG snapshot plus terminal result in
both engines/backends. Optional headed capture screenshots were generated and
inspected in each game. Scoped formatting passes.

These are development diagnostics, not admitted provenance-qualified oracles.
No production engine or recording-format code changed. Source and evidence remain
local while the versioned-format decision is pending; no remote publication.
