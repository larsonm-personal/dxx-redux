# Portable co-op records and pickup restore diagnosis, 2026-10-09

Diagnosis only. GQ2-CHUNK-0084: portable header1-82, pickup implementation frozen diff hunks1-9/new5-321, header hunks1-4/new9-38. Current HEADb661b6eb24e91adf5f3e566bc28eb73d4b679fda. All three current files equal frozen headb4997ac4115a3b2ac6d6cf0b8e1459675a7744ef.

- [x] Read complete portable header and all assigned zero-context diffs; read complete pickup implementation/header for ownership and deletion context
- [x] Trace D2 portable capture/apply, prepared roster validation, host receipt and source rollback/apply callers
- [x] Trace save metadata pending ownership, optional discard and required-world/source-rollback completeness, paired native restore callers
- [x] Read complete focused pickup restore fixture and inspect paired native pickup/packet hooks
- [x] Reconcile prior GQ1-0185 ownership: BR0195 packet authority, BR0206 restore transaction, GQR0175 clocks; preserve completed GQR0189 reward policy as distinct
- [x] Verify exact frozen/base/original attribution and current whole-text equality
- [x] Publish/import immutable report, normalize terminal records and verify queue/counts/ranks/current bindings

Keep optional ordinary-save gear discard separate from required travel-world/source rollback. apply_pending returns success after deliberate discard; callers requiring complete ownership must inspect accepted/discarded against expected counts. Validated allocation adoption removes a second allocation point; do not restore the obsolete replace-after-validation allocation or fail every ordinary save for stale pickup metadata.

Portable record is a172-byte packed private Android convention. It carries bounded timers, inventory and recovery life/revision but no object identity. D2 capture/apply require co-op pause and valid alive player state; D1 implementations refuse. Existing local object/slot is retained. Prepared data must echo the local immutable record and zero inactive records; host receive admits current freeze generation and life/revision. Whole-roster life/revision preflight is not proof every later player/object check or apply succeeds atomically. Existing BR0206 owns transaction/source recovery acceptance; GQR0175 owns timer arithmetic and valid checkpoint domains.

Pickup restore validates terminated identities, live signature/ID/eligibility, remaps only a unique signature when the saved slot no longer matches, deduplicates by pickup signature/ID and stable client ID or callsign fallback, and adopts accepted records. Disabled duplication/allocation failure clears the section and reports all pending records discarded. Existing BR0195 owns packet authority: current paired pickup/snapshot dispatch still omits supplied authenticated sender, and wire signature is not compared before recording/rebinding the current object. No forged packet or unsafe fixture run.

Implementation acceptance stays with existing owners: carry admitted sender/host plus world/object identity into pickup and replacement-snapshot admission; preserve bounded snapshot counts and exact completion, reconnect identity and eligible-object semantics. Require actual paired ordinary save/remap plus required world/source rollback to preserve complete pickup ownership; distinguish discard diagnostics from successful complete restoration. Cover portable supported values and first actual native apply/consumer clocks with existing clock/transaction plans, keeping deferred malformed/security/allocation/resource probes deferred.

No new finding, product/test/script changes, build/configure, formatter/generator, runtime/device/deferred probes, staging or commit. All assigned files absent at1996 attribution; no new inherited saving. Preserve concurrent multiplayer work and native formats/desktop behavior.

### Portable co-op records and pickup restore terminal diagnosis, 2026-10-09

- Completed GQ2-CHUNK-0084: complete portable82-line header, pickup implementation hunks1-9/header1-4 with full469/47-line shared context,13 named bindings and paired native restore/pickup/dispatch plus portable roster/source callers. GQC1053/GQD0933; report import SHA256c68632304216b5e319ac9f5e29f28d3466eff26e26fae0ac1f7c01bb544d11cd; scope fingerprint `a9ec5cd8c1180780ea10fa75eecf6fa841969bdd41491d82556650d86c12b4d9`
- Optional discard/adoption and required world/source ownership completeness remain distinct. Extend existing BR0195 pickup/snapshot sender/object authority; retain BR0206 transaction/GQR0175 clocks and completed GQR0189 reward policy. No new finding/status/runtime acceptance or inherited saving
- GQ2 now225DONE/420TODO; GQ1 remains818DONE/1TODO;269findings255remediations72DONE182TODO1DEFERRED;194OPEN75FIXED;1040 unique sorted terminal ranks/255 remediation ranks. Next0085; all remaining preflights/sweeps/investigations/worktree/current-head supplements/closure required
- Only diagnosis documents/report changed; HEADb661b6eb24e91adf5f3e566bc28eb73d4b679fda/concurrent multiplayer work preserved. No product/test/script edit, build/configure, formatter/generator, runtime/device/deferred probes, staging or commit

###0084 terminal verification

- Exact immutable report import/body/SHA, all assigned frozen hunk/range/diff fingerprints and13 current bindings verified. Queue225DONE/420TODO,1040 contiguous unique impact-sorted terminal ranks/255 remediation ranks and269/255 unchanged finding/remediation statuses verified; scoped tracked diagnosis diff check passed.0084 plan gates complete; no product/runtime acceptance. Next0085
