# Authored additive closed walls

Tranche 117 implements the authored WALL_RENDER_ADDITIVE flag for closed XL walls.
Original Pumo pmlev1 has ten such walls: 84, 85, 86, 87, 124, 125, 159, 160,
161 and 162. They use type 5, flags 20 or 28, closed state and cloak value 6.
Other additive wall types remain gated. Cameras and unsupported lightning still
prevent admission of that original level; this evidence does not establish
campaign playability or optional 3D powerup presentation parity.

## Source contract

Pinned D2X-XL revision: 2576a98b5de310e3ac7c9777df2677d1627b8bae.
`tests/reference_additive_wall.cmake` verifies source hashes and extracts the
actual InitTexturedFace classifier, wall flag and glow capability definitions.
The existing particle blend scaffold executes the actual source blend dispatch.

- WALL_RENDER_ADDITIVE is bit 4. A visible wall face selects additive mode 2.
- Mode 2 uses GL_ONE and GL_ONE_MINUS_SRC_COLOR, independently of the stock
  canvas blend option. It does not use the particle additive blend factors.
- The pinned GLOW_FLAGS excludes GLOW_FACES, so this source profile has no face
  blur pass. Face visitation still flushes a retained lightning batch.
- Source FADE_LEVELS is 31. Authored cloak 6 therefore uses opacity 25/31.
- Flat black cloak/decloak drawing with weak additive preserves destination RGB.
- Native visibility and passability are independent of this rendering flag.

The per-face binding resets on face end, invalid binding and mission teardown.
Opaque additive faces enter the shared transparency queue. The Android opaque
overlay optimization is disabled for these faces. Native engine hooks are guarded
by DXX_XL_RUNTIME; ordinary stock canvas blend modes retain their dispatch.

## Renderer validation

- Pinned source classifier/blend controls: 4608 cases, including ordinary faces,
  wall flags, texture-name classifications and stock blend options.
- Desktop EGL suite passed in 26.43 seconds (`117-source-test.log`), including
  weak blended indexed/RGBA merged textures, orientations, alpha and keyed texels,
  together with existing lightning, particle, sprite and context controls.
- Native binding checks passed 1280 stock and 1280 XL cases: flag ownership,
  source opacity, visibility, passability and scope reset. A real native face
  checks opaque additive queue admission and retained-lightning flush ordering.
- Protected Android renderer run 53e1cf6ca2934eefb9abed652612623d passed 28/28
  steps in 50322 ms. Both before and after actual context recreation it passed
  96 additive-wall cases, 28 line, 4 outline, 108 bitmap and 1 sprite case.
  The wall cases use native binding and draw calls: 64 textured blend/orientation/
  opacity/depth cases and 32 flat cloak/decloak cases. Protected app-data hashes
  were byte-identical before and after the run.

## Admission and original package

The final admission contracts passed 2/2 in 2.12 seconds. They accept closed
walls with flags 20/28, reject other wall types and preserve independent blockers.
The unchanged original Pumo archive was reinspected: 14 levels, zero parse
rejections. Its ten pmlev1 additive-wall blockers are removed; camera and
lightning blockers remain. pmlev2 cameras and pmlev3 cameras, particle owner 316
and lightning owners 320/321/322 remain blocked.

Native disk-save lifecycle qualification passed (`117-descriptor-native.log`,
with the detailed receipt in `native-runtime/controls.log`). The derived Neptune
fixture loads through normal admission, preserves flags 20/28 and cloak 6,
writes a real disk checkpoint, transitions to stock and restores the authored
binding and player energy with time resumed. Earlier fixture attempts failed
because the test assumed 24-byte records and used an overlong descriptor title;
both fixture errors were corrected without changing production serialization or
mission loading.

The final Android admission build passed in 2m25s. Later edits affect only the
host lifecycle fixture and this evidence, so the completed renderer and Android
build results are reused. Both automation catalogs passed; the protected script
extends its existing owner rather than adding another top-level test.
