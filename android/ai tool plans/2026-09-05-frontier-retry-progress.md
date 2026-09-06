# Frontier retry progress

1. Capture a fresh baseline for the four primary missions
2. Scope frontier retries to lack of progress at the same frontier and avoid counting opening animation frames as failures
3. Verify Descent conversion level 7 and compare all primary mission levels against the baseline
4. Add integration coverage and run native tests and scoped quality checks

## Implementation and results

- Retry accounting resets when a reached frontier extends to a new physical target
- Opening animation frames no longer consume the retry limit every frame
- Attempt eight refreshes the live route before deciding the frontier is still blocked
- No collision, key, door-opening, or overall time-budget rules changed
- Descent conversion level 7 now completes at frame 6495 and matches across repeated runs
- The original FirstStrike level 7 still has its separate navigation stall

Fresh before/after simulation comparison, including secret levels:

| Mission | Before ok | After ok | Total |
| --- | ---: | ---: | ---: |
| Counterstrike | 28 | 28 | 30 |
| FirstStrike, D1-in-D2 | 19 | 22 | 30 |
| Castaway Redux | 2 | 2 | 10 |
| Obsidian | 11 | 11 | 18 |

- No ok-to-non-ok regressions
- FirstStrike 24 changes from an access violation to ok; 25 and secret 1 change from timeout to ok
- Castaway 5 advances from four to five objectives, completing switch 12 before hitting unsupported activation 11 for switch 13
- The FirstStrike 24 result establishes that this route now completes, not that every possible cause of the earlier access violation has been eliminated
- Checked-in metadata and simulation JSONs were not rewritten
- Native D2 build, all 47 CTests, scoped code quality, and the new successive-frontiers integration test passed
- A second full four-mission run with Repeat 2 matched deterministically for all 88 levels, with zero infrastructure failures

Run evidence is in android/temp/frontier_primary_before_20260905 and android/temp/frontier_primary_after_20260905
