# XL gameplay tranche, November 13, 2026

This is the wind-down report for the run started at about 07:30 PST. The user
initially authorized fifteen hours, then at about 15:17 requested feature wind-down
and CI repair/monitoring until 22:00 because weekly quota was nearly exhausted.
Work remains sequential, without agents. No merges or upstream writes are authorized.

## Delivered implementation

The incoming resource path now connects retained native HAM ownership to real
MissionDefinitions and XlPreparedResources. Successive layers add incoming VHAM,
explicit target source leases, retained native bank records, target POG decisions
and pixel ownership, target sample metadata and prepared named-audio publication,
required target resources, and native bitmap translations. Canonical owners cover
equal-count target independence, source changes, malformed input, moves, discard,
and continued outgoing gameplay. Publication is bounded: native PIG/palette/GPU,
native PCM bank replacement and whole-world restore are not completed.

Repeated preparation originally exceeded the existing preview deadline. PR #480
fixes that cause with byte-verified immutable bank reuse and bounded optional model
directory searches. No deadline or required test was removed. Earlier failing
receipts at PRs #478/#479 remain historical failures, not green results.

Developer checkpoint recordings now retain native source identities required by
Android. Native route notifications are consistent across planner builds. Replay
settings capture the native Guide-Bot message preference, eliminating the remaining
Android world-state mismatch without deleting observer fields.

Published work is in [stack 471](https://github.com/larsonm-personal/dxx-redux/stack/471).
The latest implementation is [PR #493](https://github.com/larsonm-personal/dxx-redux/pull/493),
`845500bad07847b97d44c0873abc40371299885c`, based on PR #492
`f4d83ef75a6c1a5a7ae9795954d9e5ae8b54c6b3`. The full per-layer chronology, source
identities, failed receipts and qualification results are in the
[execution ledger](xl_gameplay_execution_20261110.md), beginning with the November 13
tranche. No single short replay establishes catalog playability or full G2.

## Current local qualification

- Windows/Linux canonical water capture and replay pass. Windows ordinary
  developer-OFF cold playback passes all 1,235 object frames, 1,239 world records,
  196 SIM draws and 1,808 FX draws, plus terminal equality and normal exit.
- Android optimized Debug probe passes the same object/world/SIM comparisons and
  terminal result in 64.541 seconds. Strict FX comparison still FAILS: 1,500 extra
  startup starfield draws, 3,308 actual versus 1,808 captured. No events are stripped.
- Ordinary three-ABI Android APK, developer probes OFF, passes the existing water
  owner 59/59 in 33.257 seconds. This is bounded assisted gameplay, not a campaign.
- Windows D1/D2 and Linux D2 parser/recorder controls pass. Native Guide-Bot live
  navigation and save-continuity controls pass on both hosts. Linux save-heavy
  testing first timed out on /mnt/c; identical native cases pass on the Linux
  filesystem under the unchanged deadline. Both receipts are retained.
- Current birth and cold-birth continuation pass. `birth-atomic` still FAILS:
  rejection of a corrupt XL trailer changes energy 91 to 100 (5963776 to 6553600).
  This is the known whole-save mutation defect, not successful atomic rejection.
- Automation membership remains 428 top-level / 547 support entries. Catalog and
  scoped quality checks pass. Authentic fixtures, assertions and timeouts remain intact.

Latest binary SHA-256 identities, built from the implementation subsequently
committed as PR #493:

| Artifact             | SHA-256                                                          |
| -------------------- | ---------------------------------------------------------------- |
| Windows metadata     | a8eec7bd29342d5e26ef229a376051a4663b0ab34139a3a1947d92fc295ae4e4 |
| Linux metadata       | a86ca1ac1ef288c9e317dc4f7052d414cf9a013af6ece0ca2efd9230c2f70dda |
| Ordinary Windows D2  | 0d68dacdc590a32784364093357ba75155b578b1152eb03853ce255c0d623eee |
| Ordinary Windows D1  | 5ae0ccfa8e86f35ea21bbb21501d13d442e183f99a4faa80c0efb1368e05af6e |
| Android probe APK    | 024b3933f8c6e6f62326d6f04dd46e9ced440f49df1be8e3a20ac69d86a35603 |
| Ordinary Android APK | c22bef362c0a6eb8a6347056544972aa8ebfbc3c841308b4f0aad0639b8ebe8f |
| Fresh recording      | bdfd20e2f8390f4176c243c6a75d8ebed3e87ef93da69d39a5e4a93ef833657e |
| Arena HOG            | adbfea019c1ac29ac922e15876c22ca6ce30eb02d598a98e6ef7dfdce2b2dd3e |
| Arena descriptor     | 14150755f14e210b284aa03c45d75e1b2f96be7b567bbc324bf4986b96928a71 |
| Neptune HOG          | 4016618d24238803135dabf2b33daee2a2c34a670c3f016d94ae4639c8887303 |

Evidence root: `temp/xl-resource-tranche-20261113`. Latest families are
`water-message-{windows,linux}`, `replay-message-water-clean`,
`android-water-message-result`, `android-water-message-comparison`,
`message-preference-identities.json`, and `birth-message-current-02`.
The explicit `-xl-water-physics` assist remains disclosed. The protected Android
profile manifest is unchanged before/after testing, SHA-256
`762f6f62af24134580d9c530da161d9276ea426fa3fca40ad44549d21e621f21`.

## CI at wind-down entry

PR #492 applicable public checks pass. Its private run
[43521768238](https://github.com/larsonm-personal/dxx-redux/actions/runs/43521768238)
passes the exact candidate in 858.281 seconds: 113 required owners, 16 format
witnesses and three modern source levels. Controller:
`bf40de9bc3b85c2279fe8163c621bb6c052dec12`; private asset commit:
`8c9988a96a7e306f8aac04c85e2610492917b7ac`; manifest:
`527427b5490e873c651b833d048e3f95fc13cb3306a92a452218ada17f9a95c9`.
Camera remains `not_run_missing_addon`, requiring its pinned external addon.
Selected lane/platform and scheduled-only progress skips are intentional exclusions.
They do not qualify omitted platform or camera owners.

PR #493 is still running at this entry. Its recorded merge candidate is
`34ce1a231fffaafd474caba97d8b535c0a55cd1a`. Final-head CI and any repair results
will be appended below before handoff. Green dispatch is not qualification.
Readiness remains separately FAIL at 4/8 sampled load/tick, with only two navigation
confirmations; the user explicitly leaves that result open during this wind-down.

## Preserved unfinished work and remaining scope

The untested native sound-bank implementation was removed from the working tree
and retained as stash `9783012e2a77dfdd4680812ae72b852cfaca5e17`, named
`WIP native sound bank owner; preserved for quota wind-down 20261113`, on branch
`xl_gameplay_20261113_28_native_sound_banks`. It contains five files and is not a
qualified deliverable. Prior stashes remain unchanged. Review and integrate it
only during an explicitly authorized next feature window; do not blindly apply.

Full G2 remains OPEN: prepare native PCM/PIG/palette/GPU owners, target geometry,
objects, player/AI/runtime state, clocks and RNG before a single publication
boundary. Current malformed-trailer rejection still mutates live state. G3-G7,
unassisted connected progression and catalog-wide 90% playability remain open.
The conservative catalog denominator is 1,110 with threshold 999, not 999 proven
successes. Remaining G2 estimate is 16-32 focused hours with low confidence;
remaining engineering approximately 60-120 hours, and catalog qualification is
additional and unestimated. Use the next plan and kickoff linked below.

- [Next window](xl_gameplay_next_window_20261113.md)
- [Copy/paste kickoff](xl_gameplay_kickoff_20261113.txt)

## Terminal CI receipt, November 13 at 16:21 PST

Both implementation PR #493 and documentation [PR #494](https://github.com/larsonm-personal/dxx-redux/pull/494)
have terminal results with no pending checks and no failure other than the unchanged
sampled readiness. PR #493 has 31 passed checks, eight intentional skips and one
readiness failure. PR #494 has 33 passed checks, six intentional skips and one
readiness failure. This receipt is for documentation candidate
`39a6a440e76bfed5e1e57abe7ace0001d4b5fb88`, merge candidate
`8ccdaaaa2608e8059ba4f00e17e03888213723c4`. The documentation-only commit adding
this receipt will trigger fresh checks and is monitored separately through 22:00.

| Required lane on PR #494                                                     | Result and workflow                                                                                    |
| ---------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------ |
| Native public save build, native save/replay integrity, headed replay parity | PASS, [43526135402](https://github.com/larsonm-personal/dxx-redux/actions/runs/43526135402), attempt 1 |
| D1/D2 native tests, Linux ASan, CI policy                                    | PASS, [43526135521](https://github.com/larsonm-personal/dxx-redux/actions/runs/43526135521), attempt 1 |
| Android, Linux and JVM tooling owners and finalizer                          | PASS, [43526135530](https://github.com/larsonm-personal/dxx-redux/actions/runs/43526135530), attempt 1 |
| Two-player LAN and required final gate                                       | PASS, [43526135527](https://github.com/larsonm-personal/dxx-redux/actions/runs/43526135527), attempt 1 |
| Windows/Linux XL contracts and actual developer source compilation           | PASS, [43526135454](https://github.com/larsonm-personal/dxx-redux/actions/runs/43526135454), attempt 1 |
| Windows/Linux scripts, catalog and workflow syntax                           | PASS, [43526135414](https://github.com/larsonm-personal/dxx-redux/actions/runs/43526135414), attempt 1 |
| Exact-head private asset qualification                                       | PASS, [43526156095](https://github.com/larsonm-personal/dxx-redux/actions/runs/43526156095), attempt 1 |
| Sampled load/tick readiness                                                  | FAIL, 4/8, retained separately                                                                         |

Private PR #493 run [43524367626](https://github.com/larsonm-personal/dxx-redux/actions/runs/43524367626)
passes its exact implementation head in 987.858 seconds. PR #494 private execution
passes its exact documentation head in 765.606 seconds. Each has 113 required
owners, 16 format witnesses, three modern source levels and four assisted anchors.
Controller and private asset identities match the preceding receipt. Exact-target
artifacts are retained as `pr493-private-results` and `pr494-private-results` under
the tranche scratch root. All required members and their terminal result are in
the `results.json` artifact; camera remains `not_run_missing_addon`.

Skip explanations: the separate Release probe is restricted to two historical
Pumo branches; Rust server work has no changed server input; pinned Pumo staging
and retained online receipts are manual-only modes; LAN timing export requires an
explicit evidence run; progress persistence is scheduled/manual-only. None is a
failure-blocked dependent on PR #494. PR #493's two later replay jobs were excluded
by the fresh stack-tip gate after PR #494 was linked; both actually PASS on #494.
No skipped required owner is counted as pass.

No new code fix was required during this first wind-down cycle. No merges or
status overrides were performed.

## Final user-directed closure and readiness estimate

At about 19:22 PST the user ended the hourly-watch requirement once all checks
except readiness passed, and requested the next plan/prompt plus an estimate for
8/8. The owned monitoring process was stopped; no unrelated process was stopped.
The final implementation and documentation source at that time,
81b4046f7bb61410ba85fd0da87504d294561ffd, has 33 passed checks, six intentional
skips, no pending jobs and only the known readiness failure. Merge candidate:
d0616118745485cbb8b6d8a951cb841d8d1453fd. Exact-head private
[run 43533928032](https://github.com/larsonm-personal/dxx-redux/actions/runs/43533928032)
passes attempt 1 in 1183.281 seconds: 113 required owners, 16 format witnesses,
three modern source levels and four assisted anchors. Controller and asset
identities are unchanged. Camera remains not_run_missing_addon. Final receipts:
pr494-final-checks.json, pr494-final-private-results/results.json and
pr494-readiness/results.json under the tranche scratch root.

Estimate for the specific unchanged sampled test: **three 12-hour tranches as a
planning budget, range two to four (24-48 hours), low confidence**. All four
remaining rows fail before the 120-frame simulation: BoilingPoint level 7,
Dinter level 1, Sphere level 2 and WShock20 level 6. The public receipt omits
private engine diagnostics; it cannot establish each underlying cause. Start
the next tranche with exact ordinary headless reproduction and revise the range.
This estimate is separate from full G2, connected gameplay and 90% playability.
The next plan and kickoff now carry the readiness-first sequence and safeguards.
This final documentation adjustment receives its own CI verification after push.
