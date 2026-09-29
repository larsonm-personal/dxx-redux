# Multiplayer mine flyouts

## Scope and design

- Render other players in native D1 and D1 missions rendered by D2, using the existing tunnel flight algorithm and player models/colors
- Keep a monotonic per-player exit clock, independent of the paused game clock and local camera sequence
- Reuse MULTI_ENDLEVEL_START for immediate notification. Extend Android's already versioned, world-fenced ENDLEVEL status packets with elapsed exit times so the host distributes timing to every connected client, including clients already watching flyouts
- Repeated status packets repair missed notifications without restarting animations. Secret exits, disconnected players and stale world visits do not produce ships
- Use presentation-only actors, initialized on the local exit track and advanced to each player's elapsed exit time. Keep gameplay player objects and simulation RNG untouched
- Draw callsigns at visible ships with the normal player colors, respecting tunnel occlusion
- Keep the exterior camera alive after local completion in the synchronized co-op presentation until results are released, while reporting the local presentation as ready
- Preserve the desktop wire protocol and D2 movie behavior. Desktop D1 can show exits received through its existing protocol; synchronized exterior waiting uses Android's co-op presentation barrier
- Extra explosion sequences are optional and excluded from this change to keep one mine explosion and readable arrivals

## Implementation and verification

1. Add shared exit timing and cinematic actor support with small D1/D2 hooks
2. Carry ages through authenticated Android ENDLEVEL packets and reset them with the world
3. Retain the exterior scene during co-op waiting and expose flyout state to introspection
4. Extend native integration coverage for staggered exits, catch-up, repeated announcements, visibility and teardown; add paired Android coverage using existing LAN automation
5. Run scoped formatting, both relevant CMake builds/tests and paired device verification where available

## Progress

- Inspected current tunnel simulation, render path, co-op presentation barrier and UDP forwarding
- Found that MULTI_ENDLEVEL_START is ignored during Endlevel_sequence, and ordinary MDATA forwarding only targets CONNECT_PLAYING peers
- ENDLEVEL status packets already go from clients to the host and from the host to all connected clients; use these for authoritative repeated timing instead of widening all gameplay forwarding
- Implemented shared detached actors using independent slots in the existing flythrough solver, with wall visibility checks and normal player textures/colors
- Android ENDLEVEL packets now append elapsed milliseconds (one age from a client, all player ages from the host) before the existing world stamp. Android protocol versions advanced; desktop packet layouts remain unchanged
- Timing uses the monotonic timer, not paused game time. Reordered/repeated reports can catch an actor up but cannot rewind it. Packet transit latency remains the timing error; synchronized device clocks are not required
- Exterior waiting reports the local presentation ready while leaving its camera running. Skip, force launch and normal barrier release still close the scene
- Names are drawn in the normal HUD pass after depth testing ends. Normal gameplay labels are suppressed during flyouts, so players still in the mine do not acquire floating labels outside
- Windows builds passed for both engines; upstream compatibility and gameplay-fence CTest targets passed for both engines
- Android x86_64 debug build passed; paired 20-second stagger tests passed for native D1 and D1 in D2, including results, level 2 briefing, gameplay controls and Android Back/touch controls
- Visually verified the late ship and colored callsign above the mine opening in `temp/flyout-d2-arrival.png`

## Reusable verification

```powershell
.\run-windows-build.ps1 -Target both
ctest --test-dir buildd1 -R 'test_upstream_compat|test_coop_gameplay_fence' --output-on-failure
ctest --test-dir buildd2 -R 'test_upstream_compat|test_coop_gameplay_fence' --output-on-failure
.\android\tests\test_lan.ps1 -Game d1 -D1FlyoutPlayers -SkipBuild
.\android\tests\test_lan.ps1 -Game d2 -D1FlyoutPlayers -SkipBuild
.\android\tests\test_lan.ps1 -Game d2 -D1FlyoutPlayers -D1FlyoutCase near -SkipBuild
```

The paired tests expect the current APK already installed on both devices. With
`-Pandroid.injected.build.abi=x86_64`, this AGP version writes the test-only APK to
`android/app/build/intermediates/apk/debug/app-debug.apk`; install it using `adb install -r -t`

## Completed

- Native D1 and imported D1 paired 20-second stagger scenarios passed
- Imported D1 paired short-gap scenario passed; visually verified the trailing model and callsign inside the tunnel in `temp/flyout-d2-near.png`
- Scoped mixed-language formatting/lint and `git diff --check` passed
- The optional second explosion remains excluded; the existing mine explosion is retained
