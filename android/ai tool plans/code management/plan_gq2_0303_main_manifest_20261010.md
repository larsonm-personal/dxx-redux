# GQ2 chunk0303: main manifest, 2026-10-10

Diagnosis and plans only; no implementation or runtime acceptance

- [x] Read all25 assigned frozen U0 hunks, whole176 frozen lines including supplemental closing tail, and current183 lines
- [x] Bind frozen/current/original/manifest identities and initial consumer ranges
- [x] Finish production consumer and existing-owner reconciliation
- [x] Final diagnosis/minimization report, canonical publication and independent byte audit

## Source findings and limits

Current delta adds only the seven-line non-exported MultiplayerGameService declaration in :game. Whole current engine and transport foreground services plus notification helper were read. MainActivity2445-2510 starts both protections, stops game protection on rejected start and stops both on runtime teardown. The transport service hosts separate runtime IPC/deadline/lease/wake-lock ownership; it cannot be collapsed into the game service based on shared notification construction. No fresh lifecycle, freezing, foreground permission or timeout acceptance is claimed

Whole LanJoinLinkActivity/LanInvitation were read. Exported descent link validates ACTION_VIEW and exact IPv4 syntax, bounds input and rejects broadcast addresses supplied by the host helper before forwarding a unique request to SetupActivity. Do not replace parser validation with an unverified narrower manifest host filter. Final production request consumption remains pending

Build259-289 removes Play app-ID/provider from the generated direct-install manifest and adds legacy VM-safe-mode attribute; 385-405 binds application IDs and placeholders, 420-448 diagnostic package suffix, 473-490 source selection and538-552 conditional dependencies. Source inspection only; merged manifests, packaged classes, placeholder merge and variant runtime behavior remain unverified

FileProvider authority uses applicationId and bounded FileProviderGrantStore1-96 uses BuildConfig.APPLICATION_ID. Whole XML backup/extraction resources exclude client_identity.xml with cloud/device parity; FileProvider XML exposes named cache subdirectories. Previews231-272/183-224 retain legacy onBackPressed cleanup, supporting their manifest Back declarations. These reads do not certify all backup payloads, grant lifetimes, native Back routing or external Activity inputs

Provisional RETAIN and NO_INHERITED_EFFECT: manifest is authored Android source absent at1996 original; inherited saving zero. No finding/fix ID allocated, rating/status changed or canonical DONE credit claimed. Existing owners and remaining application/worker/native/permission consumers must be reconciled before final publication

## Checkpoint

Evidence: temp/general_cleanup_20261006/gq2-0303-source-checkpoint-20261010.json
Canonical state remains302 DONE/325 TODO of627;0303 TODO. No source/test/helper changes, builds, devices, probes, staging or commits. Preserve concurrent product edits


## Application, native components and existing owner supplement

Whole DxxReduxApp73 lines binds graphics safety installation and xCrash initialization. ArchiveFiles190-208 and ExtractionLimits1-25 bind the declared256MiB decoder ceiling; heap/runtime memory acceptance remains unproved. Preview launch ranges validate request game before selecting native D1/D2 library. Metadata bridge2198-2235/service2302-2375/subclasses2568-2580 support retained isolated workers; request game/process matching needs final reconciliation, no complete worker claim

SetupActivity183-216/2789-2803/4220-4240 and LanDiscoveryTab538-558 bind receive/restore/consume and resumed/permission/callsign/conflict gates. MainActivity4103-4167 routes legacy Back edges to the native menu; this is bounded key routing evidence. LobbyService1043-1074 acquires the Wi-Fi multicast lock. No actual links, Back gestures, process death, socket traffic or permission requests were executed

Whole current network security XML and exact BR0343 section15297-15312 read. Existing owner remainsOPEN56MEDIUM-HIGH32/0/7/10/7; manifest unconditionally references developmentcleartext policy. Reference existing owner, don't allocate a duplicate or imply source inspection proves packaged release behavior. Final component/foreground/launcher owner reconciliation and publication remain pending

Checkpoint now contains36 fresh context ranges with exact current file/range hashes. All bound contexts rechecked unchanged and product diff hash remains95d3e48b8345ea8b6b9a548cfe726868b45261f5d5300009c3ed56cbfbfdbc5c. Canonical queue remains302DONE325TODO;0303TODO


## Chunk0303 published and independently audited

GQ2-CHUNK-0303DONE/GQC1131ISSUES/GQD1011NO_INHERITED_EFFECT; RETAIN. All25U0hunks/frozen176/current183delta and49freshcontexts covered. ExistingBR0343reference56 andGQF0224/GQR0209OPEN/TODO47 retained unrescored; no newroot/fix/investigation/rating/closure/runtimeacceptance/inheritedsaving. ReportSHA 73c7fbd1a9f5e69296fcc750aa73332cfc161d3b6b92fa07795ceacbb26696c7; scope 60c6a6c864ab29b17319b9e26dc83a569c9a26473aa667e0fbe000b7258bf213; snapshotSHA 0047338c35271187ccc11663b9b141261879de8552d52e4d58d631c33174adcf; independentpublicationauditPASS148 SHA 610f742d3622d5883991a3f8429a52a95e6b60e625fe5c8688a30c00991a0475. Exact5canonicalbytes/import, priorterminalsemantics/ranks/fixandownerpreservation/source/ranges/manifest/originalabsence/currentsole7lineinsert/prioraudit/product verified. Scoped docscheckPASS

All0303gatesDONE. Canonical303DONE324TODO/627;1118terminal267fix11investigations. Next0304 tenrelatedJava paths730assignedreviewlines. Remainingchunks/supplements/sweeps/historical0116auditdebt/finalheadreconciliationOPEN. No code/test/helperchanges/builds/devices/probes/staging/commits; overall diagnosisgoalactive
