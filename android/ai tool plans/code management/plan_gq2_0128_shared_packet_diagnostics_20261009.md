# Shared paired UDP packet diagnostics proposal

Diagnosis-only admitted maintenance candidate GQF-0278/GQR-0264; parent chunk0128 DONE and independently audited. Score71HIGH12/35/7/10/7; no implementation or applied saving

## Measured scope

D1 net_udp.c243-408 and D2 net_udp.c251-416 contain identical166line blocks, LF SHA256205099ab001125e81113cec8dd4bf594dae326b281be55b2035dec6f28d65b6c. Same exact bodies/locations at frozenheadb4997ac4115a3b2ac6d6cf0b8e1459675a7744ef; all332lines added by paired frozenhunk8 from base7877ad30d05887b8e19869ed4c50075e41e2f88e. Fence symbol absent from base and originalfb555eec75e1ed12c8348805ab335afb4c721b06

## Proposed later implementation

Move only this exact diagnostic block into android/app/src/main/cpp/shared/net/net_udp_packet_test_impl.h, included once at each original position within existing __ANDROID__/INTROSPECT_ON guards. This preserves static per-game state, later native ACK/drop references and existing transport-private declaration access without introducing wrappers or new runtime ownership. Keep native queue declarations/retrycursor, ACK instrumentation, direct-send drop hook, reliable_pending, public net_udp.h declarations and native game-info fixture body/ordering intact. Preserve handmade comments and exact statements. No desktop feature or wire format changes

Expected net inherited reduction332minus2includes=330lines across two native files. Shared added file166lines counted separately; no applied saving. This reduces duplicated branch diagnostics, not original engine code. Do not combine with earlier GQR0243 packetoptions or GQR0263 countdown state extraction savings

## Remaining diagnosis gates

- [x] Compare exact current and frozen paired166line blocks and attribute every line to frozen additions
- [x] Read bounded paired native ACK/drop consumers and existing shared implementation inclusion pattern
- [x] Read maintained whole packet fixture scripts and paired runner call sites, without execution
- [x] Verify paired declarations and remaining lexical dependencies/include availability
- [x] Finish duplicate-owner reconciliation and parent0128 immutable evidence audit/publication before canonical admission

## Later implementation acceptance

Build both Android game targets with diagnostics on and off; preserve desktop build guards. Verify exactly one per-game definition and visibility to later hooks. Run maintained paired maximum payload retry, reliable boundary/level-sequence and released-world MDATA fence scenarios in both games, including their native ACK/control/score/recovery/life/inventory oracles. Preserve same logs and durable result/freshness requirements. Recompute inherited original/base/head metrics; count shared additions separately. Source-only fixture review is not execution evidence, whole network admission proof or a fix to static baseline/session lifetime findings

## Chunk0128 independent audit complete and terminal handoff

Independent post-import auditPASS; artifact temp/general_cleanup_20261006/gq2-0128-publication-audit-20261009.json rawSHAf54be0dc7167cb9277e8d56db947dff2d822c31d8e71e6d13d6bf560c9ca5c84. Exact unique report import matches6e0451af6ea47f1eb2c0970a50f5df4b3c17b0001de11831797edeeece8f7403; snapshotSHA dc6bef9b48db380f1bd8bbab0de350650b2b93369f6170ea8fc272e0e1142d95; scopeSHA c6ab23f836a9376888c5dc42df349584ac01224b79454c143b38375e1a2287ff. All136 current source ranges/raw identities,71 complete tracked deltas and69 prepublication canonical lines verified. Corrected CMake hash applied explicitly; paired166line candidate extent verified within182line frozenhunk8

Eight added semantic rows and one removed old0128TODO row only; earlier canonical semantics/owner states/scores retained, BRactive/archivebytes unchanged. Fix264rows and terminal1084rows contiguous and correctly descending byscore/H/M/C/R thenascendingID, B omitted; GQR0264rank6 and0128terminalrank30. Evidence/inherited/resume raw prefixes preserved, parentplan only expectedgate4edit beforeaudit. Allfive0128 gates nowcomplete; GQC1097ISSUES/GQD0977CANDIDATE/GQF0278OPEN/GQR0264TODO71HIGH12/35/7/10/7;330expected inherited saving and166shared additions separate,zeroapplied

Numbered269DONE358TODO; next frozen unit0129. Preserve0116 historical raw-byte audit debt, supplemental coverage/sweeps/finalhead closure and current source limits. Overall goal remainsactive. This terminal coverage audit doesnotimplement or validate the later extraction. No product/source/test/script edits/builds/tests/devices/probes/staging/commits
