# GQ2 chunk 0154: network security resource diagnosis, 2026-10-10

- [x] Read assigned frozen hunk and entire XML; bind manifest, original absence and current source
- [x] Inspect main/debug manifests, build variants and current URL/authentication/reconnect consumers
- [x] Reconcile existing owner and record minimization disposition and validation limits
- [x] Prepare final report/scope, publish canonical records and independently audit exact bytes

## Assigned evidence and minimization

The single assigned frozen delta changes only the whitespace before the XML declaration terminator. All ten frozen lines equal current lines. The original 1996 tree lacks this authored Android resource. Main manifest line 38 selects the resource; debug manifest adds only SafTestProvider and supplies no transport override. The source inventory contains only this network-security XML and the main/debug manifests. No inherited engine hook, declaration or native policy is required by this packaging resource. RETAIN / NO_INHERITED_EFFECT; zero expected or applied inherited lines saved. Reverting formatter spacing would not reduce original-file merge pressure. No new abstraction or shared engine owner is warranted.

## Current transport and existing owner

BR-0343 remains OPEN and is the existing reference owner. Its full current section was read. NetworkConstants defaults to wss, but normalizeServerUrl (45-84) deliberately retains explicit ws. openConnection (332-363) stores the trimmed raw URL and passes the normalized Request to newWebSocket with no build-type cleartext rejection. Listener.onOpen (986-998) calls sendAuthenticate. sendAuthenticate/sendDevAuthenticate (406-446) requests a Play Games code when configured and otherwise sends the dev token. Reconnect (713-732) passes the stored raw URL back to openConnection. These are current source paths, not executed credential transmission or a new exploit reproduction.

Main XML explicitly permits cleartext for 10.0.2.2, localhost and 127.0.0.1. Build types (433-466) include release and non-debuggable internal initialized from debug with debug matching fallback. Source inspection supports retaining BR-0343; merged APK policy is not proved without packaging inspection. Current multiplayer UI visibility, exported-command authority, Play Games eligibility and actual endpoint reachability were not established here. Preserve their distinct existing owners and prior reachability limits. No new finding, rating, root, owner closure or runtime acceptance is claimed.

## Remediation and validation boundary

Continue the existing BR-0343 plan later: explicitly scope development cleartext to authorized development builds, deny it in main/release/internal, and reject non-development ws before opening or acquiring credentials. Do not treat NATIVE_DEBUG_BUILD as equivalent to a debuggable package: internal sets that native flag true while debuggable is false. Keep exported-command authorization under BR-0005 and endpoint identity/URL validation under their existing owners; changing this XML alone does not close them.

Required later evidence: inspect debug/internal/release merged manifests and resources; validate explicit, scheme-less, persisted and reconnect URLs for all three development hosts; verify rejection precedes credential acquisition/transmission in non-development packages and trusted wss still works. Runtime/security-sensitive reproduction remains deferred under the campaign constraint. Test-source searches found ws persistence examples, not packaging enforcement evidence; no test pass is inferred from those hits.

## Resume state

Source diagnosis complete; canonical publication pending. GQ2-CHUNK-0154 remains TODO. Canonical queue remains 294 DONE / 333 TODO of 627. Proposed next records GQC-1123/GQD-1003 require uniqueness verification before publication. Reference BR-0343 using its existing 56 MEDIUM-HIGH components 32/0/7/10/7; do not rerate the root. After 0154 the next unfinished chunk is 0296; 0155-0295 already complete.

Snapshot: temp/general_cleanup_20261006/gq2-0154-source-checkpoint-20261010.json
No product/source/test/helper edits, builds, device activity, probes, staging or commits. Existing concurrent source changes preserved. Overall diagnosis goal remains active.


## Chunk0154 published and independently audited

GQ2-CHUNK-0154 DONE / GQC-1123 ISSUES / GQD-1003 NO_INHERITED_EFFECT; RETAIN. BR-0343 REFERENCE56; current explicit ws source admission and main-resource policy retain existing owner, no newroot/status/rating/closure/runtimeproof. ReportSHA 35bdc0f7921dbe74e0afb9d43a4598570079fc43a53c6ca3978d133d6c39b3e1; scope 3d6eb96cbcb5408ad391384a6e771a48d95fc92cde79efdfca1351604669d558; snapshotSHA 265ba13cfa78ef3a7811d2c3e730d00e2f16f73d2dbb23d66d4d56f3205b72c5; independent audit PASS66 SHA 54ff8368d9bec011adaad2cf474a0e6e508cafa1c4b8bf0149e81de39f58d9ba. Exact permitted canonical bytes, prior semantic rows/ranks/owner records, source and range/manifest/original absence/prior audit/product bindings verified. Audit classifier corrected to exclude two historical investigation-resume records; full audit rerun.

All0154 gates DONE. Canonical295DONE332TODO/627;1110terminal265fix11investigations. Next unfinished0296 Android instrumentation three related paths683reviewlines. Remainingchunks/supplements/sweeps/historical0116auditdebt/finalheadreconciliation remain OPEN. No code/test/helperchanges or builds/devices/probes/staging/commits. Overall diagnosis goal active.
