# XL actor and afterburner audio

Worktree: `C:/local/dxx-redux/temp/xl-batch2-20261027`.
Branch: `xl_more_65_actor_audio`, above PR #214.
Authorized block ends October 30 at 04:57:30 UTC.

1. Route owned boss teleport/groan/final explosion and ordinary robot death
   callers to the source's explicit classes and samples. Preserve Redux and
   avoid changing already-qualified boss or robot simulation decisions.
2. Give XL an afterburner controller based on owned loop identity. Match source
   startup, class, loop bounds, release and death behavior, while preventing
   duplicate stopped owners and loops returning after release. Keep the existing
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

## Publication and verification

The first commit was published as draft PR #215 at the user's request to start
CI. Production qualification is now complete for this increment: Windows passes
both formats and source/scope comparisons; Linux passes the audio and three boss
regressions; Android passes all 101 steps with 88 PCM cases and unchanged hashes
for all 26 protected files. The source suite includes 576 afterburner and 60 actor
cases. Both automation catalogs and scoped formatting pass.

The shared production tests verify eight afterburner and five actor output cases,
stopped/muted release, earned trailer restoration, missing-owner recovery, the
actual boss/robot consumers and exact Redux afterburner PCM. They also exposed a
missing release demo event, now covered by a 43-byte recording check. The XL
S16-profile loop markers are independent of Redux's input-bank sample rate.
See [evidence](../../XL_compat/evidence/actor-afterburner-audio.md) for exact scope,
source corrections, logs and binary identities. Admission is unchanged.

Next: ambient surface selection and per-segment volume distribution. The source
uses F1_0 / (2 * emitting sides), while Redux uses F1_0 / 2 for each side. Extract
the actual source selector/setup, retain Redux as the alternate, and verify loaded
geometry, ownership and PCM before extending external sound-object playback or
installing classic replacements. Keep the new cases within existing audio test
owners rather than adding duplicate CI jobs.
