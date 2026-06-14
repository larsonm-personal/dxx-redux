# Generalize D1 Effect Destinations In D2

- [x] Trace how D2 currently loads D1 PIG/HAM metadata and whether D1 eclip destinations are preserved.
- [x] Replace or justify the hard-coded D1 destination table with a broader data-derived mechanism.
- [x] Validate that Trine 2 panel `178/2/0` still resolves to D1 `blown04` behavior.
- [x] Run focused build and code quality checks.

## Notes

- The prior table was extracted from D1 eclip destination records for the immediately relevant wall effects, but it should be generalized so other D1 levels do not expose the same class of mismatch.
- The old rows `{20,342}` through `{27,353}` were the positive D1 `dest_bm_num` values for eclip records 20 through 27 in the D1 compiled property block.
- The generalized fix reads D1's compiled eclip block from registered `descent.pig`, caches every D1 `dest_bm_num` up to D1's effect count, and converts those texture ids through `convert_d1_tmap_num()` only while a D1 compiled level is active.
- Local binary validation against the GOG D1 `DESCENT.PIG` found D1 effect 24 destroys to texture 350, which is `blown04` after conversion, matching the Trine 2 panel case. It also found D1 effect 5 destroys to texture 331, which the earlier small table did not cover.
- Scoped code quality passed. Android native build `:app:externalNativeBuildDebug` passed.
