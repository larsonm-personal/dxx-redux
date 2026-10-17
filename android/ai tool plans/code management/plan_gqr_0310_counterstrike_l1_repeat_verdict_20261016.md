# Proposed GQR-0310 Counterstrike level-1 repeat verdict, 2026-10-16

Diagnosis and conditional plan only. Proposed GQF-0324 / GQR-0310, not yet admitted canonically. P3/high, test-gap/repeat-verdict; PRIMARY 31 LOW (H/M/B/C/R 12/0/2/10/7). This normalizes an existing observation from chunks 0138 and 0140, not a newly discovered behavior

Concrete failure: test_counterstrike_level1_route_confirmation.ps1 requests Repeat=2 but selects only the first run's JSON and log. The corpus generator compares full raw result hashes, records normalized nondeterministic on mismatch, and intentionally returns zero when no infrastructure failure occurred. If run 1 passes its physical oracle while run 2 differs or is controlled nonconfirmed, this child can still print success because it never reads run 2 or the normalized verdict

Branch attribution: the complete fixture is branch-added at the frozen review boundary and its current body remains equal to the frozen body. Earlier notes explicitly identify the missing local repeat assertion but no dedicated canonical finding/remediation owns its repair. Existing producer exception containment, output publication, aggregate status and resource retirement have different mechanisms

Conditional local implementation after diagnosis restrictions lift:
1. Check the simulation runner's returned process status and require exactly two correctly selected run JSON files and their corresponding logs
2. Require both results to be confirmed. Preserve the three-objective and reactor-to-exit timing checks and physical flare/frontier log witnesses; do not replace them with status alone
3. Compare full deterministic result identity across both raw runs, and require exactly one normalized level-1 record with status ok. Preserve real engine output and its intended deterministic representation
4. Keep the corpus generator's deliberate ability to record route failures and nondeterministic outcomes without calling them infrastructure failures. The focused test must consume those outcomes
5. Keep the repair local. Do not add a generic route framework, weaken metadata expectations or change engine behavior to accommodate an old fixture

Future acceptance: run the maintained real level-1 two-repeat test through its normal wrapper with selected native host and game data. Exercise the actual verdict code with controlled artifacts where only run 2 is missing, nonconfirmed, or differs in result state while run 1 remains valid; each must fail. An identical confirmed pair must pass, while normalized nondeterministic and wrong/missing selected-level records fail. Preserve physical timing/log negative controls and propagate infrastructure failures. If a small local verdict extraction is needed for these controls, test the same production assertion body rather than a copied oracle

No native/test execution, runtime reproduction, product edit or inherited-file saving is claimed. Canonical IDs and totals change only in the later exact publication step
