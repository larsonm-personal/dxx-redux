# Rendered XL load and tick diagnostic

Keep the existing headless readiness gate and renderer admission checks intact.
Add an explicit desktop diagnostic using a real OpenGL context, normal mission
admission and 120 unassisted fixed-rate frames. Never start the route controller
or grant keys, invulnerability or robot removal. Use a separate evidence schema
so this supplemental measurement cannot silently replace hosted readiness.

Validate classic positive controls, real renderer identity/frame counts, missing
resources, unsupported unchanged originals and invalid option combinations.
Preserve exact mission/resource hashes and report every remaining blocker.
Fatal admission errors must produce a nonzero unattended exit, not a modal wait.
Run relevant native and public regression controls before a separate draft PR.

## Qualification

Base: `42f2ba5024eb9c5d0d55b8fc82a5eec3d0d9737c`. Run the desktop
`dxx-redux-d2` target with `android/tests/xl_rendered_readiness.py`; on Linux
provide Xvfb, `SDL_VIDEODRIVER=x11`, `SDL_AUDIODRIVER=dummy` and
`LIBGL_ALWAYS_SOFTWARE=1`. This exercises actual OpenGL rendering through
llvmpipe, but does not establish hardware-GPU parity or audible output.
The runner retains the 45-second deadline and requires exact mission, level,
filename, renderer identity and 120 simulated/rendered frames. Its separate
schema is rejected by the unchanged official headless scorer.

The unchanged frozen eight-case sample, using private resource revision
`b5f68b8f433e0fc170306a168534304ae64f7720`, remains four passes out of eight.
All 22 resource sizes and SHA-256 values matched its manifest; the original
11 entries are unchanged. Diehard 17, Ironstar 6, Orion Nebula 4 and Phobos-E 4
complete 120 unassisted simulation and render frames. Orion's existing D1
trigger repair warning is logged rather than opening a modal dialog. No repair
policy changes. Boiling Point and Dinter still fail existing capability guards.
Warp Shock reaches normal object dependency validation and rejects object 138,
powerup 48 (the reference slow-motion pickup); the skybox renderer guard is
preserved and satisfied by actual GL initialization.

Sphere clears all named-waveform lookups, then rejects its HXM. Its unchanged
`sp-02.rl2` SHA-256 is
`a821cc678a6e9f574fe5ec96b9475533e687aa338676bd5377cde3818355146b`.
The companion `sp-02.hxm` is 199870 bytes, SHA-256
`78686e51eaf9849e8cd9bb798920ce7d6eec81249a92c4f5ca40560383d128d9`.
It replaces robot slots up through 75, beyond the base D2 table. A separate
configuration supplies the already-qualified original Vertigo archive from the
same private revision (SHA-256
`97a4433833ec1f2fbdd2becc144a1e9726ca78b1210a71a49ddbec6182665fe6`).
That clears HXM validation and exposes the next genuine failure:
`object 239: boss profile requires an implemented XL boss runtime`.
Neither configuration is an XL readiness pass; no original bytes were edited.

Validation: 54 public XL contracts/source comparisons, classic upstream and
trigger-demo CTests, and 205 controller/runner policy tests (two needed a rerun
after restoring CMake/CTest to PATH). Exact evidence negatives include wrong
filename, incomplete frames, missing renderer, assisted runs and nonzero exit.
All ten assisted route option combinations are rejected by the actual desktop
entrypoint. Scoped quality and catalog validation pass. Catalog execution is
blocked by this environment's `/proc/self/task/.../children` visibility, exit 125. Windows runtime qualification remains hosted work.
