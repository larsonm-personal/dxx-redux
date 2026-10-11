# Digest-bound evidence report admission, 2026-10-11

GQR-0288/GQF-0302 admitted TODO/OPEN from GQ2-CHUNK-0384 after publication audit PASS4149. Diagnosis and implementation plan only under plan_general_diagnosis_resume_20261009.md. Do not run the updater against campaign ledgers while reviewing it.

## Concrete problem and required result

update-general-quality-evidence-ledger.ps1 reads report bytes, separately hashes the path, and skips whenever the ledger contains a BEGIN IMPORT prefix with the same first heading, irrespective of the computed digest. Two reports with the same heading and different content therefore receive Already imported without preserving or rejecting the different evidence. Concurrent source replacement can also bind copied bytes to a hash from a different read. The canonical process declares immutable complete inbox reports and one writer; that contract needs explicit same-identity/same-bytes admission rather than silent acceptance of conflicts.

## Allowed implementation boundary

Evidence importer and a focused integration fixture using private temporary reports/ledgers. Preserve existing imported blocks byte-for-byte, encoding and line endings; do not normalize historical canonical CRCRLF or republish the campaign. No native publication abstraction or general transaction framework is needed.

Compute the digest from the exact admitted byte buffer, distinguish exact same-heading/same-digest idempotence from same-heading/different-digest conflict, and reject conflict before any mutation. Preflight all reports and requested source removals before the first ledger write; report/inbox containment must use complete path components rather than StartsWith on a bare directory name. A failed admission must preserve ledger and sources. Preserve the single-writer contract and use a checked complete replacement or other narrowly justified byte-preserving publication so a failure cannot present a partial import as success. Delete only an admitted owned source after durable success and preserve retry diagnostics.

## Acceptance before implementation completion

- [ ] Actual importer accepts one complete ASCII report and embeds exactly its bytes and digest
- [ ] Same heading/same bytes is an unchanged idempotent success; same heading/different bytes fails clearly without modifying ledger/source
- [ ] Multiple reports containing one invalid/conflicting member fail before publication; retries preserve complete prior generations
- [ ] Source-change injection cannot produce an embedded-byte/digest mismatch; hash the captured buffer, not a later path read
- [ ] Repository/inbox sibling-prefix and outside-root paths are rejected; source-removal validation precedes ledger mutation
- [ ] Empty/BOM/NUL/control/trailing-whitespace/missing-heading inputs obey explicit printable ASCII policy; valid CRLF/LF reports preserve agreed representation
- [ ] Inject open/write/close/publication/removal failures; prior canonical bytes and unowned files survive, retained owned sources/residue are reported
- [ ] Existing historical CRCRLF ledger and reference-rewrite controls remain byte-preserving except the requested exact append/replacement; focused actual-entry checks and scoped PowerShell quality pass

Atomic ledger publication and source deletion are separate operations: deletion failure must not pretend that the durable import was rolled back. Do not solve conflicts by deleting prior imports or assigning guessed report IDs. No inherited engine edit or saving is proposed.

Canonical admission is diagnosis only; every implementation acceptance gate above remains open. Immutable source snapshot preserves the preceding proposed-plan version
