# XL actor and afterburner audio

Worktree: `C:/local/dxx-redux/temp/xl-batch2-20261027`.
Branch: `xl_more_65_actor_audio`, above PR #214.
Authorized block ends October 30 at 04:57:30 UTC.

1. Route owned boss teleport/groan/final explosion and ordinary robot death
   callers to the source's explicit classes and samples. Preserve Redux and
   avoid changing already-qualified boss or robot simulation decisions.
2. Give XL an afterburner controller based on owned loop identity. Match source
   startup, class, loop bounds, release and death behavior, while preventing
   duplicate muted owners and loops returning after release. Keep the existing
   Redux controller as the runtime alternate.
3. Execute extracted source caller/controller code and real native/Android
   events. Cover missing-loop recovery, mute/release, saved ownership and stock
   isolation with PCM and lifecycle checks; document source corrections.
4. Run relevant cross-platform builds/tests, publish the verified increment,
   and monitor CI. Continue the block without expanding unsupported admission.

Pinned source: `2576a98b5de310e3ac7c9777df2677d1627b8bae`, particularly
`main/game.cpp`, `ai/aianimate.cpp`, `ai/aiboss.cpp`, and `audio/objectaudio.cpp`.
The live XL boss consumer is `runtime/boss_spawn.cpp`, not the old core boss
path. Ordinary robot completion uses sound 11 at volume 2; boss completion uses
58 at volume 4. Both use explosion class; see/groan cues use robot class.

## Draft publication checkpoint

Published at the user's request to start CI, before the full runtime verification
in step 3 is complete. The Windows native engine builds, and the pinned-source
audio suite passes, including 576 afterburner cases and 60 actor sound-call cases.
These new checks execute the reference source; they do not yet verify the new
production controller's PCM, mute/release, save/restore or Redux isolation.
Keep this PR in draft until those native/Android integration checks are complete.
No mission admission or gameplay qualification is expanded by this checkpoint.
