# D1-in-D2 continuation: death-camera RNG isolation

Status: implemented and validated on host; all Android ABIs build

Continue the gameplay-relevant private-state audit after the rejected flyout
persistence experiment. Preserve existing working changes and user bug notes

1. Trace boss effect message consumers before deciding whether state needs saving
2. Reproduce death-camera wall avoidance consuming SIM RNG in both engines
3. Use the existing local FX vector helper for camera direction selection
4. Exercise actual collision queries across native D1, imported D1 and ordinary
   D2 with varying cosmetic seeds and camera-update counts; assert SIM state and
   count unchanged, without requiring identical camera paths
5. Build/test both engines, run scoped quality and build Android; update handoff

Boss action 4/5 receivers only restart/stop ECLIP_NUM_BOSS. Their sender cache
does not control gating robots, teleportation, cloak or damage. No new save
record or exact cosmetic packet-history gate is justified

Death-camera wall retries call make_random_vector, which consumes three SIM
draws. Both object.c files already provide make_random_vector_fx for cosmetic
directions. Actual respawn selection and item drops remain gameplay operations
and must not be moved to FX merely because they run during the death sequence

## Evidence and implementation

Both pre-fix host regressions failed at the SIM state/count assertion in the
actual set_camera_pos wall-query path. Logs: native-before.log and
imported-before.log in temp/d1-death-camera-rng. Both engines now call their
existing make_random_vector_fx helper; no RNG save/restore or reseed is added

The regression uses a closed cell so retries are guaranteed, varies FX seeds
and 1/2/8 camera updates, and checks FX was actually consumed while SIM state
and count stayed unchanged. D2 repeats profiles 2/1/2; native runs the same
geometry. Camera paths are intentionally not compared

Both CMake builds pass without new warnings. Full CTest suites pass native D1
53/53 and D2 61/61. Scoped quality passes (repository rules exclude upstream
and fixture C/C++ from automatic formatting). Android assembleDebug passes for
arm64-v8a, armeabi-v7a and x86_64. Source snapshots, the tracked patch and
source/binary/log hashes are retained in temp/d1-death-camera-rng/manifest.json

## Remaining death/network audit

- time_dead gates explosion, egg drops and ghost conversion, not just camera
  animation. Both engines normally clear it on an alive dead_player_frame;
  D2 also explicitly clears it when co-op travel settles death. Exercise
  immediate death after restore/respawn before deciding on lifecycle changes
- Audit capture admission during death through actual callers and drop timing;
  do not add presentation serialization as a shortcut
- Spawn preview seeds SIM from timer_query but also selects the actual spawn
  point and moves the player. It is gameplay, unlike wall-avoidance direction;
  investigate separately instead of changing it to FX
- last_player_bump gates multiplayer damage at packet cadence and guards
  backwards game time. Exercise repeated contact and actual travel/restore
  boundaries before deciding whether additional resets are necessary

No live-network, full-corpus replay or Android runtime qualification is claimed
for this camera fix
