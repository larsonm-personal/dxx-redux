# XL per-channel gain and pan foundation

The owned mixer now carries each channel's SDL gain and two pan bytes. Channel
volume updates also change its recorded replacement-priority volume, while
preserving class, persistence, owner, serial and playhead. Stopped channels
ignore updates. Global mixer gain combines with channel gain before sample
mixing, using SDL's integer order and signed saturation.

This is the output foundation for a shared ordinary/named/ambient pool. It does
not yet activate ordinary replacement playback or ambient sound objects. Those
consumers, their sound classes, linked-owner cleanup and saved nondefault
channel parameters remain unfinished. Their existing admission gates remain.
Existing named runtime voices retain their default full channel gain and
centered panning; their saved state and documented global-slider policy are
unchanged. No original mission is newly admitted by this increment.

## Executed source

The existing source owner now also extracts `Mix_VolPan`,
`CAudioChannel::SetVolume` and `CAudioChannel::SetPan` from pinned XL commit
`2576a98b5de310e3ac7c9777df2677d1627b8bae`. The active fixed-point macros and
`FRound` come from the same hashed source header. Independent channel storage
and SDL-call adapters record the resulting gain, pan and priority values.

Startup rounds `float(pan) / 65536 * 254`; channel updates divide by
`32767 / 127`. The implementation preserves that difference, including byte
conversion at endpoints. A negative startup pan retains the prior pan; a
negative update follows the source's signed division and byte conversion.
Volume changes retain pan and do not restart or replace the voice. Equal-volume
updates retain existing output, as the source does.

Both Windows and Linux pass 199,311 source comparisons. They include every
normal startup pan and effective volume, update values around both ends of
the normal pan interval, signed endpoints, ambient/effects volume selection,
stopped channels and identity preservation. Programmatic volume requests that
would overflow the source's signed doubling are rejected before mutation.
This does not claim unsupported caller or backend profiles are qualified.

## Actual output and existing runtime

The existing SDL_mixer owner now passes 78 actual PCM comparisons, retaining
all 54 previous cases and adding 24 with distinct gains and pans across four
simultaneous voices. It covers stereo extremes, repetitions, integer volume
composition, byte panning and clipping. The three affected Linux contracts pass
in 1.63 seconds; the two affected Windows contracts pass in 0.27 seconds. No
new public test process or membership change is introduced.

The existing Linux named-audio integration passes in 35.76 seconds, including
23/40 and 27/40 actual output, full 128-voice allocation, saved cursor/priority,
33 malformed-trailer variants, disk-through-stock and normal/secret sound
folders. Its ordinary replacement-bank checks continue to reject unapplied
resources. This protects existing playback; it does not substitute for future
ordinary/ambient consumer integration.

Android passes all 73 steps in 147,610 ms, run
`2ef1a9f34ff54a2ea9aed1815330fb0f`. Its 33 actual-output checks compare 758,784
PCM samples at 48 kHz, with 23 full-pool continuation checks and 3,063 nonzero
music samples. Disk/rewind and stock transitions pass, and all 26 protected
app-file hashes match before and after. The APK builds successfully; scoped
formatting and diff checks pass. Artifacts are under
`temp/xl-audio-controls-20261029/`.

Validation deliberately reuses the affected public source/output owners and the
existing native/Android audio integration. Unrelated full-engine suites are
not repeated locally for this isolated mixer extension.
