# Scoped RLE texture recovery

Original Boiling Point Waterworks selects `mods/boilpnt/textures/rock194.tga`.
The authored file is a 512 by 512, 24-bit type-10 TGA, 743708 bytes, SHA-256
`ffbc219965339e0f079838b1919836adc2e1037ef67ab679d717d97adf01f064`.
Its decoded top-left RGBA bytes have SHA-256
`370bf54a6cda458661dd8439c1ff749d38931b6dd005dcd0d69e6512b1a7b3e5`.

## Scope and source distinction

The pinned CTGA reader does not decode this encoding. Its SDL alternative selects
PNG inputs, and its Windows/Linux cache lookup uses a separate cache directory;
the authored `textures/256` directory is not a justified substitute. This change
is an explicit bounded compatibility extension for scoped mission textures.
It does not claim equivalence to the pinned raw CTGA decoder. Particle image
decoding keeps its existing policy, and authentic inputs remain unchanged.

The extension accepts bounded true-color 24/32-bit RLE data, validates packet and
image bounds, rejects unsupported palette/origin/interleave layouts, and applies
standard orientation before the existing bitmap alpha/key/average policy. Run
packets cannot cross rows, following the TGA 2.0 specification. The original file
contains 46107 packets and no crossing runs. Input/resource ownership, source
hashing, frame bounds and admission remain with their existing owners.

## Validation evidence

Private artifacts: `E:/dxx-redux-builds/xl-recovery-20261117`.

- `rle-pixels-02/comparison.json`: original plus 32 generated images compared
  independently with Pillow; all 33 decoded RGBA comparisons pass.
- Existing scoped texture contract: eight orientation/channel profiles, 72 malformed
  controls, and explicit alpha/key boundary checks pass.
- `bp1-rle-01/results.json`: original Waterworks passes 120 unassisted simulation
  frames with developer controls disabled.
- `rendered-bp1-rel-01/result.json`: optimized Windows game passes 120 simulation
  and 120 rendered frames on Intel Arc 140T, original level and inputs.
- `android-rle-03/d2.json`: production XL RGBA upload, 4096 exact pixel samples per
  context across two fresh contexts and eight owner-transition rows. Both D1 and
  D2 fixtures pass; SIM/FX draw counts remain unchanged. The GPU sample hash is
  `2ecb6e5402c1f9fa11d5c9514a9a544d812d54357768aef319a620b811f8b502`.
- `rle-live-d2-1-03`: original incoming Waterworks ownership passes in 10.438 seconds,
  including malformed RLE packet and missing-set rejection, independent candidates,
  moves/discard, outgoing world/pixels/SIM preservation and 120 outgoing gameplay
  frames. Save preflight distinguishes changed source identity, rejects malformed
  overrides without changing live state and recovers after restoring the original.
- `rle-live-d2-7-03`: the existing default incoming level-7 control passes in 9.078
  seconds. Both runs leave all copied mission/mod input bytes unchanged.
- `rle-live-wshock20-1-03`: the same Waterworks controls pass in 14.015 seconds with
  original Warp Shock level 6 as the outgoing XL world; copied inputs are unchanged.
- Both automation catalogs pass: 428 top-level entries and 550 support scripts.

## Retained failures and limits

The first generated RLE fixture crossed row boundaries and failed Pillow decoding;
the bounded decoder now rejects that shape. The initial two Android attempts passed
RGBA storage into a test-only RGB upload entry. Targeted pixel diagnostics identified
the mismatch; the fixture now calls the production `ogl_xl_rgba_load` entry without
relaxing comparisons. This was a fixture correction, not an engine GPU fix.

The Windows Debug rendered attempt timed out after 45 seconds during loading. The
separately labeled optimized build passed; the timeout remains a failed observation.
The local full survey `rle-64-01` is incomplete: 55 passes, five load failures and
four untested cases in 319.735 seconds, exhausting the unchanged start guard. It is
not a complete readiness percentage. No same-candidate reroll replaces that receipt.

The first live ownership invocation used the route executable, which rejects the
diagnostic command line. Its receipt remains under `rle-live-d2-1-01`. Qualification
uses the developer metadata executable that owns this diagnostic.
The second invocation used a user directory outside the fixture and was correctly
rejected by the diagnostic guard; the third uses its required `extra/preview-user`.

The GPU fixture is isolated under `/data/local/tmp`; it does not update the installed
APK. These checks establish bounded loading, pixels and ownership behavior, not
campaign completion or general replay determinism. Final published-candidate CI
and live ownership results will be recorded in the tranche ledger.
