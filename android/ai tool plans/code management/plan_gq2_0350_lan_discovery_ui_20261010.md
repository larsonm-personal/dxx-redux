# GQ2 chunk0350: LAN discovery UI, 2026-10-10

Diagnosis and implementation plans only, serially. Preserve concurrent changes; no product/test/helper edits, builds/tests/devices/probes/generators/formatters/staging/commits

- [x] Review all56 assigned frozen hunks1-56/new25-1298, current delta and enclosing source with exact identities
- [x] Trace discovery/membership/mission-resolution/launch consumers and maintained fixtures
- [x] Reconcile existing owners and actionable plans; assess simplification and inherited minimization
- [x] Publish canonical evidence and independently audit before DONE

Starting state:0349 audit PASS2921 SHA9958eed949b9fabdce4bc4cfc05f677536ffce2ed5ba0832d192f41c813e2c19. GQ2 349DONE/278TODO/627;1164 terminal;274 fixes;12 investigations. Preserve service PREPARE/START/CANCEL protocol, mission requirements, pending launch and current native ownership; reconcile existing0345/0346 launch/membership/retry findings against current UI. Prior evidence earns selected reuse credit only after exact source/report/snapshot/audit verification


## Assigned source checkpoint

Read all56 assigned hunk headers/removals and whole1337-line current source in four complete ranges. Frozen/current source is byte-identical, current delta empty; all assigned additions/removals are bound to their exact source ranges. The assigned Kotlin path is absent from the1996 reference. Source review does not complete consumer, owner or minimization gates.

Preserve service-owned manual/discovered/in-game join, mission requirement and preparation protocol. Actual request cancellation, invitation/lifecycle handoff, Ready admission, resume side effects and launch acknowledgement require consumer/fixture and existing-owner reconciliation. Portrait/landscape host/scan action rows offer a possible local simplification while preserving focus and layout. No new finding or implementation acceptance yet.

Checkpoint SHA9646be5e2a6ebfa86da309901ff757ec1a44ef7733696f04ac28fd0998fefce2; scopeae70372f4eb214f045b89be1cebd616be27c226e3261564da4ac9f6709aea2c5. All five canonical ledgers and HEAD match audited0349. GQ2 remains349DONE/278TODO/627;0350TODO. Next consumer and existing-owner diagnosis, full report/publication/audit. No product/test/helper edits or execution; goal active


## Current behavior and implementation continuation

All56 assigned frozen hunks1-56/new25-1298 were reviewed with their complete removals and additions (+396/-160). Whole1337-line current source is byte-identical to frozen; there is no current delta. The source checkpoint and independent audit bind all four enclosing read ranges. Assigned Android Kotlin is absent from the1996 original. Consumer evidence comprises33 fresh ranges and29 exact contexts explicitly reused from audited0335/0346; those prior sources and review/snapshot/audit identities were verified again. All observations are source-derived, not reproduced runtime results.

### Preserve the current join and invitation route

LanLobbyCard now routes both waiting and in-game announcements through joinDiscoveredLobby. Manual entry calls tryJoinLobbyByIp with engine probing instead of fabricating an unverified GameLaunchInfo from an arbitrary selected game. Keep fresh launcher reply priority, native read-only engine codecs, exact game/port selection, game readiness checks and actual service launch publication. The probe's Boolean means dispatch, not JOIN_ACK or successful native startup; do not label it completed connection acceptance.

The UI cancels its old manual coroutine before replacement and clears the handle only when the finishing Job owns it. The service uses manualIpAttempt, discovery/background state and captured joined/host state; it checks cancellation and ownership after engine waits. notifyAppBackgrounded and stopDiscovery invalidate attempts. Preserve these mechanisms; no blanket missing-cancellation finding. Existing BR0342/0490 owns correlated state admission, including check-to-publication intervals; no new race root is allocated from these same boundaries.

SetupActivity4241-4258/4294-4312/4393 withholds a pending invitation during hosting/joined/online/game conflicts, presents confirm/cancel and passes the request when conflict clears. That null-to-ID change retriggers the LAN effect. Setup supplies the production scanner callback at2918-2925. Thus the absence of isHosting in the downstream effect keys is not a demonstrated stalled production invitation. Preserve validated scanner/deep-link ingress, request UUID/consumption, resume/permission/callsign gates and the normal leave-game path. The callback-null fallback is not the sole production route.

Keep observer removal on disposal, currentCallsign via rememberUpdatedState, manual-job disposal and service socket/NSD ownership. Discovery deliberately survives screen recomposition; do not introduce unconditional stop on disposal without coordinating hosting, joined lobby, foreground service and native game lifetimes. Address labels and once-per-process default prefix are presentation hints, not authoritative connection identity.

### Existing readiness, mission and host controls

Self-row mission display prefers local status over the host echo. LanMissionStatusDisplayTest25 verifies local MATCH versus echoed FINALIZING only. Ready enablement accepts either local or echoed MATCH, while setReady sends local MATCH status before READY only when actually matched. handleReady validates member sender and host-known MATCH. Keep these improvements; UI enablement alone is not a source-generation guarantee.

GQR0272 remains the sole manual mission-transfer retry-series finding. The visible Resume action reaches requestMissionDownload; an exhausted old attempt3 versus a new manual attempt1/2 remains its recorded acceptance case. Preserve request/revision/sender/series rejection, bounded retries and native match requirements. Coordinate late resolver/enable publication with BR0433/GQR0102, without adding a duplicate defect.

Host Start checks save checking/warning, minimum count and every connected/ready/MATCH member; actual service start also checks eligibility. Preserve those repairs. BR0490/0496 and the0346 plan own the remaining serialized pending roster/configuration and final admission before Activity start. onLaunchRequested begins launcher preparation but returns no admission result to this UI callback. A single service-owned immutable eligibility/admission result should drive UI and actual launch, with stale attempts rejected before starting an Activity. Keep PREPARE/START/CANCEL, terminal ordering, heartbeat recovery and failed-Activity cancellation.

Current host chat is present at1051-1059, with a deliberately fixed140dp height inside the scroll container. Correct the historical BR0495 description when implementing it: chat is no longer absent. Ordinary host rows still have no Kick; the only kick action remains in LanJoinedLobbyView behind isHosting, whereas ordinary hosts take LanDiscoveryView. Extend the existing BR0495 plan for reachable authority-specific actions and state transitions, coordinated with BR0487 because kick still calls synchronous roster broadcast. Preserve existing async chat.

### Resume work, launch writes and source ownership

The resume card still resolves saved progress synchronously in remember at423-424. The actual resolver reads autosave JSON, enumerates retained checkpoints and checksums complete save bytes through MultiplayerScreen1222-1303. This is already explicitly included in GQR0084. Move that discovery into one cancellable IO snapshot keyed to game/mission/storage and lifecycle, rejecting old completion after replacement or disposal while retaining containment and checksum checks.

Do not misclassify MissionCatalog.load or VisualReplacementPolicy.summaryForPvp as main-thread scans: both actual suspend helpers already use withContext(IO). Keep that boundary. hostResume nevertheless writes defaults/restore selection/callsign and local hosted fields before the catalog lookup can reject a missing mission. writeCoopRestoreChoice uses direct final-file writes; the coroutine has no local failure presentation. Extend existing BR0506's launch-sidecar publication and GQR0102's admitted-source plan to this earlier resume preparation. Validate the mission/save/source owner before publishing the selected restore choice; preserve the prior coherent selection on failure, report a missing or rejected mission visibly, and contain write failures through the same orchestration policy. Avoid a separate persistence framework or compatibility migration.

Client resume matches stable host ID first, then host name/game/mode/mission when IDs are unavailable. The maintained tests deliberately allow changed mission details for the same stable host ID. Preserve that intended resume behavior; it is not authentication of network identity. Existing BR0342 owns authenticated control admission.

Both LAN views clear their launch event before forwarding to Setup. Current Setup suppresses duplicate launches, tracks replacement ownership and catches preparation failures, while BR0506 and0346 retain complete generation publication and pre-Activity admission. Keep the current repair and explicit owner plans rather than claiming all launch failures still escape the old main-thread path. Native game/save formats and query codecs remain in their shared/native owners; existing GQR0170 covers JNI acquisition/publication failures. No Kotlin reimplementation of game formats is justified.

### Concrete local simplification

LanDiscoveryTab848-892 and903-947 are byte-for-byte identical45-line Host/Scan rows after line extraction (LF SHA9bb0b6e2ed05ccb8d853d44f146f69c06692449a909150ae27f7dc40c5732508). Collapse the two non-hosting branches into one row, followed by the existing landscape-only spacer/Join by IP row. Keep the portrait Join by IP placement at720-749, labels, permission callbacks, weight, focus requester and button order. A new composable or generic action framework is unnecessary.

Remove unused FilterChip and tvFocusBorder imports, unused permissionActionHasInitialFocus, and hostedMission state with its two writes. Whole-file symbol occurrences prove the state is never read; the actual mission continues through the local onCreate arguments/record into LobbyService. These are existing enclosing-source cleanup opportunities, not regressions attributed to an assigned new hunk. Carry this small cleanup with the existing BR0495 UI work; no new defect/remediation ID, ranking or completion claim.

Implementation acceptance: exercise portrait/landscape with permission absent/granted, idle/scanning/hosting/joined states, controller initial focus and text-entry Back. Assert exactly one visible Host/Scan row, unchanged Join by IP placement, required host actions reachable once and no joiner authority. Source-only focus helper tests establish the selected target, not actual Compose attachment or navigation. Run scoped quality and relevant JVM/Compose/device coverage when implementing; none ran during diagnosis.

### Evidence limits and minimization

Fresh LanQrChecks185 source exercises rendered QR conceal/reveal and decoding, exported link delivery behind a hosted-lobby conflict, and actual scanner callback rejection. It intentionally avoids a target network join and does not establish successful invitation consumption, A-to-B replacement, process recreation, final readiness or native launch. Its initial overlarge read request was rejected before output; the whole185-line source was then read. MultiplayerResumePrefs likewise recovered its exact398-line endpoint before credit. No missing output receives review credit.

Selected resume tests cover newest progress, stale selection, lower/secret levels, empty saves, checksum rejection and host-match policy with their helper definitions. They do not run the real Activity composition, UI-thread latency, write-failure or service transition schedule. Exact reused LobbyLatencyChecks exercises loopback service protocol in its maintained source, but seeds a ready roster and confirms directly; it does not close the pending-membership/Activity boundary. Reused protocol/resolver/transfer/query/save fixtures and owners retain their prior evidence limits.

Disposition ISSUES/RETAIN; GQD NO_INHERITED_EFFECT, inherited saving0. Existing BR0342/0433/0487/0490/0495/0496/0506 and GQR0084/0102/0170/0272 retain status and priority. No new root, remediation, investigation, runtime acceptance or inherited extraction. Keep shared native save/query formats and desktop parity. This chunk does not award other numbered scopes, supplemental live-head deltas, sweeps, investigations or final campaign closure.


## Chunk0350 canonical publication and independent audit complete

GQC-1178 ISSUES/GQD-1058 NO_INHERITED_EFFECT; GQR0170 REFERENCE56. All56 assigned hunks (+396/-160), whole1337 frozen/current source,66 contexts/29 exact audited0335/0346 selections complete. Planned duplicate45line Host/Scan row cleanup and unused import/state removal; preserve orientation placement and focus. Existing BR0495 host Kick, GQR0084 resume discovery, BR0506 early sidecar publication and BR0490/0496 final admission plans extended. Host chat, IO catalog/visual summaries, service join ownership and actual Setup invitation conflict routing preserved. No new root/fix/investigation/status/rating or inherited saving; no runtime acceptance.

Independent audit PASS2335 SHA5c5fa3a62c35366e5491eda26a35e873d572bed72944f3bcf3b19fa492b682db. Reconstructed all five canonical files exactly; preserved prior terminal/fix payloads and definitions, investigation count and other queue rows. Verified56 full hunks, whole source/read union, empty current delta,66 exact contexts including33 fresh consumers/29 selected prior contexts and their audit/report/snapshot chains, source checkpoint/audit, three plans and mechanical cleanup evidence. Canonical context payloads relocated unchanged. Product diff unchanged between publication and audit.

Snapshot SHAc572f4c05f6f00d7fe79f8ffce0d800280a3708dea661ea61c525362eef3c3e1; reportSHA9737eee4935b9f274679841ccceba734cf72254ffaa43d7a39cbd057fdf810f0; scope4ecba13a6febb5f059bc5d0ff59c6b3b7f056de0a3e6ff7dc36a3c1c3a4a063c. Source audit PASS1058 SHAf295556a298d7b04b3bd311076d1b2fd6aaea5cac5e100c539ecf548b14b6886. Snapshot/audit plan hashes precede this closure/handoff.

GQ2 now350DONE/277TODO/627 numbered;1165 terminal;274 fixes;12 investigations. Next0351 LocalhostProxy.kt hunks1-22/new5-397. Remaining numbered scopes, supplements/sweeps/investigations, historical0116 audit debt and final live-head reconciliation remain required. Diagnosis/docs only; no product/test/helper edits or execution. Goal active
