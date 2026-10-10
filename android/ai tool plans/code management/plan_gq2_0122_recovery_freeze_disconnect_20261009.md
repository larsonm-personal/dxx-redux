# Cooperative recovery freeze cancellation and departed-peer reconciliation

Diagnosis-only implementation plan; no product changes or test execution in this tranche. Candidate finding is not yet admitted to the canonical ledger. Chunk0122 publication must assign fresh IDs and reconcile existing owners before treating this as a new canonical finding.

## Problem and evidence

A dropped-gear owner returns while the surviving host and another active peer remain. The host snapshots the third peer into freeze_waiting. If that peer disconnects before its matching REC_FROZEN reply, repeated rejoin readiness calls retain the bit and retry forever. The same surviving host has no traced disconnect/cancel/frame operation that terminates this wait. Recovery save readiness and world suspension/retirement remain blocked. This is a source-supported liveness defect, not a device reproduction.

Current shared coop_recovery.c: L533-L558 snapshots active slots and retries; L607-L620 blocks save and retirement; L634-L658 skips reclamation while waiting; L732-L738 clears a bit only for matching item revision; L796-L820 only binds objects. Paired multi disconnect and net_udp disconnect were inspected in the chunk continuation evidence: connection state and reliable tracking are cleared without a recovery cancellation hook. Shared join-wait cancellation restores transfer/roster flags without ending recovery freeze. New-game/full-restore/new-host replacement is a different recovery boundary and does not repair ordinary same-host progress.

Frozen assigned source is branch-added: base and 1996 original absent. No assigned inherited line saving. Current recovery whole LF SHA256: a5d76cf188ad8c8057c9d90b6162451ca8127f24fb01b3bd3683e5d9602a38de. Exact source/caller/fixture ranges and current deltas are bound in plan_gq2_0122_coop_recovery_continuation_20261009.md; implementation must refresh them against the then-current tree.

## Ownership and preserved behavior

This root concerns the lifetime of a recovery freeze attempt. GQF0254/GQR0240 owns final native ammo grant inflation, GQF0202 owns completed duplicate eligibility extraction, BR0195 owns packet authority, BR0206 owns coordinated restore failure, BR0278 owns duplicate autosave cadence, BR0609 owns diagnostic join-token comparison, and archived BR0225 owns authenticated reconnect identity. None of their inspected acceptance descriptions resolves an abandoned recovery wait. Keep their statuses and acceptance separate; do not close them through this work.

Retain shared Android recovery ownership and narrow paired engine hooks. Preserve monotonic gear intersection, object generation and reverse mapping checks, epoch/item/revision/life fencing, capacity and overflow credit, persistent absent inventory with optional cooperative QoL both on and off, and optional full-save versus mandatory world-source validation. Do not add another inventory ledger or rewrite the inherited death decoder; its remote_created loop is already present at the campaign base.

## Implementation sequence

- [ ] Model an explicit freeze attempt lifetime in the existing recovery owner. Bind awaited participants to the admitted connection identity/generation, not a bare reusable roster slot. Define success, participant loss, join cancellation, bounded no-response failure, and authoritative reset outcomes before editing callers.
- [ ] Reconcile participant loss and cancelled transfers through one shared operation, reached by narrow paired disconnect and actual join cancellation hooks. Stop retrying an abandoned attempt. Late replies from that attempt or a replacement connection cannot complete a later attempt.
- [ ] Preserve every known pickup reduction. A missing peer may have collected gear before reporting it; clearing its bit and granting the host's larger remembered amount is unsafe. Returning RECLAIMING to unrestricted LIVE/CREDIT or simply skipping the readiness test is also insufficient.
- [ ] Resolve the uncertain remainder explicitly within the existing ledger before enabling save/world progress. The implementation design must specify which quantities remain proven recoverable and how an unresolved remainder is retained without regranting it, including save/restore and authoritative migration. If the current states cannot represent this distinction, extend the Android-owned recovery representation directly and update its producer/consumer admission. Do not invent native save-format compatibility or a parallel ledger. Record the chosen conservation rule and why later peer return cannot duplicate or silently erase inventory.
- [ ] Complete or fail the affected join explicitly within a bounded attempt lifetime while allowing the surviving session to save and continue after that outcome. A failed join alone is not sufficient if global save readiness remains permanently blocked. Preserve the interrupted-return inventory cache and keep unrelated recovery items unaffected.
- [ ] Keep deadline/retry policy in the shared owner, using existing engine time semantics. Refresh connection admission, slot reuse, host migration and source-world readiness integration before implementation; elapsed time alone never proves an uncertain inventory quantity.

The uncertain-remainder representation is a required design decision, not an implementation already selected or proved safe. Blind pruning, forgetting gear, granting unconfirmed quantities and indefinite refusal to save do not satisfy this plan. Resolve this decision before implementing cancellation.

## Maintained validation

- [ ] Extend the real recovery native fixture with controlled timer progression and participant lifetime changes. Start a freeze with a third participant, disconnect it before acknowledgement, repeat readiness calls, and assert a bounded explicit outcome plus restored session save readiness. Assert unchanged known quantities and no unconfirmed grant.
- [ ] Cover third-peer local pickup immediately before loss, delayed collection and old freeze replies, duplicate replies, same-slot replacement, repeated owner return, cancelled joining transfer and retry. Check each item/revision/epoch boundary; no old attempt clears a new wait.
- [ ] Round-trip the chosen unresolved disposition through actual recovery save/restore and cold absent inventory, then return the disconnected peer. Compare exact inventory plus world/recovery quantities; retained uncertainty must not duplicate or disappear through serialization or migration.
- [ ] Preserve successful all-peer freeze, zero-peer freeze, late successful acknowledgement, partial capacity/overflow credit, object expiry/reuse and world suspension controls. Include D1/D2 and cooperative QoL on/off. Keep final native grant tests under GQR0240 rather than replacing them with stand-ins.
- [ ] Extend the maintained LAN runner with an actual three-peer scenario. Keep the host process alive; owner leaves and returns, awaited third peer leaves before FROZEN, and assert terminal join outcome, bidirectional remaining-session progress, ready_to_save and a real save/restore. Use durable per-device run IDs/results and exact inventory/provenance quantities. Source inspection or a two-peer test is not this acceptance.
- [ ] During implementation, run relevant maintained host/native and actual LAN acceptance to completion, paired Windows/Android builds, catalog checks for added top-level automation, and scoped quality. No such execution or passing result is claimed here.

## Completion

Close only after the actual disconnect/cancellation path terminates the wait, surviving-session saves and world operations recover, and conservation holds across uncertain pickups, persistence, replacement connection and retry. Keep diagnosis coverage completion separate from product remediation completion.
