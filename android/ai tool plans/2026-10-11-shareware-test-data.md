# Public shareware test data

## Scope

- Reuse pinned public DOS D1/D2 demo packages and bounded ZIP plus native SOW volume extraction; do not execute installers
- Make the existing generated input-demo runtime smoke use shareware by default, retaining an explicit retail profile and every result/state/RNG assertion
- Add a separately generated shareware profile to the existing secret-area baseline test while retaining full retail campaign coverage
- Keep recorded retail replays and geometry/asset-specific regressions unchanged
- Validate exact input and extracted hashes, offline reuse, failure behavior, both real game runtimes, metadata baselines, and catalog/quality checks
- Stack this branch on the Linux runner PR without merging either; executable overrides allow validation with existing builds

## Validation

- Public packages downloaded and checked against existing manifest/oracle hashes; both sets reconstructed through the native SOW volume assembler
- Existing three-frame SDL/OpenGL smoke passed for both games under Xvfb/software GL using explicit existing PR5-built executables (engine source unchanged by PR5)
- Genuine metadata baseline: D1 seven levels / 28 secrets; D2 three levels / 21 secrets; both-game and separate-game structural comparisons passed
- Retail fixture unchanged; repeated equivalent baseline generation preserved bytes and modification time
- New offline fixture/profile tests, existing baseline diff test, automation catalog validation, direct host-only catalog classification, scoped mixed-language quality and workflow lint passed
- Existing PR5 native build regression checks passed: D1 69/69, D2 78/78; this is integration validation, not a fresh build of this branch
- Full catalog runner is blocked locally by its existing Linux child supervisor `/proc/self/task/<pid>/children` namespace mismatch; guard unchanged and CI retains the test
- The synthetic runtime smoke checks RNG trace publication, not RNG determinism; recorded retail determinism coverage remains unchanged
- No Android device-import/rendering claim; no proprietary game media used
