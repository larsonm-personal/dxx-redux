# XL shared audio and ordinary-sound foundation

Worktree: `C:/local/dxx-redux/temp/xl-batch2-20261027`.
Branch: `xl_more_58_audio_controls`, above published disarming PR #205
(`77e00085ea2d63904327f00bab943f2520185ab2`). Main `cmake` stays untouched.

The additional eleven-hour block starts October 29 at 17:57:30 UTC after
local disarming verification and publication, and runs through October 30 at
04:57:30 UTC. Watch PR #205 and repair any CI failures; extend the block for
substantial disarming repairs so they do not replace the requested new work.
No merges or autonomous 3D navigation. Publish reviewable increments with
source/native/Android evidence and explicit remaining admission restrictions.

## Dependency order

1. Extend the owned mixer with actual XL per-channel volume and pan semantics.
   Execute pinned startup and update consumers, including their differing pan
   quantization, and compare mixed PCM with actual SDL_mixer. Preserve existing
   named-action output and allocation behavior. This first increment prepares
   ordinary playback; it must not falsely admit ordinary replacements.
2. Bring ordinary sample playback, named actions and their owner cleanup into
   the same XL channel pool. Include unreplaced native samples: a separate
   replacement-only pool cannot reproduce exhaustion or priority. Preserve
   caller sound classes, owner lifetimes, looping and sample-rate conversion.
   Use small engine dispatch hooks and keep Redux as its existing alternate.
3. Apply the audited ordinary replacement bank through real playback, including
   normal/secret folder transitions, malformed or foreign resource rejection,
   actual spatial output, pause, save/rewind and stock recovery. Admit only
   requirements covered by source and runtime evidence.
4. Build ambient/effect sound ownership on that foundation. Audit capacity,
   activation, enable/disable, connected-distance attenuation, movement and
   owner deletion. Resolve all original requirements without mission exceptions.
   Continue this dependency order across later increments if the block ends
   before the entire audio system is finished.

## Immediate controls and evidence

- Existing pinned `audio/audio.cpp` defines `Mix_VolPan`,
  `CAudioChannel::SetVolume`, `SetPan`, `Start` and channel allocation.
- Startup pan rounds `X2F(pan) * 254`; updates divide by `32767 / 127`.
  Both feed byte panning to SDL; do not normalize away boundary behavior.
- Source volume updates can alter replacement priority, whereas a global slider
  change does not rewrite every unlinked channel's captured priority. Retain
  the documented application-wide output policy separately from XL allocation.
- The native linked-sound registry currently has 150 entries and ordinary mixer
  playing queries are stubs. Merely loading replacement WAVs cannot establish
  ordinary/ambient fidelity. Audit and replace these consumers deliberately.
- Reuse the existing source and SDL-output owners; extend their coverage instead
  of adding duplicate public CI processes. Compare elapsed time when optimizing.
- CI trimming remains a side goal. The catalog-cache increment is already
  published; remove further overlap only where evidence shows coverage survives.

## Disarming checkpoint

PR #205 contains one new commit and all seven applicable workflows started.
Local evidence: 106 private Linux controls, sixteen original-format outcomes,
three modern source comparisons, both Windows layouts, 1,120 source comparisons,
67 Android steps, both catalogs and formatting. All fourteen original Pumo
levels remain blocked. Hosted CI is still running at this checkpoint.

## Channel-control checkpoint

The first increment implements per-channel output gain and pan, with source
startup/update quantization and live priority-volume updates. Both Windows and
Linux pass 199,311 source comparisons. Actual SDL_mixer matches all 78 output
cases; existing native named audio and folder integration passes in 35.76
seconds. Android passes 73 steps, 33 output checks and protected-data recovery.
See [channel-control evidence](../../XL_compat/evidence/audio-controls.md).
No ordinary/ambient admission gate changes. The next increment must connect
ordinary and named consumers to shared ownership before admitting replacements.
