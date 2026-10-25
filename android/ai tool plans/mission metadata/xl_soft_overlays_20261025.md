# Mission-owned soft overlays over opaque bases

Branch `xl-soft-overlays` starts from fully qualified PR134 head `7616667bb7f42b4b37728bf320394585f37c1196`. Independent review and all seven workflows passed, including final-mip GLES checks and native/headed replay integrity.

## Bounded resource witness

Original BoilingPoint fan03.tga has one non-key alpha104 pixel, ordinary cutouts and purple super-holes. Its SHA256 is `8b51c97b97074f09260647b6db29f6df99939959397572c2f5dacb1d3d705799`. The authored Waterworks sides 1255/1 and 1256/3 use fan03 texture336 over texture226, metl060. The control levels have the same overlay/base pair; see [original material associations](xl_static_masks_materials_20261025.json).

The original metl060.tga is 50,213 bytes, SHA256 `9ad2eab3b3e1e1d00350e999f96266f839b44ea0a73b11b02750da8314aabeb9`. A fresh exact-engine decoder check confirms 128x128 with all 16,384 pixels opaque and no keys. Stock texture226 maps to bitmap1456 metl060 with neither transparency flag in all six retained retail PIGs; HAM SHA256 is `5233242206c677d65db7f075dd61f2b0a1b7bbe8cd65f56d769efaee1cc38b4d`. Asset provenance remains revision d9fbeaf6eb059fd4504308bfd6364734775b7cba and the [corrected 31-file survey](xl_texture_followon_evidence_20261025.json).

## Implementation boundary

Admit soft static or animated overlays only over a renderer-proven opaque base, on a normal full-opacity surface. Keep soft base/standalone materials rejected: those need separate world render-pass/depth-order qualification. In particular, this does not unlock the original door53 base strip or whole BoilingPoint levels.

Reuse the mission capability/owner/generation gates, bounded frame cache, source-name precedence, POG priority and authoritative eclip/wclip selectors. Classify complete sources before fitting. Fit and mip soft color in premultiplied form, retain a separate conservative binary super-hole mask, and compose over the opaque base in the existing shader. Never change stock gameplay bitmap flags, animation clocks, events, saves or RNG. Classic/imported-D1/LAN paths retain their existing behavior.

Prove numeric composition, ordinary cutouts and super-holes separately with synthetic production-renderer cases, including rotations, filtering/final mips, draw order and depth, invalid/unknown bases, fades/additive surfaces, POG and identity/lifecycle boundaries. Retain pinned original import/publication/native resolution and clearly separate it from original-artwork GPU qualification. No private artwork is committed or uploaded.

## Validation and evidence limits

Local checks passed: nine native contracts, three original-mission/native lifecycle integrations, 18 focused JVM import/publication tests with zero skips, both Android engine builds, both GLES fixture builds, scoped formatting and the automation catalog. The original import test now verifies both fan03 and metl060 through publication, exact native byte resolution and teardown.

The renderer fixture exercises production source selection, upload, wall shaders and effect-frame sampling with synthetic artwork and injected mission identity. It is not a full mission-load GPU test. Original fan03/metl060 artwork was decoded and its authored association inspected privately; original-artwork GPU fidelity and whole-level playability are not claimed. GPU execution and exact-head CI must pass before this slice is considered qualified.

Soft color is stored premultiplied solely inside the scoped frame cache. Source classification remains separate from upload eligibility: a soft strip encountered on an unsupported base can be retried once when a valid overlay surface appears. Failed uploads stay negative until the existing replacement reset. Both shader uniforms and material metadata reset through existing boundaries. The bounded cache and exact-frame/POG precedence are shared with prior animation slices; no second resource pipeline is introduced.
