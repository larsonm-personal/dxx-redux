# GQR-0253 mission fingerprint string escaping plan, 2026-10-09

## Problem and scope

GQF0267 OPEN: manual Escape-JsonString uses continue inside a single-input switch, then also appends the same character after that switch. Current generator can reject valid quoted text or publish changed decoded control text. Use actual string/sidecar ownership to fix it; do not merge cache completeness, AcoustID selection or automation template roots. Current source equals frozen; diagnosis is static control-flow plus official PowerShell semantics, no dynamic failure claim.

Allowed implementation scope: game_data/fingerprint_mission_zip_music.ps1 and existing android/tests/test_acoustid_regeneration.ps1, with shared writer/fixture only if actual reuse requires it. No inherited D1/D2 edits. Preserve root/track/source field shape and generated comments, shared normalization/atomic publication and unchanged-content bytes/mtime policy.

- [ ] Serialize every string exactly once with the existing serializer/shared writer; prefer deleting manual escaping to another new helper when semantics remain clear
- [ ] If manual escaping is retained, make switch cases/default mutually exclusive or explicitly continue the enclosing foreach; preserve controls and supported Unicode
- [ ] Extend actual writer/readback acceptance for quotes, backslash, standard and other controls, BMP/supplementary Unicode, adjacent escapes and current empty/null behavior; compare exact decoded values rather than path existence alone
- [ ] Pass ordinary tracklist/cache labels through actual Add-AcoustIdResults and writer, including optional fields, while retaining plain-track controls and failed-publication previous bytes/mtime
- [ ] Verify ordinary generated sidecars remain unchanged except the previously incorrect escaped content; retain idempotent shared writer and existing completeness/provenance owners
- [ ] Run maintained AcoustID regeneration/parser/publication checks on supported PowerShell hosts, relevant catalogs and scoped quality in implementation; use local controls without live AcoustID or payload regeneration

No implementation or test execution in this diagnosis tranche. Deferred malformed-media/security/allocation/resource-pressure probes remain deferred. See immutable GQF-0267 mission fingerprint string escaping diagnosis 20261009 evidence for exact sources and language reference.
