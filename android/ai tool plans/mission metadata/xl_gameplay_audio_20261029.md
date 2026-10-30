# XL gameplay audio dispatch

Worktree: `C:/local/dxx-redux/temp/xl-batch2-20261027`.
Branch: `xl_more_62_audio_dispatch`, based on published PR #209 checkpoint
`f98b43063029f4f5fae01e26c240736d2930b2b4`.
The active eleven-hour block ends October 30 at 04:57:30 UTC.

Connect ordinary single-player sound events to the separately implemented XL
mixer and owners. Keep Redux as the alternate path. Most implementation belongs
under `android/XL_compat`; core edits should pass missing caller information or
select the appropriate lifecycle. Use controlled events, not level navigation.

1. Verify logical translation and forcefield volume behavior against the pinned
   source. Preserve the distinction between `PlaySound` negative physical IDs
   and object/segment logical IDs. Audit source defaults for custom volume.
   Extend duplicate/lifetime evidence; keep the documented inactive-union and
   container-compaction corrections explicit.
2. Add logical XL entry points over the verified physical APIs. Audit classes at
   robot, explosion, weapon and afterburner callers. Do not infer every sound's
   class from its emitting object's type: source segment sounds are generic,
   while selected object and local weapon calls pass explicit classes.
3. Select complete XL creation, stop, pause/resume, queue and looping lifecycles
   at the engine boundary. A consumed one-shot returns no owner even when it
   plays successfully; never treat that return as permission to replay it through
   Redux. Keep channel identities and ownership in their respective runtime.
   Check transitions in both directions and restore while loading a level.
4. Exercise actual gameplay callers and actual output: weapon fire, robot and
   explosion cues, afterburner, fixed segment sounds, shared-pool replacement,
   slider updates, pause, disk/rewind, reopen and stock isolation. Check native
   Windows/Linux and Android integration before expanding admission. Native
   ambient effect objects and external WAV owners remain separate requirements.

The caller audit already identifies source `CreateWeaponSound` in
`weapons/createweapon.cpp`, robot/boss calls in `ai/aiboss.cpp` and
`ai/aianimate.cpp`, explosions in `effects/fireball.cpp`, and afterburner calls
in `main/game.cpp`. The pinned reference remains commit
`2576a98b5de310e3ac7c9777df2677d1627b8bae`.

Reuse the existing native audio runner and source executable. Their runtime-only
modes avoid repeating the separately registered primitive suite. Public XL CI
compiles the source adapters; tests requiring retail geometry run locally.
Keep watching PR #209 and the rebased stack. Do not merge branches or reintroduce
their old pre-rebase history.

## Published checkpoint scope

This checkpoint adds logical sound translation, sound classes, forcefield volume
scaling and XL entry points over the existing physical playback APIs. Ordinary
gameplay callers are not connected yet; admission is unchanged. Seismic looping
and its saved ownership remain follow-up work. Queued voice callers in this tree
are multiplayer-only and can remain deferred with multiplayer support.

Validation passed on Windows and Linux: native builds, the source audio suite
(including 8,160 logical translation and 81 forcefield comparisons), and the full
native audio runtime runner for both fixture formats. The native runner covers
actual PCM through the new entry points, 20 forcefield controls, and existing
save/restore checks. Scoped formatting passed. Android has not been rerun for
this checkpoint. Logs are under `temp/xl-gameplay-audio-20261029` in this worktree;
Windows native runtime receipts remain under
`temp/xl-shared-audio-20261029/windows-runtime`.

## Looping follow-up

The engine looping entry points now select XL at runtime, with a saved
slot/serial binding in schema 22. Native/source checks pass on Windows/Linux;
Android build, catalog checks and all 101 steps of its existing audio runner pass
with the extended loop fixture. All 26 protected app files were restored with
matching hashes. See `android/XL_compat/evidence/gameplay-audio.md`.

Next, exercise actual seismic events and restored/replaced loops. The current
`Seismic_sound_playing` flag is separate from the mixer binding and is not part
of `seismic_runtime_state`; do not assume that audio-only restore checks prove
event-level continuation. Then connect other ordinary/classed callers, keeping
their lifecycle and runtime registries separate from Redux.
