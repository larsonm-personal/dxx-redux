# Native persistence controls require valid pause state

The seismic checks exposed a native test-harness defect: some fixtures resumed
time after the low-level save had already resumed it, while others called that
save without supplying its caller pause. Native release builds allowed the pause
depth to become negative. `game_is_time_paused()` then returned true even though
the fixture intended to continue running.

This does not by itself invalidate every directly called consumer assertion in
those fixtures. It does invalidate the assumption that their continuation runs
in a valid unpaused world. A pause-aware frame path could remain inert while
other directly invoked consumers still passed.

## Reproduced failure

Before correcting the calls, the existing 15/34 fixture received one assertion
immediately after its redundant `start_time()`:

```cpp
require(!game_is_time_paused(), "15/34 save/restore left the simulation paused");
```

The Windows native build completed, then the original-layout test failed with
that exact message and exit code 1. Removing the redundant resume while keeping
the assertion made the same control pass, including its 68 packed light sources,
5,083 deltas and ordinary disk-through-stock restoration. The local before/after
logs are `temp/xl-seismic-20261030/native-pause-negative-control.log` and
`native-pause-corrected-control.log`.

## Correction and scope

Successful `state_save_all_sub()` calls consume their caller's pause.
The caller must stop time once before invoking it; an additional resume after
save or subsequent low-level restore is incorrect. The follow-up replaces 41
redundant resumes in 29 native fixture headers with running-state assertions.
Four missing caller pauses are added for the removed-object delayed-event save,
stock object-pool save, stock trigger-capacity save and stock wall-capacity save.
In total, 30 native fixture headers change.

The preceding seismic increment separately corrected the audio fixtures. This
follow-up covers bosses, generators, object events, walls/doors, capacities,
layouts, lighting, custom actors/projectiles, textures, spawn markers, segment
properties and messages. Existing gameplay, source, persistence and malformed
state assertions remain. No test is skipped and no timeout is increased.

Production Redux and Android pause implementation, application binaries, mission
admission and supported-feature claims are unchanged. The existing Android
wrappers have their own pause ownership; these native low-level corrections
must not be copied into them without inspecting that contract.

Both Windows and GCC native builds pass. The complete required Linux acceptance
selection passes all 106 tests, sixteen original-format witnesses and three
modern source comparisons. CTest takes 443.68 seconds; the test/inspection phase
takes 447.142 seconds. The complete audio owner passes in 138.63 seconds within
its unchanged 150-second bound. This includes both seismic layouts and the
earlier audio pause corrections as well as every affected native fixture owner.

The corrected Windows control binary has SHA-256
`3a9e06799ccad8f3a21d9fcb15aa4e2f25048117b199d3d36768654d44bad4bb`.
The complete Linux receipt is `temp/xl-seismic-20261030/native-pause-full-linux.log`;
individual results remain in `temp/xl-private-ci-20261028/run/commands.log`.
Scoped formatting and whitespace checks pass. This is successful native
component acceptance, not original campaign completion or new Android gameplay
qualification.
