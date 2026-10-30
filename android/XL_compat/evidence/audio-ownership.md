# XL sound-owner lifecycle foundation

The owned audio library now has the source's 500-entry sound-owner registry and
128-channel shared allocator. Voices distinguish named actions, linked sound
owners and unlinked requests. Replacement retains the old voice identity long
enough to retire its owner without stopping the new voice. Finite owners are
removed on completion; forever owners detach and remain restartable. Registry
mutation belongs to the game thread under the mixer lock; the PCM callback only
advances voices and records completion.

This increment supplies and verifies the lifecycle library. The runtime still
only routes named actions through the owned mixer. Ordinary playback, actual
caller classes, linked spatial updates, ambient objects and saved nondefault
channel/owner state remain unfinished and gated. No admission rule changes and
no original mission is newly qualified. The new ownership tests combine real
library allocation, PCM mixing and cleanup; they are not engine consumer tests.

## Executed reference and intentional correction

The existing source test extracts the actual pinned XL functions from commit
`2576a98b5de310e3ac7c9777df2677d1627b8bae`: sound-object start/stop,
lowest-volume suspension, deletion, object/segment destruction, completion,
cleanup, active counting, channel completion and unregistering. Actual CStack
Grow, Shrink and Delete methods operate on independent bounded storage.
Channel start/output and native-object lookup are adapters. The separate
existing channel allocator and SDL PCM comparisons retain their own coverage.

Additional source pins:

- `audio/objectaudio.cpp`: `e44484af297ce0c20c92feb00bfb2795316980434e3f1e1d85194c1d46b992e3`
- `include/cstack.h`: `7ee46bb90556774f7d983b5737815813e5a6e96e4195be71f9f58dfc6705f8ee`

The executable reference reproduces a source bug: CStack deletion shifts all
following entries, but DeleteSoundObject repairs only the last owner's channel
as if deletion had swapped entries. Deleting the first of three live owners
leaves both survivors with wrong back-references; source Cleanup then discards
both unrelated sounds. Our ordered deletion repairs every shifted binding.
This is an intentional correction, not a claim of identical buggy behavior.
Source owner order, selection and occupancy are compared before erroneous
cleanup; production binding invariants and surviving PCM are checked separately.
The single-shift source control remains correct and is checked too.

The 942 source lifecycle comparisons cover counts through 500, finite/forever
completion, first/middle/last deletion, object lifetime reuse, object/sample and
segment/side/sample filters, suspension ties, the 42-channel permanent threshold,
deferred start and unavailable output. A first test failure came from calling
EndSoundObject while its channel was still playing. The corrected test executes
actual SetPlaying(false) and UnregisterChannel before owner completion, matching
the source caller sequence. It does not change production to match that invalid
setup. Source suspension with no eligible owner is rejected defensively rather
than indexing -1; normal compared thresholds stay within defined source input.

## Output and lifetime controls

Fourteen mixed-pool controls execute the production registry and mixer together:

- Preserve surviving PCM, serials, playheads, gain and pan after ordered removal.
  Equal numeric action/object indices do not share ownership.
- Mix linked, named and unlinked voices together against hand-calculated PCM.
- Complete finite playback in the real mixer, then reconcile before or after
  slot reuse. A late completion cannot stop its replacement.
- Fill 500 owners and 128 channels; replace the quietest finite or forever owner
  with each owner kind. Rebind a newly started owner if retirement shifts it.
- Keep forever owners restartable after a backend stop and distinguish reused
  native object indices by signature.

Windows source/PCM controls and Linux source/PCM/SDL controls pass. The existing
78 SDL comparisons remain unchanged. The mixed-pool controls also pass Linux
AddressSanitizer and UndefinedBehaviorSanitizer with leak detection and immediate
failure enabled. No new CI test process is introduced: the existing source and
voice owners execute these cases. Public membership remains 43/46; required
private membership remains 106.

The final affected Windows contracts pass in 0.24 seconds and Linux contracts
in 1.63 seconds. The rebuilt native named-audio integration passes in 32.28
seconds, covering existing output, 128-channel continuation, disk/rewind,
malformed saved state, folder scopes and stock recovery. It protects existing
runtime behavior; it does not qualify ordinary or ambient engine consumers.
Only previously recorded unrelated teleport/projectile test warnings appeared.
Scoped mixed-language formatting and git diff checks pass. No additional Android
run is claimed for this library-only increment.

Artifacts: `temp/xl-audio-ownership-20261029/` in the active worktree.
