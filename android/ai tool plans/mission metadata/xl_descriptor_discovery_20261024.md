# XL descriptor discovery prerequisite

## Refreshed study and scope

PR120/121/122 remain open and unmerged at 92aca5067, 416a87de1 and
b8ab82bd9 respectively. New isolated branch `xl-descriptor-discovery` starts at
b8ab82bd916a97f35201cf3ec810429b663159f1. No parent branch is modified.

The original Observatory descriptor fails native mission selection before level
loading: `could not load requested mission obsrv-xl`. Its first key is
`d2x-name`, absent from Redux's native name lookup. Select this descriptor
prerequisite rather than combining descriptor, translation and no-damage gameplay.
The source reference remains arbruijn/d2x-xl at
2576a98b5de310e3ac7c9777df2677d1627b8bae, gameio/mission.cpp.

A no-damage arena is not yet justified. Raw segment flags are separate from the
old-type-13 property mapping. Reference ambient consumers only use low bits, but
this slice neither waives unknown flags nor changes their runtime admission.
No-damage semantics span firing admission, resource consumption, weapon creation,
direct contact and central damage; splash knockback precedes shield protection.
They need dedicated tests rather than a blanket damage guard. The previous LGT
proposal remains excluded: LGT must not replace HAM lighting.

## Behavior and limits

Recognize the exact case-insensitive `d2x-name` key during native discovery and
resolve the normal bounded level list and companion HOG. Keep XL as an explicit
mission identity distinct from `name`, `xname`, `zname` and `!name`. Never infer
stock extra-HAM semantics from the XL descriptor. Alternate HOG directives and
critical extensions remain rejected. Empty or overlong XL titles, conflicting/repeated headers, overlong level filenames
and malformed counts or secret origins fail. Valid secret mapping uses the existing
native bounded parser. XL discovery is limited to D2 `.mn2` descriptors.

All level loading under this identity remains blocked before geometry mutation,
including stock-format files and developer previews. This is descriptor support,
not Observatory resource closure, level translation, presentation or gameplay.
The original POG is resolved as an archive member but is not installed/rendered.
No save layout, SIM behavior, LAN default or active Counterstrike controller changes.
D1 behavior is unchanged; the shared exact-key helper is additive.

## Provenance

Assets repository d9fbeaf6eb059fd4504308bfd6364734775b7cba,
`missions/d2xxl_downloads/levelpack.7z`:

- `missions/kruel/obsrv-xl.mn2`: 297 bytes, SHA-256
  `8a2946c7f786f4834e1c9e303a8d4248b419b89479a750d0d6c1917fbe1b6317`
- `missions/kruel/obsrv-xl.hog`: 82014 bytes, SHA-256
  `e89acaad09cefa0c09fc4b487c56b6077477cfe20169f364fe54f627040b01e7`
- Embedded `obsrv-xl.rl2`: 52176 bytes, SHA-256
  `edf4bcf7db90a979028f883ba9b72d5c584821f0ef22b00cadf6009e409be5df`

Original files remain unmodified and private. Synthetic descriptors are generated
only in a private test directory. No proprietary binaries are committed.

## Acceptance

- Original native mission title, one level and exact companion HOG resolve
- No extra-HAM fallback or gameplay definition change
- Original level and known-good stock level both rejected under XL identity
- Rejection preserves geometry and SIM draw count; stock load recovers
- Observatory descriptor -> stock -> Observatory descriptor transitions unmount
  the companion archive and retain stock gameplay definitions
- Exact key, empty title, invalid count, alternate HOG and critical directive tests
- Original hashes checked before and after integration
- Existing classic, XL, LAN and replay checks remain green

Remaining sequence: resource/flag qualification, bounded 17/36 runtime translation,
then source-matched no-damage behavior with native save/replay and original-region
traversal, followed by emission/presentation qualification. Observatory has no exit
trigger; eventual acceptance is arena behavior, not campaign completion.

## Android/catalog boundary

`mission-metadata-core/MissionDescriptorPolicy.kt` already recognizes `d2x-name`,
including its existing `parsesD2xNameWithoutOrdinaryName` test. `MissionZip` and
`GameFileFormats` delegate to that shared metadata policy. Metadata validity is
not native admission; its existing title preference remains unchanged.
`XL_compat/android/jni.cpp` uses the same native compatibility report changed here;
`kotlin/XlCompatibilityTarget.kt` still turns blocked reports into
`unsupported_format` analysis failures. The report continues to say blocked and
`playable: false`. Android UI/runtime behavior is source-reviewed, not emulator
qualified in this slice. No Kotlin preference or LAN behavior is changed.

## Validated result

Fresh GCC 14 Debug ordinary D2 build: 98/98 tests pass. Standalone XL suite:
5/5 pass, including original Observatory inventory/descriptor and original Robotech
water/native save replay. The descriptor test also checks exact geometry bytes,
SIM state/draw count and game time across denied level loads, and uses an invalid
synthetic HAM sentinel to detect accidental extra-HAM decoding. Survey audit and
scoped formatting/lint pass.

The ordinary, XL-disabled metadata executable independently changes from mission
selection failure to resolving `Observatory XL` / `obsrv-xl`, then exits 1 with
`XL descriptor loaded, but level gameplay remains unqualified`. It does not emit
successful level analysis. Existing headless teardown allocation diagnostics remain;
this is not a leak qualification. No emulator or audible/rendered output is claimed.

## Review corrections

Descriptor-only selection now skips the native mission asset service before it
frees models or reads HAM/sound data. Skipping only the extra-HAM branch was
insufficient: a companion HOG could shadow `descent2.ham` during base loading.
A synthetic companion HOG containing an invalid stock-named HAM proves that the
shadow is mounted yet not decoded; definitions/model payload and stock recovery
remain intact. Direct HAM loading is likewise refused for XL descriptor identity.
Normal mission resource loading is unchanged.

XL names use complete bounded first-line validation and full title validation on
mission load. An `A`, 80 spaces, then `B` cannot be accepted as a truncated `A`.
Headers reaching the native line-buffer bound are rejected, even if a short title
precedes a long comment. Indented mixed and repeated headers are rejected using
consistent whitespace handling. Public title-boundary tests and native fixtures
cover these cases.
