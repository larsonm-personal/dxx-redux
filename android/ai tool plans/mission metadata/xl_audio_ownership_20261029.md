# Owned XL audio lifecycle and shared playback

Worktree: `C:/local/dxx-redux/temp/xl-batch2-20261027`.
Branch: `xl_more_59_audio_ownership`, above PR #206
(`843d578b27db8965d7f09f051ef3343618122416`). PR #205 disarming remains under
CI observation. Keep both published heads intact unless a verified fix is needed.
The active eleven-hour block ends October 30 at 04:57:30 UTC, subject to extra
time for substantial disarming repairs. No merges or autonomous navigation.

## Source findings that determine the implementation

- The current native linked-sound registry has 150 entries. Pinned XL has 500,
  stored densely. Inspection of the actual CStack header shows ordered deletion
  by memmove, while DeleteSoundObject only rebinds the last entry as if it were
  swapped. Execute that mismatch explicitly; do not silently assume swap-removal.
  Reusing Redux's registry would preserve the wrong capacity and lifecycle.
- Permanent source objects may consume about a third of the 128 channels
  (`33 * count / 100`), with lower-volume suspension to admit a louder object.
  Redux uses a quarter and has no equivalent suspension choice.
- XL `EndSoundObject` deletes a finite owner but leaves a forever owner with
  channel -1. `DestroyObjectSound(-1)` checks runtime object signatures; lost
  forever owners stop while finite owners are removed. Actual source functions
  must establish the less obvious active-count and compaction behavior.
- Ordinary mixer playing queries are stubs in Redux's SDL_mixer backend. XL
  requires real occupancy and completion semantics. Do not claim shared
  exhaustion while ordinary voices remain in another pool.
- Source channel classes include generic, ambient, player, robot, laser,
  missile and explosion. A sample number alone cannot always identify its
  caller class. Audit caller boundaries before routing ordinary requests.
- Named voices already have earned event timing, serials, sample identity,
  allocation cursor and saved playheads. Retain those semantics while adding
  ordinary ownership. Do not introduce a second unrelated XL allocator.
- Audio-thread mixing must not mutate engine object/sound-owner registries.
  Reconcile completed/replaced channels on the game thread with checked owner
  identities, and exclude concurrent mixer mutation with the audio lock.

## Work and verification

1. Execute pinned ownership/cleanup/channel consumers with independent storage
   adapters. Cover the 500-entry boundary, dense removal, moved back-references,
   finite/forever completion, deleted/reused native objects, suspension ties and
   permanent thresholds. Keep source quirks distinct from defensive malformed
   input handling.
2. Implement the owned registry and shared voice/resource lifecycle under
   `android/XL_compat/`. Use small C dispatch hooks where the existing engine
   enters ordinary audio; Redux remains the inactive alternate. Prepare both
   replaced and unreplaced ordinary samples before activation.
3. Route actual ordinary/named requests through one pool with their real class,
   volume, pan, repetitions and owner. Preserve output-order semantics, source
   rate conversion and slot reuse. Validate source callbacks and actual PCM,
   rather than merely checking that a voice was allocated.
4. Extend canonical saved state for ordinary owners and nondefault channel
   controls. Check all fields/resources/lifetimes before publication. Exercise
   normal/secret mission scopes, disk, rewind, pause, backend reopen and stock
   recovery. Retain existing named fixtures as regression evidence.
5. Admit ordinary replacement requirements only after those consumers pass.
   Ambient object activation/spatial behavior remains a separate detected gate
   until implemented and verified. Refresh unchanged original inventories only
   when admission actually changes.

Publish reviewable increments with precise evidence. The target is complete XL
singleplayer fidelity; these dependency steps are not a substitute for that
scope. Keep CI savings evidence-based and preserve required source oracles.

## Lifecycle foundation checkpoint

The source witness reproduced unrelated-owner loss after deleting the first of
three entries. The implementation preserves source ordering but repairs every
shifted binding. The 942 comparisons now execute actual SetPlaying(false) and
UnregisterChannel for active completion. Fourteen mixed-pool PCM/lifetime cases
pass on Windows/Linux and with ASan/UBSan, including replacement before game
thread reconciliation. Unlinked requests use owner -1; named and linked kinds
still require a nonnegative identity.

This completes the isolated lifecycle library, not steps 2-5's engine routing,
resource installation, spatial consumers or persistence. Publish it with those
limits explicit, then continue the shared runtime in the same worktree. The
rebuilt native named integration passes in 32.28 seconds, protecting the shared
AudioVoice layout and allocator defaults. No original inventory is
regenerated because admission has not changed.
