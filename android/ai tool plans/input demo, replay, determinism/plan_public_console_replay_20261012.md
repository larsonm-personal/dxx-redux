# Native console input replay for D1 and D2

- Reuse the normal replay loader, loaded-start, and simulation-frame functions for both native games
- Add the D1 console executable and make its launcher initialize native assets and player defaults
- Remove the console-only checkpoint restriction so the shared start path validates supported start modes
- Exercise immutable public new-level captures with DISPLAY unset; fix presentation-only startup dependencies without altering simulation or expected data
- Verify all seven public scenes, normal rendering behavior, and existing D2 checkpoint replay; retain strict frame, diagnostic, terminal, and SIM comparisons

## Audio shutdown follow-up

- Reproduce the intermittent post-result D2 console crash without changing replay data
- Diagnose the active audio callback surviving SDL library teardown using a temporary unload timing probe
- Share the existing headless audio initializer with an exit cleanup that closes digi and SDL before engine allocation cleanup
- Add an asset-free lifecycle regression that checks the real audio device is closed on successful and early-error process exits
- Rebuild both console targets, repeat the reproducer, and rerun immutable replay and CTest checks
