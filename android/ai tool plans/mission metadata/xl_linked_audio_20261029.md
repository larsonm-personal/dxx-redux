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
