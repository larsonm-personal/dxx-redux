# Wall-camera recovery checkpoint

Immediate parent: build-margin PR 557, head
32e081bac70529d70148b4a2e87e8f6f6f8b90f6. Historical stack: 471. No merge or
auto-merge authorized. This checkpoint qualifies ordinary wall cameras rather
than dropping their records or merely suppressing their trigger actions.

## Behavior and scope

The pinned XL camera manager creates monitor views from wall CAMERA triggers at
level setup. Its activation dispatch is inert but still obeys actor gates,
delays, cooldown and post-action flags. The renderer now retains a private
viewer for each wall binding, using source-side center and normal zero converted
through the existing source-matched angle and matrix helpers. No gameplay object
is allocated. Existing attached-camera owners and dead-owner freeze behavior
remain separate. Mirror bindings retain the existing source-matched UV path.

Only wall type 16 is newly admitted. Smoke actions 20-24 remain blocked. No
teleporter-camera, multiplayer or optional rendering-profile qualification is
claimed. Compatibility reporting distinguishes setup-time monitor creation
from activation bookkeeping. New native integration is confined to the existing
developer-only desktop probe router; production XL policy stays in XL_compat.

The source oracle extracts actual Setup, vector-to-angle, angle-table and matrix
functions, plus actual trigger Operate/delay/countdown and diagnostic definitions.
It checks both DBG modes. Source CCamera::Setup receives the center computed by
the already qualified geometry compiler; the new comparison does not independently
requalify side-center compilation. Source revision and pins are recorded in
xl_recovery_inert_triggers_20261116.md and checked by CMake extraction.

## Local evidence

Private root: E:/dxx-redux-builds/xl-recovery-20261116.

- camera-64-01: complete ordinary developer-OFF survey, 58/64, six load failures,
  zero simulation failures, original 8/8, 278.328 seconds, no budget exhaustion.
  Authentic assets and resource pins, cohort, 120 frames and watchdogs unchanged.
  Sphere 5 and Warp Shock 3 both pass. This measured the parent plus recorded
  working patch, not the clean parent. camera-trigger-01/measurement-source-hashes.json
  identifies the binary, tracked patch and untracked source at measurement time.
- camera-trigger-01/native-flags-final: Debug native integration and independent
  source comparison pass. Covers 256 flag combinations, actor/malformed-link
  rejection, immediate/chained/permanent/random delays, autoplay, disk restore,
  stock transition and continuation after attached actor removal.
- 558 native viewer samples across all six sides of derived Neptune match
  extracted Setup placement/orientation. Invalid source faces reject, object
  allocation and SIM RNG stay unchanged. A one-unit matrix mutation rejects.
- Full effect reference run on original Neptune and inventory contracts pass.
- rendered-sphere: four original monitor faces captured at 320x240, with
  65709, 46184, 73851 and 70048 visible pixels respectively. rendered-warp:
  one original face captured at 320x240 with 67018 visible pixels. Both pass
  opacity, GL error, viewer ownership, object count and SIM RNG checks. Explicit
  visibility is an assist; these are not earned traversal or visual parity claims.
- android-camera-01: x86_64 Debug APK builds with outputs/native staging on E:.
  device-02 passed 24 automation steps covering captured monitors before/after
  quick-save restore and retained one-shot camera flags. App-state before/after
  hashes match. The final-source APK repeat also passed all 24 steps in device-final
  (run 47c45b056d684d8faa5d0c45b96a2335, 7.414 seconds), with matching restored
  hashes. APK SHA256: cfad8a11e19693c5dc01f1916018e9ee48efefabecfe7311e306b2bc6b76f22a.
- Automation catalog validation passes. Master catalog passes with its new
  optional output directory on E:, retaining the unchanged 4 GiB reserve and
  default CI behavior. The original C: reserve failure remains recorded.

The Android compiler found min/max macro expansion in ambient_sound.h. Parenthesized
numeric_limits calls repair the compile failure without changing range checks.
The first Android test assertion expected numeric 1 rather than boolean true;
the captured monitors passed, the assertion was corrected, and the entire test
was repeated. Earlier mission-title, admission, extraction/include-order and
desktop-link failures are retained in camera-trigger-01 rather than overwritten.

## Acceptance and remaining work

Candidate CI is pending until this checkpoint is published. Parent checks are
not evidence for this source. Record exact head/merge/controller/run identities
and applicable owner outcomes after publication. Linux and asset-free public
checks, private behavioral owners and the unchanged private survey must be
inspected; pending or skipped checks are not passes.

Six remaining expanded failures are Boiling Point 1-5 and Warp Shock 4. Original
Android G2 rejected-save continuation and startup-FX mismatch remain open. This
checkpoint establishes bounded camera behavior and load/tick progress, not
campaign completion, general visual parity or a playability percentage.
