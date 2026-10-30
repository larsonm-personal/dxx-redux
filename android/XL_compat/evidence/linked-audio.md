# XL linked audio update and native consumers

The shared XL mixer now has a linked frame policy and explicit native APIs for
object and segment sounds. Geometry is prepared outside the SDL audio lock;
stable owner serials associate those results with the registry after channel
completion or replacement compacts its entries. Named, unlinked and linked
sounds use the same 128-channel pool. A looping source retains its lifetime when
inaudible and restarts when audible; an object signature change deletes the old
lifetime. Effects-slider changes and the existing XL frame hook run updates.

Ordinary gameplay callers are not switched over. Linked-owner save/restore,
native ambient sound/lightning effects, external WAV owners, complete creation
source comparisons and wall-change qualification remain unfinished. The current
save reader still rejects linked voices. No admission rule changes or campaign
playability claims follow from this checkpoint. Native effect objects are not
admitted, so source-policy coverage of enabled/disabled effects does not claim
that those objects run in the engine.

## Executed source policy

The existing source executable extracts `CAudio::SyncSounds` from pinned commit
`2576a98b5de310e3ac7c9777df2677d1627b8bae`, together with actual channel gain/pan
consumers. Its 11,529 frame comparisons cover missing/reused objects, sound and
lightning enable state, custom channels, main versus ambient gain, gain rounding,
deferred/unavailable starts, silence and restart. Object storage, channel start
storage and supplied spatial outputs are independent adapters; geometry and the
full-pool allocator have separate source comparisons. All earlier 942 ownership
comparisons remain.

The production mixed-pool suite now has 17 controls. The added full-pool update
starts owner 500, replaces finite owner 1 and removes missing owners while
retaining the other playing bindings, playheads and exact PCM. Missing first and
middle owners cannot disturb named or unlinked playback. All 17 controls pass
AddressSanitizer and UndefinedBehaviorSanitizer with leak detection and immediate
failure enabled.

## Native integration

Windows and Linux pass the existing named-audio runtime runner for 23/40 and
27/40 fixtures, now also exercising:

- Native source movement and listener rotation, with actual callback PCM checked
  against the separately qualified sample bank and channel controls
- Out-of-range stop/restart, effects-slider updates and stable owner identities
- Object-slot reuse without disturbing unrelated named or ordinary voices
- One-shot unlinked playback, deferred permanent owners and custom channels
- Fixed segment sounds, the half-unit duplicate radius and side/sample deletion
- The distinction between missing-object cleanup retaining a stopped forever
  owner and a frame update deleting that missing lifetime

Linux completes the full existing runtime test in 39.62 seconds, within its
unchanged limits. Existing shared-bank, named-action, disk/rewind, 48 kHz reopen,
resource-scope and 2,232 loaded-mine source comparisons still pass. Their save
coverage applies to named/unlinked voices, not the new linked owners. Only the
previously recorded teleport/projectile test warnings appeared in the Linux
build. No Android integration result is claimed for these new consumers yet.

The movement fixture initially put the source directly on the listener's ear
axis. The pinned ear-angle function returns zero at both exact unit-dot
boundaries, so that case cannot assert opposite panning. The ordinary movement
fixture now uses off-axis positions; the independent source tests retain axial
edge coverage. Production math was unchanged. The earlier full-pool test also
corrected its expected `SetPan(65536)` bytes to `{254, 0}` and corresponding PCM;
the independently source-compared channel implementation was unchanged.

## Deliberate source corrections and limits

Ordered-container binding repair remains the documented correction in
[ownership evidence](audio-ownership.md). Native object duplicate detection also
avoids a source union bug: `FindObjectSound` reads `m_linkType.pos.position` when
the active member is `obj`. Fresh signatures overlap the position's x field;
recycled entries can retain old segment coordinates. Native lookup instead
matches the live object index, object signature and physical sound. This
correction is explicit; creation is not claimed to match that erroneous memory
interpretation. Segment lookup retains source position/radius semantics.

Persistent object creation uses sound class as decay, as the pinned source does;
one-shot creation uses its supplied decay. Physical-sample callers retain
responsibility for logical translation and forcefield custom-volume scaling.
The native UI supplies one SFX gain to both categories; the policy separately
verifies distinct main/ambient gains for their future settings consumer.

Artifacts: `temp/xl-linked-audio-20261029/`, plus the existing Windows/Linux
`named-audio/runtime.log` outputs, in worktree
`C:/local/dxx-redux/temp/xl-batch2-20261027`.
