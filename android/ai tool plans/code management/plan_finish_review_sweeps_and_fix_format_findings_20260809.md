# Finish review sweeps and fix format findings

## Plan

- [x] Re-read repository and adversarial-review instructions and inventory the remaining queue
- [x] Complete R1-SWEEP-006 through R1-SWEEP-014 sequentially with sol-5.6-medium
- [x] Validate the completed sweep tranche, ledger integrity, and absence of ACTIVE rows
- [x] Rank up to 40 open non-network findings by file-format and import correctness relevance
- [ ] Fix ranked findings sequentially with focused regression coverage and proportionate builds (24 of up to 40 complete)
- [ ] Move each completely fixed finding to the done ledger and record exact validation evidence (24 archived)
- [ ] Run final ledger, quality, test, build, and diff validation

## Current status

All nine remaining review sweeps are complete. Twenty-four ranked format findings are fixed and archived, through BR-0185. Their focused regression tests, proportionate native or JVM builds, Android debug builds for maintained ABIs, scoped quality checks, and diff validation passed. BR-0082 remains open because complete body admission needs new scope-aware metadata integrity or a side-effect-free staged engine reader. BR-0096 now rejects malformed and directory-orphan descriptors but remains open until same-stem HOG/DXA contents are structurally checked for referenced levels. BR-0091 remains open with its required cross-language sidecar schema migration recorded. BR-0103 remains open because a complete fix requires immutable generations, a process-wide transaction owner, fsynced candidates, an atomic manifest-pointer switch, and restart recovery rather than a partial lock-only patch. BR-0185 now retains canonical container-relative member paths, performs exact normalized song-reference matching before unique-leaf fallback, rejects ambiguous leaf association, and displays colliding leaves with qualified identities. The next queued remediation is BR-0237.

## Remediation queue

This is a priority queue, not a promise to broaden a fix when live-code investigation shows shared ownership or a larger design dependency. Complete fixes are archived individually; findings that need broad redesign remain open with the blocker recorded.

1. BR-0372, DXA robot weapon indices
2. BR-0217, replay weapon-order permutations
3. BR-0220, replay weapon-selector domains
4. BR-0221, replay frame-time invariants
5. BR-0291, replay difficulty domain
6. BR-0383, restored thief inventory indices
7. BR-0390, restored scheduler ticks
8. BR-0261, pilot weapon-order permutations
9. BR-0267, pilot cockpit mode
10. BR-0321, replacement image decode size
11. BR-0333, level control-center links
12. BR-0014, excess CUE FILE directives
13. BR-0183, CUE MSF field ranges
14. BR-0184, CUE data-track sector modes
15. BR-0093, ZIP preamble validation
16. BR-0045, legacy save-header validation
17. BR-0082, metadata-backed save-body validation
18. BR-0096, mission descriptor-to-asset admission
19. BR-0039, fingerprint database entry admission
20. BR-0040, fingerprint duration matching
21. BR-0069, PKG scan and extraction catalog parity
22. BR-0072, deterministic SOW scan manifests
23. BR-0091, music-name sidecar bounds
24. BR-0092, optional mission music inspection
25. BR-0102, extraction-record source identity
26. BR-0103, extraction-tree transactional publication
27. BR-0104, engine-compatible mission staging paths
28. BR-0185, path-qualified song references
29. BR-0237, complete HOG MIDI/HMP enumeration
30. BR-0238, unique mission MIDI classification
31. BR-0235, save metadata append failures
32. BR-0211, cooperative progress inventory publication
33. BR-0212, cooperative save metadata discovery
34. BR-0208, rewind snapshot publication
35. BR-0218, embedded checkpoint temporary ownership
36. BR-0219, embedded checkpoint expansion bounds
37. BR-0207, rewind mission identity
38. BR-0222, replay artifact-set publication
39. BR-0233, verified headless output publication
40. BR-0320, bounded texture-index construction
