# GQI-0012: briefing command identity investigation, 2026-10-10

Status: proposed OPEN ordinary investigation from GQ2-CHUNK-0340. Diagnosis only; no confirmed defect, remediation ID, runtime reproduction or implementation.

## Source evidence

- net_udp_join_wait.c:41-112 publishes current.briefing or the constant 1 as UI generation. begin resets current and cancel, then publishes another active generation 1; end publishes inactive. cancel accepts only active plus matching generation. The source is absent at the review base and unchanged in lines from frozen head to current.
- reader_pump:153-166 consumes the cancel bit and moves Network_status to NETSTAT_MENU. Paired net_udp.c D1:7084-7114 and D2:7289-7319 begin and end waits around preparation/StartNewLevel/retry; a failed join returns to native menus. This is a real cancellation effect, not only a diagnostic field.
- CoopBriefingOverlayView.kt:29-48 cancels a press on changed generation or disabled action, but not on a changed enabled command kind. ACTION_UP:139-145 checks only pointer/generation/canLaunch/bounds. The callback sends just generation; android_input.c:1330-1349 first tries current join cancellation, then current briefing launch.
- Ordinary briefing host consumption is stronger: coop_briefing.c:640-700 drains the mutex mailbox, then coop_transition_policy.c:200-208 requires generation, host, BRIEFING phase and host readiness. Preserve this behavior. A join phase change within one join attempt can intentionally retain Cancel join and is not itself a bug.

## Missing evidence and admission gate

Establish a supported same-Activity ordering where an old visible pointer/accessibility action survives native end/begin or command-kind replacement, with no intervening UI update that clears it, and reaches a different wait or action. Native calls are synchronous on the UI caller; do not invent a queued asynchronous JNI request. Generation reuse alone establishes a weak identity contract, not that users can hit the schedule. Existing polling, view visibility, native-menu routing and teardown may prevent it and must be included.

Use maintained actual D1 and D2 join/briefing flows with a bounded scheduling barrier: record owner/phase/generation, displayed command, pointer down/up, native acceptance and engine consumption. Exercise wait A ending then B beginning before the next UI poll, same-briefing retry, command-kind replacement, inactive/changed-generation controls, ordinary valid cancel and valid host launch. Assert an old command never changes a new owner's state; a fresh command works once. No network/device/build/test/probe is run in the diagnosis tranche.

If reachable, admit a separate command-identity finding and carry an explicit native-owned operation token plus action kind through publication and request validation; consume only the matching live owner. Cancel gestures when that identity changes. Keep a process-lifetime monotonic token separate from remote briefing IDs, preserve native host/phase validation, and add paired actual-view/native acceptance. Do not move session, wire or native format policy to Kotlin or add persistence migration readers. If routing prevents the schedule, close the investigation with that causal evidence and retain the current boundary.

## Ownership

This is distinct from GQR-0170/BR-0044 JNI allocation/UTF publication, BR-0029 raw diagnostic snapshot ownership, BR-0245 SDL admission and GQI-0005 axis queue measurement. Those do not establish join command lifetime. Prior0121 retains mailbox/generation policy but does not demonstrate this cross-wait/action schedule. No new confirmed root or fix rank is proposed.
