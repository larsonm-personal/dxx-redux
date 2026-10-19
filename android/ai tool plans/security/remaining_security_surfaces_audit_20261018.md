# Remaining security surfaces audit

## Objective

Complete a read-only security audit of the application surfaces not covered by
the bad-level/mod-data and direct-UDP reviews. Identify confirmed defects and
defense-in-depth gaps, propose concrete fixes and verification, and make no
runtime fixes during this audit.

## Threat model

Treat imported saves, checkpoints, demos, media, archives, deep links, Android
IPC, matchmaking messages, relay traffic, transferred missions, server clients,
JNI inputs, and exported diagnostics as hostile. Review memory safety,
authorization, integrity, confidentiality, bounded CPU/memory/storage use,
transactional failure, and compatibility with valid legacy data.

## Method

1. Inventory trust boundaries, entry points, formats, authentication decisions,
   process boundaries, and current tests.
2. Select the modules that implement each boundary and record their line counts.
3. Review every selected module through its final contiguous approximately
   500-line block, including D1/D2 counterparts.
4. Trace high-risk inputs from admission through parsing, allocation, state
   publication, and cleanup.
5. Confirm candidate findings against call sites, format invariants, and existing
   tests; distinguish confirmed defects from hardening recommendations.
6. Record proposed fixes that preserve valid legacy behavior, plus regression,
   malformed-input, sanitizer, and integration tests.
7. Perform a dependency/advisory and release-configuration review using pinned
   local versions and authoritative upstream advisories.

## Coverage areas

- [x] Save files, player files, and co-op checkpoint transfer
- [x] Classic demos and input-demo/replay formats
- [x] Matchmaking client, relay, mission transfer, reconnect, and server
- [x] Movies, subtitles, MIDI/HMP, sound banks, CUE/BIN, and decoder boundaries
- [x] Android exported activities, intents, dynamic receivers, PendingIntents,
  FileProvider, and SAF grants
- [x] Disc, installer, and archive backends not covered by the prior audit
- [x] JNI array/string/path/file-descriptor and process-lifecycle boundaries
- [x] Cross-cutting CPU, memory, storage, concurrency, and log-growth limits
- [x] Secrets, diagnostic logs, crash reports, and export redaction
- [x] Dependencies, TLS/network policy, build variants, signing, and release
  configuration

## Required deliverable

Create `android/security_scan_remaining_surfaces_20261018.md` containing:

- Executive summary and explicit limitations
- Trust-boundary and attack-surface inventory
- Complete approximately 500-line coverage ledger for selected modules
- Prioritized finding/fix ledger with severity, confidence, evidence, proposed
  repair, compatibility constraints, and verification plan
- Existing defenses worth preserving
- Recommended implementation order
- Clear separation of confirmed vulnerabilities, likely defects, and
  defense-in-depth work

## Progress

- [x] Existing level/mod and direct-UDP audit scope reviewed
- [x] High-level inventory complete
- [x] Module selection and line-count ledger complete
- [x] Save/checkpoint/player-file deep review complete
- [x] Demo/replay deep review complete
- [x] Matchmaking/relay/transfer/server deep review complete
- [x] Media decoder deep review complete
- [x] Android IPC/storage/JNI deep review complete
- [x] Resource, privacy, dependency, and release review complete
- [x] Candidate findings validated against callers and tests
- [x] Final report complete
