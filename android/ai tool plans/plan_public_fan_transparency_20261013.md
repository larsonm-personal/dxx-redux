# Public D1 animated transparency investigation

## Goal

Determine whether the preserved animation startup phase fault can change an actual
ordinary weapon collision on reachable shareware geometry. Keep the published
monitor/mine corpus and its oracles unchanged. No engine repair, pose injection,
world edits, cheats or saved starts.

## Steps

1. Observe native-decoded palette-index masks for the eight real fan01 frames and
   base texture 83, plus authored side geometry, normals, connectivity and ship
   gun points. Use engine readers, not a duplicate PIG parser
2. Identify an adjacent-frame transparency-mask difference with adequate aiming
   margin. Check native texture-UV and swept-projectile collision semantics
3. Develop a short ordinary-control route to the fan. Prefer one ordinary flare
   if its sticking behavior gives a clear opaque-versus-pass-through witness
4. Capture native inputs, replay unchanged in imported D1 with matched legitimate
   settings, and preserve the earliest physical difference or disconfirming result
5. If practical and compact, review and publish a clean-source recipe, then
   recapture and qualify native console/headed plus imported headed paths with
   separate small immutable native oracles and causal controls

## Workspace

Stack after published PR32 head 2759583ef5a4dca6a0bb776342455eb8246ae2e1.
Exploration compiles only the fixture translation unit into a separate diagnostic
output, linking this worktree's stable 8b19ab9ff engine objects read-only. Inherited
version strings do not qualify these diagnostics. Preserve qualified executables,
all raw evidence, and the shared 4 GiB free-space reserve.

## Exploratory result and qualification gates

The ordinary route reaches segment 135 at frame 219. Three 324-frame recordings
release all flight inputs at frame 299, then fire one flare, omit that pulse, or
shift it from frame 299 to 300. Each compact input is about 56 KB. Native D1's
positive flare crosses side 4 into segment 142 during SIM 300. Imported D1 samples
opaque pixel 59 at (47,34), sticks on wall 5 and draws awareness RNG one frame
earlier, causing the unchanged carried-RNG guard to abort before input 301.
All 301 shared gameplay/RNG samples match before that collision. Raw identity,
storage and animation differences remain recorded from startup.

Both controls complete with matching paired gameplay/RNG. The delayed shot also
has naturally changed muzzle geometry and samples (46,34); it is input-timing
isolation, not a frozen-geometry phase intervention. The native reader separately
queries the exact positive impact point without changing state: the phase 0
bitmap gives merged pixel 255/open, and phase 1 gives 59/closed. Adding both pixel
observations and screenshots leaves the full recording and RNG byte-identical.

Before admission, publish and retain an immutable clean source checkpoint, rebuild
with verified source stamps, recapture independent native oracles, and qualify
native console/headed and imported headed outcomes plus causal and rejection
controls. Expected imported failure must match the precise collision and strict
abort fingerprint; arbitrary runner failures must never count as a reproduced
fault. Do not extend the existing passing parity scope or repair the engine here.
