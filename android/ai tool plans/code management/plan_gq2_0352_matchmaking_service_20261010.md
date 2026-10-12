# GQ2 chunk0352: Matchmaking service, 2026-10-10

Diagnosis and implementation plans only, serially. Preserve concurrent changes; no product/test/helper edits, builds/tests/devices/network/probes/generators/formatters/staging/commits

- [x] Review all60 assigned frozen hunks1-60/new5-1176 and enclosing source with exact identities
- [x] Trace connection/authentication/reconnect, state publication, probing and launch consumers and maintained fixtures
- [x] Reconcile existing owners and actionable simplification/inherited decisions
- [x] Publish canonical evidence and independently audit before DONE

Starting state:0351 audit PASS2758 SHA869ee957e0944425b61c1c465303194cf984f002ee159003000be9b0d3cb7088. GQ2 351DONE/276TODO/627;1166 terminal;275 fixes;12 investigations. MatchmakingService current equals frozen1563-line source. The complete file has62 diff hunks; this queue scope owns only1-60. Later61-62 are a separately grouped queue scope and receive no terminal credit here. Reuse selected0351 consumer reads only after exact snapshot/report/audit/source identity checks


## Assigned source checkpoint

Read all60 assigned hunk headers/removals (+494/-196) and whole1563-line current/frozen source, with eight fresh ranges and six exact audited0351 selections. Current source equals frozen byte-for-byte; current delta empty. Complete diff has62 hunks;61-62 belong to already assigned0337 and receive no duplicate queue credit here. Original1996 path absent. Consumer/owner/minimization review remains pending.

Preserve synchronized connection/network generations, current-WebSocket authentication callbacks, lazy job publication and probe join/mutex before game handoff. Reconcile actual state-reset differences, pendingGameInfo/lastLobbyId ownership, transport send acknowledgement, URL/trust/auth fallback and per-peer probing against existing findings before new root admission. Potential common close/failure reset cleanup must retain reason-specific logs and saved lobby semantics.

Checkpoint SHAbdeb4e76ce46d0ae6ad67e93ddbe4cd1bdd2d57d2afc3dd2a8d287f0e1fe164b; scope06c405f3e20b5fb801c216984b8993fd1069fcc950016deb86390e4a5bbb2c2c. HEAD and five canonical ledgers match audited0351. GQ2 remains351DONE/276TODO/627;0352TODO. No product/test/helper edits or execution; goal active


Source checkpoint independent audit PASS1116 SHA16ef933cec6ef20553fec66c63ce628d58b79d3dd1b7724291caaff6e54f4f0f. Verified all60 assigned hunks+494/-196, whole1563 current/frozen read union, byte equality/empty delta, eight fresh and six exact audited0351 source ranges, base/frozen/current identities,1996 absence, prior0351 evidence, HEAD/manifest and unchanged627 queue/canonical ledgers. Hunks61-62 confirmed assigned to DONE0337; no duplicate queue credit. Product diff unchanged. No runtime or consumer completion.

Next: current authentication/URL/trust/reconnect, STUN/UPnP and connectivity consumers, state publication/launch ownership and actual maintained fixtures; reconcile prior findings and concrete common-reset cleanup before canonical publication.0352 remainsTODO;351DONE/276TODO/627. Checkpoint SHAbdeb4e76ce46d0ae6ad67e93ddbe4cd1bdd2d57d2afc3dd2a8d287f0e1fe164b; scope06c405f3e20b5fb801c216984b8993fd1069fcc950016deb86390e4a5bbb2c2c. Diagnosis/docs only; goal active


## Current behavior and implementation continuation

All60 assigned frozen hunks1-60/new5-1176 (+494/-196) and the complete1563-line current/frozen enclosing service have source coverage. Frozen/current bytes are equal. Later hunks61-62 belong to completed0337 and receive no duplicate credit. Eight fresh source ranges and six exact audited0351 ranges form the source checkpoint. Consumer evidence is separately bound; historical test results are not fresh execution.

### Preserve current repairs

The service serializes connection replacement and network retirement. Listener open/message/closed/failure callbacks use withConnection to reject a different socket or manual disconnect. Delayed Play Games authentication captures the original connection and checks it before sending. Reconnect captures connectionGeneration and checks it after delay. These repairs supersede BR0439's historical unguarded terminal/authentication description; retain its full ordering/cancellation acceptance.

STUN jobs publish lazily after their handles are assigned, own a captured candidate socket, and publish only for the current network generation. UPnP creation checks generation and job activity under the owner before accepting the mapping; rejected mappings receive compensating removal. Preserve that repair to BR0458 rather than repeating the historical unconditional stale-field publication claim. Connectivity checking is serialized by probeMutex, tests job activity, and game handoff cancels and joins probing jobs before taking that mutex, checking the exact handoff job and transferring the socket to a published, lazily started proxy.

Same-UID foreground-service IPC now passes native host state to the default-process matchmaking owner; runtime-session gating and the maintained codec/session tests preserve archived BR0004. Do not restore the old process-local Activity polling or infer five-minute online/late-join runtime acceptance from these source checks.

### Concrete simplification and existing owner plans

Centralize the repeated current-connection terminal reset used by onClosed/onFailure. Keep one owner operation for socket detachment, network-job retirement and immutable state publication, with explicit inputs for error/reason and reconnect intent. Preserve onFailure's conditional lastLobbyId retention across failed reconnects and onClosed's capture of the current lobby. Manual disconnect must clear saved rejoin intent; leave remains a distinct lobby transition. Avoid a general networking framework and avoid moving state-reset logic into UI callbacks.

This cleanup must implement the existing BR0341/0433/0439/0502 acceptance together where their fields intersect:

- Clear or explicitly preserve each field by connection/lobby ownership: candidates, connectivity pairs, relay, STUN addresses, ICE outcome, current lobby, chat, connection info, pending launch and server-scoped lobby lists. Current retirement resets worker handles and stunCompleted but does not clear the whole observable snapshot. Unexpected close/failure still retain more diagnostic state than leave/manual disconnect; NetworkEventsPanel displays retained data while disconnected.
- Bind pendingGameInfo and accepted configuration to the current lobby/connection and replace it from an acknowledged authoritative update under BR0433. A global fallback surviving leave and endpoint replacement is not a valid source for a new lobby. Keep BR0502's separate UI resume operation correlated with the exact connection and a fresh lobby-list response; a CONNECTED flag alone is insufficient.
- Recheck reconnect ownership before the initial RECONNECTING state/log publication as well as after delay, and keep the check/publication atomic with disconnect and replacement. Cancellation alone does not roll back an already running callback. Retain BR0439's barriers for A-to-B-to-C connections and delayed Play Games completion.
- Treat queued game launch, proxy and native launch preparation as one current attempt, and clear or reject ended-session intent at the existing admission boundary. Preserve Setup's pending proxy identity cleanup and consumed-launch clearing. Do not expand this local reset into a new native wire format or reopen the LAN launch barrier work.

Endpoint and authentication work stays with BR0499/0500/0501/0343/0090. Parse and validate before replacing a working owner or persisting suggestions, resolve once on bounded generation-owned IO, and bind the resulting endpoint to authenticated TLS policy. The current private-address trust-all client and explicit cleartext exception are existing findings, not new roots. Current Compose direct/resume callers still persist raw input before calling connect; Setup automation reaches the same owner. The normal browser is presently hidden by lanOnlyMultiplayerRelease=true, which does not repair these maintained service/automation paths. DirectInstall auth deliberately omits Play Services; production fallback must implement the server's signed keypair/PoW protocol instead of a dev token. Server skip verification in local integration is not production authentication evidence.

BR0148 retains fair per-peer connectivity and current admission identity. ConnectivityChecker returns the first successful pair, while only total failure reports relay for all peers. The generation/mutex repair does not supply all missing per-peer terminal results. BR0136 retains typed IPv6/address handling; BR0405 retains complete response validation in the production STUN listener fixture. Keep these protocol-specific changes at their transport owners.

BR0457/0458 retain bounded, cancellable optional UPnP, current mapping ownership, confirmed compensation and truthful deletion results. The actual parent awaits UPnP before candidate publication, and current HTTP/SOAP bodies still use unbounded readText. deletePortMapping discards the nullable SOAP result and removeMapping reports success on a null/error response. Preserve the added generation/activity acceptance check while implementing the remaining owner plan and ensuring optional discovery cannot suppress STUN/relay completion.

BR0156 retains one authorized lobby chat broadcast rather than per-recipient client fanout. BR0132/0497/0498 retain relay authority, dynamic admission and complete proxy retirement, as reconciled in0351. Keep asynchronous auth and control sends associated with the current owner; a local WebSocket enqueue is not a server acknowledgement. No new finding, remediation, investigation or status/rating change is needed for these existing roots.

### Native boundary and inherited minimization

startGame performs CoopSaveCompatibility.hostWarning before sending START_GAME. The exact audited0337 native compatibility consumer, format definitions and JNI string boundary are retained. GQR0170 already owns acquisition/exception/Unicode/partial-result acceptance for jni_resume_save.cpp; reference priority56 (32/0/7/10/7), without duplicating that finding or claiming its implementation complete.

NO_INHERITED_EFFECT, saving0. MatchmakingService is absent from1996, and the concrete reset/endpoint/ownership cleanup belongs in the launcher service. Preserve native save-format and compatibility authority in the existing shared native code. No inherited file edit, packet-format relocation or whole native diff credit is justified here.

### Maintained fixture assessment and implementation acceptance

RuntimeGameStateBridgeTest tests real codec/session-policy helpers, including complete state, round-trip diagnostics and one disconnect. It does not drive a real WebSocket, delayed authorization, DNS/TLS, discovery cancellation or service replacement. The selected native/Setup/proxy consumer ranges and previously audited proxy fixture remain source evidence only.

test_mp.ps1 uses actual service commands and two emulators, but configures SKIP_GPGS_VERIFY=true and FORCE_RELAY=true. Its substring send_sync fallback can skip paired state assertions and still report full success. Existing BR0656 owns that false-pass root; a future passing run on the fallback path cannot close this review's connection, direct/relay, paired-state or production-authentication gates. No script, build, device, server or fault probe ran during this diagnosis.

Implementation should extend maintained production-path coverage with controllable socket/auth/resolver/IGD boundaries, preserving normal multiplayer integration. Required sequences include delayed A terminal/auth callbacks after B/C opens; reconnect publication racing Cancel; manual disconnect versus unexpected close/failure; failed rejoin preserving only its intended lobby identity; populated diagnostic sentinels across each terminal path; fresh versus stale lobby configuration/list responses; rejected URL before owner replacement; canceled discovery before/after mapping acceptance; failed deletion; and three-or-more-peer mixed transport completion. For each, assert one current owner, no stale state/send/publication, bounded resource retirement and truthful visible outcome. Correlate live D1/D2 host/join state and failure reasons without the BR0656 fallback laundering missing evidence.

Apply the common reset as a small owner-level change with a documented field policy, then implement the separately scoped endpoint/auth/transport findings in their existing plans. Run scoped quality, relevant Gradle/native builds and actual maintained integration only in the implementation tranche.


## Chunk0352 canonical publication and independent audit complete

GQC-1180 ISSUES/GQD-1060 NO_INHERITED_EFFECT; GQR0170 REFERENCE56. All60 assigned hunks1-60 (+494/-196), whole1563-line frozen/current source and72 contexts including16 exact prior selections verified. Hunks61-62 remain credited only to0337. Concrete common terminal-reset plan preserves reason-specific reconnect and saved-lobby semantics; existing state/authentication/discovery owners extended without new root/fix/investigation/status/rating or inherited saving. Current generation/job ownership repairs retained. No runtime acceptance

Independent audit PASS2576 SHA3323be7caddbfdf183db6ecaab584cbc9f5e5e4cb63f5cc71ed6f6d8d1b404d8. Reconstructed all five canonical files exactly, preserving old terminal/fix payloads and definitions and every other queue row. Verified source and assigned hunks, byte equality/empty current delta, context and prior evidence chains, plan and local cleanup semantics. Product diff unchanged between publication and audit. Current refresh confirms all five canonical hashes still match

Snapshot SHAfd4af8a985e62582bb0f0e55db1e3edc576ea8926e3ecc8ce348667a686a6dcf; report SHA82893cb7102f9024f4dd143f52024e2d0c683ebef8e7ea5b2c3950341e857659; scopeb71c5caa2fbd12e36e0a752229104717ec0987c0244bff8b6ac3003345d35ee8. Snapshot/audit plan hashes precede this closure/handoff

GQ2 now352DONE/275TODO/627 numbered;1167 terminal;275 fixes;12 investigations. Next0353 MissionTransferService.kt frozenL1-L737. Remaining numbered scopes, supplements/sweeps/investigations, historical0116 audit debt and final live-head reconciliation remain required. Diagnosis/docs only; no product/test/helper edits or builds/tests/devices/probes/staging/commits. Goal active
