# GQR-0276: Preserve lobby mode through mission download finalization, 2026-10-10

Diagnosis and implementation plan only. Proposed GQF-0290/GQR-0276; admission occurs with chunk0353 publication. No implementation or runtime reproduction

## Source-backed defect

MissionTransferService.kt506 always calls MissionCompatibilityResolver.resolve(context, requirement, "coop"). Its download, receive and finalizeMission parameters do not carry the admitted lobby mode. The current LobbyService grant caller1906-1938 supplies the requirement and attempt but drops joined.mode

LAN creation explicitly enables mission downloads and passes the selected mode to hostLobby (LanDiscoveryTab1112-1164). The host dialog offers managed transferable wrappers independent of coop mode (CreateGameDialog567-586,660-680), MissionInfo.transferable50-74 has no coop restriction, and download UI235-267 is likewise mode-independent. MissionCatalog15-21 excludes anarchy-only entries for coop. MissionScannerManagedArchiveTest135-144 demonstrates that an imported anarchy-only wrapper belongs to the anarchy catalog and is excluded from coop

A missing joiner can accept a valid host offer in anarchy/team mode, receive and verify the exact wrapper, import it and promote it, then fail the hardcoded coop catalog lookup at507. The download catch188-201 publishes ERROR/mission_finalization_failed using the transfer attempt and calls onFinished(false). This misclassifies successfully installed compatible content. Preserve the actual-mode admission rule; do not solve it by accepting anarchy-only content in coop

Current full frozen/source SHA55c76daf9b7c79ad8ee674ce2d93282fecb6857b846c2a5b0de94be53761df3f,737lines, one complete assigned addition+737/-0. The file is absent at the GQ2 base and1996 reference. This is an assigned change, not a preexisting enclosing-code defect

## Recovery and related ownership

ERROR permits periodic resolver refresh. JOIN_ACK2042-2058 uses the actual lobby mode, so client compatibility can recover. But resolver102-112 emits default attempt0/no transferId. When the host has accepted a transfer attempt>=1, shouldAcceptMissionStatus190-193 rejects that correction. Known-member handleJoin1566-1592 preserves missionStatus; Ready1673 and start2117-2123 use the retained host status. There is a source-admitted schedule in which client recovery leaves host Ready/start blocked. New membership can reset that owner, and some packet orderings can differ; no inevitable permanent failure or runtime reproduction is claimed

The corrective refresh's ordering identity is an extension of existing GQR0272, not a second new finding. BR0440 covers admitting anarchy-only content into coop; this root rejects valid anarchy content during download finalization. GQR0270 concerns short-name ambiguity, GQR0102 source bytes, BR0342 authenticated lobby authority, and BR0186/0103/0472 import publication. Preserve those owners and statuses. Canonical active/archive/general/minimization plans and outstanding bugs were searched for finalization and anarchy-transfer/hardcoded-coop mechanisms; no exact competing root was found

Proposed P2/high, priority44 MEDIUM (23/0/4/10/7). Incorrect visible finalization and a Ready/start obstruction with membership-replacement workaround; no crash/data-loss/exploit claim. Inherited saving0

## Implementation

- [ ] Capture the current admitted lobby mode with the accepted transfer request and pass it through download/receive/finalizeMission to the compatibility resolver
- [ ] Treat mode, lobby membership, requirement and selected content generation as one captured operation context; reject stale publication if that owner changes. Use the existing owner/cancellation boundary
- [ ] Keep chunk/full digest checks, writer retirement, one current transfer callback, import admission and host Ready/start checks
- [ ] Use the requested mode for final native-authoritative catalog matching; no duplicated Kotlin descriptor parser, wire compatibility layer or inherited engine edit
- [ ] Coordinate GQR0272: a terminal resolver refresh must retain or explicitly replace the admitted ordering identity so the host can accept a current correction and reject delayed old-series reports. Do not simply remove the lower-attempt comparison
- [ ] Keep typed import failure distinct from unsupported mode/content, preserving source/registry rollback in its existing owner plan

## Future validation

- [ ] Drive actual production request/grant/socket transfer/import/finalization for an anarchy-only managed wrapper in supported anarchy/team modes; require first terminal MATCH, correct exact bytes/identity, and both endpoints Ready/start before and after refresh
- [ ] Include ordinary coop-compatible wrappers, incompatible coop admission, missing/disabled/different hash, copy fallback, failed import, cancellation, changed mode/requirement and leave/rejoin controls
- [ ] Exercise finalization ERROR followed by a successful actual-mode catalog refresh with host attempt1/2/3 retained; require accepted current recovery without stale-report rollback
- [ ] Extend maintained resume/refresh/catalog fixtures and actual LAN integration. Existing helper assertions do not drive this completion path; test_lan.ps1's selected3098-3188 path launches native host/join directly through lan_launch
- [ ] After implementation run scoped quality, relevant JVM/native/Android builds and actual multiplayer integration. Register any new runner using repository rules. No builds/tests/devices/probes run during this diagnosis

## Admission

Admitted with audited0353 as GQF0290 OPEN/GQR0276 TODO, P2/high priority44. Publication audit PASS3051 SHA8a5fc6227121889d745b94b1f4d3d2c11751cf0477bac2ffe1891d777e9bbfa2. Plan hash in snapshot/audit precedes this admission note. Implementation and runtime acceptance remain pending
