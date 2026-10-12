# GQ2 chunk0351: Localhost proxy, 2026-10-10

Diagnosis and implementation plans only, serially. Preserve concurrent changes; no product/test/helper edits, builds/tests/devices/network/probes/generators/formatters/staging/commits

- [x] Review all22 assigned frozen hunks1-22/new5-397, current delta and enclosing source with exact identities
- [x] Trace proxy startup/relay/peer/migration/launch consumers and maintained fixtures
- [x] Reconcile existing owners and actionable plans; assess simplification and inherited minimization
- [x] Publish canonical evidence and independently audit before DONE

Starting state:0350 audit PASS2335 SHA5c5fa3a62c35366e5491eda26a35e873d572bed72944f3bcf3b19fa492b682db. GQ2 350DONE/277TODO/627;1165 terminal;274 fixes;12 investigations. Preserve actual proxy owner, cancellation, source/peer identity and generation repairs. Prior context earns selected reuse credit only after exact current source and original review/snapshot/audit verification


## Assigned source and current-delta checkpoint

Read all22 assigned hunk headers/removals (+64/-48) and whole614-line current source. Whole575-line frozen source maps through equal regions plus every changed-away line in the complete29-hunk/110-line current U0 delta. All hunks belong to this queue scope; original1996 path absent. Consumer/owner/minimization review remains pending.

Preserve lazy start, synchronized peer/worker registration, one failure callback, idempotent close-before-cancel shutdown and sibling socket closure before coroutineScope waits. Current ProxyDiagnostics mediates socket operations; its actual receive/send/closing semantics and fixtures must be read before judging packet-reset or failure behavior. Existing relay/dynamic-peer authority and packet-budget owners require reconciliation. Separate blind-probe job lifetime needs actual scope/caller review; no new root asserted.

Checkpoint SHAc825200d525a461edaaa782e2761f5268566aa72a920a79237233b83e3c5a6ab; scope49388b9ad3b7b32df93587e8189e4f5ba76d026a4110475c311ec5c25b60e5d3. HEAD and five canonical ledgers match audited0350. GQ2 remains350DONE/277TODO/627;0351TODO. No product/test/helper edits or execution; goal active


Source checkpoint independent audit PASS1053 SHA82f9997169f7ce1b8099a4c164265fcf77801e198c426b8e03c36b7e6e5c6e60. Verified all22 assigned hunks+64/-48, whole614 current read union, exact575-line frozen mapping and complete29-hunk/110-line current delta, base/frozen/current identities,1996 absence, prior0350 evidence, HEAD/manifest and unchanged627 queue/canonical ledgers. Product diff unchanged. No runtime acceptance or consumer completion.

Next: current ProxyDiagnostics helper and real proxy fixtures, caller startup/socket ownership, failure and migration callbacks, relay/dynamic-peer admission and existing-owner reconciliation.0351 remainsTODO;350DONE/277TODO/627. Checkpoint SHAc825200d525a461edaaa782e2761f5268566aa72a920a79237233b83e3c5a6ab; scope49388b9ad3b7b32df93587e8189e4f5ba76d026a4110475c311ec5c25b60e5d3. Diagnosis/docs only; goal active


## Current behavior and implementation continuation

All22 assigned frozen hunks (+64/-48), the whole614-line current source, the complete29-hunk current delta and its575-line frozen mapping are reviewed. Current consumer identities include production ProxyDiagnostics, real proxy failure fixtures, the service startup/failure/late-join/migration owners, server relay, both native protocol headers and PDATA/MDATA producers, runtime IPC and aggregate stats display. Coverage is confined to the recorded ranges; no whole MatchmakingService, SetupActivity or native transport review is claimed.

### Repairs and behavior to preserve

LocalhostProxy starts workers lazily and serializes start, peer registration, snapshots and shutdown. A stopped flag rejects late registrations. Failure reporting is once-only; shutdown closes sockets before cancelling jobs. Peer forwarding closes sibling sockets in finally before coroutineScope waits. These repairs supersede BR0498's historical claim that the collections have no synchronization or stopped flag; they do not by themselves prove child retirement, coherent snapshots or every race acceptance.

ProxyDiagnostics wraps actual receive/send/close operations, records successful sends only after DatagramSocket.send returns and retains the original exception. It rate-limits failure output per operation, distinguishes keepalive success and suppresses expected closing failures. It does not choose retry versus stop. Current local gameplay forwarding catches a plain IOException and continues; keepalive lets that exception reach the owning worker, which retires the proxy. SocketException branches have their own closed/EPERM policy. Do not describe diagnostic instrumentation as a new error-policy fix or infer a real-device cause from injected failures.

MatchmakingService publishes a new proxy before starting its lazy workers and rejects failure callbacks for a different proxy identity. Online handoff joins old probing work, checks network generation/current job under the probe and service owners, then transfers candidate-socket ownership. Preserve these repairs. The service-wide SupervisorJob survives individual proxies; sendLateJoinProbes still starts an untracked job in that scope. Closing its captured old socket prevents a new-route publication, but pending DNS/probe work can outlive shutdown.

### Existing findings and concrete continuation

- BR0132 retains relay token/source/slot validation and credential-redaction acceptance. The shared receive path currently accepts a relay slot from byte4 without checking relay endpoint/token; the server's validation does not authenticate that client receive boundary. Do not create a duplicate finding.
- BR0497 retains authenticated, expiring dynamic-peer admission and fair capacity. Unknown endpoints can still allocate persistent routes before engine approval. Put cap checking, unique slot reservation and admission in the serialized owner; the current size check is outside addDynamicPeer's synchronized operation, and dynamic slot selection must not collide with signaled peers.
- BR0498 retains structured ownership and completion of all proxy jobs, including blind probes, with shutdown/add/snapshot race acceptance. Preserve repaired lazy start, stopped rejection and close-before-cancel ordering. BR0148 retains per-generation/per-pair probe identity; cancellation must reject stale work after suspension/DNS and prevent stale commands reaching a replacement.
- BR0507 retains migration/setup transactionality: createProxy retires the old proxy before replacement resources are fully acquired, and Setup migration setup still crosses fallible parsing/launch preparation. Acquire candidate resources under attempt ownership and publish only after admission; close the candidate on failure and preserve the prior usable owner where the contract permits.
- BR0136 owns address-family handling; do not add another IPv6 parsing finding for late-probe host:port splitting.
- BR0348 owns packet budgets across native production, direct transport, relay envelopes and path MTU. The proxy has4096-byte receive buffers and prepends a five-byte relay header, while the server accepts a larger datagram. Derive the real maximum emitted packet in both games before resizing or claiming reachable truncation; selected object/PDATA/MDATA paths are not an exhaustive bound. Do not carry forward the old2048-era maximum as current proof.
- BR0341 retains connection-generation diagnostic invalidation. BR0446/0447 retain logging budget/process ownership; NetLog's NETWORK sink is synchronous. No new logging framework or bulk relocation is justified here.

Historical R1-0436 records a JDK21 DatagramPacket reuse probe receiving one byte then100 bytes without truncation. That is historical evidence only. No new probe was run, and an unsupported packet.length-reset defect is not admitted.

### New packet-classification finding and simplification

Proposed GQF-0289/GQR-0275: both receive paths classify byte0 as PDATA for0x22 or MDATA for0x25/0x26. Both engines define and write PDATA16 and MDATA17/18 into byte0 before sending. Ordinary PDATA/MDATA therefore does not increment these diagnostic categories; unrelated bytes can do so. Forwarding itself is independent of this classification. The affected output is periodic logcat pdata/mdata fields, not the aggregate PeerProxyStats, runtime IPC codec, stats overlay or ProxyDiagnostics success totals.

The two six-line classification blocks are identical after indentation normalization, and each persists unchanged across base/frozen/current. Normalized SHA256 0d5fa1f1ca23a89f2cc2cfeba6b53893b90add6144843380a21717e6b1230c1f; base408-413/498-503, frozen424-429/514-519, current458-463/552-557. This is an enclosing-source pre-existing defect, not an assigned-hunk regression.

Remove the duplicated semantic counters, byte decoding and misleading category labels. Preserve aggregate transport diagnostics, original failure details and native packet-category diagnostics, which already use the actual engine symbols. Keep protocol knowledge in the engine; do not create Kotlin constants, JNI interfaces or a shared protocol parser solely to retain redundant logging. Detailed implementation and acceptance: plan_gq2_0351_proxy_packet_diagnostics_20261010.md.

### Fixture assessment and acceptance limits

The maintained proxy-tests module compiles actual production LocalhostProxy/ProxyDiagnostics/NetworkConstants and uses real UDP sockets with an injected send failure. Tests inspect continued gameplay forwarding and keepalive failure/closure, and can compare an opt-in historical snapshot. The current fixtures preserve the first error and preceding successful traffic. Their Android Log stub discards logcat calls, and one-byte test payloads do not exercise native categories, relay envelope limits, dynamic admission or adversarial shutdown races. The regular Gradle unit runner includes :proxy-tests:test. Historical snapshot source was not reviewed or executed in this tranche.

During later implementation, extend the actual production proxy fixture to observe the affected diagnostic sink and exercise both shared and per-peer receive paths with direct and relay payloads. Check byte-for-byte forwarding, aggregate counts and preserved failure output, and absence of misleading semantic labels. Existing owner plans retain their broader runtime/race/MTU/admission gates. No tests, builds, device, network or fault probes ran in this review.

### Inherited minimization

NO_INHERITED_EFFECT, saving0. The assigned Kotlin path is absent from the1996 tree. Remove obsolete launcher-only protocol decoding at its owner; retain native constants, producer bodies and diagnostics in both engines. Native consumers provide protocol evidence, not whole-file diff credit or justification for an inherited extraction. GQR0264's paired native diagnostic extraction remains a separate previously planned change.


## Chunk0351 canonical publication and independent audit complete

GQC-1179 ISSUES/GQD-1059 NO_INHERITED_EFFECT; GQR0275 PRIMARY33. All22 assigned hunks (+64/-48), whole614 current/575 mapped frozen source, complete29-hunk delta and53 contexts verified. NEW GQF-0289 OPEN/GQR-0275 TODO: remove incorrect duplicated proxy packet classification, preserving native protocol authority, aggregate transport/IPC counters and actual-success/original-error diagnostics. Both classification blocks predate the assigned hunks; source-derived P3/high, priority33 LOW(12/0/4/10/7), no runtime reproduction or inherited saving.

Preserve synchronized/stopped/lazy proxy ownership and current-generation callbacks. Existing BR0132/0136/0148/0341/0348/0446/0447/0497/0498/0507 and GQR0228/0264 owners retain status/rating; detailed acceptance and repaired historical claims are reconciled in the chunk plan. No unsupported DatagramPacket-reset bug or demonstrated relay truncation claim.

Independent audit PASS2758 SHA869ee957e0944425b61c1c465303194cf984f002ee159003000be9b0d3cb7088. Reconstructed all five canonical files exactly; preserved prior terminal/fix payloads, findings/remediations/investigations and every other queue row. Rechecked assigned hunks/current delta/mapping/source read union,53 contexts including three unchanged relocated canonical ranges, source checkpoint/audit and prior0350 chain, historical classification bodies, native constants/producers, aggregate stats boundary and both plans. Product diff unchanged between publication and audit. Two audit script variable collisions were corrected before PASS; failed attempts wrote no audit artifact and imply no product finding.

Snapshot SHA1e6ce44012a16511200ac84440ba6f67275186c44618d825043a53a2ef1beffb; report SHA42bf50b1bfc07f9e856e2f7aabd6874c5af3df15034431183ac0b2be462d429f; scopebd52c8e30a78aea97c46245943af72a7570dc48514fc51158e658b248d3c6a17. Snapshot/audit plan hashes precede this closure/handoff.

GQ2 now351DONE/276TODO/627 numbered;1166 terminal;275 fixes;12 investigations. Next0352 MatchmakingService.kt hunks1-60/new5-1176. Remaining numbered scopes, supplements/sweeps/investigations, historical0116 audit debt and final live-head reconciliation remain required. Diagnosis/docs only; no product/test/helper edits or builds/tests/devices/probes/staging/commits. Goal active
