# Xfing Plain Texture DXA Conversion 20260518

## Goal
Redo the Xfing conversion so generated DXAs contain editor-readable replacement texture image files instead of raw binary patch payloads, while keeping only small JSON patch data for level and HAM metadata that cannot be represented as texture files.

## Tasks
- [x] Confirm engine texture replacement lookup and remaining metadata gaps
- [x] Add PIG bitmap payload decoding to PNG in the shared conversion library
- [x] Rewrite converter output layout around plain texture files
- [x] Update verifier for image-based DXAs and forbidden full-data checks
- [x] Regenerate UUD1 and UUD2 DXAs
- [x] Run verification and scoped code quality

## Notes
- Texture payloads are now normal PNG files in the DXA tree, not `.bin` patch payloads
- D1 writes unique texture replacements at the archive root, matching the current Redux bitmap-name lookup
- D2 writes texture replacements under `textures/d2/sets/<set>/` to avoid texture-name collisions; D2 now searches those paths from the active PIG name
- D1 level remaps and D2 HAM texture/eclip/wclip table additions remain compact RFC 6902 JSON Patch metadata rather than full RDL/HAM files

## Results
- Generated `game_data/mods/xfing/dxx_tp/tmp/plain_texture_dxa/uud1tp-textures.dxa`: 334884 bytes, 124 entries, 121 PNGs, 3 JSON files
- Generated `game_data/mods/xfing/dxx_tp/tmp/plain_texture_dxa/uud2tp-textures.dxa`: 2021369 bytes, 918 entries, 914 PNGs, 4 JSON files
- UUD1 manifest: 120 texture PNGs, 0 index-only texture paths, 72 D1 level surface deltas represented as 144 RFC 6902 operations
- UUD2 manifest: 908 texture PNGs across six texture-set PIGs, 0 duplicate index-only texture paths, 18 texture HAM deltas, 5 eclip deltas, 3 wclip deltas represented as 31 RFC 6902 operations; extra HAM bitmap files use generic `idxNNNN.png` names
- The verifier rejects `.pig`, `.ham`, `.hog`, `.mn2`, `.rdl`, `.rl2`, `.256`, and `.bin` entries, and checks every manifest-listed PNG and mask hash

## Validation
- `./game_data/mods/xfing/convert-xfing-minimal-dxa.ps1 -Game both`
- `./game_data/mods/xfing/verify-xfing-minimal-dxa.ps1`
- `./android/run-code-quality.ps1 -Fix -Paths game_data/mods/xfing/xfing_minimal_dxa_lib.ps1,game_data/mods/xfing/convert-xfing-minimal-dxa.ps1,game_data/mods/xfing/verify-xfing-minimal-dxa.ps1,android/ai tool plans/plan_xfing_plain_texture_dxa_conversion_20260518.md`