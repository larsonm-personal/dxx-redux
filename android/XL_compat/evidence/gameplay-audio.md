# Gameplay audio dispatch

The XL logical entry points translate sound IDs through the loaded `Sounds` and
`AltSounds` tables, preserve the source's entry-point distinction for negative
physical IDs, and apply forcefield custom-volume scaling only to the logical
forcefield hum. Source comparisons cover 8,160 translations and 81 forcefield
creation cases. Native Windows/Linux controls exercise actual PCM, explicit
classes, low-memory mappings, rejected requests and 20 forcefield cases.

The engine's looping start, volume and stop entry points now select XL playback
for active XL levels. Redux remains the stock path. The XL loop controller holds
both a channel slot and its serial; a replacement voice cannot be changed or
stopped through a stale loop controller. Bank installation and global stop clear
that controller. Pause preserves playback phase through the existing XL mixer.

Schema 23 saves the optional loop binding alongside the voices and linked owners,
plus seismic volume cadence (the next deadline relative to the saved epoch and
the previous tremor volume).
Restore validates the slot, serial, infinite playback, generic persistent class
and unlinked ownership before applying any audio state. There is no compatibility
reader for the previous development schema.

## Evidence and scope

The pinned source remains `2576a98b5de310e3ac7c9777df2677d1627b8bae`.
The source adapter executes `StartLoopingSound`, `PlayLoopingSound`,
`ChangeLoopingVolume` and `StopLoopingSound`; five volume cases verify requested
playback controls, invalid logical-ID preservation, volume updates and stop.
The adapter also reproduces the source's full-scale startup truncation to zero.
The port deliberately keeps fixed-point volume full-width. Pause continuity and
serial-qualified ownership are deliberate behavior, not claims of identical
source storage or source pause/restart behavior.

Windows and Linux native builds and source tests passed. Both native fixture
formats (23/40 and 27/40) passed the complete audio runtime runner, including:

- Actual PCM through the engine looping API, full-scale startup and live gain.
- Ten malformed loop-binding cases rejected without changing earned state.
- Engine pause/resume continuity and stale-slot volume/stop isolation.
- Mixed loop, linked and named playback through trailer and disk restoration.
- A stock transition that actually starts and stops a Redux loop while the XL
  mixer stays empty, followed by restored control of the saved XL loop.
- A 48 kHz backend reopen and output-rate conversion of saved playheads.

The final Linux runner passed in 45.34 seconds with unchanged timeouts. Android build
and both automation catalog checks passed. The extended Android PCM/rewind/disk
runner passed all 101 steps in 253.393 seconds at 48 kHz, checking 849,408 samples,
45 output cases, five linked cases, five shared cases and 23 pressure controls.
Its linked fixture now saves a seismic loop and checks that restored volume
controls address the same slot/serial. All 26 protected app files were restored
with matching hashes. No new CI job or longer timeout was added.

Artifacts are under `temp/xl-gameplay-audio-20261029` in worktree
`C:/local/dxx-redux/temp/xl-batch2-20261027`; full native runtime receipts remain
in the existing Windows and Linux named-audio output directories.

## Seismic event continuation

The first event-level regression failed on Windows: restoring an earned
Earthshaker loop with the unrelated legacy playback flag cleared restarted the
sound. The failure is retained in `windows-seismic-before.log`.

XL now derives that flag from the live slot/serial binding before the seismic
frame. After the frame, it recovers missing audio for an ongoing disturbance
and stops its own loop when the disturbance ends. The hooks leave stock Redux
behavior unchanged. The implementation lives in `runtime/seismic_audio.cpp`;
`weapon.c` contains only the guarded calls.

A shared native/Android fixture executes actual Earthshaker and level-disturbance
events. Its six PCM cases cover initial output, restored phase, the next source
gain/cadence update, stale-flag recovery, ongoing-disturbance recovery and channel
retirement. It also checks eight atomic timing rejections, clock rebasing,
expiration without stopping unrelated playback, and unchanged simulation RNG.
This fixture complements the existing mixed-voice trailer/disk/rewind tests;
it does not simulate campaign navigation or certify all seismic physics.

The complete native audio runner passes on Windows/Linux; the latest Linux run
took 48.57 seconds. Android build, catalog checks and all 101 integration steps
pass. The extended Android run took 247.773 seconds at 48 kHz and checked 904,192
samples across 51 output cases, including six seismic cases. All 26 protected
app files were restored with matching hashes. All 52 additional private runtime
checks passed in 319.42 seconds before the final bank-reset cleanup; the final
native and Android audio runs also verify that XL-to-stock loading clears seismic
flags and cadence immediately, without a frame. Unchanged public CI contracts,
already-passed audio and navigation are excluded from that selection.

Other ordinary sound callers, caller
classes, ambient emission, wall changes and classic replacement installation
remain unfinished. Admission has not been expanded. Multiplayer queued voice
callers remain deferred with multiplayer support.
