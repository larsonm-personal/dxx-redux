# GQ2 chunk0346: Lobby membership and launch, 2026-10-10

Diagnosis and implementation plans only. Work serially, preserving concurrent edits. No product/test/helper edits, builds/tests/devices/network/probes/generators/formatters/staging/commits.

- [x] Read all48 assigned frozen hunks103-150, including removed and added source; verify current and enclosing scope
- [x] Trace actual membership, transfer, readiness and launch consumers and maintained fixtures
- [x] Reconcile existing finding ownership, simplification and inherited minimization
- [x] Publish canonical report/snapshot and independently audit before DONE

Starting from audited0345: GQ2 345DONE/282TODO/627;1160 terminal;272 fixes;12 numbered investigations. All five canonical hashes and HEAD062a4c0f467fedf956ba12f497a2e7842833846b revalidated. The previous goal turn made authoritative progress.

Assigned LobbyService.kt hunks103-150/new1269-2404. Whole3105-line current source matches the prior reviewed frozen/current identity e8a90a8454ad56498214f9d4416d4743aa48bd6addc489d59b63b37037e30fda. Prior enclosing reads alone do not mark this assigned chunk complete. Newly admitted GQF-0286/GQR-0272 owns manual retry-series ordering; avoid duplication while tracing this chunk's request/grant paths.


## Findings and current behavior

Existing BR-0490 and BR-0496 own membership admission and readiness through the start transition. This chunk adds current preparation/Activity-boundary evidence; it does not introduce a duplicate finding.

startGame checks count, connected/MATCH/ready state at2112-2123, captures players in PendingHostStart at2193 and emits PREPARE. confirmGameLaunch at2200-2208 checks only the pending info object, lobby identity, hosting and gameStarted; it does not recheck member count, readiness, connection, mission status or the captured roster. handleJoin/Leave/Ready and mission-status updates can change the live roster during asynchronous setup. A new member is accepted while pendingHostStart exists because handleJoin gates on gameStarted, which remains false until confirmation.

Known-member JOIN heartbeat replay now recovers lost PREPARE/START/CANCEL. Preserve this repair; do not repeat the historical claim that every late member necessarily never receives START. The remaining ordinary gap is that a changed/unready/disconnected roster can outlive the preparation eligibility check. An added member can later receive the pending packet through heartbeat even though absent from its captured recipients.

Actual consumer boundary: SetupActivity.kt:2595-2710 performs asynchronous process, save, file and mission preparation; awaitHostLaunch returns immediately for a host. At531-614, after further route-metadata handoff, startActivity(intent) runs at594 before confirmGameLaunch at599. Merely adding a rejection to confirmGameLaunch is too late to prevent that Activity launch.

Preserve BR-0342 correlated sender/session admission; the start handler already validates joined host/lobby and ordered launch attempts, and the helper rejects terminal rollback. JOIN_ACK still lacks a captured pending-join request owner; PLAYER_LIST/JOIN_REJECT/KICK do not receive a sender argument. No new authentication finding.

GQR-0272 already owns the manual transfer ordinal reset proven in0345. Assigned hunk121 contains the request/grant/status/timeout implementation; extend its coverage without another root. Preserve request UUID, sender/lobby/revision/attempt checks, compare-and-set publication and stale callback cancellation. Late mission resolver/enable publication still needs existing BR-0433/GQR-0102 lobby/source-generation acceptance.

Keep watchdog recovery and socket ownership, existing NSD32/peer64 admissions, heartbeat repairs, asynchronous save checks and shared native query/save formats. Existing BR-0486/0487/0488/0489 and GQR-0209/0170 acceptance is retained as reconciled in0345.

## Implementation continuation for BR-0490/0496

- Keep one serialized hosted state and explicit Waiting/Preparing/Committed transition in the existing LobbyService owner
- Bind preparation to one member/configuration generation and immutable launch info. Define pending-stage join policy explicitly: reject new membership or cancel/restart preparation; never silently append a member outside the admitted generation
- Serialize member add/remove, ready/match/connection changes, preparation cancellation and final admission through the same owner. Invalidate or explicitly reconcile the preparation when its admitted membership changes
- Add an owner-checked admission step before SetupActivity starts the host Activity, after long preparation/handoff. A stale or ineligible attempt must fail visibly without starting the Activity or emitting START
- Define the interval between final admission, Activity start success/failure and START publication so concurrent membership changes cannot reopen the race. Preserve the existing rule that a failed Activity start does not release clients
- Keep slow process, disk, metadata and save work outside the state monitor. Use a bounded existing-owner transition instead of holding the LobbyService lock through Android Activity calls or adding a second session framework
- Preserve current terminal ordering, lost-PREPARE handling, heartbeat retransmission, retry attempt identity, client wait cancellation and explicit return-to-lobby behavior. Coordinate authentication with BR-0342 and source identity with GQR-0102

Future acceptance must barrier actual service JOIN/LEAVE/READY/status updates against recipient capture, long setup preparation, final admission, Activity start, success callback and every control send. Cover new joins, unready/disconnected/removed members, fewer than two members, requirement changes, stop/rehost, duplicate start, cancellation, lost prepare/commit/cancel, reordered old packets and a newer launch attempt. Assert no rejected/stale attempt starts a host Activity, no failed Activity releases a client, and every admitted member belongs to the committed ready generation. Test UI enablement and direct service callers together. Use the real launcher and service/UDP path; helper-only tests do not establish this boundary.

LobbyLatencyChecks:128-210 seeds an already-ready roster and calls confirmGameLaunch directly. It tests prepare/fail/retry/commit, heartbeat recovery and client ordering, but not membership mutation during actual Activity handoff. LanLaunchPreparationTest checks packet ordering only. These fixtures were read, not executed. After implementation, run scoped quality, relevant JVM/service instrumentation, Android build and maintained multi-client integration, registering any new top-level automation.

## Simplification and inherited minimization

Simplify toward the existing lobby state owner with one eligibility/admission decision and explicit lifecycle, preserving the new two-phase launch protocol rather than layering independent flags or snapshot validators. Keep transfer retry identity with GQR-0272.

Assigned LobbyService.kt is absent from the1996 reference. Shared native save/query format authority remains appropriate; do not move engine format policy to Kotlin or broadly deduplicate inherited engines. NO_INHERITED_EFFECT, inherited saving0. Leading existing GQR-0170 REFERENCE56 (32/0/7/10/7). No new finding/remediation/investigation, rating or status change.

Source review complete:48 assigned hunks (+675/-66), whole3105 frozen/current identity and72 bound contexts, including56 exact audited0345 reuse selections. Final canonical publication and independent audit pending;346 remainsTODO until both gates pass.


## Chunk0346 canonical publication and independent audit complete

GQC-1174 ISSUES/GQD-1054 NO_INHERITED_EFFECT; GQR-0170 REFERENCE56 retained. Existing BR-0490/0496 continuation records the current pending-membership and pre-Activity admission gap: startActivity precedes confirmGameLaunch, so a post-launch rejection cannot prevent the host Activity. Preserve PREPARE/START/CANCEL, terminal ordering, known-member heartbeat replay and initial eligibility checks. GQR-0272 remains the sole manual transfer-series root. No new finding/fix/investigation/status/rating or inherited saving; no runtime acceptance.

Independent audit PASS2354 SHA87e502d752e9a78dc065fbe5717a77cd47af14c434d2ca778bdafe457962c613. Reconstructed all five canonical files exactly; preserved prior terminal/fix payloads, finding/fix/investigation definitions and the627-entry queue. Verified48 assigned hunks103-150 (+675/-66), whole3105 frozen/current equality, complete audited whole-source reuse plus fresh1250-2445 coverage,72 contexts,56 exact0345 reuse selections and related retry-plan provenance. Published canonical rows relocated by unique identical payloads; product diff unchanged between publication and audit.

Snapshot/checkpoint SHA9526aba59a2e84a8d9a301c3d183f3f86234281b9ca8385b6b99818e0e28b130; reportSHA07ffa342a1a1a953ba2519cb5038d6da5110dbdc4e12d36ffaba20b4ef0aad02; scope7be92a3b50aa209b447306bc04771d4138c3ca2f3b898eba432ad67666c4ff81. Snapshot plan hash describes its prepublication state; this audited handoff and checkbox closure follow the audit.

GQ2 now346DONE/281TODO/627 numbered;1161 terminal;272 fixes;12 numbered investigations. Remaining numbered scopes, supplements/sweeps/investigations/historical0116 audit debt and final live-head closure remain required. Diagnosis/docs only; no product/test/helper edits or execution. Goal active.

Next queue: GQ2-CHUNK-0347 | [ ] TODO | source | medium | authored-source | android/app/src/main/java/com/dxxredux/app/MainActivity.kt | diff hunks 1-85, new L8-L2345 | -
