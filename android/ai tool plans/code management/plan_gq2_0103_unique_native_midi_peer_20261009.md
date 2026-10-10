# Unique native HMP metadata peer selection, 2026-10-09

Diagnosis only. Distinct GQF-0274/GQR-0260 admitted at0103 terminal normalization; not implemented and no runtime acceptance. Existing sidecar identity GQR0093 and mission classification BR0238 do not own embedded HMP/MID peer choice

## Problem and source proof

Installed enumeration find_midi_peer_index returns the first case-insensitive same-stem .mid in the HOG catalog. Whole222-line hog_midi_catalog.h retains every accepted HMP/MID entry in directory order and has no duplicate-name rejection. With empty-text game05.hmp and valid game05.mid/GAME05.MID containing different text, enumeration inherits whichever matching MID appears first. Catalog fileSize/entry offsets keep the payload read bounded but do not establish unique peer identity

PhysFS find_midi_peer similarly returns the first case-insensitive same-directory leaf match. Actual PHYSFS enumeration can expose distinct-case loose names; complete uniqueness must be established before reading/adopting a candidate. Do not infer duplicate exact names are enumerable or that every native mount permits case aliases. The HOG enumeration source proof independently establishes the defect

The existing music plan explicitly forbids ambiguous inheritance. Current MissionZipMusic.midiMetadataPeer uses same-source, same-directory normalized stem and singleOrNull; whole62-line JVM fixture rejects case-alias competing peers and wrong directories. Native paths can disagree with this policy. SetupSections actual IO metadata loader only follows a peer for no direct text and HMP, preserving original playback

## Implementation boundary

- Keep parsing, active song slots, file lookup and formats in shared native owners. Make both native selectors retain a candidate only when exactly one admissible same-container/directory MID exists; detect a second matching candidate before parsing either. Preserve direct metadata when ambiguity exists, source provenance and inherited=false
- Define duplicate identical entries explicitly. Conservative unique identity rejection matches current ZIP singleOrNull; do not silently accept first bytes based on case/order or guess duplicate titles. If a broader duplicate policy is desired, it needs one shared documented identity contract and equivalent ZIP/native acceptance
- Preserve HMP playback bytes and outer duration. PhysFS adoption retains original HMP duration; metadata provenance identifies selected MID only on successful unique adoption. Keep HMQ non-inheritance, direct valid event precedence and sidecar precedence
- Keep existing invalid/no-peer/allocation statuses truthful, and coordinate GQR0170 producer cleanup/JSON exception containment. Do not introduce persistent schema compatibility, a new general resolver framework or inherited D1/D2 hooks

## Future acceptance

Use actual installed HOG enumeration and PhysFS resolver with a valid empty-text HMP and two valid distinct-title MID aliases in both entry/name orders. Ambiguity must never advertise an inherited source/title; direct HMP metadata must win without unnecessary peer reads. Exercise one unique mixed-case MID, no peer, different stems/directories/containers, empty/no-text peer and HMQ negative controls

Assert source filename, inherited flag, raw events/summary and original duration through native JSON, picker/preview and paired active-song metadata. Keep exact original preview bytes, slots and soundtrack selection. Run existing ZIP JVM peer fixtures for parity, meaningful native resolver/enum fixtures including actual file lifetime and registration, paired relevant Windows/Android builds and scoped quality

Do not substitute a manually passed inherited flag serializer test for actual peer selection. Native parser CTest does not compile PhysFS or enumeration. Fault/resource/malformed/security/runtime probes remain deferred in this diagnosis tranche; no fixture created or executed
