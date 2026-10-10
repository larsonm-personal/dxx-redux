# GQ2 chunk0298: navigation repeat instrumentation, 2026-10-10

- [x] Read all277 assigned frozen lines and bind current/original/manifest scope
- [x] Trace caller/runner, production repeat timing/focus/lifecycle/slider interfaces
- [x] Reconcile existing owners and simplification/minimization/coverage limitations
- [x] Final report/scope, canonical publication and independent byte audit

## Assigned source checkpoint

Whole frozen NavigationRepeatChecks1-277 freshlyread in two untruncatedoutputs; currentsource lines equalfrozen, original1996absence verified. Exact evidence snapshot temp/general_cleanup_20261006/gq2-0298-source-checkpoint-20261010.json. No inherited hook attributed yet; productionrepeat and windowroot owners require contextbeforefinaldisposition.

Fixture replaces launcherContent with real NavigationRepeatRoot,20scrolling Compose rows,LabeledSlider and NavigationAlertDialog/NavigationDialog/NavigationDropdownMenu. It supplies synthetic gamepad KeyEvents and HAT MotionEvents through real launcher/window dispatch; explicit focus seed and100/350/600ms waits. Asserts initial single movement, hardware repeat suppression, heldnavigation/HAT/floatadjustment, no changes afterrelease, three dialogkinds repeat/uppermovement bound/release and closingdialog prevents a hold reaching launcher. MotionEvent recycledfinally. This testsproduction UI eventhandling without actualcontroller discovery/device disconnect/nativeengine input.

holdTime keeps nominaldelay+3interval+50 then bounded3s readywait; later selected<=7 is an upperduplicate-timer oracle. Scheduling/timing sensitivity requires productiontiming/runnernegativecontrols, not immediateflakedefect claim. HAT neutral checks row>=4 but doesnotcapture stable row aftersettle as key/slider cases do. Windowfocus assertioncomment is not standaloneproof launcherhold cancellation whiledialog remainsvisible; actual roots/timers need tracing. No overall helperfinally restores originalcontent/release on assertion failure; caller-ownedActivity/runnerforce-stop boundary remains to inspect. KeepmeaningfulrealCompose/slider/dialog oracles rather than consolidating justfor sharedonMain/touch boilerplate.

No new finding/status/rating/closure/runtimepass atcheckpoint. All277assignedread; gate2-4pending.0298TODO297DONE330TODO/627. No code/test/helperchanges/builds/devices/probes/staging/commits;goalactive.


## Repeat ownership, caller and runner checkpoint

Whole NavigationRepeat.kt, NavigationRepeatRoot.kt, NavigationDialogs.kt and ControllerKeyDispatch.kt read. Android static repeat timings use reflection with500/125ms fallbacks; polling direction tracks nextrepeat. Keydispatch captures accepted destination, ignores orphan/duplicate hardware repeats when scheduler owns ticks, identitychecks heldkey before/aftercallback, removes scheduledRunnable and sendsrelease through same destination. Whole ControllerKeyDispatchTest purefixture read: destinationchange/repeats/orphan, synthetic500/125schedule/release and releaseAll/gameplaynonrepeat oracles. No execution; synthetic scheduler doesnotcertify actual main-thread/window lifecycle.

Windowroot owns timer through view.postDelayed and CompositionLocal marker; device removalrelease, observerunregister, focusloss/dispose release, touchrelease, duplicatepreview suppression and guardedredispatch preserve perwindowownership. Alert/Dialog/Dropdown wrappers markwindowowner; repeatVerticalDpadFocus133-219 keeps traversalrules while usingroot keys and onlyfallbackcoroutine whenrootabsent. Standalonefallback stillcancelsonmatchingkey/dispose, not windowfocus; currentroot improvement qualifies historicalBR0451 but doesnotprove everycallsite usesroot or closeowner. FullBR0451section16576-16591 reread: historicalsyntheticrepeat window/lifecycle owner remains OPEN.

SliderNavigationChecks1-124 boundedcaller read: launcher/content,configbackups/orientation, repeated D1/D2 settingsviews and callsNavigationRepeatChecks onregularslider path. Finallyrestoresconfigs/orientation but doesnotfinishActivity; outerPowerShellfinally force-stops selectedpackage afterdiagnostics. Configrestoreexception may skiporientationreset, an existingfixturetransaction/lifetime concern, notnewroot admitted here. Navigationhelper itself has nofinallyrelease/contentrestore. Parenthosttimeout killsPowerShell withoutguaranteedAndroidexecutor teardown; coordinateBR0543. RecoveryInstrumentationbranch/single-target install proof belongs prior0296/0297auditedcontext, not new full instrumentation credit.

SetupActivity2345-2479 wholehysteresis/synthetictransitions/motion routing and2502-2565 keydispatch read. HATneutral emitskeyrelease throughsame selectedtarget; nohardwaredriver actualdelivery. Controllerpicker remap/status and activitydispatch delegatesnormalkeys toCompose. LabeledSlider3519-3536 is productionMaterialSlider with tvFocusBorder; fixture usesrealfloatcontrol, no inherited/nativehook. Nativegame/menu input remains separateowners, avoid broadinput abstraction extraction.

Whole slider runner test_slider_navigation.ps1 read: emulator-only, selectedslider versusgraphicscapabilities, instrumentation300s, PASS/FAIL/crash oracle, diagnostics/poststopfinally/envrestore. Aggregate timeoutoverride300s andneedsInstrumentation list1753-1768 freshlyread; test_slider_navigation absent, sameGQF0280/GQR0266 missingcapabilityroot for isolatedselection. Existing300s parent versus300s instrumentation plus setup/cleanup matchesexistingdeadlineownerBR0658 coordination. Add selectedslider and capabilities-switch tosameprovisioningplan atfinalcanonicalextension; no newfinding/rating. Fullroot/JVMsource doesnotestablish everyfocusloss/Home/activityrecreate/device-removal acceptance.

RetainrealCompose/slider/dialogbehaviorchecks, puremocktimingfixture and distinctpolling/window/eventowners. Optionalcleanup improve held-key/neutral/release assertions throughguardedfinally; uppermovementbound sensitivity needsactualcontrolledschedulingnegativecase beforeflakedefect. Provisioningextension andexistinglifecycle/transactionowners reconciliation/finalminimization/report/publicationpending.0298TODO297DONE330TODO. No code/test/helperchanges or build/device/probe/execution,goalactive.

```json
{
  "checkpoint": "0298 repeat owners and runner consumers",
  "diagnosis_only": true,
  "fresh_context": [
    {
      "lines": 61,
      "path": "android/app/src/main/java/com/dxxredux/app/NavigationRepeat.kt",
      "raw_sha256": "8e109c4bf732c07a268dd554fc917d6750b8ec15873f2346bf67e29f72177915",
      "whole_file": true
    },
    {
      "lines": 149,
      "path": "android/app/src/main/java/com/dxxredux/app/NavigationRepeatRoot.kt",
      "raw_sha256": "e03b607521e14d1c87f1b700a0ff5672521a17bcf26814297323dce5f42b603f",
      "whole_file": true
    },
    {
      "lines": 89,
      "path": "android/app/src/main/java/com/dxxredux/app/NavigationDialogs.kt",
      "raw_sha256": "b3af0f83169730d6bf2a8f79c60d3caf3293d794a515274948724db1ca77d3d4",
      "whole_file": true
    },
    {
      "lines": 62,
      "path": "android/app/src/main/java/com/dxxredux/app/ControllerKeyDispatch.kt",
      "raw_sha256": "1957ab9a750e6078b1bd31bb8126822b44a8b12a536bd2954d473c0fc09763e4",
      "whole_file": true
    },
    {
      "lines": 111,
      "path": "android/app/src/test/java/com/dxxredux/app/ControllerKeyDispatchTest.kt",
      "raw_sha256": "039cd6732f051da7c8135b26c0b838814e8e7988e324dc5b39bc7e7497438836",
      "whole_file": true
    },
    {
      "lines": 25,
      "path": "android/tests/test_slider_navigation.ps1",
      "raw_sha256": "dec01415d6037599c9cb89ff764f36e3dfc8fd8d6d6387c3a0e0edd37a8a5552",
      "whole_file": true
    },
    {
      "end": 124,
      "path": "android/app/src/androidTest/java/com/dxxredux/app/SliderNavigationChecks.kt",
      "range_lf_sha256": "27bce8e55ddd6c56bb59821930fe3f0839006468a6760e7f3a4fbd6f539c12ee",
      "raw_sha256": "f18d30c0ff455dc1fe1cf65b1ffc1a8c273a8b97ed5d6f586dd5e8dd5e451b09",
      "start": 1
    },
    {
      "end": 219,
      "path": "android/app/src/main/java/com/dxxredux/app/DpadFocusUtils.kt",
      "range_lf_sha256": "87cf9cb4225dcf5b33f68c44de64d9292f16b732eb2bea35f7c817a4db05f62a",
      "raw_sha256": "d7c4585f6b5bce0515a4bdce3d44b7635f0ea3982937735ebca953694ac10f92",
      "start": 133
    },
    {
      "end": 2479,
      "path": "android/app/src/main/java/com/dxxredux/app/SetupActivity.kt",
      "range_lf_sha256": "a71356591276896c81bb12a699cd0fde7d312b9786a6d23d1b7adc29b76f9944",
      "raw_sha256": "553b24ed39dd6b423b7663176b1604575dfaee80c625353e7c0316a4291fd097",
      "start": 2345
    },
    {
      "end": 2565,
      "path": "android/app/src/main/java/com/dxxredux/app/SetupActivity.kt",
      "range_lf_sha256": "72376c365ee674b2e14563a09b83b6ba1690940b551e318e17e37f97267fc276",
      "raw_sha256": "553b24ed39dd6b423b7663176b1604575dfaee80c625353e7c0316a4291fd097",
      "start": 2502
    },
    {
      "end": 3536,
      "path": "android/app/src/main/java/com/dxxredux/app/TouchEditorPage.kt",
      "range_lf_sha256": "0333b8926030ae4235f095c0131d9978542c00fd938b09c3a46716e737d49b12",
      "raw_sha256": "f639e8cb8d6eb4cd1282f3c140ac19dd3db8b55242e5fad1fcd45c55ad05e52d",
      "start": 3519
    },
    {
      "end": 16591,
      "path": "android/ai tool plans/code management/branch_adversarial_review_ledger.md",
      "range_lf_sha256": "45348fd6fe1e7f1592b170b5833bb9e757cd94024ef6dfb99b8adcfc6388446e",
      "raw_sha256": "5d3b3546092545d246084591a968933f966e1676fc7c47e79163cfafe142b5fa",
      "start": 16576
    },
    {
      "end": 469,
      "path": "android/run_all_tests.ps1",
      "range_lf_sha256": "09bd7d317d00ebc3588aed4471fdd405a6df07eeda1857faca085b64f681f4b9",
      "raw_sha256": "de93ee1524285ad44729b0469c2479d428d51f46910bda5fa1e2b6721bd955cb",
      "start": 467
    },
    {
      "end": 1768,
      "path": "android/run_all_tests.ps1",
      "range_lf_sha256": "8fcfa06b7ab9d3175c14c4836d36c00d7b8a1daaa5d99436e615323abbfe42cf",
      "raw_sha256": "de93ee1524285ad44729b0469c2479d428d51f46910bda5fa1e2b6721bd955cb",
      "start": 1753
    }
  ],
  "product_diff_sha256": "95d3e48b8345ea8b6b9a548cfe726868b45261f5d5300009c3ed56cbfbfdbc5c",
  "runtime_acceptance": false
}
```


## Final disposition

All277assigned frozen/current lines covered. Whole repeat timing/keydispatch/windowroot/dialog wrappers and maintained pure scheduler fixture, bounded launchermotion/key/focusmodifier/Materialfloat slider/caller and runner sources support RETAIN. Expected/applied inherited reduction0: authored Android instrumentation dispatches Android view/Compose featureowners and changes no inherited D1/D2 hook/declaration/format. Native engine input remains separate existingowner; replacing different polling/window/keyboard policies with one abstraction adds coupling without original-file payoff.

Extend existing GQF0280/GQR0266 for selected test_slider_navigation instrumentation provisioning (regularslider/navigation and CapabilitiesOnly variants), same45MEDIUM23/0/2/10/10REFERENCE. No new finding/fix/investigation/status/rating. Existing BR0451 remainsOPEN with historical claim qualified by current windowroot focus/dispose/touch/device removal cancellation; standalonefallback and everycaller/lifecycle/Home/recreation/hardware schedule are not certified. No runtimeclosure or newlyconfirmed stalehold/flake.

Retain real Compose rows/float slider/threewindowtypes, hardwarerepeat suppression, heldkey release and dismissedwindow leakage oracles. Local simplification/futurevalidation: helperfinally releases outstanding keys/HAT and restores ownedcontent; caller config/orientation restoration survives earlier cleanup error; stable-HAT-neutral and direct focusloss/background/disconnect negativecontrols complement synthetic scheduler. Timing upperbound<=7 needs deterministic schedule context if revised; do not weaken duplicate-timer detection to make a slow test pass. ExistingBR0658 parent/child totaldeadline andBR0543 guaranteed ownedexecutor shutdown remain distinct root owners.

Sourcefixture reads and mocked scheduler assertions supply no current test pass, Android hardware acceptance, native delivery or allwindow equivalence. FinalGQC1126/GQD1006 proposed subjectuniqueness. Gates1-3complete, report/snapshot/publication/auditpending.0298TODO297DONE330TODO. Remainingchunks/supplements/sweeps/auditdebt/finalheadreconciliationOPEN. No code/test/helperchanges or execution;goalactive.


## Chunk0298 published and independently audited

GQ2-CHUNK-0298 DONE/GQC1126 ISSUES/GQD1006 NO_INHERITED_EFFECT; RETAIN. Existing GQF0280/GQR0266 selected slider/capability instrumentation provisioning extension45REFERENCE; no new finding/fix/investigation/rating/closure/runtime acceptance/inherited saving. BR0451 focus/lifecycle and BR0658 deadline/BR0543 executor owners retained. ReportSHA 7d40e4b01faaeb25dff34b19a2b04f4c828fe06aa3377f82da066d21d47178a6; scope 0770ee43340905b2bea201977693eb32999c55fae2602c5bf820f01cf43f1f29; snapshotSHA 5e81a2ea0e7171bc49ac5c16bc7b54c69bcea5c6cb2a3c40f8cc24cfdd8f38b5; independent publication audit PASS80 SHA 2645a55dd5fb058648b0d6ae9e82d993c2bb8e2ab50bd185cbc79153408f5f48. Canonical publication and source bindings verified by saved audit; product diff unchanged, scoped docs check PASS.

All0298 gates DONE. Canonical298DONE329TODO/627;1113terminal266fix11investigations. Next0299 RecoveryInstrumentation.kt, assigned frozen lines1-639; prior bounded helper reads do not establish whole-file coverage. Remaining chunks, supplemental/current-delta reviews, sweeps, historical0116 raw-byte audit debt and final-head reconciliation remain OPEN. No product/test/helper changes or builds/devices/probes/staging/commits. Overall diagnosis goal active.
