# GQ2 chunk0351: proxy packet diagnostic ownership, 2026-10-10

Diagnosis and implementation plan only. Proposed GQF-0289/GQR-0275; canonical admission pending. No product/test/helper edits or runtime reproduction

## Evidence and impact

LocalhostProxy.kt393/396 owns pdataForwarded and mdataForwarded. Its shared receive delivery458-463 and per-peer receive552-557 classify the first payload byte using0x22/0x25/0x26. Both d1/main/net_udp.h115/124-125 and d2/main/net_udp.h117/126-127 instead define PDATA16, MDATA_PNORM17 and MDATA_PNEEDACK18. Paired native senders initialize len=0, store those symbols at buf[len], append the remaining payload and send that same buffer.

Consequently the periodic logcat category totals do not measure the named packet types. Real PDATA and MDATA contribute to aggregate receive totals but not these categories. Other first-byte values can falsely contribute. There is no claim of packet loss, changed forwarding, broken IPC/overlay totals or a reproduced gameplay failure.

The six-line classification body is unchanged across base/frozen/current, in both receive paths after indentation normalization: SHA256 0d5fa1f1ca23a89f2cc2cfeba6b53893b90add6144843380a21717e6b1230c1f. Base408-413/498-503; frozen424-429/514-519; current458-463/552-557. This is a pre-existing enclosing-source defect, not introduced by the22 assigned hunks.

P3/high source confidence; priority33 LOW, H/M/B/C/R=12/0/4/10/7. The impact is misleading network diagnosis and duplicated engine-format knowledge. It is separate from BR0341's stale diagnostic generation, BR0498's membership/snapshot lifetime, BR0132's relay authority, BR0446/0447's logging budgets and GQR0228's signed packet CLI formatting. GQR0264 shares paired native diagnostics and does not repair these Kotlin constants.

## Implementation

- [ ] Remove pdataForwarded/mdataForwarded fields, both payload classification blocks and their periodic logcat category labels
- [ ] Preserve aggregate receive/send/byte counters and their existing meaning, transport direction/peer context, failure details and ProxyDiagnostics actual-success totals
- [ ] Keep engine-native semantic packet diagnostics as the format authority; add no Kotlin packet table, JNI helper or protocol abstraction for this logging
- [ ] Extend the actual production proxy fixture's log capture and forwarding coverage, without removing its gameplay/keepalive failure checks
- [ ] Run scoped quality, the maintained Gradle unit runner and relevant Android compilation during implementation; use ordinary D1/D2 multiplayer acceptance to verify diagnostic usefulness

This is a local removal. No inherited edit, wire change, IPC schema migration or change of retry/shutdown policy is required. Do not fold relay admission, packet budgeting or job ownership fixes into this patch.

## Required acceptance

The existing host proxy tests compile production sources and run real UDP traffic, but android.util.Log currently discards every line. Capture the actual affected sink. Drive shared-socket delivery and per-peer receive, covering direct and relay stripping. Send opaque payloads beginning with the actual native categories and with the previously misclassified values, including lengths beyond one byte. Use native-header-derived values for semantic assertions if needed, rather than a second maintained protocol table.

Verify exact payload forwarding, unchanged aggregate counters/IPC shape, absence of misleading pdata/mdata labels, and retained transport context. Keep existing injected gameplay-send recovery and keepalive-failure retirement/original-error evidence. Protocol-specific behavior is not required in the proxy after removal. Native source review and ordinary paired-game diagnostics establish that semantic categories remain available at their engine owner.

Source evidence establishes the mismatch, not runtime reproduction or completion of existing admission/race/MTU owners. No implementation or test execution in this tranche.


## Canonical admission

GQF-0289 OPEN/GQR-0275 TODO admitted by audited0351. P3/high source confidence; priority33 LOW(12/0/4/10/7). Publication audit PASS2758 SHA869ee957e0944425b61c1c465303194cf984f002ee159003000be9b0d3cb7088. Implementation and runtime acceptance remain pending; no code edited
