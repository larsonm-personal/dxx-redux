# Invalid level texture fallback

Replace out-of-range primary and overlay texture references before table consumers run, in both engines. Use gray rock from the existing D1/D2 conversion mapping, preserve valid references and overlay orientation, and log the original and substituted indices without interrupting loading.

Update the loader integration fixture to require successful loading and simulation for Comet and the two rejected Insane assets. Build both Windows engines and run the focused fixture.

## Results

- Both loaders substitute D1 texture 2 or D2 texture 43 (gray rock), preserving valid references and overlay rotation, and log original/replacement indices
- The D1-to-D2 unknown-texture diagnostic also uses logging instead of the warning callback
- Windows D1 and D2 builds passed; existing weapon ordering return-path warnings remain unrelated
- D2 CTest: 49/49 passed
- All four loader fixtures passed under `android/temp/invalid_texture_fallback`; Comet, BRINSANE, and Insane each confirmed their route, while KCXF2 retains its existing routing timeout after successfully loading and reaching blue
- Scoped code quality and diff whitespace checks passed
- No canonical simulation data was regenerated; no full corpus or Android device run was performed
