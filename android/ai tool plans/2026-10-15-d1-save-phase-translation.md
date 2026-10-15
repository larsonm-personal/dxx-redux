# D1 checkpoint phase and Fusion translation

## Scope
- Admit native D1 version 21 with its finalized phase and Fusion-active tail
- Preserve native versions 15 through 19 and reject Android layouts 20 and 22
- Decode and validate the whole checkpoint before committing runtime state
- Exercise actual native D1 saves through the D2 translator and gameplay continuation

## Plan
1. Extend the translator version gate, runtime staging, and final commit
2. Adjust existing native checkpoint fixture offsets for the finalized eight-byte tail
3. Add a reusable native-save producer and imported continuation fixture with malformed-tail and legacy controls
4. Run targeted rebuilt native/D2 fixtures against the same public D1 assets; record exact build provenance and limitations

## Constraints
- Do not change the phase or Fusion core implementation, clocks, controls, or public data
- No publication or merging
- No PowerShell, SDL interception, codec workaround, or zero-aspect configuration

## Validation completed
- Native producer and imported D2 consumer both exit 0
- Six Fusion states, native/opposite endian, sixteen gameplay frames each
- Exact phase, player physics, energy, Fusion state/projectiles and SIM/FX RNG
- Android/unknown version refusal, malformed-tail atomicity, native19 controls
- Targeted CMake-command rebuilds reuse other existing engine objects; this is
  not a clean full-current-tree build
- Public shareware Fusion geometry is explicitly synthetic laser geometry
- No PowerShell formatter/catalog run under the task's explicit constraint
