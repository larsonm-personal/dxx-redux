# Layer 03: native/XL boundary review

Scope: `0a2df4aeb..013b7234`, plus the user's retained uncommitted plan revisions.
Review and refactor the continuation layer; do not start the next gameplay tranche.
No commit, push, PR or merge is part of this cleanup request.

## Plan

1. Classify all 16 changed `d2/` files by ownership, not just diff size.
2. Move isolated candidate resource lookup/mount policy and XL screenshot
   readback policy into `android/XL_compat/runtime`, retaining narrow native hooks.
   Move the XL-only mission-preparation header and developer build definitions
   into that boundary too.
3. Keep single-source native save/AI codecs and native state capture at their
   existing owners; do not duplicate formats or expose private globals simply
   to reduce the `d2/` line count.
4. Build affected ordinary/developer Windows and Linux targets. Reuse the
   Observatory ownership controls, existing screenshot/time-continuation probe
   and ordinary replay. Run scoped quality and record exact results.
5. Update the architecture guidance and next-window documents with the boundary,
   new tested identities and remaining limitations. Preserve earlier evidence.

## Initial classification

The layer changes 16 `d2/` files: 746 additions and 503 deletions. This is not 746
lines of independent XL behavior. In particular, `ai.c` replaces a separate
preflight/read pass with one retained native decode (188 added, 238 removed), and
`guidebot_route.c` shares field lists with its typed snapshot (65 added, 130 removed).

| Paths                                 | Decision                                                                                                                                                         |
| ------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `mission.c`                           | Extract candidate archive namespace, path/origin policy and mount lifetime; retain descriptor parsing, identity reservation and native selection/commit plumbing |
| `arch/ogl/gr.c`                       | Extract XL framebuffer/read-buffer policy; leave screenshot naming, timing and image writing with the native owner                                               |
| `main/CMakeLists.txt`                 | Move the new XL developer navigation definitions to the existing XL target configuration                                                                         |
| `ai.c`, `ai.h`, `state.c`, `object.c` | Retain native codec/context changes; relocating them behind an XL-only loader would split stock/save semantics                                                   |
| `d1_in_d2_ai.c`, `d1_in_d2_ai.h`      | Retain original-D1 record dispatch in its existing owner; it is not XL format policy                                                                             |
| `cntrlcen.c`, `cntrlcen.h`            | Retain const-correct native reactor definition access                                                                                                            |
| `digiobj.c`, `digi.h`                 | Retain one bounded native logical-sound translator shared by live and prepared views                                                                             |
| `escort.c`, `guidebot_route.c`        | Retain small accessors over owner-private state and single-source field lists; do not export private state for observers                                         |
| `game.c`                              | Retain the small clock lifecycle hooks; their implementation already lives in XL compatibility                                                                   |

## Implemented boundary

- `runtime/mission_resource_scope.c/.h` now owns candidate relative-path checks,
  isolated HOG mount names, case-correct/origin-checked lookup, borrowing an
  already-owned live archive, and busy-handle release. `mission.c` keeps the
  native descriptor parse, reserved identity and publication lifecycle, with
  small calls into that resource owner. No alternate parser or hidden `.inc`
  implementation was introduced.
- `mission_prepare.h` moved from generic Android shared code into
  `XL_compat/runtime`; its API is enabled only for the XL runtime.
- `runtime/screenshot_readback.c/.h` owns desktop framebuffer selection and exact
  read-state restoration. Native screenshot naming, image writing and timing
  remain in `arch/ogl/gr.c`; the existing non-XL fallback remains there too.
- Developer metadata navigation definitions moved to `runtime/configure.cmake`.
  `d2/main/CMakeLists.txt` now matches the layer's parent byte-for-byte.
- The architecture requirement is explicit in `.github/copilot-instructions.md`,
  the XL README and the next-window plan. Future substantive native changes must
  identify the native owner that requires them rather than simply growing the
  engine files as convenient integration locations.

Against the same layer parent, the native diff is now **15 files, 666 additions
and 502 deletions**, or **164 net added lines instead of 243**. The cleanup itself
removes 79 net lines from `d2/`. The remaining native codec/context and private
state-access changes are intentional; moving those wholesale would separate the
existing native format implementation from its ordinary consumers. D1 has no
matching policy hook to add for these D2-only helpers, and no D1 source changed.

## Validation and outcome

The known whole-save atomicity defect remains outside this refactor's scope;
no G2 acceptance, Android qualification or hosted-CI result is implied. Validation
uses the existing owners and unchanged assertions. No new test framework or
per-level runner was added to the repository's test catalog.

Evidence is in `temp/xl-layer03-architecture-20261112/`; final Linux runtime output
is in `/var/tmp/dxx-pr464-20261111/private/architecture-review/`. Scratch scripts
there only invoke existing owners and comparison functions. The
[build manifest](../../temp/xl-layer03-architecture-20261112/build-identities.json)
records the base commit, every moved/changed source hash, removed source path,
eight executable hashes and pinned reference commit.

Completed implementation checks:

- Windows/Linux ordinary developer-OFF engine and compatibility targets build.
- Windows/Linux developer-ON engine and metadata targets build, including the
  moved navigation definitions. No new warning originates in changed code;
  existing weapon-return, packed-member and test-record warnings remain.
- Original Observatory owner passes on both hosts with retained-definition,
  isolated-read, uppercase/path rejection, busy lease, discard, snapshot-commit,
  malformed/archive/identity and stock recovery controls. The pinned reference
  compares 12 firing and four contact cases. Windows 17.859s; Linux 5.376s.
- Existing original WShock20 level-6 time/screenshot probe passes on both hosts:
  seven screenshots and read-state/GL-error checks, native warm restore and
  matched 12-frame replay result. Windows 26.141s; Linux 16.850s. Setup uses five
  position assists, no inventory assists and no assists during recording. This
  is a mechanics/readback witness, not connected play or level completion.
- Windows/Linux `^test_upstream_compat$` passes, one combined CTest owner each,
  zero skips: Windows 55.62s, Linux 26.40s. This is not the full native CTest suite.
- Windows/Linux ordinary replay of the retained pre-refactor recording passes strict
  assets/result/RNG/clean-exit checks. Detailed comparison to the original live
  capture matches 12 object frames, 16 world records, 21 SIM and 12 FX events.

The current ordinary executable hashes are:

| Executable                                                   | SHA-256                                                            |
| ------------------------------------------------------------ | ------------------------------------------------------------------ |
| `temp/xl-day-20261110/build-d2-reference/main/d2x-redux.exe` | `78beccdce0bf71319050132583e1aaf33076ef9d1fc2cbee8f7ea2089b1ead98` |
| `/var/tmp/dxx-pr464-20261111/private/stock/main/d2x-redux`   | `3100032f743ff97df5ddd8b68be01d72d92766f22f17178068b480e2fa17274a` |

The original live-capture/demo bytes and old failure evidence were retained.
This cleanup is uncommitted and unpublished; no Android/macOS build or hosted CI
was run, and no package was assembled. The next worker must include the new files
and header move when committing; a tracked-only diff omits the new helper sources.

The remaining work is the existing G2 transaction, using this narrower boundary.
Do not reopen native codec placement merely because its diff is large; instead,
review the next native change for whether it implements a native format/state
operation or new XL policy. Keep new policy on the XL side of the hook.

### Failed setup attempts and interpretation

The first Windows Observatory invocation used the full gameplay installation
instead of the owner's configured retail-only directory and failed its zero-HAM-
warning assertion. The corrected invocation used the original retail fixture;
no assertion, input bytes or native source changed to obtain that pass.

The first Linux Observatory invocation wrote its test workspace on `/mnt/c` while
compilation was active and timed out at the owner's unchanged 60 seconds. After
the build completed, the same owner passed in its normal native Linux filesystem
workspace in 5.376s. The failed log remains at `linux/observatory.log`; the timeout
was not increased or reclassified as a passing test.

The scratch summary initially expected the warm probe's `complete` field to be
true. Existing normal replay exits after writing its actual result, leaving the
probe at `phase=native_restored`, as it also did before this refactor. Corrected
summary checks use the actual 12-frame result, native restore/rejection counters,
matching-result log and terminal exit status. Original receipts are unmodified;
the summary explicitly retains `warm_proof_complete=false`.
