# Native route audio and crash diagnostics continuation, 2026-10-09

GQ2-CHUNK-0070 diagnosis complete: all11 assigned scopes and complete current sources/deltas reviewed; named production consumers reconciled. No implementation or fresh runtime acceptance.

## Existing-owner acceptance

- GQR-0160: establish callback lifetime ownership before any device/hidden/buffer dereference. Coordinate player handle publication/background operations/teardown; preserve supported-format and failure-cleanup repairs. Barrier-test entry versus close/reopen/enqueue failure under deferred race instrumentation
- BR-0233: checked complete route JSON/checkpoint and audio WAV/stem/metadata publication, explicit write/flush/close failure status and prior-generation preservation. Keep source identity and whole-request success correlation, not merely existing final file or earlier OK line
- BR-0240/0241: coherent breadcrumb publication/read boundary and process-lifetime snapshot ownership; session-specific restore marker does not repair shared breadcrumb path or plain-slot races
- GQR-0167: strict crash/outbound JNI conversion, well-defined diagnostic truncation and first-exception acquisition cleanup; preserve completed0039 scopes
- Controller/briefing: retain documented Kotlin/native constants, normalized controller precision and paired Android-guarded conversion. Preserve engine format/layout ownership and bounded diagnostic interfaces; avoid broad upstream duplication cleanup
- Route: retain isolated filesystem, dummy audio shutdown order, canonical diagnostic seed and explicit sandbox/transition semantics. Inspect actual terminal status rather than invent generic start-failure hang; named validation failures set terminal status

Deferred malformed/media/allocation/resource/race/CheckJNI probes remain explicit. Implementation tranche should exercise actual producers/consumers with fixed expected output and authoritative process outcomes, then relevant native/platform checks. No new finding/status or original-file saving.

Terminal report SHA2563e637bd150909c1f6f142a8029bd05accde637ab6994d439f8917b30001fdb6d; scope fingerprint2be7c919716896539cd68613699a9ff1e4316e33f0bd9dd4984c63e530604653.
