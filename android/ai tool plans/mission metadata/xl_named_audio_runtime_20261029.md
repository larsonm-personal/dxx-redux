# XL named audio runtime

Continue the authorized run on xl_more_51_named_audio, stacked above the owned
POG and catalog-cache increments. Keep Redux audio as its existing alternate.
The objective is real action-30 playback with owned resources and complete
lifecycle evidence, then reuse that foundation for ambient sound objects.

## Implementation order

1. Use the pinned named-audio source contracts as the event reference. Preserve
   positive repetition counts, zero/negative infinite playback, strict cooldown
   boundaries, wall/object differences and authored enable/disable behavior.
   Restore active voices directly from saved positions instead of replaying the
   source's broken finite-voice restart through its cooldown check.
2. Prepare immutable SND/WAV resources before changing the live level. Preserve
   stereo channels and resource identity. Resolve builtin aliases before file
   lookup; keep ownership and lookup precedence explicit. Reject unresolved or
   unimplemented external providers with a resource-specific error.
3. Add an XL-owned voice mixer with bounded storage and no allocations, file
   access or engine-object access in the audio callback. Connect it to the
   existing SDL audio output without changing Redux sample conversion. Track
   actual consumed frames, repetitions, completion, pause, volume and cleanup.
4. Integrate wall/object actions and frame maintenance, then serialize resource
   identity, cooldown state and voice positions. Validate a complete snapshot
   before applying it. Cover disk saves, rewind, level/mission transitions and
   restoration through stock gameplay.
5. Verify independent PCM/mixer expectations, pinned source event behavior and
   every unchanged Pumo named resource. Exercise normal Android output using
   controlled actions and captured mixed samples. Keep original Pumo admission
   blocked on its other requirements; do not navigate the campaign with AI.
6. Run the relevant native/public/Android owners, both automation catalogs and
   scoped mixed formatting. Record the precise supported profile and remaining
   limitations before publishing. Inspect CI and fix actual failures.

## Acceptance boundaries

Decoder success or a successful sound-start return is insufficient. Evidence
must show output samples, correct channels, repeats, silence while paused,
resumption from the saved point, natural completion and cleanup before Redux.
Cross-rate conversion and simultaneous voices need explicit controls. Retain
negative controls for corrupt resources and snapshots; none may change a live
voice or silently fall through to a foreign resource.

Ambient sound-object spatialization and effect-object lifecycle are subsequent
consumers. This increment must not advertise those consumers as implemented.

## Result

Implementation and local acceptance are complete within the recorded profile.
Windows/Linux public selections pass 43/46 tests; the complete private owner
passes 103 tests, sixteen original-format outcomes and three source-level
comparisons. Android passes all 73 steps and 33 output comparisons covering
749,056 samples. Stock D2 conversion/gain checks also pass. See
[the evidence record](../../XL_compat/evidence/named-audio-runtime.md) for exact
receipts, rejected controls and unqualified behavior.

The next audio consumer needs mission/level sound-folder ownership: all 644
Pumo ambient records name resources outside its HOG. The original distribution
contains the corresponding `mods/pmines/sounds` tree. Qualify that provider and
the ambient source semantics before granting effect-object admission.
