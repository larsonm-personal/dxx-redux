# Loader bounds and reusable sanitizer coverage

1. Inspect the actual invalid texture/wall-animation references in comet and KCXF2 L4. Fix at the loading/usage boundary, preserving valid level semantics and rejecting invalid required references rather than inventing passable geometry.
2. Add shared opt-in CMake sanitizer instrumentation and separate Windows build directories. Preserve incremental rebuilding, matching symbols, and normal build artifacts. Do not terminate unrelated compiler processes.
3. Thread the instrumented build selection through existing demo replay/build guards. Add one runner for CTest, demos, D1-in-D2 demos, and route simulations, with per-category logs, nonzero failure propagation, and no canonical regeneration.
4. Validate the two assets, all engine CTests, representative/full demo coverage as practical, and the four primary routing targets. Distinguish sanitizer findings from existing replay mismatches. Document unsupported instrumentation and remaining findings explicitly.

## Findings and implementation

- KCXF2 L4: the initial decoded wall data was misleading. Its first wall really is a door using clip 4 at serialized wall offset 113633. Object decoding stopped at 113541, and the loader ignored the offset, reading 92 bytes too early. Reading only the wall offset then exposed the same sequential-position assumption in later sections. Both engines now seek to the declared offsets for walls, triggers, reactor links, and matcens; D2 also seeks its lighting sections. A sanitizer replay now loads KCXF2 and collects blue before a later routing stall. Do not describe clip 104/53 as actual malformed mission animation references.
- Comet: overlay texture 14924 on segment 51 side 0 remains invalid after section seeking is corrected. Reject before ambient sound, collision, or render table lookups; do not substitute a texture with different collision behavior.
- Retain wall-reference and animation bounds validation, applying animation lookups only to animated wall types.
- Demo corpus coverage exposed a separate post-result crash: console replay completion closed the game window and entered the graphical main menu without loaded fonts. Apply the existing console stop-path guard to the finish path too. The final D2 L9 exit demo passes after the fix.
- Add separate ASan builds/stamps/runtime DLLs, demo diagnostic capture, native exit checking even after a result appears, CTest coverage, config and loader fixtures, and filtered/full route corpus execution without baseline writes. Parameterized fixtures live under helpers so ordinary test discovery does not invoke them without arguments.

## Verification

- ASan engine tests: D1 41/41 and D2 48/48, including section positioning and invalid-offset rejection
- Final native D1 demo pass: 4/4 in `android/temp/sanitizer_d1_sections_validation`
- Final D2 console demo pass: 11/11 in `android/temp/sanitizer_d2_sections_validation_retry`; the same report includes the final KCXF2/Comet loader fixtures and D2 engine tests
- D1-in-D2 demos: 4/4 in `android/temp/sanitizer_final_validation`
- Unterminated config, concurrent rewrite, and isolated settings fixtures: pass
- Core routes: 88/88 ran without sanitizer/native-process failures in `android/temp/sanitizer_final_validation/routes`; all 88 compact level records match the checked-in records exactly, including objectives, frames, RNG boundaries, and existing failure statuses
- Normal Windows builds succeed for both engines; normal D2 CTest passes 48/48. The existing normal D1 build has CTest disabled; D1's complete unit suite was exercised in its separate ASan build
- Scoped formatting/lint and build-guard tests pass. No checked-in mission metadata or simulation baselines were regenerated
- The full 1794-level corpus has not been rerun after the section-offset fix. Comet intentionally remains a load error, and KCXF2 still has a later routing stall/projection mismatch rather than a confirmed full route
