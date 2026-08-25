# Regression data failures and newly cataloged music tracks

1. [x] Locate the referenced regression run and extract the first actionable failure from each stage
2. [x] Correlate each failure with the current worktree and generator output
3. [x] Compare the two changed music catalogs with `HEAD` and identify the newly added source tracks
4. [x] Trace recent history for the discovery and fingerprint logic that admitted those tracks
5. [x] Record causes, distinguish expected regeneration from defects, and recommend any follow-up

## Outcome

- The referenced run is `run_20260824_074305`
- CD extraction failed because `Get-DxxTreeSha256` produces the pinned hash under PowerShell 7 but a different hash under Windows PowerShell 5.1 due to host-dependent `Sort-Object` ordering
- Fingerprint regeneration failed for the same false bounded-Python tree mismatch on three sampled non-ZIP mission archives
- Mission metadata had four shifting analysis timeouts plus two instances of the same false Python hash failure; the timed-out missions passed in earlier runs and are not stable content regressions
- The two music catalogs contain no added or removed records and are semantically identical to `HEAD`
- The forced fingerprint stage intentionally reprocessed both sampled audio missions; Windows PowerShell 5.1 then escaped apostrophes as `\u0027`, producing the only catalog diffs
- Commit `3374adf0` exposed both host-dependent behaviors by enabling the workflows to run under the current Windows PowerShell 5.1 host

## Fix implementation

6. [x] Make dependency tree hashing use explicit ordinal path ordering across PowerShell hosts
7. [x] Make mission music JSON string escaping byte-stable across PowerShell hosts
8. [x] Preserve child stage exit codes when Windows PowerShell 5.1 detaches redirected `Start-Process` objects
9. [x] Add focused cross-host regression coverage and run scoped code quality checks
10. [x] Run a focused emulator metadata check and record whether analysis timeouts remain

## Fix outcome

- Tree manifests now use ordinal path ordering. The Python and machfs tree pins were rotated to hashes derived from the same already-verified local trees under that stable ordering.
- PowerShell 7 and Windows PowerShell 5.1 both accept the pinned Python and machfs trees.
- The bounded Python identity probe and its quoted-argument regression test now avoid double-quoted Python `-c` literals that Windows PowerShell 5.1 strips during native argument passing.
- Mission music JSON escaping is implemented directly, so printable apostrophes remain literal on both hosts. The two semantic-only catalog diffs were normalized away.
- Regression stages now write the direct child exit code through an atomic sidecar. Both hosts preserve exit code 7 while still returning promptly when a detached descendant holds output pipes.
- Automation logcat snapshots are capped at 400 lines and previously displayed lines are not printed again.
- Windows PowerShell 5.1 strips double quotes from native `-c` arguments, so bounded Python probes now use equivalent single-quoted Python literals; both hosts pass the bounded-runtime test.
- Focused Windows PowerShell 5.1 metadata runs passed all 11 steps for `ROGUE.zip` in 4:10 and for formerly preflight-failed `nefarious.7z` in 5:37, with regression publication disabled.
- The full CD extraction check exposed a Windows live-output race when a native extractor removed a temporary Mac archive after the supervisor opened it. Live measurement now tolerates that disappearance while final strict validation still rejects unstable output.
- The two affected Mac discs, `d1 mac 2nd bin+cue` and `Descent - Mac macplay`, both passed focused forced extraction after the supervisor fix.
