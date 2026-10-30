# Linked XL audio integration

Worktree: `C:/local/dxx-redux/temp/xl-batch2-20261027`.
Branch: `xl_more_61_linked_audio`, based on the published shared-audio checkpoint
`6ad5c1f3f64d8b179169e6eff9c33a1c4d5136c8` (PR #208).
The active eleven-hour block ends October 30 at 04:57:30 UTC.

Implement XL behavior as its own runtime; Redux keeps its existing path. Use
controlled events and captured output, without autonomous level navigation.

1. Execute the actual pinned `SyncSounds` consumer with independent object,
   spatial-output and channel-storage adapters. Compare missing/reused objects,
   disabled sound/lightning effects, custom channels, gain quantization, ambient
   updates, main-volume startup, deferred starts and restart behavior.
2. Extend `AudioOwners` with the verified frame decisions. Resolve stable owner
   serials across dense-container removal and channel replacement; preserve every
   surviving binding. Geometry remains qualified separately and feeds this policy.
3. Install native object/segment creation, destruction and spatial updates using
   `ClassicAudioSpatial` and the shared bank. Audit duplicate lookup and explicit
   caller classes against source. Avoid holding the audio lock while computing
   routes when owner updates can be prepared on the game thread instead.
4. Save complete owner state and bindings, including stopped forever owners and
   device-rate conversion. Verify motion, effect enable/disable, object reuse,
   replacement, pause/reopen, disk/rewind and stock isolation in native/Android
   integration before gameplay dispatch or replacement admission.

`SyncSounds` deletes missing object owners even when they loop forever. This is
different from `DestroyObjectSound(-1)`, which stops and retains forever owners.
Disabled effects stop but retain owners; custom sounds still receive spatial
owner values but skip automatic channel gain/pan changes. Old/new gain comparison
uses the source's rounded, unclamped mixer scale. Startup uses main effects gain;
later ambient channel updates use the ambient setting. Preserve these differences
and independently verify their observable output.

Continue small CI reductions only where existing checks demonstrably overlap.
The previous checkpoint removed duplicate common helper execution and duplicate
primitive execution in the captured-mine source oracle; all coverage remains.

## Native integration in progress

The published policy checkpoint is PR #209, commit `34bd48e`. Both public XL CI
platforms passed its actual-source comparisons. The native consumer now shares
the same owner registry and mixer as named and unlinked playback, prepares
geometry outside the SDL lock, and updates from the frame and effects slider.
Object/segment creation remains explicit; ordinary gameplay callers are not
installed yet. Sound/lightning effect objects are still
outside native admission, so the native object adapter does not claim those
effect enable/disable consumers despite the policy's source coverage.

One deliberate source correction needs its own evidence: pinned
`FindObjectSound` reads `m_linkType.pos.position` while the active union member is
`obj`. Fresh object signatures overlap the position's x component; recycled
storage can retain segment-position bytes. Native duplicate detection instead
matches the active object's index, signature and physical sound, without reading
the inactive union. This is not a claim of byte-for-byte duplicate behavior in
the buggy source. Segment duplicate detection retains the half-unit radius.

The native control passes 23/40 and 27/40 on Windows/Linux: moving
source/listener PCM, out-of-range stop and restart, slider updates, object-slot
reuse with unrelated voices, one-shot and deferred/custom behavior, and segment
destruction. Linux's full existing runtime test takes 39.62 seconds. The 17
mixed-owner controls also pass ASan/UBSan with leak detection. Full creation
source comparisons and ordinary/ambient gameplay dispatch are still required;
see `android/XL_compat/evidence/linked-audio.md`.

## Linked persistence checkpoint

Schema 21 now serializes linked owners and their shared-channel bindings,
including quiet and deferred lifetimes. Native integration adds 30 atomic
rejection cases, trailer/disk-through-stock restore and 48 kHz reopen output
checks before and after save restoration. The Android diagnostic extends the
existing runner to 101 steps with moving-source PCM and linked rewind/disk
restoration. Platform results and remaining limits are recorded in the evidence.

The working branch is `xl_more_61_linked_audio`, in
`C:/local/dxx-redux/temp/xl-batch2-20261027`, published through PR #209. The main
checkout remains on `cmake`; it does not contain this branch's unmerged work.
