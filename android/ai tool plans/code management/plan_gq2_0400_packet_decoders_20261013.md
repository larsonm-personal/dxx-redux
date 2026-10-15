# GQ2 chunk 0400 packet decoder diagnosis, 2026-10-13

Diagnosis and plans only, serial under plan_general_diagnosis_resume_20261009.md. No product/test/helper edits, builds, tests, device/network calls, probes, generators, formatters, staging or commits. Preserve the ignored local evidence ledger and Google Drive backup.

- [x] Read exact frozen/base assigned source and current correspondence; audit source checkpoint
- [x] Review ignore producers, native packet layouts and paired diagnostic consumers
- [x] Deduplicate layout candidate and retain existing session/signed-owner/log/error owners
- [x] Finish authored simplification, inherited disposition and direct rating
- [x] Publish canonical report/snapshot/dispositions and independent audit before DONE

Assigned scope: android/tools/code-quality/.gitignore L1-L4 and android/tools/decode_object_packets.py diff hunks 1-23, new L20-L393. Four ignore lines plus 118 old/new decoder review lines = 122 assigned lines. Full current bodies are 4 and 413 lines; extra whole-file context is not another queue assignment. Decoder frozen changes are formatting only: base and frozen ASTs agree without executing either source. Current delta contains nine complete hunks retaining bracketed log admission and nonzero/suppressed-loss behavior for incomplete decoding.

Prior 0399 publication PASS4185; 399 DONE / 228 TODO / 627 units. No 0400 finding or terminal coverage is admitted yet. No inherited file effect or saving.

Source-static context lead: D2 adds a 65-byte asset-identity field after the nine common bytes. UPID_OBJECT_HEADER_SIZE is 74; both diagnostic tools start entries at byte 9. The ordinary D2 identity writer zeroes all 65 bytes. A native complete END record is 83 bytes with local -2 at 74; tools instead read local 0/owner 0/remote 0 from identity padding and expect a 264-byte object body at 18, needing 282 bytes. This is a source-derived valid-producer witness, not an executed packet, CLI result or runtime failure. Current native receiver logs accepted RX packets. Existing fixture constructs the nine-byte layout only.

Future proposal, conditional on dedup: select explicit game/protocol layout or require unambiguous validated admission; support native D1 header 9 and D2 header 74, including zero/32/64 identity profiles. Preserve native network identity/security and signed owners, markers, 264-byte bodies, bracketed logs, complete decode failure status and comparison suppression. Do not alter native wire layout to match diagnostic assumptions. Keep BR-0609 session/token aggregation as a distinct existing owner. Remaining whole-owner and fixture/control qualification is required before admitting a finding.

Routine authored cleanup lead: expected_full, remaining and tx_init assignments have no consumers; bytes_lost subtracts the same raw length from itself. Reconcile any proposed deletion/output change with existing diagnostic contracts and session-owner plans. No deletion or behavior change implemented.

Retention helper initially rejected the default 4 GiB reserve (C: about 3.79 GiB free). Its explicit 3 GiB reserve passed for these small documentary artifacts; removed zero files. No build/cache/evidence cleanup. No canonical evidence-ledger rewrite in this checkpoint stage.


## Entry context checkpoint

Entry checkpoint SHA 35cf112e561363e90f720bc4ae01399f31e8990d325336a26e04700bbeef8bdc; independent PASS69 audit SHA 062f65b52f54f96f71adbbd21eab88772f55a68899a5d3e12bd5cb4a598b8694. Seventeen exact fresh source contexts / 1007 lines, two complete current BR owner definitions and three complete FIXED description rows. Whole PowerShell decoder426, maintained CLI fixture225 and runner20 read; bounded native/identity/installer paths retain explicit range limits. Source-level mismatch witness, no executed packets/test/build/network result. Earlier attempted entry collector stopped on backticked root row matching before writing any artifact; current collector uses parsed cells, and this independent audit binds every exact whole row.

Only source and entry-context gates checked. Chunk0400 remainsTODO; no new finding/remediation/rating/terminal admission. Candidate plan is unadmitted. Complete wider dedup/frozen layout attribution, authored cleanup/direct rating/consolidated consumer and canonical publication next.


## Consolidated disposition prepared

Full selected owner/description-row dedup retains BR-0609 session correlation and FIXED BR-0608/GQF-0242/0243/0244. Whole canonical BR body search yields only0608/0609 for decoder/header identity symbols; ten current related root rows were read complete. Native asset-source transaction0212/save pause0256 and extraction0278/counter0289 are distinct. Truncated broader decode root output earned no credit; selected complete rows recovered independently.

Fresh frozen producer/header/identity ranges and base producer prefix prove header74 was introduced before frozen review while decoder23hunks are AST-formatting only. Native D2 receiver boundary loop3159-3195 freshly read; valid END83 passes structural count/record/end-offset admission before RX logging. No packet executed.

Consolidated proposed NEW GQF-0309/GQR-0295: diagnostic protocol layout mismatch, P2/high47MEDIUM23/0/4/10/10. Correct tools and paired producer-derived fixtures; retain native identity/wire/security, signed owners, error status and session partition owner. No inherited proposal/saving. Routine unused expected_full/remaining/tx_init have zero static loads; optional later removal must preserve diagnostics and future session design, not a separate defect or status change. bytes_lost and objects_lost_in_packet have no local output consumer in reviewed tools/tests/helpers; bounded search only, no external API claim.

No new canonical finding yet; admission only after consumer audit and immutable canonical publication.


## Chunk 0400 audited canonical publication, 2026-10-13

COMPLETE. GQC-1229ISSUES/GQD-1109NO_INHERITED_EFFECT;GQ2-CHUNK-0400DONE. All five diagnosis gates checked only after independent publication PASS5648 and current five-canonical identity verification. New GQF-0309OPEN/GQR-0295TODO P2/high47MEDIUMPRIMARY23/0/4/10/10 actual protocol layout mismatch. Source-static native/frozen/current witness; no decoder/test/network execution or implemented saving. Existing BR0609OPEN and BR0608/GQF0242/0243/0244FIXED preserved.

Report SHA 083a3b2efe40790397fe8ebc2126c6b3ee5acba2c08c9f7664dc6621851394ac;immutable snapshot SHA a626df2578e5936014c71f3426e90f3f5fba533283beb723aabbfa198b6bb85c (seven qualification artifacts). Nine-operation mutation SHA 8550e63f33c490fded881f3f73caa9310e6409f9a60e81290337e5276fa8248e; independent publication audit SHA 06286f285b87354cc17731d969627b77cee1e990e15dd00a0b39aad618523d86. Exact canonical replay preserves all prior whole roots,queue bytes,rank suffixes/endings/sorts and exact evidence import. Current/base/frozen contexts,source identities/AST/hunks/manifest/owners/root rows/prefinal plans/HEAD/product verified.

GQ2 now400DONE227TODO627;1216terminal ranks295fix ranks309findings295remediations12numbered investigations. Next numbered scope0401:seven related CMake paths433reviewlines. Broader queue/preflights/sweeps/investigations/finalHEAD and dirty-untracked supplements/closure remain pending. No diagnosis product/test/helper changes,build/test/device/network/probe/formatter/generator execution,staging or commit. Canonical evidence ledger remains ignored local file; Drive backup receipt pending.


Drive storage receipt0400: DxxReduxEvidenceLedgerBackup reachedReady/LastTaskResult0 after explicit trigger and Running267009 observation. Local Drive copy388658869bytes/checksum e40932e56c6ce2d405ce8221f540512e19e0422e857e209dfdc78f8dae503953 and sidecar match audited canonical evidence ledger; no pending copy remains. Helper verifies copied bytes and unchanged source before replacement. Cloud sync completion not claimed.

Next0401 exact grouped scope recovered: audio-tag-metadata-deps.cmakeL1-L62,dxx-headless-targets.cmakeL1-L96,dxx-sanitizers.cmakeL1-L26,dxx-verified-dependencies.cmakeL1-L69,fluidsynth-music.cmakeL1-L107,input-demo-codec-deps.cmakeall3hunks/newL3-L15,ymfm-music.cmakeL1-L62. All seven current whole bodies freshly read without truncation and current delta empty. Codec3hunks replace Git FetchContent declarations with verified archive helper; six wholly added modules read through identical current bodies. Discovery only until source checkpoint/audit; no0401 terminal/consumer credit or new owner claim.
