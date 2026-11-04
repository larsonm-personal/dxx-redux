# D2X readiness: sampled load/tick

The separately visible `D2X readiness (sampled load/tick)` status measures the
fraction of a fixed eight-level cohort that loads the original advertised level
and executes 120 unassisted simulation frames at 60 Hz without player death or a
level transition. It is not campaign completion, rendering, combat qualification,
or a statistical estimate of all D2X-XL compatibility. No route controller, robot
removal, invulnerability, key grants or edited level data are used for this metric.
The headless player has no input and difficulty is 2; engine RNG seed is 1.

`android/tests/xl_readiness_inventory.json` records 1,111 advertised normal/secret
level entries across all 23 archives in `missions/d2xxl_downloads` at assets commit
`9dad21bd65ba05665ac6fe315f37fe7461e85725`. This includes classic/imported and
multiplayer content, and repeated levels in different packages count as separate
entries. An advertised entry is not evidence that its level exists or will load.
Archive and descriptor hashes are verified before execution. Staging preserves
resource bytes and removes competing descriptors from the mission search tree.

The fixed cohort contains one level from each of the four newly supplied campaigns
(Boiling Point, Dinter, Sphere and Warp Shock), plus one level from each of four
other archives. Archives, then missions and levels, use a published SHA-256 ranking
with seed `d2x-readiness-cohort-v1`. The inventory is enumerated without engine
admission or previous success filtering. Membership is committed and independent
of candidate SHA. Failures are never replaced; a cohort change requires explicit
review and starts a new comparison series. Eight cases are only a small coverage
sample, so the report gives inventory coverage and the new-campaign result too.

Unsupported formats, failed loads/simulation, crashes and timeouts count as
unsuccessful cases in the full eight-case denominator. Infrastructure failures and
untested entries make the percentage unavailable, while retaining confirmed X/8.
A checkpoint receipt is written before execution and after each case so an
interruption does not erase remaining cases. Failed load runtime classification is
unknown; successful classic/imported and XL-active paths are reported separately.
These distinctions prevent classic successes from looking like XL-specific passes.

After a successful load/tick probe, a separate process attempts the existing
assisted route to segment 20 (or the last segment for a smaller level), requiring
at least three visited segments. This auxiliary outcome never raises the primary
percentage and an auxiliary infrastructure error never erases a proven load/tick.
Each load/tick process has a 45-second watchdog; each assisted route has 15 seconds
wall time and 30 seconds simulation time. The worst-case probe budget is eight
minutes plus download, extraction and build. No automatic retries or rerolls occur.

The trusted workflow revalidates the approved author's exact same-repository PR
head before fetching assets. It stages hash-verified inputs, removes checkout
credentials and the private checkout before building candidate code, and publishes
only bounded classifications. Candidate code can read staged assets under the
existing explicit author trust assumption. A fresh trusted reporting job verifies
the current PR head and cohort before publishing. The measurement process returns
success when measurement is complete, even when compatibility is poor; the
separate readiness status and report job fail unless all eight cases pass. Original
anchor regression status remains separate. No branch protection setting changes.
The new controller jobs take effect only after this workflow change reaches the
trusted default branch through normal review; they cannot self-activate from a PR.

## Local reproduction

With the pinned private assets checkout and verified combined D1/D2 retail data:

```sh
python android/tests/xl_readiness.py run \
  --engine BUILD/main/dxx-redux-d2-headless-route \
  --assets PRIVATE_ASSETS --data RETAIL_RUNTIME \
  --catalog android/tests/xl_readiness_inventory.json \
  --work temp/readiness-new --seven-zip /path/to/7z --target-sha HEAD_SHA
python android/tests/xl_readiness_report.py \
  --receipt temp/readiness-new/results.json \
  --catalog android/tests/xl_readiness_inventory.json --local-sha HEAD_SHA
```

Build the executable from `d2` with `BUILD_TESTING=ON`. Keep raw logs and extracted
assets private. Only the bounded `results.json` is intended for publication.
Camera addon subchecks are outside this metric; the separate native regression
receipt reports whether they ran. Full-level HXM robot admission remains a separate
behavioral regression assertion, not a readiness percentage contribution.
