# Boiling Point 1 image audit

The ordinary developer-OFF original-case reproducer now identifies the selected
resource: `mods/boilpnt/textures/rock194.tga`. It is a scoped wall texture, not an
authored particle image; the generic decoder's earlier wording obscured that
distinction. The small runtime change adds the retained selected path to a
scoped-texture decode exception without changing selection, decoding or admission.

Private evidence: E:/dxx-redux-builds/xl-recovery-20261116/image-audit-01,
original-scoped/0/engine.log. The unchanged pinned BoilingPoint-XL archive and
authentic runtime resource profile were staged by the existing readiness runner.
This remains a failed original case and adds no readiness credit.

The selected file is 743708 bytes, type 10 RLE true-color, 24-bit, 512x512, with
no color map. SHA256:
ffbc219965339e0f079838b1919836adc2e1037ef67ab679d717d97adf01f064.
The mission texture directory has 34 uncompressed 24/32-bit images and this one
type-10 image. No rock194.png sibling exists there. A 256-directory variant also
exists; its presence alone does not establish source selection or permission to
substitute it into the frozen measurement.

## Source constraints

Qualified reference revision: 11038eb53dd86f3bcdc027dddd3f93c60f02536e.

- gameio/tga.cpp SHA256:
  9f1c15043e89acd2481eee1b0b9466e0c522c84ab1ae94a7195e1a22d99e687d.
  CTGA::Read first tries ReadImage when USE_SDL_IMAGE is enabled. ReadImage
  replaces the extension with .png and requests PNG explicitly; it does not
  supply an RLE-TGA fallback. The raw CTGA path reads rows directly and has no
  type-10 packet decoder. Its header acceptance alone does not prove valid RLE
  pixels or complete payload consumption.
- gameio/loadtextures.cpp SHA256:
  bf1ae25e058d6ca745e5833ea0e2c0e425c5716571f6dc5f109a49d67c5452b6.
  FindHiresBitmap and MakeBitmapFilenames consider separate cache folders,
  shrink factor, cache enablement and file dates. ShrinkFactor caps texture
  quality at 256 pixels. Cache search semantics must be traced through actual
  configured directory ownership before qualifying a smaller variant.

Do not add generic RLE support and call it source equivalence solely because
the format is standard, accept a truncated raw read, transcode the original,
or select the 256 variant only to obtain a pass. The next step is to establish
the intended source resource profile and cache directory mapping for this
original package. If that profile truly selects the compressed file, treat
proper RLE decoding as an explicit compatibility extension requiring independent
pixel/alpha/orientation and malformed-packet evidence, not an allowlist repair.

## Remaining validation

The diagnostic change builds in the ordinary host target and the original case
still rejects with the same decoder reason plus its selected resource path.
Scoped-texture contracts and final-candidate CI must be recorded separately.
The broad recovery plan still includes Boiling Point equipment producers, smoke,
door playback, the movement-255 record and Warp Shock's wall-animation dependency.
