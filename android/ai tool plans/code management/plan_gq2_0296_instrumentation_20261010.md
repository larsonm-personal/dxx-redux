# GQ2 chunk 0296: Android instrumentation diagnosis, 2026-10-10

- [x] Read all 683 assigned frozen lines and compare current/original/manifest bindings
- [x] Trace instrumentation selection, production consumers and runner lifetime/cleanup
- [x] Reconcile prior owners and assess simplification, inherited impact and fixture limitations
- [x] Final report/snapshot, canonical publication and independent exact-byte audit

## Assigned source checkpoint

Whole frozen ControllerOverlayChecks.kt 1-423, CoopSessionChecks.kt 1-96 and EngineQueryChecks.kt 1-164 read with untruncated bounded output. All current source lines equal frozen; all three authored files absent in 1996 original. Exact raw, line and manifest bindings saved in temp/general_cleanup_20261006/gq2-0296-source-checkpoint-20261010.json. No live source edits.

ControllerOverlayChecks exercises real TouchOverlayView dispatch, bitmap drawing, controller menu keys and InputMixer edge capture for D1/D2. Checks include repeatable bomb activation, D2 held/cancel release, persistent controller menus versus touch dismissal, saved layout file byte preservation, default controller coverage filtering, connection releasing held fire/axes/toggle/double-tap, and preserving Guide/More menus on disconnect. Controller availability and output callbacks are synthetic; this does not certify actual hardware discovery, native input delivery or full activity lifecycle. Private field/method reflection couples the fixture to implementation. Main-thread work wrapped through runOnMainSync; bitmap and MotionEvent cleanup use finally. View deactivation occurs on success paths only; failure cleanup and scheduled callback lifetime require production and runner tracing. Fixed sleeps (100/300 ms) imply timing sensitivity, not a demonstrated flake.

CoopSessionChecks exercises singleton LobbyService host adoption and host-to-client transitions, JOIN_ACK/PLAYER_LIST packet dispatch through reflection, hosted port/level/difficulty/requirement state, no migration relaunch and in-game join launch metadata. Sender address and launch inputs are constructed. Discovery stop in finally is present. This does not prove actual packet source authority, concurrent role-change barriers, native migration or real peers. Reconcile current service behavior and existing authority/launch owners before any finding or closure.

EngineQueryChecks captures a real engine UDP reply then replays it through loopback sockets to test loss/truncation retries, wrong source port, version denial, silence, launcher-lobby preference, engine fallback and stopping pending discovery. Actual engine precondition is externally supplied. Success under three seconds is checked against a ten-second timeout; this is not a universal runtime bound. Two socket acquisitions/binds precede try/finally, and responder cancellation is awaited before subsequent cleanup. Acquisition failure, responder exception and assertion failure cleanup need inspection; no independent root admitted yet. Test reads both codecs but only chosen game replies per invocation, so runner must invoke both games for paired credit.

## Bounded caller context

RecoveryInstrumentation.onCreate57-73 and branches93-101/121-129/139-143 freshly read after a combined output was truncated. Only these bounded ranges receive read credit. They select suites and report success after each check returns. Full onStart failure path, runner registration/timeouts/serial execution, and real production overlay/lobby/query functions remain pending. No test invocation or runtime PASS is inferred from result strings.

## Minimization and resume

These are authored instrumentation helpers; no inherited reduction identified from their declarations. Native codec/query and input hooks must be traced before final NO_INHERITED_EFFECT/RETAIN disposition. Keep meaningful behavioral checks; do not merge different suites merely to remove shared instrumentation parameters/reflection boilerplate. Pending cleanup/timing questions are hypotheses until deduplicated and tied to supported failure paths.

0296 remains TODO. Queue 295 DONE / 332 TODO of 627. Prior0154 PASS66; broader supplements/sweeps/audit debt/final-head reconciliation remain open. No code/test/helper changes, build/device activity, probes, staging or commits. Goal active.


## Runner and production lifetime checkpoint

Whole test_controller_overlay.ps1, test_coop_session.ps1 and test_manual_ip_engine.ps1 read. Controller runner isolates selected package, pre/post force-stops it and gives instrumentation90s; coop gives instrumentation the Adb30s default and stops in finally after diagnostics. Both restore ANDROID_SERIAL after post-stop, but setup occurs before try. A post-cleanup command exception could bypass serial restoration; Adb currently returns null on caught command failures so do not infer every failed device cleanup throws. Native exit-status interpretation and full test-target initialization not certified.

Whole EngineQuery.kt160lines and jni_engine_query.cpp71lines read. Probe launches separate game-specific IO queries and cancels both after first success; query uses connected UDP socket,50ms receive slices,250ms retries, cancellation checks, native decoder and use-based close. Decoder remains in native code compiled against canonical per-game protocol headers and rejects wrong packet kinds/version/prefix/string/count/status. JNI exception/allocation/error lifetime limits belong to existing native owners; source read does not prove fault safety. CMake source registration515-535 read. Only D1 request-dispatch4501-4511 and an initially mislocated D2 context were inspected; paired D2 dispatch and frozen/inherited attribution still pending. Do not broaden test-helper review into extracting the native wire format to Kotlin.

Current LobbyService stopDiscovery428-509 invalidates manual attempt, cancels preparation/transport/discovery state and closes socket. adoptMigratedHost610-634, stopHosting638-660, joinLobby663-710, tryJoinLobbyByIp753-879, joinDiscoveredLobby/readiness/emit2446-2501 freshly read. Current generation/state rechecks surround engine await and launch; these narrow checks do not prove every async barrier. Coop test requires readiness data; no successful device run inferred. TouchOverlayView isActive239-251 deactivates/releases/stops mouse drain; whole setLayout1038-1077 releases latched/held controls and conditionally preserves menus. RecoveryInstrumentation catch151-169 reports FAIL but supplies no general helper-specific view/socket teardown; wrapper force-stop mitigates fixture residue but does not guarantee safe in-process cleanup.

Manual-IP runner has 60s host readiness,120s instrumentation,60s join,45s disconnect and60s late-join waits plus setup and bounded commands. Aggregate discovery743-799 supplies no manual-IP timeout override or Game arguments; Invoke-SingleTest1824-1845 uses default120s when override absent and1900-1925 kills child on expiry. Full timeout dictionary absence confirmed by scoped search, with402-457 selected context read (not entire dictionary read credit). This is a concrete mismatch in declared budgets, not observed timing failure. Runner defaults D2, so aggregate does not establish paired D1 coverage; direct -Game d1 remains supported. Test-suite registration hits located but not fully read; aggregate instrumentation build/install coverage for two-emulator manual-IP suite remains to trace.

BR-0163 full current owner12834-12850 reread: LAN fixture cleanup can skip later resources on an earlier cleanup exception. BR-0658 full current owner19730-19745 reread: authoritative child/parent total deadline mismatch already owned for multiplayer test. Coordinate any manual-IP budget extension with that owner; no canonical extension/new root/rating is admitted at this checkpoint. Engine fixture acquisitions before try and child cancellation-before-close remain local cleanup simplification candidates; supported failure-path and dedup evidence pending. Do not claim a current product leak from a test-only synthetic view or process whose wrapper force-stops it.

Source-only checkpoint; gate2 remains pending final paired/context/runner admission.0296TODO,295DONE332TODO. No product edits or tests. Exact fresh context bindings follow.

```json
{
  "checkpoint": "0296 runner and production lifetime contexts",
  "diagnosis_only": true,
  "fresh_context": [
    {
      "end": 169,
      "path": "android/app/src/androidTest/java/com/dxxredux/app/RecoveryInstrumentation.kt",
      "range_lf_sha256": "45a7324b49d6525748fa650269241294b7db305671961a76f70ec32c792d0ddf",
      "raw_sha256": "2f1c3dd6e37d6264fc7ea48c4cb5f87de4acfcc5a89e79eadca91b8878f1ea1a",
      "start": 151
    },
    {
      "end": 251,
      "path": "android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt",
      "range_lf_sha256": "da537d64ec69dbcc9cb85ee49a57cf957f32eb48433ccee9fed78fd76a9ba17e",
      "raw_sha256": "3a8a3c2e44e2a5372eab603e2d79de35171d58e8a6abd8755b3eb4e2ae59c6ee",
      "start": 239
    },
    {
      "end": 1077,
      "path": "android/app/src/main/java/com/dxxredux/app/TouchOverlayView.kt",
      "range_lf_sha256": "5636b028421fa1fdc8d8e8457eb9358d2d0ce170efbdfef7edc59629664ee9de",
      "raw_sha256": "3a8a3c2e44e2a5372eab603e2d79de35171d58e8a6abd8755b3eb4e2ae59c6ee",
      "start": 1038
    },
    {
      "end": 98,
      "path": "android/helpers/test_helpers.ps1",
      "range_lf_sha256": "32262f404b25ee53c1b7ae69b04338a3b65745a7bee048e73caa7ae4747c29c5",
      "raw_sha256": "84ed2a0e0ed751248662f5adb6d4378d3240b3162e3af261faa306b5fa13420e",
      "start": 47
    },
    {
      "end": 572,
      "path": "android/helpers/test_helpers.ps1",
      "range_lf_sha256": "e50d57f29c7c1247aec067acd3fb5ccbca9f2fc04d5dc63f2cc653b8069ec636",
      "raw_sha256": "84ed2a0e0ed751248662f5adb6d4378d3240b3162e3af261faa306b5fa13420e",
      "start": 537
    },
    {
      "end": 509,
      "path": "android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt",
      "range_lf_sha256": "340ea5ebf55ae6446c288d3a88cfb19b9879f9fc8300b21856ec77ebd2ae0252",
      "raw_sha256": "e8a90a8454ad56498214f9d4416d4743aa48bd6addc489d59b63b37037e30fda",
      "start": 428
    },
    {
      "end": 634,
      "path": "android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt",
      "range_lf_sha256": "351cd8c402346907d68baa97d5922feef419301c2136357edef021bfd2834bfa",
      "raw_sha256": "e8a90a8454ad56498214f9d4416d4743aa48bd6addc489d59b63b37037e30fda",
      "start": 610
    },
    {
      "end": 710,
      "path": "android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt",
      "range_lf_sha256": "a527cf0b10e49ef11a8dd21a567edc8fde8fd49ee36cd63dd6f73a38b4355621",
      "raw_sha256": "e8a90a8454ad56498214f9d4416d4743aa48bd6addc489d59b63b37037e30fda",
      "start": 638
    },
    {
      "end": 879,
      "path": "android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt",
      "range_lf_sha256": "51263a86244966f1b61ebf7045d56362f562a274e1c850c913eedfb3bcd74515",
      "raw_sha256": "e8a90a8454ad56498214f9d4416d4743aa48bd6addc489d59b63b37037e30fda",
      "start": 753
    },
    {
      "end": 2501,
      "path": "android/app/src/main/java/com/dxxredux/app/lobby/LobbyService.kt",
      "range_lf_sha256": "935a991ce74bd94e5fff481eff8986f2726bf5fda145386e0eddb497f326230a",
      "raw_sha256": "e8a90a8454ad56498214f9d4416d4743aa48bd6addc489d59b63b37037e30fda",
      "start": 2446
    },
    {
      "end": 457,
      "path": "android/run_all_tests.ps1",
      "range_lf_sha256": "4fbdf408f1783b8d02f843d4bf01ad3fd8771ff1599ed7faf48367b67ab63ec0",
      "raw_sha256": "de93ee1524285ad44729b0469c2479d428d51f46910bda5fa1e2b6721bd955cb",
      "start": 402
    },
    {
      "end": 799,
      "path": "android/run_all_tests.ps1",
      "range_lf_sha256": "7161e3fc872b803a83b4bb80ebe4bdad3f4b71446860c3e2d346f0bd3b937c19",
      "raw_sha256": "de93ee1524285ad44729b0469c2479d428d51f46910bda5fa1e2b6721bd955cb",
      "start": 743
    },
    {
      "end": 1845,
      "path": "android/run_all_tests.ps1",
      "range_lf_sha256": "8cb96455262241e0e13311a018acfab30fbc3dbeef4460e03a50db5580790e33",
      "raw_sha256": "de93ee1524285ad44729b0469c2479d428d51f46910bda5fa1e2b6721bd955cb",
      "start": 1824
    },
    {
      "end": 1872,
      "path": "android/run_all_tests.ps1",
      "range_lf_sha256": "e645b820a373044dec8175e7bf8128159982851ceba1cd5b979e08dc422c9b08",
      "raw_sha256": "de93ee1524285ad44729b0469c2479d428d51f46910bda5fa1e2b6721bd955cb",
      "start": 1846
    },
    {
      "end": 1925,
      "path": "android/run_all_tests.ps1",
      "range_lf_sha256": "8f4205119ae22ddcf66b9c7560b9e22fc1f3957b57348af6a69e783578d8d62d",
      "raw_sha256": "de93ee1524285ad44729b0469c2479d428d51f46910bda5fa1e2b6721bd955cb",
      "start": 1900
    },
    {
      "end": 535,
      "path": "android/app/src/main/cpp/CMakeLists.txt",
      "range_lf_sha256": "600dc7e69884fb59de65745e2b52aec7af958f7158743c430f8212721da9592a",
      "raw_sha256": "f3e277840219058ce27a476d4e80133e90bb65b445f0430eb39d215f74945f44",
      "start": 515
    },
    {
      "end": 4511,
      "path": "d1/main/net_udp.c",
      "range_lf_sha256": "b3e8bdaf1b0817d7ef07cefd08c9006e3016260f854dd32a07c71716487c5989",
      "raw_sha256": "a1727da218f7f5b302a240680c2b4100ce607e18343b16d96e4248a14fb33309",
      "start": 4501
    },
    {
      "end": 12850,
      "path": "android/ai tool plans/code management/branch_adversarial_review_ledger.md",
      "range_lf_sha256": "bb15b5e76749b8dd3a9c5b49338bf210b44f968ca77610f46e76cc4e0930be97",
      "raw_sha256": "5d3b3546092545d246084591a968933f966e1676fc7c47e79163cfafe142b5fa",
      "start": 12834
    },
    {
      "end": 19745,
      "path": "android/ai tool plans/code management/branch_adversarial_review_ledger.md",
      "range_lf_sha256": "fbcd89e9c2fdeced792958c8b4bbabea390592590b5565903e5a30a4ba7e7ec8",
      "raw_sha256": "5d3b3546092545d246084591a968933f966e1676fc7c47e79163cfafe142b5fa",
      "start": 19730
    },
    {
      "lines": 24,
      "path": "android/tests/test_controller_overlay.ps1",
      "raw_sha256": "b40fe3135b3d2a8277deab52005a102fefb472c48bd99d504739485e58cab7be",
      "whole_file": true
    },
    {
      "lines": 21,
      "path": "android/tests/test_coop_session.ps1",
      "raw_sha256": "1d5c59d4816333cc224849dc9de5975201781fe4f0e413a173e341de4cddaf3a",
      "whole_file": true
    },
    {
      "lines": 61,
      "path": "android/tests/test_manual_ip_engine.ps1",
      "raw_sha256": "465398d9ff452f84c103136f665de8e0a66ada793d4952a17d3e742ac34ec7b9",
      "whole_file": true
    },
    {
      "lines": 160,
      "path": "android/app/src/main/java/com/dxxredux/app/lobby/EngineQuery.kt",
      "raw_sha256": "ccaa70d29458a6d853e5a8bc96120f9de58b4eaf8b740e49b9cc2a6e2ad0a5f3",
      "whole_file": true
    },
    {
      "lines": 71,
      "path": "android/app/src/main/cpp/jni_engine_query.cpp",
      "raw_sha256": "7cffa9f46f8861291357c9431bf0477aaa8c5ec1f4f270f33d3943abfd2409dc",
      "whole_file": true
    }
  ],
  "new_findings_admitted": false,
  "product_diff_sha256": "95d3e48b8345ea8b6b9a548cfe726868b45261f5d5300009c3ed56cbfbfdbc5c",
  "runtime_acceptance": false
}
```


## Paired native dispatch and instrumentation provisioning checkpoint

D2 request dispatch4637-4647 freshly reread without truncated output. Paired D1/D2 dispatch calls native request/version/info owners; Android instrumentation replay should preserve native per-game headers and codec rather than copying formats into test/Kotlin policy. These tests add no direct inherited hook; complete frozen/original dependent attribution remains final gate3 work, not a claim of whole networking minimization.

Aggregate preflight1559-1605 and build1751-1768 freshly reread after truncated combined output. needsInstrumentation is derived only from a selected single-emulator name list; manual-IP dual-emulator test is absent. Even when the flag is true because another test is selected, preflight installs AndroidTest APK only on primary preflightEmu1. Tier4 block2200-2261 provisions app/data on secondary but never installs instrumentation there. Whole Install-AppAndData704-716 reads install main APK and standard data only; nested Install-ApkOnDevice body remains to inspect before final admission. Manual-IP always runs RecoveryInstrumentation on emulator-5556. Thus the apparent provisioning gap is not merely missing a name in the single-emulator list: the secondary target needs a current matching test APK as well. Fresh-device absence or stale test APK mismatch has not been reproduced; no device work in this tranche. Ordinary test selection/success on a reused secondary must not count as proof of complete aggregate provisioning.

BR-0543 bounded additional per-app lifecycle evidence18012-18015 reread untruncated. It explicitly recognizes parent PowerShell termination does not stop a separately running Android executor. Do not claim a full BR-0543 owner read from the earlier truncated fullsection output. This supports coordinating later manual-IP timeout/cleanup validation with existing BR-0658/BR-0543, without asserting historical cleanup gaps are universally current.

Next: inspect nested APK installer and test catalog/time budget definitions, bind complete dictionary absence and paired native original attribution, reconcile provisioning root against existing current/done owners, then finalize report and canonical records. No new root/extension/status/rating admitted yet. Gate2/3 pending;0296TODO,295DONE332TODO. Source/product unchanged; no code/test/helper edits or execution.

```json
{
  "checkpoint": "0296 paired dispatch and secondary instrumentation provisioning",
  "diagnosis_only": true,
  "fresh_context": [
    {
      "end": 4647,
      "path": "d2/main/net_udp.c",
      "range_lf_sha256": "b3e8bdaf1b0817d7ef07cefd08c9006e3016260f854dd32a07c71716487c5989",
      "raw_sha256": "44f78c1444244ccc517d4466b54bfb2bc3cbac777bd4f0fae56727202f166f07",
      "start": 4637
    },
    {
      "end": 1605,
      "path": "android/run_all_tests.ps1",
      "range_lf_sha256": "38832f209f2301cd4033e5d9142118e3b0bc0a89de880078c855d80131278b90",
      "raw_sha256": "de93ee1524285ad44729b0469c2479d428d51f46910bda5fa1e2b6721bd955cb",
      "start": 1559
    },
    {
      "end": 1768,
      "path": "android/run_all_tests.ps1",
      "range_lf_sha256": "5504a4c6ac539cec41382e3643e6b39df391163436ec3d3115c9c6f1a7368681",
      "raw_sha256": "de93ee1524285ad44729b0469c2479d428d51f46910bda5fa1e2b6721bd955cb",
      "start": 1751
    },
    {
      "end": 2261,
      "path": "android/run_all_tests.ps1",
      "range_lf_sha256": "131a6dce2b22f07409a3292fffc4e3b910969b1f8fda6acabad8c8db984fd799",
      "raw_sha256": "de93ee1524285ad44729b0469c2479d428d51f46910bda5fa1e2b6721bd955cb",
      "start": 2200
    },
    {
      "end": 716,
      "path": "android/helpers/test_helpers.ps1",
      "range_lf_sha256": "b3f080d4f8e354e9dfc69f11b01c696a8a5cbb81e896fa162bed6a73c933d958",
      "raw_sha256": "84ed2a0e0ed751248662f5adb6d4378d3240b3162e3af261faa306b5fa13420e",
      "start": 704
    },
    {
      "end": 18015,
      "path": "android/ai tool plans/code management/branch_adversarial_review_ledger.md",
      "range_lf_sha256": "0e7068876fed73ba976b745cc197b0169d329eebc8fd246e22b7fdb7e92df79f",
      "raw_sha256": "5d3b3546092545d246084591a968933f966e1676fc7c47e79163cfafe142b5fa",
      "start": 18012
    }
  ],
  "product_diff_sha256": "95d3e48b8345ea8b6b9a548cfe726868b45261f5d5300009c3ed56cbfbfdbc5c",
  "runtime_acceptance": false
}
```


## Final review decisions

Nested Install-ApkOnDevice2808-2834 whole function freshly read untruncated: main app APK only. All timeout dictionary402-492 read; manual-IP override absent. New GQF-0280/GQR-0266 provisioning root proposed with source-supported fresh-secondary/stale-test dependency trigger. Existing BR-0658 deadline and BR-0543 executor lifetime remain reference owners; no runtime failure, owner closure/rerating or duplicated deadline root. Separate new provisioning score45 MEDIUM23/0/2/10/10. Later parent/child deadline and explicit D1 selection coordination in new implementation plan do not close historical owners.

Diff-minimization assessment: RETAIN / NO_INHERITED_EFFECT. Three authored instrumentation helpers, native JNI codec and Android CMake/test registration require no additional inherited declaration/hook for the fixture. Current paired game-info request dispatch uses existing canonical engine protocol owners. Entire inherited dependency path metrics at original/frozen are d1/net_udp.c +2427/-622, d2/net_udp.c +2622/-665, d1/net_udp.h +93/-7, d2/net_udp.h +95/-7. These whole-file totals include many unrelated networking features and are not removable fixture cost. JNI codec is authored +71/-0. Existing paired packet diagnostic extraction GQF-0278/GQR-0264 already owns its exact166line paired diagnostic block and explicitly retains native game-info owner. Preserve private packet layout/version/reconnect-auth constants; no wire-format movement to Kotlin or broad network extraction justified here. Expected/applied inherited saving0. Actual producer wire/layout parity and JNI allocation fault acceptance remain existing owners, not certified by source fixtures.

Controller tests simulate coverage/hardware and callbacks; Coop tests bypass real receive through reflection; EngineQuery tests exercise actual chosen-game captured bytes but full paired runtime requires runner -Game d1 and -Game d2. Synthetic view failure cleanup, socket/job acquisition cleanup and fixed sleep replacement are localized future simplification candidates with zero inherited payoff, not proof of product leak/flake. Keep distinct suites and genuine behavioral oracles. Full firmware/packet authenticity, concurrency/reordered peers, private native structures, merged APK/device behavior and allocation/CheckJNI acceptance not exercised.

Gates1-3 complete. Final report and canonical publication/audit pending; proposedGQC1124/GQD1004 plusGQF0280/GQR0266 require uniqueness before writes.0296TODO295DONE332TODO. No source/test/helper changes or execution; goalactive.


## Chunk0296 published and independently audited

GQ2-CHUNK-0296 DONE / GQC1124 ISSUES / GQD1004 NO_INHERITED_EFFECT; RETAIN. NewGQF0280/GQR0266 instrumentation target provisioning45MEDIUM23/0/2/10/10 admitted source-only. ExistingBR0658deadline/BR0543executor lifetime and native/JNI/network owners preserved; no implementation/runtimepass/closure/inheritedsaving. ReportSHA 3203eac6963b3cd0ce8415e08eabf80e13ace9cc5fe7423bdecf550ca0adf483; scope d274825f4a2bfb4d8617a07e82f279a1c3fbf03819dac2ec372a600563c2a0d2; snapshotSHA 7e8cc3ee56f1e3fb35b7c80040e8970efd37244f3de3e85b7059a85923c28165; independentauditPASS140 SHA d6277f4197ac128f8f71986420268793ece54f52d80ad306af8ab249d061cd0a. Exact allowedcanonicalbytes/import, oldsemanticranks/owners/source/current/range/manifest/originalabsence/prioraudit/product bindings verified.

All0296gatesDONE. Canonical296DONE331TODO/627;1111terminal266fix11investigations. Next0297 fourinstrumentationhelpers736assignedlines. Remainingchunks/supplements/sweeps/historical0116auditdebt/finalheadreconciliation remainOPEN. No code/test/helper changes/builds/devices/probes/staging/commits; overallgoalactive.
