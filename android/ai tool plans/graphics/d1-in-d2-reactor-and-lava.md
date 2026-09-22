# D1-in-D2 reactor wreck and lava colors

- [x] Compare original D1/D2 reactor model and lava effect tables with runtime conversion
- [x] Fix confirmed asset mapping errors while preserving the destroyed-light fix
- [x] Add focused native integration regressions and verify failures before fixes
- [x] Build D2, run relevant tests, run scoped quality, and record limitations

## Findings and fixes

- Retail D1's reactor uses model 39 and wreck 40; D2's equivalent uses models 93 and 94. The D1 asset overlay installed D1 dead-model mappings but retained D2's reactor definition and live object model. Entry 93 therefore became -1, and explosion completion deleted the reactor
- Read D1's reactor model from the original object-type table, plus its four-slot gun geometry table, when validating/staging D1 assets. Apply the definition and update live reactor objects alongside the D1 model table. Existing wreck objects retain the D1 wreck model. Leaving D1 reloads the D2 definition through the existing HAM restoration path
- D1 lava texture 333 (`misc11`) converts to D2 texture 409. D1 effect 10 corresponds to D2 effect 66, while D2 effect 10 is unused. Index-based loading skipped the D1 frames, leaving D2 lava displayed with D1's palette
- Match D1 wall effects to original D2 effects by converted wall texture, and restore the whole original effect array when leaving D1. This also handles the other relocated wall effects without hard-coded slot numbers
- The earlier destroyed-light palette conversion is preserved and does not itself affect lava

## Validation

- Read-only parsing of the original GOG D1 PIG and D2 HAM confirmed the model numbers, wreck mappings, and lava effect relocation
- Extended native integration fixture failed first at the relocated-lava pixel assertion. After fixing lava, it failed at the reactor live-model assertion. Both now pass
- Coverage includes animation updates, repeated D1 entry, restoration of relocated D2 effects, actual reactor wreck selection through `maybe_delete_object`, preservation of an existing wreck, D2 reactor restoration, and rejection of invalid model references, excessive gun counts, and truncated gun data
- Existing monitor and destroyed-light regressions remain passing
- Windows D2 build and all 59 D2 CTest tests passed
- Scoped mixed-language quality checks passed. The native fixture was additionally formatted and checked with pinned clang-format 20 because it is outside that script's C/C++ scope. `git diff --check` passed
- No Android build or in-game visual playtest was performed

Build, test, and quality logs are under `temp/d1-reactor-lava-*`
