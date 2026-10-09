# D1-in-D2 support and qualification evidence

Updated October 8, 2026. This is the E6 scope artifact for the
[consolidation plan](d1-in-d2-consolidation-plan.md). F1-F5 remain in progress;
the table distinguishes tested behavior, admission policy and unverified scope.
Native D1 remains available and is not being retired.

## Content editions

| Source                                                              | Current imported admission                                  | Authoritative evidence and limit                                                                                                                                                                                                                                                                                                                                                                                                           |
| ------------------------------------------------------------------- | ----------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Registered PC 1.4/1.5 layout, including the tested GOG 1.4a package | Full reader validates and prepares the source               | PIG size 4920305, SHA-256 `093F9CC029200E9D71D5E14F2F06E5E876A658DD64DC664D6911C5D24D7B64FE`; campaign, custom definitions, save/rewind, rendering and LAN evidence in the ledger. Fresh eight-recording corpus is still running                                                                                                                                                                                                           |
| GOG macOS installer containing the same registered PC data          | Same source as the preceding row                            | Extracted HOG and PIG hashes equal the Windows GOG package. This is not evidence of the original Mac game format                                                                                                                                                                                                                                                                                                                           |
| Early OEM layout, PIG size 5039735                                  | Admitted to full validation                                 | No actual source-package runtime evidence in this session; unverified                                                                                                                                                                                                                                                                                                                                                                      |
| PC DOS 1.4 shareware, uncompressed and compressed PIG               | Shared table/POF/sound reader prepares D2 assets            | Both representations pass all seven host level loads and Android emulator gameplay/save/load/rewind. The uncompressed demo also passes the complete seven-level campaign and ending on emulator and physical arm64. Original-package runtime qualification is recorded below                                                                                                                                                               |
| Test Flight PC demo                                                 | Same PC shareware reader; three-level campaign              | All three host level loads match native D1; Android emulator gameplay, save/load and rewind pass. The three-level campaign and ending also pass on the emulator (91 steps)                                                                                                                                                                                                                                                                 |
| Earlier PC shareware variants                                       | Known PC shareware formats admitted to full validation      | Shared format support is not a version whitelist. No authentic earlier-edition runtime fixture has been qualified                                                                                                                                                                                                                                                                                                                          |
| Early registered PC 1.0 PIG layouts                                 | Unsupported by the imported base reader; use native D1      | Separate from PC shareware; no new decoder is claimed                                                                                                                                                                                                                                                                                                                                                                                      |
| Original Mac registered and Mac shareware layouts                   | Unsupported by the imported base reader; use native D1      | Actual Mac demo PIG size 2714487, SHA-256 `B4608A1D0E6191AC6F07410D9714C591C77605A84BCCDB882C2611BD885A2905`, was incorrectly marked ready and failed at vclip decoding. Android now rejects it before a game process starts, explains the unsupported edition and retains native D1 readiness. Before/after evidence: `temp/d1-edition-scope-evidence/{mac-demo-before,mac-demo-rejected}`. This tests admission, not native Mac gameplay |
| Unknown or modified registered-layout PIG                           | Full reader must validate; size alone never proves validity | Keep existing staged validation of spans, references, models, clips, bitmaps, sounds and source identity. The edition query only rejects known unsupported layouts                                                                                                                                                                                                                                                                         |
| PG1/DTX/HX1 custom content on registered PC base                    | Validated combined generation                               | Actual custom checkpoint, rendering-demo, asset retirement and malformed-input evidence in the ledger. Not a claim that every third-party mission is tested                                                                                                                                                                                                                                                                                |
| Optional registered D2 Guide-Bot source                             | Available independently when its dependencies validate      | Cold deployment, docking, file/memory restore, cooperative rewind, missing/changed-source rejection and recovery verified. D1-only baseline does not require these files                                                                                                                                                                                                                                                                   |

## Original-package PC shareware qualification

The [PC shareware implementation milestone](d1-shareware-in-d2-20261008.md) is complete.
On October 8, 2026, `android/tests/test_demo_import.ps1 -Serial emulator-5582`
passes all 11 catalog archives through the production importer, checks exact
installed bytes and manifests, verifies readiness separately for native D1,
native D2 and D1-in-D2, and verifies failed-import preservation. Both PC downloads
(`desc14sw.exe` and `descent 1 demo 1-4.zip`) each pass D2 gameplay, file restore
and rewind (37 steps), then door/effect/reactor checks, all seven levels and the
ending (123 steps). Runtime consumes the production-import outputs directly on
the device, including their manifest; it does not substitute host-extracted data.
A null `runtime_suite` explicitly means an archive has no runtime qualification
in this corpus. Readiness alone does not establish playable content.

The final corrected build also passes the seven-level demo ending, persistence
and a 69-step demo/D2/demo round trip on the attached API 33 arm64 device using
the isolated diagnostic app. Test Flight passes its 91-step three-level campaign
and ending on API 34 x86_64. Compressed DOS data has independent host level,
source-switching and Android persistence evidence.

The final audit caught missing D2-only weapon defaults in the shareware adapter.
Registered and shareware readers now share weapon and robot default initialization.
Native/imported demo gameplay traces match all pickup, damage, drop, reactor,
explosion and contact cases, and 41 weapon-rendering artifacts match exactly.
Repeated missing table/model and truncated-HOG loads preserve the active assets;
changed sources reject the saved identity and original sources recover it.

Final evidence and APK hashes: `temp/d1-shareware-reader/final-qualification.json`.
The same directory contains `qualified-fixtures.json`, `final-original-package-runtime.log`,
`final-package-*`, `final-physical-*`, `qualified-flight-*`,
`{demo,flight,compressed}-source-final-host.log`, `shareware-gameplay-rules-final.log`
and `shareware-weapon-comparison-final.json`. Earlier qualification manifests are
historical and predate the final adapter-default correction. These runs exercise
app-private production import/staging; they do not newly qualify every storage
provider or the broader registered-content replay corpus.

## Platforms and runtime modes

| Target                                   | Evidence                                                                                                                                                                                                                                                               | Remaining qualification                                                                                                                           |
| ---------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| Windows x86 native D1 and imported D1    | Both executables build; native and ordinary D2 host suites, actual campaign/persistence and renderer checks pass                                                                                                                                                       | Current paired corpus `temp/d1-selection-death-corpus` is live. Historical recording/native disagreements remain separate from engine parity      |
| Android x86-64, imported D1              | Actual D1-only level travel, both-installed switching, overlay/controls/Back after cooperative level travel, memory/file restore, rewind and companion controls verified. Edition admission and the registered-PC 67-step level-transition control pass on the new APK | Broader presentation coverage remains open; artifact hashes and exact source boundary are in `temp/d1-edition-scope-evidence/implementation.json` |
| Android x86-64, native D1 control        | Actual memory restore/rewind and native-reference Spreadfire capture pass                                                                                                                                                                                              | Not a complete replay corpus on device                                                                                                            |
| Android arm64-v8a and armeabi-v7a        | Both engines compile in the three-ABI APK. Physical arm64 API 33 / Mali-G77 MC9 passes DOS demo gameplay, save/load, rewind, the seven-level ending and a D1/D2/D1 round trip in the isolated diagnostic package                                                       | armeabi-v7a runtime remains unqualified here; the arm64 demo check does not qualify the entire registered-content replay corpus                   |
| Linux and macOS executables              | Cross-platform source paths retained                                                                                                                                                                                                                                   | No current build/runtime evidence in this Windows session                                                                                         |
| Imported LAN co-op with matching sources | Physical level-one exit through level-two overlay/input/Back; host/client rewind and companion ownership verified                                                                                                                                                      | Not evidence for every competitive mode or mixed engine/version pairing                                                                           |
| Rendering demos                          | Imported version 16/type 4 source identity, seek/rewind/export and opposite-endian cases verified                                                                                                                                                                      | Original native version-13/type-2 playback remains unsupported in D2                                                                              |

## Current persistence contracts

| Stream                            | Discriminator                                             | Owner and evidence                                                                                                                       |
| --------------------------------- | --------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| Native D1 checkpoint              | Version 19                                                | Native writer plus `d1_save_translate`; explicit active wall-blast suffix, native source IDs, checked opposite-endian import             |
| Imported D1 file/memory save      | Version 40                                                | D2 framing plus owned base/custom and optional-source identity; missing/changed sources reject without silently substituting definitions |
| Ordinary D2 file/memory save      | Version 38                                                | Ordinary engine layout and namespace; used as a control in persistence and cooperative tests                                             |
| Imported rendering demo           | Version 16, game type 4, identity event 52                | `newdemo` framing and owned identity check; old ambiguous imported type-3 recordings rejected                                            |
| Imported network source admission | Fixed 65-byte identity before object/full synchronization | Shared network transport and owned source check; malformed/source mismatch matrix and actual Android peers verified                      |

These rows do not replace the F1-F5 exit gates. The current replay report must
remain incomplete while the independent state/identity audit, required corpus
verdicts or declared runtime evidence are missing. Cosmetic animation repetition
is excluded, and graphics must not advance or reseed SIM RNG.

The unsupported Mac-demo Android admission case passes both with D1 data alone
and with registered D2 installed. Ordinary D2 readiness is retained in the
latter case. Both emulators have the final APK pinned in implementation.json;
isolated test files and preferences are restored.

Subsequent bitmap-boundary verification is pinned in
temp/d1-bitmap-facts-evidence/implementation.json. The final APK passes the same
67-step registered D1-only interaction/level-transition control after primary
emulator recovery; both emulators have it installed. Host integration verifies
published stock/custom bitmap flags and registered D1/D2 switching. The replay
corpus was frozen before this change and remains separate qualification evidence.

Metadata initialization evidence is in temp/d1-metadata-init-evidence. Actual
native D1 and ordinary D2 host/Android workers pass failed-initialization
retirement, fresh-process recovery and healthy reuse, including request-level
failure without retirement. Both emulators have its pinned APK; the registered
D1-only 67-step game control passes. Imported in-game active-level metadata
still selects a D2 worker and requires D2 base files, so that behavior is not
qualified by these passes. The frozen corpus also predates this worker repair.
