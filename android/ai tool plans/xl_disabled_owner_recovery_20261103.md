# Pumo disabled-owner recovery, November 3 2026

Continue the authorized compatibility goal until November 3 2026 19:00 PST
(November 4 03:00 UTC); stop new implementation at 18:40 PST. Read the superseding
version-policy handoff temp/play-version-floor-20261103/final_handoff.md and the
ordered overnight plan. Do not restore rewritten history or use the recovered
play_deployed_lineage branch as a development base.

Current base is refreshed PR335 fc53ba621cf7329ff793c93c404bb32c2e463f58, after the
user rebased the unmerged PR332/333/335 stack onto cmake 827765f02a6d92fb60694cf253830ec006b30cb8.
The merged Play floor is authoritative: normal uploads use max(count*10, Play+1).
New child xl_pumo_disabled_owner_recovery; preserve the stale scratch child and
its pending docs, all three stashes, original assets and previous bundles.

## Question and gates

Does ordinary Load Last Save reject a real imported Pumo save while its owner is
disabled, preserve stock save bytes before restoration, and recover after normal
re-enable? The scratch proof on APK29470 succeeded; promote it into the existing
protected integration owner without changing engine admission or resources.

1. Add PumoUiDisabledOwner to test_xl_addon_import, exclusive with cold/isolation
   modes. Reuse the original UI callback import and normal toggle controls. Seed
   the existing isolation script only through its first stock manual save
2. Run registered negative/recovery support scripts through ordinary launcher
   Load Last Save. Require fresh, complete native receipts and retain errors,
   final state, renders and stock-bank hashes after each case before rollback
3. Preserve the full profile with the existing verified offloaded backup/finally.
   Test public addon controls and the new original-resource mode serially on the
   re-identified owned emulator. Classify harness failures separately
4. Replace the observed obsolete ADB missing-files hint with normal launcher UI
   import/demo guidance. Validate actual cold launcher UI on the committed APK
5. Scoped mixed-language quality, both catalogs and diff checks; commit/push a
   roughly one-hour child PR using fork-only gh stack, no merges. Build a current
   head universal candidate, verify all six IDs/package/version/hash and test the
   exact APK. Preserve earlier bundles and deliver promptly with explicit limits

Next priority: full-original PMC inner gates, earned keys and ordinary robot
combat/contact/gunpoints. Existing opening/catalog/disabled-owner results do not
prove those interactions. Keep the bounded rendered GuideBot unsupported actor
truth; inspect rendered-guidebot.md before any changed route.

All original overnight constraints continue: no proprietary bundled assets, no
DXA builder/distributor, tap APK/UI import, no Vertigo substitution, no mission
byte edits, grants, forced exits or weakened guards. Preserve stock saves,
loading/import progress, briefing sound/pages and errors. Phone permission is
read-only; device tests serial/protected with hash restoration and owned cleanup.
No subagents. Record CI links without waiting. Every checkpoint includes changes
and reasons, artifacts/IDs/hashes/UI instructions, tested/untested scope, exact
stack/targets/heads, checks/pending CI, preservation/cleanup, ranked next tasks
and a fresh-context prompt carrying all constraints and this repeating rule.
Future cutoff stays [USER-SUPPLIED ABSOLUTE CUTOFF AND TIME ZONE] until supplied;
do not automatically start a following goal.

## Afternoon first-gate outcome

Public addon control passes with protected restoration. New reusable full owner
passes on unchanged APK29470:29seed/26negative/9recovery, fresh IDs and stock-bank
hash equality before rollback; whole protected manifests remain FEF76174...faf2.
See evidence/owned-model-android.md for exact IDs/scope. First attempt's storage
preflight failure is retained separately, with verified restoration. Only one
old owned-device duplicate transfer was removed after a SHA-identical host copy.
No guard or original resource bytes changed. Current-head APK/UI verification
is still required before delivery. Next is original full-model PMC inner gates.
