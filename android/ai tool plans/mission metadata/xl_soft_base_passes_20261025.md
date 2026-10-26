# Explicit soft-base rendering passes

This child branch starts from qualified PR135 `0653c8e4fa2cc8290ea501c59b2adcf5b3aa3c43`. The parent remains untouched. This is a prerequisite rendering slice, not automatic soft-wall admission or a whole-mission unlock.

## Why admission needs another boundary

Redux's native OGL world renderer visits geometry, objects, then translucent walls in three separate passes (`d2/main/render.c`, around 2365-2550). Its wall classification uses gameplay wall flags, and ordinary draws retain depth writes. Texture alpha alone cannot safely place a face into those passes: partial pixels could occlude objects drawn later, while late wall blending could be ordered incorrectly against translucent objects. Changing gameplay wall flags would also affect portal traversal and is outside this slice.

Pinned XL uses a shared transparency renderer with queued faces and objects and depth writes disabled during transparency (`arbruijn/d2x-xl@2576a98b5de310e3ac7c9777df2677d1627b8bae`, `render/transprender.cpp`, `CTransparencyRenderer::Render`). A full equivalent would require a deliberately qualified shared ordering policy, including intersecting geometry, transparent objects, portal visibility and overflow fallback. A weighted approximate blend would be a different rendering policy, not a source-faithful shortcut.

## Selected small stage

Implement an explicit single-layer wall material draw primitive with two mutually exclusive passes: fully opaque coverage writes depth; partial coverage uses premultiplied source-over with depth testing and no depth writes. Zero coverage draws nothing. The caller must provide far-to-near ordering for the soft pass after opaque geometry/objects. Restore blend/depth state and the soft shader mode on every exit. Reuse existing owner-validated source decoding, frame mapping, cache budgets, masks and POG precedence.

No ordinary world-render call site will invoke this primitive yet. Normal mission rendering must retain its existing stock fallback for soft bases. There is no command-line enablement, global sticky mission mode, automatic visibility change or invented sort order. Only a validated XL mission resource context and matching base-texture face may use the primitive; classic/imported-D1/multiplayer, layered overlays, additive/faded contexts and invalid passes remain excluded.

The next integration decision is between a shared ordered transparency queue (including relevant objects) and a more conservative qualified scene subset with a provable ordering contract. This PR must not imply either policy has been implemented.

## Original witness and evidence limits

Original `mods/boilpnt/textures/door53#0.tga` is 1,049,637 bytes, SHA256 `b8de6bca114401a30af53f445d50dc027c5475414833711b23c04203b8686c3a`. Exact engine decoding finds four 256-square frames, 69,906 ordinary cutouts, 1,453 partial-alpha pixels and no purple keys. Stock blastable/base clip46 has five applied frames. BoilingPoint segment502/side4, wall31, uses base858 and no overlay. This is a blastable base witness, not an opaque moving door. See [wall provenance](xl_wclip_resource_evidence_20261025.json) and [corrected decoder survey](xl_texture_followon_evidence_20261025.json).

Assets remain pinned to `d9fbeaf6eb059fd4504308bfd6364734775b7cba`; levelpack SHA256 is `b4b519dc5755b795fd49cc3786a0421b96cd099dcc8f002925ddfd7c038b2719`. Original import/publication/native resolution is already covered and will remain checked. Private artwork is not committed or uploaded. Current feature-branch CI does not permit private retail inputs under its trusted-controller gate; do not change that gate for this slice. Local accelerated emulation is unavailable. Original-artwork GPU and full mission-load-to-render evidence must remain explicitly unqualified unless a supported private execution path is established.

## Acceptance

Use the actual engine draw path and shader with synthetic assets. Prove numeric blending over geometry, foreground-object depth, opaque/partial/zero disjointness, two ordered soft layers and a deliberately reversed-order negative control, filtering/final mips, actual blastable applied-frame selection, state restoration, rejection/failure cleanup, classic/XL/stock and GL reset isolation. Preserve authoritative wall/effect state and SIM/FX RNG. Do not replace ordering checks with a successful decoder assertion or relax pixel tolerances to pass.

Run scoped formatting, existing native contracts and original-resource integrations, both Android engine/fixture builds, the existing GPU campaign and final-head CI. Publish a separate draft PR targeting `xl-soft-overlays`. No merges or changes to completed branches.

## Local validation

Both Android engines and D1/D2 renderer fixtures compile. Nine native contracts and three original-resource integration tests pass; 18 focused JVM import/publication tests pass with no skips. Scoped formatting and the automation catalog pass. The previously denied master-catalog execution was not retried. These are build/CPU checks; the new rendering assertions still require the supported CI emulator campaign.
