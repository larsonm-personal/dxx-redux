# CD Regression Full Run Investigation

## Goal

Determine which changes produced by the 2026-07-23 full CD regression run are
legitimate oracle improvements, stale-result churn, or real extraction
regressions after recent SIT, HOG, and related archive parser changes.

## Plan

- [x] Preserve the generated specs as investigation evidence
- [x] Inventory the full run outcomes and every changed tracked file
- [x] Compare changed specs with their prior committed results and file oracles
- [x] Trace suspicious extraction changes through parser code and git history
- [x] Classify regressions, improvements, infrastructure failures, and bit rot
- [x] Recommend focused fixes and reruns without accepting weakened oracles

## Initial evidence

- Full run result: 27 passed, 9 skipped, 2 failed
- Changed saved results: 5
- Failures:
  - `Descent - Levels of the World (USA)`
  - `Dimensions for Descent (USA)`
- Skip to pass changes:
  - `Descent - Test Flight (USA)`
  - `Descent II - Destination Quartzon 3D (Europe)`
  - `Descent II (USA) (3-Level Interactive Preview)`
- Total runtime: 01:25:01

## Constraints

- Do not regenerate, normalize, or revert the changed specs during diagnosis
- Treat a transition from full verification to file-only as suspicious until
  the runner path and prior contract explain it
- Do not update expected files to match current output until extraction
  correctness is established independently

## Findings

### 1. The default workflow rewrites both the oracle and its result

`game_data/run_all_cd_regressions.ps1` defaults to all of the following:

- forced host extraction
- forced regression-spec generation
- Android extraction tests with `-SkipLaunch`

This is unsafe for regression detection. A current extractor failure can replace
the committed expected output before the Android comparison runs. The Android
file-only run then also overwrites the prior full-launch evidence in
`last_test_result`.

The top-level generated fields changed for only one source, `d2 mac`. The other
35 regression specs changed in generated timestamps and/or mutable
`last_test_result` evidence.

### 2. Most `full` to `file_only` changes are runner metadata damage

Twenty launchable sources retained pass status and the same expected-file
verification, but changed:

- `test_mode`: `full` to `file_only`
- `level_reached`: the previously reached level to `null`

That is caused directly by the default `-SkipLaunch` option. It is not evidence
that launch support regressed, but the generated diff must not be accepted
because it discards stronger prior evidence.

`test_extract.ps1` always persists the latest result. It needs an evidence
monotonicity rule so a file-only pass cannot replace a full pass.

### 3. `d2 mac` is a likely real host extraction regression

The committed oracle classified the image as `d2_full`, extracted 17 recognized
files, and required:

- `descent2.ham`
- `descent2.hog`
- `descent2.s11`
- `descent2.s22`
- `groupa.pig`

The current forced extraction produced only nine loose HFS data/media files,
classified the disc as `unknown`, and lost every `descent2.*` file. The source
image SHA-256 values did not change.

Focused instrumentation found the exact cause. The HFS volume contains an
installer named `Install Descent II`, while the native extractor recognized
only `Install Descent`. It therefore skipped the 23 MiB STi2 installer and
fell back to the nine loose HFS files. The prior PowerShell extraction path
recognized all three known installer names.

The native path also selected either installer output or loose HFS output,
while the established oracle merges installer files with non-duplicate loose
HFS files such as the three MVLs. The repair must recognize the D1 and D2
installer names and merge both sources. Replacing whole-installer heap buffering
with a bounded read-only file mapping remains a useful safety improvement, but
the 128 MiB limit was not the immediate cause for this disc.

### 4. The two five-minute failures exposed a level-pack import gap

`Descent - Levels of the World (USA)` and `Dimensions for Descent (USA)` do not
contain SOW archives. Host extraction successfully produced 509 and 192 total
files respectively.

Commit `a965dea2` changed their previously null `expected_files` lists to 191 and
88 real level-pack filenames. Their earlier file-only passes therefore did not
prove that the level files were imported. The first stronger run exposed the
failure.

Android post-processing only hoists nested files whose names are in
`ALL_GAME_FILENAMES`. That set contains the fixed core D1/D2 filenames, not
arbitrary `.hog` and `.msn` level-pack names. Setup introspection reports only
the root set directory. The runner consequently waits for nested custom level
files that can never appear in `set_files`, then reports the timeout as
`file_push_failed`.

These are real functional/test-contract failures, not evidence of a SOW parser
regression and not proven bit rot. The correct repair is to support and verify
custom level files in the imported set, or explicitly define a different
recursive verification contract. Do not remove the new expected-file lists.

### 5. The five OEM/Vertigo verification-count drops are runner races

Four Destination Quartzon sources and the Windows ISO variant changed from
three verified files to zero. Vertigo changed from four to three. They remained
skips.

For sources whose prior status is not pass, `test_extract.ps1` permits an
incomplete direct-import skip after only three identical polls. A nonempty
partial file list can be stable briefly while the background extraction thread
is still working. The run then exits early and records the partial count. The
3D Quartzon source completing quickly enough to pass is consistent with this
race.

Completion should be signaled explicitly by the setup import API. A short
"stable list" interval is not a valid completion signal.

### 6. The three skip-to-pass changes are mixed

- `Descent - Test Flight (USA)`: a legitimate file-only improvement. Its two
  committed expected files were found, but this does not establish launch
  support.
- `Descent II - Destination Quartzon 3D (Europe)`: likely a legitimate
  file-only readiness improvement; its unchanged three-file oracle passed.
- `Descent II (USA) (3-Level Interactive Preview)`: artificial. The
  `-SkipLaunch` pass branch runs before the preview-specific
  `android_launch_unsupported` skip branch.

### 7. Failure/result reporting is misleading

Both direct-import timeouts were persisted as `file_push_failed`, although file
staging succeeded and the failure happened while waiting for imported output.
The failure path also retained the default `test_mode = full` even though the
suite was invoked with `-SkipLaunch`.

The timeout report should include:

- import completion/error state
- current root and recursive file counts
- missing expected filenames
- relevant setup/import log output

Infrastructure failures should not replace the last known product result.
The run also contained transient ADB-daemon failures while staging `d2 mac`;
those retries eventually recovered.

### 8. Unrelated workspace change

`.vscode/settings.json` merely moved `temp/` from `files.exclude` to
`search.exclude`. It is unrelated to the CD run.

## Recommended repair order

1. Stop forced spec regeneration from serving as the comparison oracle. Keep
   oracle refresh as an explicit reviewable operation.
2. Make saved evidence monotonic: file-only results must not erase full-launch
   results, and infrastructure failures must not replace product results.
3. Fix `d2 mac` by removing the whole-installer 128 MiB buffering requirement,
   then restore and independently verify the committed 17-file oracle.
4. Add an explicit setup import state (`running`, `complete`, `failed`) and use
   it instead of stable-file polling.
5. Support arbitrary recognized custom mission files during level-pack import,
   then rerun the two focused level-pack cases.
6. Correct failure names and timeout diagnostics.
7. Rerun the OEM/Vertigo skips after explicit completion signaling.
8. Run full launch checks separately and preserve them as stronger evidence.

## Acceptance guidance for the current diff

Do not accept the generated spec diff as a baseline update.

- Reject the `d2 mac` oracle changes.
- Reject all `full` to `file_only` evidence downgrades.
- Reject the two timeout results and the OEM/Vertigo partial verification
  counts as durable product evidence.
- Retain the strengthened level-pack expected-file lists from `a965dea2`.
- Re-evaluate the two credible file-only improvements after runner completion
  signaling is fixed.
