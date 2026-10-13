# Recorded all-weapons grants

Base: PR 33, 5a53592924ebe7c9e0bd4237397a5a2eca1e1211

## Scope

Record native all-weapons actions as ordered semantic input, using the same live and replay implementation. Preserve D1 scourge/bigred and D2 honestbob effects, including cheat enablement, zero score, ammo, energy, lasers and repeated grants. D1-in-D2 honestbob chooses the D1 loadout from the active content profile. Do not unlock excluded shareware slots or synthesize missing retail assets. Unsupported recognized cheats cancel input capture before mutation.

## Steps

1. Add strict v5 enable/grant commands, recorder entry points and matching C++/Python validation tests
2. Extract shared native grant behavior and route both engines' live handlers and replay dispatch through it; cover Android's existing native text-input route
3. Capture fresh new-level D1 and D2 shareware scenarios using actual native input, repeated grants and real Spreadfire/Gauss use; separately test D1-in-D2 and recording-off/fail-closed behavior
4. Review source before source checkpoint; build only necessary native, headless and unit targets, preserving a 4 GiB disk floor
5. Derive compact demos and immutable oracles only from live recordings; verify headed/headless replay and wrong/missing-grant controls
6. Independently review evidence before fixture publication and draft PR; verify exact-head CI to terminal, never merge

## Limits

Native D1 has no Gauss. Scourge is the D1 shareware baseline; bigred remains faithfully represented without changing native selection policy. D2 shareware honestbob retains its existing exclusions. Android source routing is covered, but no Android device fidelity claim without an on-device run. Original demos, native oracles and qualified historical binaries are immutable.
