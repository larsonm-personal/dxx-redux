# Chunk0512 MIDI consumer continuation, 2026-10-21

Diagnosis and plans only under plan_general_diagnosis_resume_20261009.md. Previous conversational answer was a status explanation and made no authoritative progress. This continuation revalidated the unchanged worktree and completed new source review and an independently checked evidence checkpoint

HEAD 092f4c7d247eaeb26160933540f3bdbf2f318d4a. All five canonical identities remain unchanged from the installer/deployment checkpoint. Canonical queue remains 511 DONE / 116 TODO / 627. Only the source gate is closed; consumer completion, owner/disposition and publication remain open

## Evidence and preservation

Fourteen fresh contexts cover 1796 range-lines, including ten whole files. Combined with the preceding phase, this is 94 contexts / 11707 range-lines; overlaps are retained and do not claim unique source coverage. Three whole prior documents are captured, including the current GQR0131 continuation and 0497 synthesis

The native header and all 710 lines of hmp_android_shared.c were recovered in bounded reads after an earlier clipped display. Preserve legacy interchange versus playback separation, measured GM volume/reset behavior, FM/device track selection, explicit approximate fallback, branch restoration, ended-track exclusion and EOF reset/one-tick semantics. Source coverage is not runtime, malformed-input, allocation or whole-program acceptance

The actual exporter invokes the production shared converter and TML and checks output completion. The Python fixture exercises compiled conversion and independently decoded MIDI state; standalone discovery intentionally skips without an exporter. Full extractor CTest supplies the executable. Dependency-light registrations are visibly marked SKIP, not successful no-op tests

The registered PowerShell wrapper requires all three CTest names and runs CTest with timeout60 inside parent210. Keep unique run directories, checked child exit, producer lock and atomic summary. SyntheticOnly success and missing-reference SKIP2 remain different outcomes. Broader total-budget and cleanup owners still need final reconciliation

The comparator preserves ordered channel messages, timing, effective note state and SysEx without automatic alignment or clock stretching. Prefix priming is explicit and restricted to retained program/bank/pan state. Maintained manifests bind HMP/capture hashes and finite explicit windows, require legacy mismatch, and add a level7 all-device negative control. Level8 explicitly covers the opening only, not the full repeat

## Pending comparator observation, not admitted

For two valid SMFs and finite starts/tolerance, an explicitly NaN duration is not rejected by the sign or file-end comparisons. Both event windows and note-state selections become empty, so all channel and SysEx comparisons can pass. The public CLI accepts float duration input. This is a source-path observation only; neither comparator, CLI, exporter nor fixture was executed

The checked-in manifests contain finite durations. This does not establish a false PASS in maintained stock runs. NaN tolerance does not pass max_error <= tolerance, and infinite duration is rejected by finite file ends; do not generalize the witness to every nonfinite argument

Existing GQF0144/GQR0131 covers successful converter semantics and actual-game repeat selection, not an explicit comparator-window admission requirement. GQF0307/GQR0293 concerns another actual audio validator and must not be merged solely because it also involves nonfinite values. Bounded owner searches found no explicit comparator-window owner, but full prior/publication reconciliation is still required before admission or assigning priority

Future acceptance should validate finite start/window/tolerance values and derived duration in the actual comparator. Extend the existing ordinary SMF fixture and actual CLI controls, preserving finite match/mismatch and partial-window rejection. A strict finite JSON barrier may be complementary. No product/test edits or executions are authorized by this checkpoint

## Review and audit limits

Initial searches named two nonexistent files; corrected locators were used. A broad NaN substring search also matched provenance and clipped; it earned no complete owner-read credit. A follow-up UTF-8 whole-section header search avoided those substring matches. That search is a locator/dedup aid, not complete owner acceptance

The independent audit checks actual current/HEAD equality for every captured source, exact ranges/payloads/extents, current prior documents, all five canonical identities and finite maintained manifest values. It does not execute or prove runtime behavior

Evidence `temp/general_cleanup_20261006/gq2-midi-consumers-0512-20261021.json` SHA256 6bcc67d60dc03c466d77f1e56d6605424e2229041f6e8ca7a64b719eee31c482

Audit `temp/general_cleanup_20261006/gq2-midi-consumers-audit-0512-20261021.json` PASS 97, SHA256 8cf16933da26489cd810dd48ab4e3152e1de4ef8f01c855f5b4e397dde1576d0

| Path | Range | Whole LF SHA256 | Payload SHA256 |
| --- | --- | --- | --- |
| android/app/src/main/cpp/shared/hmp_android_shared.h | 1-31 | 11079b51cf4c9285ec04cb26cb46ed422c1c77389970e1f6b63e51470149c6b9 | 11079b51cf4c9285ec04cb26cb46ed422c1c77389970e1f6b63e51470149c6b9 |
| android/app/src/main/cpp/shared/hmp_android_shared.c | 1-710 | 3e4024014a4d84228da0601d0160779906e6cb470a3cc2fdb6b1b17d44e189a3 | 3e4024014a4d84228da0601d0160779906e6cb470a3cc2fdb6b1b17d44e189a3 |
| android/tests/midi_diff.py | 1-318 | e2366c08200b803b8cdf5b60e4328ba89789d583b958b3d6e764e3c002065f19 | e2366c08200b803b8cdf5b60e4328ba89789d583b958b3d6e764e3c002065f19 |
| android/tests/test_hmp_playback.py | 1-241 | d5c01605ff5dd6c809a89d039610ae77d23a0a3dabd85a41feaff270c2ceffb7 | d5c01605ff5dd6c809a89d039610ae77d23a0a3dabd85a41feaff270c2ceffb7 |
| android/app/src/main/cpp/extract/hmp_midi_export.c | 1-137 | 537e559f62a5b653405f370fbb879b63ef98e6a2c8e11540a8620cc808896469 | 537e559f62a5b653405f370fbb879b63ef98e6a2c8e11540a8620cc808896469 |
| android/tests/run_dos_midi_parity.py | 1-76 | a8ee76c68babdd39ead3ec76ea56565e4dcbc915cd55218238af5a962458ccfa | a8ee76c68babdd39ead3ec76ea56565e4dcbc915cd55218238af5a962458ccfa |
| android/tests/test_dos_midi_parity.ps1 | 1-111 | f943f9f3ae86ff6e49c990c94d212abd14f324fe57939e58baaea5fe09d3771e | f943f9f3ae86ff6e49c990c94d212abd14f324fe57939e58baaea5fe09d3771e |
| android/tests/fixtures/dos-midi/descent14-game02.json | 1-12 | 4ee9884484eb1124002e24114131a0e6d8d1d59fedc534d59c0dfba697889531 | 4ee9884484eb1124002e24114131a0e6d8d1d59fedc534d59c0dfba697889531 |
| android/tests/fixtures/dos-midi/descent14-game07.json | 1-13 | b0fdc9ab2409a120736e491428015d50e4946a4df6b1096054c95b7e00b59c34 | b0fdc9ab2409a120736e491428015d50e4946a4df6b1096054c95b7e00b59c34 |
| android/tests/fixtures/dos-midi/descent14-game08.json | 1-13 | 666cf77ddbbed5ecb4eebe2f043c42b65eebac2d1dc269f810fd3b04970a1fbe | 666cf77ddbbed5ecb4eebe2f043c42b65eebac2d1dc269f810fd3b04970a1fbe |
| android/app/src/main/cpp/extract/CMakeLists.txt | 552-569 | 3225d5e7aaf3586f4610cd1c0cf13136cd93486b6e1e5fd080a2e21a06e9df48 | 7b3e90f08e7f908b00b6afcccbbd59a45804a4aa96788729f6dd6b294980f0a8 |
| android/app/src/main/cpp/extract/CMakeLists.txt | 620-690 | 3225d5e7aaf3586f4610cd1c0cf13136cd93486b6e1e5fd080a2e21a06e9df48 | fe7cd6562bf00c914c6a0091862dd1a2735030a6fed2ffb927b5815ea5decb98 |
| android/app/src/main/cpp/extract/CMakeLists.txt | 890-928 | 3225d5e7aaf3586f4610cd1c0cf13136cd93486b6e1e5fd080a2e21a06e9df48 | fa354323121eae934ae250e2813ce189efd6157d66727ee076610899bd36581e |
| android/tests/host_test_suites.json | 1514-1519 | 9c28450758ee1cb21f165fd67ed1ef3272358d7057962e1f6a959119e90e2639 | 08331b71eb4574adf6e42dd1351aaf35b52c5848fb059739a21777a8c32acfb6 |

## Remaining work

- Disc import owner and actual launcher/import pipeline consumers
- Verified dependency CMake/tool/manifest/updater contracts
- Native DPOG behavioral and registration boundaries
- Complete all16 consumer inventory, existing owners, prior publications and direct disposition
- Reconcile bodyless Play commit failure against complete0426 canonical publication
- Final report/snapshot, exact canonical replay and observed mounted Drive proof, then remaining broad queue/sweeps/current-head supplemental coverage and closure

No new finding, remediation, owner status, rating or inherited saving. No product/test/helper edit or execution, build, formatter, generator, network/device/media/archive/security/resource/allocation/fault probe, staging or commit. No canonical append or new Drive generation. Google Drive remains the mounted ledger backup destination; no remote cloud-sync claim. Goal active
