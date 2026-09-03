# GuideBot simulation bundle diff review

## Plan

- [x] Inventory changed simulation files and compare aggregate status counts
- [x] Identify every status transition, especially `ok` to non-`ok`
- [x] Separate completed runs from missing, truncated, or unattempted levels
- [x] Compare objective progress, frame counts, RNG state, and failure reasons
- [x] Correlate important changes with current mission metadata routes
- [x] Summarize regressions, improvements, systemic patterns, and priorities

## Findings

- The bundle selected all 985 D2 levels and ran for 1,820 seconds. Its stage
  result is `fail`, not a clean completed regression pass.
- Across the 975 changed level records, `ok` increased from 153 to 308,
  `timeout` decreased from 662 to 490, and `failed` decreased from 113 to 87.
  There are 164 new successful levels.
- Nine records changed from `ok` to another status. One is MOON 11 becoming
  `not_run` after an infrastructure failure. The other eight represent five
  unique physical levels because Counterstrike/Vertigo levels are duplicated
  in Trinity.
- The five unique executed regressions are Belial System XL secret 4, Vertigo
  level 11, Counterstrike secret 3, Counterstrike secret 5, and Lost Levels
  level 23. Their checked metadata routes all remain `ok`.
- The duplicated regressions have identical frame counts and failure points in
  both packages. This is deterministic evidence of a behavior change, not
  random run noise.
- The run had 36 infrastructure failures: 34 level-zero work items were passed
  to an executable that rejects `-level 0`, and both copies of EAF2 secret 2
  crashed with Windows access violation `0xC0000005`.
- Failed work items without a new result are written as `not_run` when their
  identity or route hash changed. Matching old results are silently retained,
  which hides the two EAF2 crashes inside otherwise plausible regression JSON.
- All executed runs retained RNG start state 1 with zero calls. No repeat was
  classified as nondeterministic. Ending RNG changes track changed frame paths.
- Among 144 levels that stayed `ok`, the median new frame count is 63.2 percent
  of the old count. 140 became faster, one was identical, and the three slower
  records are the same Bahagad level through duplicate mission packaging at
  18 percent slower.
- Of 462 persistent timeouts, 101 reached more objectives, 342 reached the same
  number, and 19 reached fewer. The largest losses are Lost Levels 14 and
  Obsidian 13.

## Implementation

- [x] Define an engine launch level for metadata records numbered zero while
  retaining zero in regression identity and output
- [x] Emit an explicit infrastructure-error level result instead of `not_run`
  or a silently retained prior result
- [x] Add runner integration coverage for level-zero launch translation and
  infrastructure-error publication
- [x] Re-run representative level-zero missions and verify that they execute
- [ ] Reproduce and diagnose the EAF2 secret 2 access violation
- [x] Reproduce Counterstrike secrets 3 and 5 with focused logs
- [x] Fix the first confirmed engine or planner defect without weakening route
  validation
- [x] Build D2, run CMake and PowerShell tests, and run scoped code quality

## Fix progress

- Metadata level `0` now launches engine level `1` while preserving level `0`
  in the work identity and generated regression record.
- Engine launch and process failures now publish `infrastructure_error` with
  the failure reason, so a previous result cannot silently conceal a crash.
- The route-certifier shared ABI now forces MSVC pack alignment 8 and rejects
  a summary layout other than 72 bytes at compile time. AddressSanitizer found
  the previous producer/consumer mismatch overwriting a 68-byte global with a
  72-byte value.
- EAF2 secret 2 still has a second failure after the overwrite is removed: an
  indirect null call while evaluating the live physical path from segment 7.
  This remains open and is not being reported as fixed.
- Counterstrike secret 3 reproduces as a no-progress timeout with no completed
  objectives. Secret 5 reproduces as an immediate no-actionable-goal failure.
