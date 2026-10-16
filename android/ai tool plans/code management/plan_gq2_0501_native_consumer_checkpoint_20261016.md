# Chunk 0501 native and route consumer checkpoint, 2026-10-16

Diagnosis and plans only, serial work under plan_general_diagnosis_resume_20261009.md

Added35explicit contexts2214overlapping lines, including5whole files. Combined with the route predecessor this is49contexts3178lines/12wholefileinstances. Exact scoped bytes and current file hashes are retained; all12assigned current identities and all5canonical ledgers still match the source checkpoint

Progress: temp/general_cleanup_20261006/gq2-native-consumer-progress-0501-20261016.json
SHA6bfad416df713b3617edf6d50815807a69a2aa662b8f68fb6139c9b3c27cba6a
Independent scope/identity/current-source/canonical replay: PASS183
Audit: temp/general_cleanup_20261006/gq2-native-consumer-audit-0501-20261016.json
SHA6e0ef66bb1505b3a77218a9374fc2cc6564c80c012955ac70365b73f6bf50c6a
Predecessor progress SHAc9199208b9fcbd5120db565630580f4127bfd63d9aa27b2fd2dbf7b950f4d593

## Retained semantics and evidence limits

- Headless route aggregation waits for all repeats, records native process status, rejects missing/unreadable outputs and unexpected failures, and compares every later raw file hash with run1. A mismatch publishes nondeterministic. Qualification of the previous checkpoint: the L10 wrapper reads only run1 directly, but its normalized ok assertion also rejects repeat disagreement through this actual producer. Do not admit a missing-repeat defect from the wrapper body alone
- Native exit2 with an output is an accepted routing outcome; unexpected infrastructure failures set batch exit1, while expected process timeouts are separate. Batch exit0 does not mean every route completed. Executable snapshot/hash check and fresh per-worker settings are retained. Actual physical geometry boundaries, complete pool lifecycle and blanket base-mission replacement remain pending
- Whole shared autoselect runtime helper validates exactly4ints, swaps all fields, validates flags and queued identities, then applies only when requested. Quad16 is valid for D1 and active imported D1 gameplay; ordinary D2 rejects it. Paired writers and versioned preflight/restore call sites share the helper; D2 secret restoration consumes without applying. This does not establish whole-save or durable-publication acceptance
- Actual paired pickup bodies preserve inventory acquisition independently of suppressed selection, retain empty-secondary fallback, and distinguish duplicate/current identity, cutoff, firing delay and no-fire behavior. Actual paired new-ship branches reset consumed flags only for the local player. Imported primary helper is gated by content mode and preserves quad selection identity
- Both game CMake targets compile the native autoselect fixture into the real headless upstream executable; default main calls it. CTest supplies a per-build fixture directory, and main mounts current directory. Fixed test filenames are build-local; concurrent invocations in the same directory are not proven isolated
- Loaded-asset pickup fixture calls native/imported behavior and an actual selected ordinary D2 mission before quad rejection. Controlled corridor geometry/inventory, parameter traces and SIM RNG invariants are retained as controlled evidence, not a gameplay traversal or replay
- JNI checks exact lengths, byte domains and permutation validity before scanning pilots. Summary chooses nanosecond-newest valid pilot with lexical tie. Paired pilot loaders fall back to defaults for invalid orders. D1 helper checks7/6 complete conversions after casting to bytes; source-string assertions do not establish integer truncation or trailing-token policy. Existing owners must be reconciled before any root admission
- Whole bounded RLE validator checks dimensions, table width and arithmetic bounds, little-endian declared size, every row payload bound, exact decoded width and exact final terminator/input consumption. Actual traced production caller is imported D1 custom-bitmap staging: extent/read/allocation validation precedes publication, invalid buffers are freed and big-row encoding is rejected there. D2-only test directly links this validator and is registered with CTest. Do not extend that proof to every engine RLE loader

Only the source gate is closed. Consumer/owner/disposition/publication gates remain open. Next asset extractor/production boundaries, bot transport/protocol/lifetime, bounded Python admission/supervisor, remaining native route boundaries and base-mission replacement, then whole existing owners/raw quality/prior plans and direct disposition. No product/test/helper edit or execution, build, formatter, generator, device/network/security probe, new finding/status/rating/runtime/original-saving claim, canonical evidence append, Drive copy, staging or commit. Canonical500DONE127TODO627; broader goal remains active
