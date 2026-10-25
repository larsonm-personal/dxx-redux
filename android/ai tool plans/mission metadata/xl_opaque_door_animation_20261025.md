# Mission-owned opaque wclip frame mapping

Child branch `xl-opaque-door-animation` starts from fully qualified PR132 head `8cb50ac6bf499568d28cd34ac4b337d5e613fb63`. All seven workflows and independent review passed before implementation. This slice changes presentation only and does not admit additional missions.

## Bounded implementation

Read the actual wall texture mapping and its validated door or blastable-wall clip. For N stock frames and M replacement frames, accept only 1 <= M <= N <= 50 and select floor(stock_index * (M - 1) / (N - 1)); the one-frame case selects zero. This visits every authored frame and preserves both endpoints without adding timers or silently skipping extra source frames. There is no sub-frame interpolation. Ambiguous repeated stock mappings and strips with more frames than the stock clip remain unsupported.

Require the sealed D2 mission texture capability, selected owned HOG, package generation, bounded segment/side/wall identity, correct base/overlay slot, and matching applied texture. Reuse the existing opaque decoder, bounded strip cache, lighting flags and production drawing. POG/custom resources retain priority. Classic, imported-D1 and multiplayer paths remain unchanged. A closed wall with no clip does not become animated merely because its texture has an animated filename.

There is no new simulation or save state. Opening, closing, waiting, reversal, obstruction and blast damage continue to change native wall mappings. Rendering reads those mappings without changing Walls, ActiveDoors, WallAnims, Textures, time or RNG. Saved mappings reconstruct the replacement frame directly.

## Evidence and acceptance

Use synthetic opaque strips for positive moving-door rendering tests. Exercise real native door updates, both sides and texture slots, reversal and waiting, damage mappings, filtering, source counts, malformed/ambiguous mappings, POG priority and classic/XL/owner isolation. Compare authoritative state and SIM/FX RNG across repeated draws. Retain the existing renderer runner and native save/replay checks; distinguish controlled fixture evidence from full mission loading.

The retained original `mods/boilpnt/textures/door53#0.tga` is 1,049,637 bytes, SHA256 `b8de6bca114401a30af53f445d50dc027c5475414833711b23c04203b8686c3a`, and contains four 256-square frames. Exact engine decoding finds 69,906 ordinary cutouts and 1,453 partial-alpha pixels, with no purple keys. It must remain rejected by opaque admission. Add its exact original import/publication/native-resolution check without claiming original-artwork GPU rendering.

Stock clip 46 has five frames, play_time 65536 and flags 7 (exploding, blastable, base texture), using textures 858 through 862. Original BoilingPoint wall 31 and Canyon walls 24, 22, 23 and 21 are blastable walls using clip 46. Control2 walls 600 and 601 are closed walls with no clip despite using this material. These observations do not establish gameplay support or a moving-door witness.

Provenance: test assets revision `d9fbeaf6eb059fd4504308bfd6364734775b7cba`; levelpack archive SHA256 `b4b519dc5755b795fd49cc3786a0421b96cd099dcc8f002925ddfd7c038b2719`; original hashes and decoder results in [the measured manifest](xl_texture_followon_evidence_20261025.json). Game version 40 wall records use 25 serialized bytes despite declaring size 24, because flags became 16-bit at game version 37; see [pinned XL CWall::Read](https://github.com/arbruijn/d2x-xl/blob/2576a98b5de310e3ac7c9777df2677d1627b8bae/main/wall.cpp#L1208). Native timing and wall mappings remain defined by [the existing Redux wall code](https://github.com/larsonm-personal/dxx-redux/blob/8cb50ac6bf499568d28cd34ac4b337d5e613fb63/d2/main/wall.c#L265).

A direct-member survey found no TGA files in the other 18 retained XL download archives or inspected ZIPs (Belial excluded). This is not a nested-archive completeness claim. No original opaque moving-door witness was established. Animated masks and partial-alpha composition remain separate later slices.

## Initial validation

Nine contracts pass, including exhaustive endpoint and no-skipped-frame checks for every admitted stock/source count pair. All three existing original-resource native integrations pass. All 18 focused JVM tests pass without skips, including the new original door53 import/publication/native byte-resolution case. Both Android engines and both GLES fixtures compile; scoped formatting and the automation catalog pass. The production GPU door cases and final-head CI are pending, so compilation is not a visual qualification claim.

The controlled renderer uses real door opening, closing, reversal, automatic waiting and blast damage updates; its native wall-record round trip and restored side mapping are not a full mission save/load campaign. It retains the parent fixture's injected mission identity/reset spy. Actual original-artwork GPU rendering remains unverified. [Compact original wall association evidence](xl_wclip_resource_evidence_20261025.json) records the level hashes and serialized record interpretation.
