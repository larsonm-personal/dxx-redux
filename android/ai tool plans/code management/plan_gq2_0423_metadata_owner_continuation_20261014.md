# Chunk0423 metadata owner continuation, 2026-10-14

Diagnosis and plans only. No product, test, helper or generated asset changes. Supplement plan_gq2_0423_demo_guide_continuation_20261014.md; terminal chunk admission remains pending.

## Current metadata qualification

All 11 package filename/digest pairs in the README agree with DemoInstallerPackages. All 26 README output rows have matching filename/digest entries in the current known_versions.jsonc. The three Mac oracle archives and their 12 declared output rows agree with the README names, digests and lengths. All 11 public demo oracle package digests agree with the Kotlin model; all 35 public output filename/digest identities occur in known_versions. These are comparisons of maintained declarations, not fresh archive acquisition, payload measurement, extraction, licensing or runtime proof.

The current known_versions database contains 404 version entries and five filename/digest pairs with multiple version labels. KnownVersions loads a list per portable filename, but lookup still returns the first matching digest's label. identifyPackage then votes over those selected labels. BR0411 retains ownership; README hash recognition does not prove a unique distribution identity for shared bytes.

## Existing owners and current repairs

BR0175 remains OPEN. hash_assets now admits both .msn and .mn2, deduplicates filename/digest/version triples rather than discarding all later aliases, rejects unknown folders in Force mode, requires a nonempty Force baseline and rejects a generated set missing maintained baseline triples. Preserve these repairs. The mapping still omits d2demo10_extracted; ordinary mode warns and admits an unknown folder label, while Force rejects it. Do not repeat the old first-label deduplication or missing-.msn mechanism as current. Inputs and output groups still follow source enumeration order; no byte-stability run was performed.

BR0612 remains OPEN. The version writer still interpolates file/version strings; it now parses reconstructed JSONC before calling the atomic text writer. This blocks some malformed output before publication but does not establish exact typed-value round trips: valid escape sequences can still alter values. Preserve parse-before-publication, use typed JSON string serialization in later implementation, and validate parsed equality and schema as well as syntax.

BR0233 remains OPEN. The delegated atomic text helper uses a GUID sibling temporary file and Replace with a backup for an existing destination, Move for a new destination, bounded retries for selected Windows sharing errors, and final scratch cleanup. Do not describe the current hash_assets publisher as an unconditional direct write. Whole-helper source inspection does not prove concurrency, durable persistence, cancellation or failure-injection behavior. The writer has no unchanged-text check; ordinary no-new-entry runs avoid calling it, while forced generation needs separate byte/time-preservation acceptance.

The current three-archive Mac oracle is a historical declaration with oracle_tool set to unar; it does not contain the package SHA or detailed tool identity now produced by extract_mac_demos. Do not retroactively attribute new producer provenance fields to the existing oracle, and do not refresh it during diagnosis.

## Conditional implementation and acceptance

1. Under BR0175, map every supported helper output including d2demo10_extracted to its declared canonical version, retain all valid version aliases, and reject unsupported labels according to an explicit ordinary/Force policy. Use a shared machine-readable package model if it simplifies ownership without moving engine format knowledge into the launcher.
2. Exercise each D2 alias output alone and both together in reversed creation/enumeration order, ordinary and Force modes, paired D1/D2 mission descriptors, unknown folder policy, incomplete baseline and duplicate triples. Require canonical labels, complete maintained identities and stable bytes.
3. Under BR0411, preserve all candidate labels per filename/digest, distinguish an unambiguous distribution from shared bytes, and make package inference independent of database and input-map ordering. Exercise all five current ambiguous pairs and complete or partial package sets.
4. Under BR0612/BR0233, serialize typed values, validate full parsed equality/schema and stage complete publication. Exercise quotes, backslashes, all control characters, Unicode and literal comment markers; invalid generation must preserve prior bytes and time. Include unchanged ordinary and Force output plus controlled replace/move failures and concurrent writers in a later authorized implementation tranche.
5. Preserve README full versus compact D1 PIG identities, Mac sound-bank aliases and distinct archive wrappers. Treat the older completed launch-offer plan's hashes and passes as historical evidence only; it does not replace current extractor or metadata acceptance.

Remaining0423 work: qualify current DOS/Mac helper and Android StuffIt/BinHex/delegated bridge boundaries in a persisted checkpoint, reconcile archive/resource/tool/host owners and prior dispositions, then author complete direct disposition and canonical publication. No new finding, remediation, rating or status is assigned here.
