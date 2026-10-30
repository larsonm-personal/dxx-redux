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

Schema 22 saves the optional loop binding alongside the voices and linked owners.
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

This qualifies the looping audio entry points and persistence. The seismic
gameplay event flags and their interaction with restored/replaced loops still
need controlled event-level verification. Other ordinary sound callers, caller
classes, ambient emission, wall changes and classic replacement installation
remain unfinished. Admission has not been expanded. Multiplayer queued voice
callers remain deferred with multiplayer support.
