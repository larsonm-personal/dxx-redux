# GQ2 chunk0353: Mission transfer service, 2026-10-10

Diagnosis and implementation plans only, serially. Preserve concurrent changes; no product/test/helper edits, builds/tests/devices/network/probes/generators/formatters/staging/commits

- [x] Review full assigned frozen source L1-L737 and current delta with exact identities
- [x] Trace mission offer/request, transport, installation, cancellation and maintained fixtures
- [x] Reconcile existing owners and actionable simplification/inherited decisions
- [x] Publish canonical evidence and independently audit before DONE

Starting state:0352 publication audit PASS2576 SHA3323be7caddbfdf183db6ecaab584cbc9f5e5e4cb63f5cc71ed6f6d8d1b404d8. GQ2 352DONE/275TODO/627;1167 terminal;275 fixes;12 investigations. Assigned MissionTransferService.kt current equals frozen737-line source SHA55c76daf9b7c79ad8ee674ce2d93282fecb6857b846c2a5b0de94be53761df3f. Path absent at GQ2 base and1996 reference; full-file added scope. No other chunk or consumer credit implied


## Assigned-source audit complete

All737 assigned frozen/current lines read in four complete fresh ranges. The file is absent at the GQ2 base and1996 reference; one complete addition hunk+737/-0 is exactly the read source. Current delta is empty. Source checkpoint SHA103f7a6e49ef16ed580fe5cbcda5edfb28b02a7d628e78732958ba80c64c09d6; scopea8749fe93fb4d1d3ace957b3377b4fa287a4d979a4039a79d814a0d3fc430c1a. Independent audit PASS772 SHAc9b217533b1e7bcf58ebd11b09e31407cf65b4fbb589ac14295185dab8113582 verifies each added line, range union, source/blob/manifest/base/original, prior0352 audit, all five canonical bytes, HEAD, plan and unchanged627 queue. Product diff unchanged. Source checkpoint plan hash precedes this continuation

## Consumer diagnosis checkpoint

Fresh reads trace actual LobbyService request/grant/status/terminal/Ready/start and refresh paths, host dialog and download UI, complete requirement/status schema, catalog and identity/policy helpers, resolver, current ModManager import/registration/reorder/publication boundaries and maintained resume/refresh/managed-archive fixtures. Exact ranges are in the consumer checkpoint; bounded ranges do not confer whole-file credit

### Preserve current behavior

Keep one current ClientTransfer, lazy job publication before start, clientWriter until blocking work has really returned, socket close on cancellation and synchronized current-job callback admission. LobbyService validates sender, lobby, revision, request UUID and attempt before accepting a grant and rechecks callback request/lobby/host/revision before compare-and-set publication. Keep complete chunk digests, fsync before sidecar publication, final whole-file hash, excess-byte rejection and typed finalization failure

Host admission reinspects actual locally managed wrapper policy, compares expected identity, consumes expiring address-bound tokens and limits active serving slots to7. A valid offered requirement caps wrapper bytes at256 MiB. These are existing safeguards, not proof of bounded chunk metadata, complete deadlines, per-member fairness or immutable source bytes. Current ModManager save uses AtomicFilePublication; do not repeat BR0472's historical direct-write claim. moveUp uses content lock then atomic publication transaction and reload. registerMissionZip remains a separate direct local mutation/save in the source read

### Concrete ordinary-path candidate: finalization loses lobby mode

MissionTransferService506 calls resolve(context, requirement, "coop") unconditionally. download/receive/finalizeMission have no mode input. Actual LAN host dialog allows offered managed anarchy missions; LanDiscoveryTab1112-1164 passes the selected mode and requirement to hostLobby. The download UI235-267 has no coop-only restriction, and the current grant caller1906-1938 passes requirement/attempt without joined.mode. MissionCatalog15-21 filters anarchy-only entries only when mode is coop; the managed-archive fixture135-144 explicitly distinguishes the two modes

For an otherwise valid offered anarchy-only wrapper, the transfer can hash, import, enable and promote it successfully, then the coop-only lookup returns non-MATCH. require at507 throws; the download catch188-201 emits ERROR/mission_finalization_failed with the current attempt and onFinished(false). This is source-derived; no network or runtime reproduction. Do not characterize this as bytes failing verification or as a permanent client-side inability to discover the imported mission

Recovery matters: ERROR is eligible for refresh because LobbyService183-184 excludes only active/resumable states. JOIN_ACK2042-2058 resolves with the actual lobby mode. MissionCompatibilityResolver102-112 creates a fresh report with default attempt0 and no transferId. If the host has already accepted the transfer's attempt>=1 status, shouldAcceptMissionStatus190-193 rejects that corrective attempt0 report. Host Ready1673 and start2117-2123 still require retained MATCH. Thus an admitted schedule leaves the client displaying recovered MATCH while the host retains failure (or a prior nonterminal phase); this is not a claim that every packet ordering has that result. Membership replacement may recover it

The default-mode mechanism is distinct from BR0440's incompatible coop selection/native admission, GQR0270 short-name ambiguity and GQR0272 manual retry-series reset. Search of canonical code-management documents and outstanding_bugs for finalization/anarchy-transfer/hardcoded-coop found no exact duplicate. Final canonical admission/disposition remains pending; do not allocate a finding ID before final owner review. The refresh attempt0 loss belongs with existing GQR0272 ordering identity rather than another ordinal-reset root

Implementation direction: capture the admitted lobby mode with the request, carry it through the existing transfer/finalization operation and use it for post-import compatibility. Reject stale mode/lobby/requirement generations before publication; do not infer coop from wrapper data or permit anarchy-only missions in coop. Preserve series/attempt identity when a resolver refresh replaces a transfer terminal report, using GQR0272's admitted membership/requirement owner. Keep host Ready/start admission unchanged

Future acceptance must drive actual request/grant/download/import/status/refresh/host Ready/start, using an anarchy-only wrapper in valid anarchy/team modes and ordinary coop wrapper controls. Assert correct first terminal status, exact bytes/identity and both endpoints MATCH before and after refresh. Force a finalization error followed by successful catalog correction, stale refresh after new series, manual retry, changed mode/requirement and leave/rejoin. Existing MissionTransferResumeTest checks retained attempt2 only in one helper result; LobbyMissionRefreshTest uses attempt1 throughout and does not cover terminal refresh to default0. No fixture execution is claimed

### Bounds, source identity and publication continuations

GQR0212 is the existing shared metadata/work/live-memory owner. Receive accepts any positive peer chunk_size and nonempty hash array, then validatePartial allocates ByteArray(chunkSize) for a nonzero resumable partial. Request/header byte ceilings and the requirement's total byte cap do not bound that allocation. Validate typed header/sidecar schema, expected revision/content, fixed supported chunk size, exact ceil(size/chunkSize) hash count and hash grammar before allocation or partial truncation. Bound sidecar readText materialization and cumulative hashing/JSON work under the same attempt. Keep declared/actual wrapper limit and exact EOF checks; use small injectable limits for future negative coverage, not giant allocations. Complete monotonic request/transfer deadlines and cancellation checks are needed beyond per-read15s timeouts

Host authorizations are retained until consumed or stopHost clears the map; expiry is checked only when consumed, and playerId is stored but not consulted by serve. The seven serving slots are aggregate, not per-player admission, and slot refusal closes the socket rather than publishing QUEUED. Reconcile live-member revocation, token retention/rate bounds and one-active-transfer-per-member with existing BR0342 and shared admission owners. Do not claim an authenticated exploit or run network/resource probes. Future validation needs repeated grants without consumption, expiry, member departure, host replacement, stalled reads and one busy member beside another legitimate member

GQR0102 already owns policy/identity/payload source generations. Host policy scan, cached size/mtime identity and later stream open are separate steps; fixed immutable source identity must bind them. Preserve copied-source rehash in resolveImportedMissionIdentity and the deliberate managed-source premise for accepted moved identity; do not confuse equal length with demonstrated byte stability

Existing BR0186/0103/0472 retain archive, derived tree and registry transaction ownership. uniqueDownloadName checks the original candidate once and otherwise returns an eight-character digest suffix without reserving/rechecking that fallback. ModManager sanitization can also change the final name. Preserve the previous owner on candidate collision, inspection/copy/rename/extraction/registration failure and cancellation; do not make a deterministic short suffix an overwrite authorization. Current import moves the verified partial before later extraction/registry work, so failed finalization needs explicit source/partial recovery policy

finalizeMission resolves the active file set at finalization, then uses another active-set lookup inside resolver. Bind consent/request/import/catalog/result to one selected content generation, and coordinate existing publication/admission owners rather than layering another manager. Check cancellation at the actual commit boundary; callback suppression alone does not undo published import, reorder or soundtrack side effects. Future barriers should cover active-set change, canceled import and manager mutation between import and catalog check

### Concrete simplification

promoteImportedMission repeatedly sorts an already sorted listMods result and calls moveUp once per preceding item. Every moveUp reloads and rewrites the manifest under its transaction. Prefer one filename-based promote operation in the existing manager owner: reload once, preserve relative order of every other entry, publish a unique stable order once and make no-op/absent cases explicit. Integrate it with the accepted import transaction and cancellation policy. No generic ordering framework, new native format or broad engine deduplication is warranted

The assigned service is absent from1996. Proposed transfer/mode/admission/publication changes belong to launcher owners; inherited saving0 at this checkpoint. Native mission format and compatibility authority remain intact. Final inherited disposition and complete consumer/fixture/owner review are still pending

## Remaining work before canonical publication

Finish original-plan and actual integration-runner reconciliation, exact existing BR0342/GQR0102/GQR0272 and publication-owner boundaries, mode-candidate duplicate/admission decision, and any selected audited prior context reuse. Verify every credited current source again because ModManager has concurrent changes. Then write the final report/snapshot, publish canonical findings/evidence/queue and independently audit.0353 remainsTODO;352DONE/275TODO/627. No new canonical root/status/rating or runtime acceptance; goal active


## Chunk0353 source and consumer checkpoint independently audited

All737 assigned lines and complete added-file diff+737/-0 covered; current equals frozen and current delta empty, base/1996 path absent. Source checkpoint SHA103f7a6e49ef16ed580fe5cbcda5edfb28b02a7d628e78732958ba80c64c09d6; scopea8749fe93fb4d1d3ace957b3377b4fa287a4d979a4039a79d814a0d3fc430c1a. Source audit PASS772 SHAc9b217533b1e7bcf58ebd11b09e31407cf65b4fbb589ac14295185dab8113582

Saved42 fresh consumer/document ranges across17 files, including actual grant/download/import/catalog/status-refresh/host Ready/start/known-member paths, maintained resume/refresh helpers and selected managed-archive assertions. Candidate: finalization hardcodes coop for an admitted anarchy-only download. Correct-mode refresh may fix client status but emits attempt0, which a host retaining transfer attempt>=1 rejects; existing membership refresh preserves the host report. Mode root admission remains pending; refresh ordering belongs to GQR0272. Preserve current writer/job/hash/atomic-save repairs. Concrete one-transaction promotion and existing bounds/source/publication continuations are in the chunk plan

Consumer checkpoint SHA5ff5220f0b366e681f1fcf9cbd5ca278ee4a8451479096d78a078d9d00e2f2a9; scope6f98af5b9be3c11407ce478e8482dfacb8ffdd482fb4b7ca255aa6a9cc1996b4. Independent audit PASS162 SHAea9535c8c9d9d83fce3fa6531276a2fef12a84abdbf50608624c78c6f791d1ac verifies each fresh/revalidated range and whole-file claim, source/audit chain, actual mode/default-attempt/host-ordering/refresh source assertions, current plan, all five canonical bytes, HEAD and unchanged queue. Product diff unchanged at both checkpoint audits. Plan hashes precede this handoff

Next: remaining exact existing-owner/integration-fixture review and mode-candidate admission decision, live source revalidation, final report/snapshot/publication/audit.0353 remainsTODO;352DONE/275TODO/627;1167 terminal/275 fixes/12 investigations. No new canonical root/status/rating, inherited saving, product/test/helper change or execution. Goal active


## Final reconciliation and publication decision

All assigned source and required selected consumers are reviewed. Admit the distinct hardcoded-coop finalization root as proposed GQF0290/GQR0276, P2/high priority44 (23/0/4/10/7), with plan_gqr_0276_mission_transfer_mode_20261010.md. The default-attempt terminal refresh is an existing GQR0272 continuation; no second ordering finding. BR0342 authentication and GQR0102 source-generation definitions were read exactly and retain ownership. Current atomic registry/save and content-lock repairs qualify historical BR0472 claims; archive/derived-generation/current-set/cancellation transaction work stays with existing owners

Selected test_lan.ps13098-3188 launches native host and join through lan_launch, with actual process/network readiness checks. It does not drive request/grant/download/import/mode-sensitive finalization in this selected path. Search of maintained PowerShell/JSONC test/helper/script roots found no explicit requestMissionDownload/mission_download/mission_transfer/download_mission/missionTransfer/downloadMission entry; this is a bounded search result, not a claim that no possible UI-driven integration exists. Future acceptance must exercise the actual transfer service

Final disposition ISSUES/NO_INHERITED_EFFECT, terminalRETAIN, inherited saving0. Leading existing GQR0212 REFERENCE56 (32/0/7/10/7); new mode fix PRIMARY44 in the fix ranking. Preserve all old statuses/ratings. The concrete one-transaction promotion cleanup and exact existing-owner implementation/acceptance plans remain in this document. No production/test/helper changes or execution. Canonical publication and independent audit are next


## Chunk0353 canonical publication and independent audit complete

GQC-1181 ISSUES/GQD-1061 NO_INHERITED_EFFECT; terminalRETAIN/GQR0212 REFERENCE56. All737 assigned frozen/current lines, full addition+737/-0, empty current delta and51 contexts verified. NEW GQF0290 OPEN/GQR0276 TODO, P2/high priority44 (23/0/4/10/7): carry admitted lobby mode through download finalization instead of hardcoded coop. Dedicated implementation/actual-service acceptance plan. Corrective refresh attempt0 extends existing GQR0272; no second ordering root or old status/rating change. One-transaction promotion planned; existing bounds/source/authorization/import owners and current repairs preserved. Inherited saving0; no runtime reproduction

Independent audit PASS3051 SHA8a5fc6227121889d745b94b1f4d3d2c11751cf0477bac2ffe1891d777e9bbfa2. Reconstructed all five canonical files exactly; old rank payloads, finding/fix definitions, investigations and other queue rows preserved. Verified every assigned added line, whole read union, source/blob/base/original/manifest/delta, source/consumer checkpoint-audit chain,51 exact contexts with two unchanged relocated canonical rows, both plans, actual mode/refresh/host-ordering/Ready/start source and local simplification/current-save assertions. Product diff unchanged between publication and audit

Snapshot SHAb1764436f4a931fd5d3352044e0a6f624a2dec5d26421151b5a7bfca20e7841b; report SHA3dee3f3793210f7f6c8e6a828b6611bf07a21d97db8de606ca702f6a1c11e80b; scope0f96ee68e345a02ce88f5356fe951151dd20c6cf68e09b293c1f29fd05866ab2. Snapshot/audit plan hashes precede this closure and admission note

GQ2 now353DONE/274TODO/627 numbered;1168 terminal;276 fixes;12 investigations. Next0354. Remaining numbered scopes, supplements/sweeps/investigations, historical0116 audit debt and final live-head reconciliation remain required. Diagnosis/docs only; no product/test/helper edits or builds/tests/devices/probes/staging/commits. Goal active
