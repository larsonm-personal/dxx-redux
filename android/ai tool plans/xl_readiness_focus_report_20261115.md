# Readiness-focused progress: 4/8 to 7/8 locally

The user explicitly authorized authentic Vertigo and XL resources in CI through
the private assets repository. This resolves the earlier prohibition on adding
those prerequisites. Original mission archives, eight selected levels, 120
unassisted frames, 60 Hz, seed, difficulty and watchdogs remain unchanged.

## Changes and measured outcomes

PR 515 provisions authentic Vertigo and camera resources from private revision
dc6565f6b5400d7c700fa8378e766b2354b6ceb6. Both files already existed there; no
private upload was necessary. Their sizes and SHA256 values are verified before
staging, after staging and at measurement. The receipt records a separately
pinned resource profile and the reporter rejects a missing or altered profile.
Mission inventory remains pinned to 9dad21bd65ba05665ac6fe315f37fe7461e85725.

The native child distinguishes explicit no-render execution from interactive
renderer admission. No-render startup still validates authored fog colors and
retains the existing fog state and skybox geometry/collision owners. It does not
allocate a presentation framebuffer. Interactive startup still requires the
existing supported renderer. No clock, RNG, physics, material or resource
definition is replaced, and ordinary Android timeline guards remain unchanged.

| Case                        | Stock-only baseline          | Authentic resources, baseline engine   | Native candidate      |
| --------------------------- | ---------------------------- | -------------------------------------- | --------------------- |
| Boiling Point level 7       | Missing HXM base definitions | Custom-texture declaration without POG | Same rejection        |
| Dinter level 1              | Missing HXM base definitions | Skybox renderer admission              | 120 frames, XL active |
| Sphere level 2              | Missing HXM base definitions | Fog renderer admission                 | 120 frames, XL active |
| Warp Shock level 6          | Missing camera addon         | Skybox renderer admission              | 120 frames, XL active |
| Four classic/imported cases | 120 frames each              | 120 frames each                        | 120 frames each       |

Both ordinary developer-OFF Windows and Linux candidates measure 7/8. Linux
runs with DISPLAY and WAYLAND_DISPLAY removed and SDL_VIDEODRIVER=invalid;
this is genuine headless execution, not Xvfb. Supplemental assisted navigation
remains separate and does not contribute to the score. Historical stock-only
4/8 is retained as a different runtime-input profile, not relabeled 7/8.

## Evidence and validation

Private evidence root: temp/xl-readiness-focus-20261115. baseline-receipt.json
contains baseline/source/catalog/input identities; runtime-local-receipt.json
contains the four measurement receipts and native source/binary hashes.

- baseline-01: unchanged stock-only ordinary engine, 4/8
- provisioned-01: repository-downloaded authentic resources, original engine,
  4/8 with all original HXM/camera failures cleared
- provisioned-02: production prepare/run/report CLI and required profile, 4/8
- provisioned-03: ordinary Windows native candidate, 7/8
- provisioned-linux-01: ordinary Linux native candidate without display, 7/8
- 51 private-asset tests and 19 controller tests pass on Linux
- 61 public reference contracts pass in 43.37 seconds
- Windows Debug upstream compatibility passes in 104.21 seconds
- Linux Debug upstream compatibility passes in 39.89 seconds
- Original Pumo/effect Windows Debug native controls pass in 17.829 seconds:
  interactive missing-renderer rejection plus no-render hub/fire preflight,
  existing particle/lightning ownership, rejected payloads and recovery
- Ordinary Android build passes all three ABIs, developer tests OFF, 2m37s

Frozen Android APK SHA256:
b7f7e11bd086e0e4ae30fed91d553a71f1d2ee2084697d83ecbf899ec68d0f90.
It was not installed; this is build evidence, not new device-runtime evidence.
No emulator/application state was changed in this tranche.

Windows route binary SHA256:
fa4aed80c43c66d9d499f41afcd596fcc0905151ae99e9edbbc66b8406bada50.
Windows Debug native owner SHA256:
4f46f239eb9ec06d43b1f846136726f0fd27b07d1fac08721b6ed945296afda7.
The native changes were uncommitted during these runs; receipts retain the
parent SHA and exact changed source hashes, not a false clean-HEAD claim.

The initial Windows Python whole-suite attempt failed on missing POSIX
subprocess prerequisites. All attempts are retained. Existing native warnings
and allocator diagnostics remain visible in their logs. The four stashes,
authentic inputs, .codex and unrelated quality-evidence work remain preserved.

## CI and remaining work

This is a pre-final CI snapshot. PR 515's initial head eb075b3b1 failed the
workflow policy check because the newly pinned checkout adds a third private
token use. Follow-up 634abc984 verifies that checkout's pin, sparse paths and
disabled credential persistence; the targeted check passes. Final source IDs
and outcomes belong in the final CI receipt. Pending jobs are not passes. The trusted
controller executes from cmake; candidate changes cannot self-deploy its new
resource staging. Until reviewed integration and a fresh exact-candidate run,
hosted readiness still uses the older stock-only input profile. No merge or
queue operation is authorized or performed.

The runtime child's exact final-head CI, Linux original Pumo controls and final
source/input preservation audit must be recorded in the final local handoff
receipt and PR description. These post-publication results are not implied by
this committed snapshot. No whole G2-G7, rendering, campaign, physical-device
or 90% playability claim follows from this 120-frame survey.

One selected case remains: Boiling Point level 7. The supplied descriptor says
custom_textures=yes, but the package has no level POG and contains mod textures.
The pinned source parser treats that declaration as metadata, but relaxing our
guard requires proving the actual texture/material ownership and collision
semantics. No missing POG, replacement skin, level bytes or expected output was
fabricated. A finite 8/8 promise is still unsupported until that dependency is
resolved and the ordinary complete cohort passes.

Planning estimate: one more focused tranche for the remaining case, with a
second possible if its texture requirements expose deeper ownership work.
This is conditional, not a delivery guarantee. Hosted qualification additionally
requires reviewed integration of the trusted resource-provisioning controller.
